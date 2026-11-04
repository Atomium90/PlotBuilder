#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Math/RandomStream.h"
#include "FurnishingRuleRow.h"
#include "HouseLayout.generated.h"

/** One letter of a layout grid and the room it stands for. */
USTRUCT(BlueprintType)
struct FLayoutRoomDef
{
	GENERATED_BODY()

	/** The single character that stands for this room in the grid rows. Not '.', not a space, unique in the layout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FString Letter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	ERoomType Type = ERoomType::LivingRoom;

	/** Rooms that share the same non-empty group may swap types when a house is built. None = the type is fixed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName SwapGroup;
};

/** A door between two rooms of a layout. The generator picks where along their shared wall it goes. */
USTRUCT(BlueprintType)
struct FLayoutLink
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FString RoomA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FString RoomB;
};

/** The grid of one storey. */
USTRUCT(BlueprintType)
struct FLayoutLevel
{
	GENERATED_BODY()

	/**
	 *  One string per row, one character per room cell (200 cm), written like the level seen from above
	 *  (Top viewport): the first row is the highest X, the first character the lowest Y. Every row of every
	 *  storey has the same length.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FString> Rows;
};

/** Block of cells in the text grid of a layout, inclusive on both ends. Row 0 is the first string, column 0 its first character. */
struct FLayoutRect
{
	int32 MinRow = 0;
	int32 MinCol = 0;
	int32 MaxRow = 0;
	int32 MaxCol = 0;

	/** True if the two blocks sit side by side with a wall between them: one starts right after the other ends on one axis and they overlap on the other. */
	bool TouchesAlongAWall(const FLayoutRect& Other) const
	{
		const bool bOverlapRows = FMath::Max(MinRow, Other.MinRow) <= FMath::Min(MaxRow, Other.MaxRow);
		const bool bOverlapCols = FMath::Max(MinCol, Other.MinCol) <= FMath::Min(MaxCol, Other.MaxCol);
		const bool bStackedRows = MaxRow + 1 == Other.MinRow || Other.MaxRow + 1 == MinRow;
		const bool bStackedCols = MaxCol + 1 == Other.MinCol || Other.MaxCol + 1 == MinCol;
		return (bStackedRows && bOverlapCols) || (bStackedCols && bOverlapRows);
	}
};

/** One room read from a layout grid. */
struct FParsedLayoutRoom
{
	FString Letter;
	int32 Level = 0;
	FLayoutRect Rect;
	ERoomType Type = ERoomType::LivingRoom;
	FName SwapGroup;
};

/** A layout checked and read into rooms. Rooms are in Legend order, so an index means the same room every time. */
struct FParsedLayout
{
	/** Size of the grid in cells: the same on every storey. */
	int32 Rows = 0;
	int32 Columns = 0;

	TArray<FParsedLayoutRoom> Rooms;

	/** Pairs of indices into Rooms, each pair joined by a door. Both rooms are on the same storey. */
	TArray<TPair<int32, int32>> Links;

	/** Index into Rooms: the room whose outer wall gets the front door. */
	int32 EntranceRoom = INDEX_NONE;
};

/**
 *  A layout after the seed's choices: grown to a size and with its types swapped, but not mirrored yet
 *  (the generator may still flip it so the front door faces the truck). Rooms are in the same order as
 *  FParsedLayout::Rooms, with their rects in the grown grid.
 */
struct FResolvedLayout
{
	int32 Rows = 0;
	int32 Columns = 0;

	TArray<FParsedLayoutRoom> Rooms;
	TArray<TPair<int32, int32>> Links;
	int32 EntranceRoom = INDEX_NONE;

	/** The seed's mirror choice, already limited to the flips the plan allows. */
	bool bMirrorX = false;
	bool bMirrorY = false;
};

/**
 *  A hand-written house plan (GDD §9.1, §9.2 step 4): the rooms of each storey as a grid of letters, which
 *  rooms are joined by a door, and which room has the front door. AHouseGenerator reads it instead of
 *  cutting the footprint at random, so the hall, the wings and the way through the house are the
 *  author's, while the size, the mirroring, the doors and the room types still vary with the seed.
 *
 *  Example, 4 rows of 5 cells (K kitchen, L living room, D bedroom, B office, S bathroom):
 *      KKLLB       Links: K-L, D-L, B-L, S-L
 *      KKLLB       EntranceRoom: L
 *      DDLLS
 *      DDLLS
 *
 *  Rules (checked by Parse, and by the editor's data validation): every letter is in the Legend and is
 *  a filled rectangle on a single storey; every Link joins two rooms that share a wall; the rooms of a
 *  storey are all reachable through the Links; the EntranceRoom is on the ground floor and reaches the
 *  outside. '.' (no room, for L-shaped footprints) is reserved but not supported yet.
 */
UCLASS(BlueprintType)
class UHouseLayout : public UDataAsset
{
	GENERATED_BODY()

public:

	/** One grid per storey, ground floor first. The bungalow has one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutLevel> Levels;

	/** Letter to room type, in the order the rooms are numbered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutRoomDef> Legend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutLink> Links;

	/** Letter of the room whose outer wall gets the front door. It must be on the ground floor and touch the edge of the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FString EntranceRoom;

	/**
	 *  Per row of the grid: how many extra copies of that row the generator may add to make the house
	 *  deeper along X (0 = the row never grows). Empty = no row grows. A copied row copies its letters, so
	 *  the rooms it crosses get bigger and stay rectangles. Applied to every storey alike.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	TArray<int32> RowStretch;

	/** Same as RowStretch, per column, to make the house wider along Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Size", meta = (ClampMin = "0"))
	TArray<int32> ColumnStretch;

	/** The generator may flip the plan along X (reverse the rows). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation")
	bool bAllowMirrorX = true;

	/** The generator may flip the plan along Y (reverse the columns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation")
	bool bAllowMirrorY = true;

	/**
	 *  Checks the plan and reads it into rooms. False, with one readable message per problem in OutErrors
	 *  (all of them, not just the first), if it cannot be built; OutLayout is then incomplete.
	 */
	bool Parse(FParsedLayout& OutLayout, TArray<FString>& OutErrors) const;

	/**
	 *  Applies the seed's choices to a parsed plan. Draw order, part of the determinism: mirror X, mirror
	 *  Y, the size along X, the size along Y, which rows then which columns grow, then the type swaps,
	 *  group by group. The size is drawn in MinCells..MaxCells (X = rows, Y = columns), kept between the
	 *  plan's own size and the most its stretch allows, and never above FreeCells (what the plot can hold;
	 *  the plan itself must already fit it).
	 */
	void Resolve(const FParsedLayout& Parsed, FIntPoint MinCells, FIntPoint MaxCells, FIntPoint FreeCells, FRandomStream& Stream, FResolvedLayout& OutLayout) const;

#if WITH_EDITOR
	/** Runs Parse so the editor's Validate Assets reports a broken plan before a house is generated from it. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
