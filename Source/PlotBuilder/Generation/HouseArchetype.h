#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HouseArchetype.generated.h"

class ABuildablePiece;
class UDataTable;
class UHouseLayout;
class URoofKit;

/** Roof the generator puts on top of the last storey. The values are stored: only ever append. */
UENUM(BlueprintType)
enum class EHouseRoof : uint8
{
	None,
	/** Only the ceiling slab, one per room cell, straight on the walls. */
	Flat,
	/** Two slopes meeting at a ridge along the longer side, a gable on each end wall. */
	Gable,
	/** Four slopes: a hip at each corner, a ridge along the longer side or a point on a square. */
	Hip
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

	/**
	 *  Known-good seed AHouseGenerator::GenerateValid builds when every re-roll of the requested seed
	 *  failed validation. Pick one that validates for this archetype (the default is only a guess).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size")
	int32 FallbackSeed = 1;

	/**
	 *  Number of storeys, the ground floor included. With a range (MinFloors / MaxFloors below) it is the default: the
	 *  count a house gets when neither the seed's draw (AHouseGenerator::bRandomFloors) nor the mission (FloorsOverride) asks for another.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "1"))
	int32 Floors = 1;

	/**
	 *  Fewest storeys this archetype may be given, by the seed or by the mission. 0 = no range: the house always has
	 *  Floors storeys. A range only works with plans that repeat their typical storey (UHouseLayout::bRepeatTypicalStorey),
	 *  since any other plan has a fixed number of grids.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0", ClampMax = "30"))
	int32 MinFloors = 0;

	/** Most storeys this archetype may be given. Must be at least MinFloors for the range to count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0", ClampMax = "30"))
	int32 MaxFloors = 0;

	/** A reinforcement slab replaces the normal floor slab every N storeys (GDD §8, D28). 0 = never. Irrelevant under 2 storeys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	int32 SubAnchorInterval = 0;

	/** Ground floor and foundation in one piece (one per room cell). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> FoundationClass;

	/** Floor slab of the upper storeys (one per room cell and storey). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> FloorSlabClass;

	/**
	 *  Reinforcement slab used instead of FloorSlabClass every SubAnchorInterval storeys. It is also the ceiling slab
	 *  under every roof (whatever SubAnchorInterval says), so a roof always starts from a stability reset: the first
	 *  row of roof tiles rests on its edge. Empty = FlatRoofClass is used for the ceiling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> SubAnchorSlabClass;

	/** Plain exterior wall. Also the fallback when an opening class below is left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> WallClass;

	/** Exterior wall with a door opening. One per house, on the ground floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> DoorWallClass;

	/**
	 *  Flight of stairs (BP_Stairs: Edge anchor, 4 x 2 m, climbs one storey) the generator places for each
	 *  stair of a plan (UHouseLayout::Stairs). A house of two storeys or more needs it, and plans that have stairs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> StairsClass;

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
	 *  Post (BP_CornerPost, Vertex anchor) the generator adds under a slab that would otherwise fall.
	 *  Interior walls carry nothing, so a slab more than one cell from an exterior wall has no support
	 *  and a house needs a side of 4 cells or less. A post holds up the four slabs around its vertex.
	 *  After the slabs are placed the generator checks stability, and for each slab that would fall it
	 *  puts a post at one of its corners, preferring a vertex on an interior wall so the post reads as part
	 *  of the partition. Empty = no posts: a house with a side of 5 cells or more is rejected and re-rolled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> SupportPostClass;

	/**
	 *  Ceiling lamp (a buildable with the Ceiling anchor): the generator hangs one from the slab above the
	 *  middle cell of every room, on every storey. Empty = no lamps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> CeilingLightClass;

	/**
	 *  Ceiling slab laid on every room cell above the last storey, under any roof (flat or pitched), when SubAnchorSlabClass
	 *  is empty: use the floor slab. Its FloorBottom sockets rest on the walls' WallTop, and the ceiling lamps of the last
	 *  storey hang from it. The roof tiles don't work here: even the flat one (BP_Roof_Flat) has its RoofLow sockets at
	 *  RoofStep 1, made to join the RoofHigh of a slope tile, not to sit on a wall top.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pieces")
	TSubclassOf<ABuildablePiece> FlatRoofClass;

	/**
	 *  Hand-written plans (UHouseLayout) the generator picks from by seed. With one, the rooms, doors and
	 *  front door are the plan's, and the size is the plan's stretched towards MinRoomCells..MaxRoomCells
	 *  as far as the plan allows. Empty, or none usable (wrong number of storeys, too big for the plot,
	 *  failing validation) = the footprint is cut at random using MaxRoomArea and MinRooms below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms")
	TArray<TObjectPtr<UHouseLayout>> Layouts;

	/** Partition wall between two rooms, centred on the edge. Empty (with the door below) = no interior walls, the house is one big room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms")
	TSubclassOf<ABuildablePiece> InteriorWallClass;

	/** Partition wall with a door opening. Every cut between two rooms gets exactly one, so all rooms are reachable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms")
	TSubclassOf<ABuildablePiece> InteriorDoorWallClass;

	/**
	 *  A block of room cells bigger than this is cut in two (recursively). 4 = at most 2 x 2 cells, 4 x 4 m.
	 *  A smaller value gives more, smaller rooms.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms", meta = (ClampMin = "1"))
	int32 MaxRoomArea = 4;

	/** If the cuts above leave fewer rooms than this, the biggest rooms are cut again until there are enough (or none can be cut). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rooms", meta = (ClampMin = "1"))
	int32 MinRooms = 4;

	/** DT_FurnishingRules (rows of FFurnishingRuleRow): which props each room type can get. Empty = rooms are left bare. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	TObjectPtr<UDataTable> FurnishingRules;

	/**
	 *  Relative chance of each standing of a house: entry 0 is standing 1 (basic), entry 1 standing 2, entry 2
	 *  standing 3 (high end). The seed rolls one standing per house, and it decides which rows of the furnishing
	 *  table a room leans towards (see TierMismatchFalloff), so a house is coherently modest or well off.
	 *  Empty, or no positive weight = standing 2.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	TArray<float> StandingWeights = {1.f, 2.f, 1.f};

	/**
	 *  Multiplier on a furnishing row's Weight for each tier its QualityTier is away from the house's standing:
	 *  a row of the same tier keeps its Weight, one tier off gets this fraction, two tiers off its square. Only
	 *  the odds inside a group change: a group with a single row is always furnished, whatever the standing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0.01", ClampMax = "1"))
	float TierMismatchFalloff = 0.3f;

	/**
	 *  Thickness of the exterior walls, in cm. They are pivoted on their outer face, so their room-side
	 *  face is this far inside the edge line. Furniture is placed against that face.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0"))
	float ExteriorWallThickness = 20.f;

	/** Thickness of the partitions, in cm. They are centred on the edge line, so half of it sits on each side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0"))
	float InteriorWallThickness = 10.f;

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

	/** The pieces a pitched roof is built from (URoofKit). Empty = every house gets a flat roof, whatever the weights say. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roof")
	TObjectPtr<URoofKit> RoofKit;

	/**
	 *  Relative chance of each roof shape: the seed rolls one per house. All three at 0 = no roof at all. A pitched roof
	 *  needs RoofKit and a footprint at least 2 cells across the ridge, or the house falls back to the flat roof.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roof", meta = (ClampMin = "0"))
	float FlatRoofWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roof", meta = (ClampMin = "0"))
	float GableRoofWeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roof", meta = (ClampMin = "0"))
	float HipRoofWeight = 0.f;

	/**
	 *  How much the footprint tilts the roll, without ever deciding it: with s = short side / long side (1 = square), the
	 *  four-sided roof is weighted by 1 + Bias * (2s - 1) and the two-sided one by 1 - Bias * (2s - 1). A square favours the
	 *  four-sided roof, a 2:1 rectangle is neutral, a long thin one favours the two-sided roof. The flat roof is not affected.
	 *  0 = the weights alone; capped at 0.9 so no shape is ever ruled out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roof", meta = (ClampMin = "0", ClampMax = "0.9"))
	float RoofShapeBias = 0.4f;
};
