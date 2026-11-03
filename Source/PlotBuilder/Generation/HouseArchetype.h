#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HouseArchetype.generated.h"

class ABuildablePiece;

/** Roof the generator puts on top of the last storey. Gable joins on Thursday with the two-storey house. */
UENUM(BlueprintType)
enum class EHouseRoof : uint8
{
	None,
	/** One slab (FlatRoofClass) per room cell, straight on the walls. */
	Flat
};

/**
 *  One house type as data (GDD §9): footprint bounds, number of storeys, which piece classes to use,
 *  how often walls get openings. AHouseGenerator reads it and nothing else, so a new house type is a
 *  new Data Asset, never new code. Sizes are in room cells (200 cm, two grid units).
 */
UCLASS(BlueprintType)
class UHouseArchetype : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Smallest footprint, in room cells (X = width along the plot's X axis, Y = depth). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "1"))
	FIntPoint MinRoomCells = FIntPoint(3, 3);

	/** Largest footprint, in room cells. The generator also clamps to what the plot can hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "1"))
	FIntPoint MaxRoomCells = FIntPoint(5, 5);

	/**
	 *  The footprint is centred on the plot, then moved by a seeded random number of room cells within
	 *  +-this on each axis (and never out of the plot). 0, 0 = always centred.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	FIntPoint PlacementJitter = FIntPoint(0, 0);

	/** Number of storeys, the ground floor included. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "1"))
	int32 Floors = 1;

	/** A reinforcement slab replaces the normal floor slab every N storeys (GDD §8, D28). 0 = never. Irrelevant under 2 storeys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	int32 SubAnchorInterval = 0;

	/** Ground floor and foundation in one piece (one per room cell). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> FoundationClass;

	/** Floor slab of the upper storeys (one per room cell and storey). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> FloorSlabClass;

	/** Reinforcement slab used instead of FloorSlabClass every SubAnchorInterval storeys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> SubAnchorSlabClass;

	/** Plain exterior wall. Also the fallback when an opening class below is left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> WallClass;

	/** Exterior wall with a door opening. One per house, on the ground floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> DoorWallClass;

	/** Exterior wall with a single window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> WindowWallClass;

	/** Exterior wall with the large window (160 x 200, piers on each side). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> LargeWindowWallClass;

	/** Exterior wall with the full-width window (200 x 200). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> WideWindowWallClass;

	/**
	 *  Piece laid on every room cell above the last storey for RoofVariant = Flat: use the floor slab.
	 *  Its FloorBottom sockets rest on the walls' WallTop. The roof tiles don't work here: even the flat
	 *  one (BP_Roof_Flat) has its RoofLow sockets at RoofStep 1, made to join the RoofHigh of a slope
	 *  tile, not to sit on a wall top.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> FlatRoofClass;

	/**
	 *  Chance, per exterior wall segment (door excluded), of each opening. Rolled once per segment in
	 *  this order: wide, large, single; whatever is left is a plain wall. Keep the three below 1 in total.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings", meta = (ClampMin = "0", ClampMax = "1"))
	float WideWindowChance = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings", meta = (ClampMin = "0", ClampMax = "1"))
	float LargeWindowChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openings", meta = (ClampMin = "0", ClampMax = "1"))
	float WindowChance = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roof")
	EHouseRoof RoofVariant = EHouseRoof::Flat;
};
