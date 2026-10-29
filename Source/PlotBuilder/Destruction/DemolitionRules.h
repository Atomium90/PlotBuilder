#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DemolitionTypes.h"
#include "DemolitionRules.generated.h"

class USoundBase;
class UCurveFloat;

/** Per-tool multipliers for one row of the tool x material matrix - wraps a TMap so UDemolitionRules can hold one per EDemolitionTool (UE doesn't support a TMap of TMaps as a UPROPERTY). */
USTRUCT(BlueprintType)
struct FToolMaterialMultipliers
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TMap<EDemolitionMaterial, float> Multipliers;
};

/**
 *  Tunable constants for demolition (UDurabilityComponent, AExplosiveCharge, ADemolitionTool),
 *  kept off to the side so they're editable without recompiling - same philosophy as
 *  UStructuralRules. Covers HP and damage-state thresholds (GDD §7.3/§7.6) and the tool x
 *  material matrix (GDD §7.5).
 */
UCLASS(BlueprintType)
class UDemolitionRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Base hit points per material - see GDD §7.3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TMap<EDemolitionMaterial, float> MaterialBaseHP;

	/** HP multiplier per piece class, applied on top of MaterialBaseHP - see GDD §7.3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TMap<EPieceClass, float> PieceClassFactor;

	/** Remaining-HP ratio below which a piece is Damaged rather than Intact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	float DamagedThreshold = 0.75f;

	/** Remaining-HP ratio below which a piece is Critical rather than Damaged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	float CriticalThreshold = 0.35f;

	/** Tool x material damage multipliers - see GDD §7.5. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TMap<EDemolitionTool, FToolMaterialMultipliers> ToolMaterialMatrix;

	/** Placeholder impact sound per material (hit feedback v1) - depends on what's hit, not what hit it, same as the damage tint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Feedback")
	TMap<EDemolitionMaterial, TObjectPtr<USoundBase>> ImpactSounds;

	/**
	 *  Fraction of a piece's own stability it still transmits to whatever rests on it, keyed by its
	 *  remaining-HP ratio - GDD §7.6's c(h): 1.0 at h >= 0.75, ~0.6 at h = 0.35, ~0.3 at h = 0.
	 *  Optional - ComputeStabilityCoupling falls back to a hardcoded approximation of the same curve
	 *  when this is unset, so an unassigned curve is a valid default, not a data error.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TObjectPtr<UCurveFloat> StabilityTransmission;

	/** MaterialBaseHP[Material] x PieceClassFactor[PieceClass]. Falls back to 100 (and logs a warning) if either key is missing from its map. */
	UFUNCTION(BlueprintPure, Category = "Destruction")
	float ComputeMaxHP(EDemolitionMaterial Material, EPieceClass PieceClass) const;

	/** BaseDamage x ToolMaterialMatrix[Tool].Multipliers[Material]. Falls back to a 1.0 multiplier (and logs a warning) if either key is missing. */
	UFUNCTION(BlueprintPure, Category = "Destruction")
	float ComputeDamage(EDemolitionTool Tool, EDemolitionMaterial Material, float BaseDamage) const;

	/** ImpactSounds[Material], or nullptr if unset - no warning, a missing placeholder sound is not an error. */
	UFUNCTION(BlueprintPure, Category = "Destruction")
	USoundBase* GetImpactSound(EDemolitionMaterial Material) const;

	/** StabilityTransmission->GetFloatValue(HPRatio) if assigned, otherwise the GDD §7.6 fallback curve (piecewise linear through (0, 0.3), (0.35, 0.6), (0.75, 1.0), flat at 1.0 beyond). */
	UFUNCTION(BlueprintPure, Category = "Destruction")
	float ComputeStabilityCoupling(float HPRatio) const;
};
