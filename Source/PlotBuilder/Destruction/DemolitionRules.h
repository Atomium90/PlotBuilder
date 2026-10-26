#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DemolitionTypes.h"
#include "DemolitionRules.generated.h"

/**
 *  Tunable constants for demolition (UDurabilityComponent), kept off to the side so they're
 *  editable without recompiling - same philosophy as UStructuralRules. Today's skeleton only
 *  covers HP and damage-state thresholds (GDD §7.3/§7.6); the tool x material matrix is added
 *  once the first tool exists.
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

	/** MaterialBaseHP[Material] x PieceClassFactor[PieceClass]. Falls back to 100 (and logs a warning) if either key is missing from its map. */
	UFUNCTION(BlueprintPure, Category = "Destruction")
	float ComputeMaxHP(EDemolitionMaterial Material, EPieceClass PieceClass) const;
};
