#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DemolitionTypes.h"
#include "DemolitionRules.generated.h"

class USoundBase;
class UCurveFloat;
class AFragmentDebris;
class UStaticMesh;
class UGeometryCollection;

/** Per-tool multipliers for one row of the tool x material matrix - wraps a TMap so UDemolitionRules can hold one per EDemolitionTool (UE doesn't support a TMap of TMaps as a UPROPERTY). */
USTRUCT(BlueprintType)
struct FToolMaterialMultipliers
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TMap<EDemolitionMaterial, float> Multipliers;
};

/** One per-mesh fracture override (UDemolitionRules::MeshFractureOverrides) - the authored Geometry Collection to spawn instead of generic AFragmentDebris shards, plus its chunk count for UDebrisSubsystem's cap bookkeeping (a Geometry Collection is one actor, not Count separate actors, so the cap can't infer this from the spawn loop the way the generic path does). */
USTRUCT(BlueprintType)
struct FMeshFractureOverride
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	TObjectPtr<UGeometryCollection> Collection;

	/**
	 *  Authored chunk count - used as this Geometry Collection actor's weight against
	 *  UDemolitionRules::FragmentGlobalCap. Set per mesh when authoring, independently of every
	 *  other entry - a tiny piece (e.g. a corner post) can stay low, a larger one can go up to 8-10.
	 *  No code-side clamp here, unlike UDemolitionRules::ComputeFragmentCount which only governs the
	 *  generic fallback path below, never this one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	int32 FragmentCount = 6;
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

	/** Minimum random delay (seconds) before a Compromised piece fails on its own - GDD §7.6, D29. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float CompromisedDelayMin = 0.5f;

	/** Maximum random delay (seconds) before a Compromised piece fails on its own - GDD §7.6, D29. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float CompromisedDelayMax = 3.f;

	/** Placeholder warning cue played/looped while a piece is Compromised (creak) - GDD §7.6. Structural warning, not per-material like ImpactSounds, since it's about the graph finding a bottleneck, not about what's being hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	TObjectPtr<USoundBase> CompromisedWarningSound;

	/** Fixed reaction time added to every piece's collapse release time, before the per-hop spread - GDD §7.7. Not covered by MaxCascadeSpread's compression. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float BaseDelay = 0.25f;

	/** Seconds of release delay added per BFS hop from the piece(s) that just vacated the structural graph - GDD §7.7. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float HopDelay = 0.12f;

	/** Max random offset (+/-) added to a piece's hop-based delay, so same-hop pieces don't release in lockstep - GDD §7.7. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float Jitter = 0.08f;

	/** Hard cap, in seconds, on the hops*HopDelay+jitter spread of one collapse batch - big cascades get their hop delays compressed to fit, BaseDelay is untouched - GDD §7.7. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float MaxCascadeSpread = 3.f;

	/** Outward horizontal impulse strength applied to a piece the moment it detaches - away from the centroid of its still-registered neighbours (GDD §7.7 Layer A, direction bias). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float CollapseImpulseStrength = 400.f;

	/** Fragment count per piece class for fracture-on-destroy v1 (generic per-class shards, not per-mesh) - GDD §7.8. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	TMap<EPieceClass, int32> FragmentCountPerClass;

	/** Fragment tint per material, read by AFragmentDebris via Custom Primitive Data - mirrors ImpactSounds' per-material map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	TMap<EDemolitionMaterial, FLinearColor> FragmentTintColor;

	/** Seconds a fragment lives before being destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float FragmentLifetime = 6.f;

	/** Seconds before FragmentLifetime over which a fragment fades out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float FragmentFadeDuration = 1.f;

	/** Outward impulse strength applied to a fragment at spawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float FragmentImpulseStrength = 300.f;

	/** Radius around the scatter origin within which a fragment spawns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float FragmentScatterRadius = 60.f;

	/** Fragment actor class to spawn - a BP child of AFragmentDebris carrying the fade material, same C++ base + BP wrapper split as ADemolitionTool/BP_Sledgehammer etc. Falls back to the plain AFragmentDebris class (no visual fade material) if unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	TSubclassOf<AFragmentDebris> FragmentClass;

	/** Global cap on live debris actors (UDebrisSubsystem), across every fragment and per-mesh Geometry Collection combined - GDD §7.8. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	int32 FragmentGlobalCap = 300;

	/** Linear speed (cm/s) below which a settled AFragmentDebris is put to sleep - saves physics cost on fragments that stopped moving but haven't timed out yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	float FragmentSleepVelocityThreshold = 15.f;

	/** Piece mesh -> authored Geometry Collection override (Chaos Fracture Mode, 3-6 chunks) for the handful of common meshes worth a real per-mesh fracture - GDD §7.8. Falls back to the generic per-class AFragmentDebris shards (above) for any mesh without an entry here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Collapse")
	TMap<TObjectPtr<UStaticMesh>, FMeshFractureOverride> MeshFractureOverrides;

	/** FragmentCountPerClass[PieceClass], clamped to [3, 6]. Falls back to 4 (and logs a warning) if the class is missing - every class should have an entry. */
	UFUNCTION(BlueprintPure, Category = "Destruction|Collapse")
	int32 ComputeFragmentCount(EPieceClass PieceClass) const;

	/** FragmentTintColor[Material], or a neutral grey if unset - no warning, matches GetImpactSound's "missing placeholder is not an error" stance. */
	UFUNCTION(BlueprintPure, Category = "Destruction|Collapse")
	FLinearColor GetFragmentTint(EDemolitionMaterial Material) const;

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
