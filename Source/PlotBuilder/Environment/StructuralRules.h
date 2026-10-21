#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructuralRules.generated.h"

/**
 *  Tunable constants for the structural support graph (UStructuralSupportComponent), kept off to
 *  the side so they're editable without recompiling. CellSize/FloorHeight must match whatever grid
 *  the pieces are actually authored and placed on (see the greybox spec doc) - they're only used to
 *  turn a piece's world transform into an integer grid cell for the neighbor lookup.
 */
UCLASS(BlueprintType)
class UStructuralRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Stability lost crossing a vertical link (stacked on top of / resting under). */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	float VerticalLoss = 0.1f;

	/** Stability lost crossing a horizontal link (side by side) - much higher than vertical, so side-only contact alone never counts as support. */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	float HorizontalLoss = 0.6f;

	/** A piece whose stability falls below this collapses. */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	float CollapseThreshold = 0.1f;

	/** Grid cell size in cm (horizontal). */
	UPROPERTY(EditDefaultsOnly, Category = "Structural|Grid")
	float CellSize = 200.f;

	/** Floor-to-floor height in cm (vertical cell size). */
	UPROPERTY(EditDefaultsOnly, Category = "Structural|Grid")
	float FloorHeight = 300.f;
};
