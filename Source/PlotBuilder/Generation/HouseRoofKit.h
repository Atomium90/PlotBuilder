#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HouseRoofKit.generated.h"

class ABuildablePiece;

/**
 *  The pieces a pitched roof is built from, one role each. AHouseGenerator reads it through UHouseArchetype::RoofKit and
 *  picks the piece for each cell from the roof's shape, so a new mesh (a nicer ridge cap, a different pyramid) is a new
 *  Blueprint assigned here, never new code. One asset is shared by every archetype.
 *
 *  Every piece is a plain buildable that carries its own structural sockets; the generator only decides which class goes
 *  where, at which RoofStep and with which quarter turn. Conventions the meshes follow:
 *  - Slope climbs towards +X local; Hip has its high corner at (+X, +Y); Filler, Tent, Pyramid and HipEnd are flat-like tiles
 *    (mesh 100 cm above their pivot, RoofLow on the four sides at RoofStep 1) placed one step lower than the ring they close.
 *  - Tent has its ridge along X local; HipEnd is closed (sloping end) on its -X local side and open to +X.
 *  - GableHalf climbs towards +Y local (its MirrorVariantClass the other way); GableFill and GablePeak are 20 cm edge pieces
 *    pivoted on the outer face like the walls, sockets RoofLow (0,0), and for the fill also RoofHigh one step up.
 */
UCLASS(BlueprintType)
class URoofKit : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Plain slope, 200 x 200, one step (100 cm) of rise. Needed for any pitched roof. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	TSubclassOf<ABuildablePiece> Slope;

	/** Flat tile (BP_Roof_Flat). Needed for any pitched roof: it closes the top of an odd-width roof, and stands in for any finishing piece left empty below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	TSubclassOf<ABuildablePiece> Filler;

	/** Convex corner (BP_Roof_Hip). Four-sided roofs only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	TSubclassOf<ABuildablePiece> Hip;

	/** Ridge cap along the top strip of an odd-width roof (two slopes, or the middle of a four-sided roof's strip). Empty = Filler. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finishing")
	TSubclassOf<ABuildablePiece> Tent;

	/** The single cell where a square, odd-sided four-sided roof comes to a point. Empty = Filler. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finishing")
	TSubclassOf<ABuildablePiece> Pyramid;

	/** Closed end of the top strip of a four-sided roof (strip of two cells or more). Empty = Filler. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finishing")
	TSubclassOf<ABuildablePiece> HipEnd;

	/** Rectangle stacked under a gable's slope, one per step. Needed for two-sided roofs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gable")
	TSubclassOf<ABuildablePiece> GableFill;

	/** Triangle closing the gable on top of the fills. Needs MirrorVariantClass set (the half that climbs the other way). Needed for two-sided roofs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gable")
	TSubclassOf<ABuildablePiece> GableHalf;

	/** Triangle on top of the middle column of a gable of odd width, closing the end of the Tent. Empty = the column stops at its fills. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gable")
	TSubclassOf<ABuildablePiece> GablePeak;
};
