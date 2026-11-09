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

/** Direction a stair flight climbs, read on the text plan: Up goes towards row 0 (+X in the world), Left towards column 0 (-Y). */
UENUM(BlueprintType)
enum class ELayoutClimb : uint8
{
	Up,
	Down,
	Left,
	Right
};

/** One step of a climb in the plan's own axes: X is the change of row, Y the change of column. */
inline FIntPoint LayoutClimbStep(ELayoutClimb Climb)
{
	switch (Climb)
	{
	case ELayoutClimb::Up: return FIntPoint(-1, 0);
	case ELayoutClimb::Down: return FIntPoint(1, 0);
	case ELayoutClimb::Left: return FIntPoint(0, -1);
	default: return FIntPoint(0, 1);
	}
}

/**
 *  One flight of stairs of a plan (BP_Stairs: 2 room cells long, 1 wide, climbing one storey). The flight
 *  starts on Level and arrives on Level + 1. It needs a free cell in front of it (the approach), and the cell
 *  after its top (the landing) must be floor: so along its climb it uses four cells in a row, approach,
 *  Row/Column and the next one (the two flight cells), then the landing.
 */
USTRUCT(BlueprintType)
struct FLayoutStair
{
	GENERATED_BODY()

	/** Storey the flight stands on (0 = ground floor). It arrives on Level + 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	int32 Level = 0;

	/** Row of the first flight cell: the one right after the approach cell. 0 is the first string of the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	int32 Row = 0;

	/** Column of the first flight cell. 0 is the first character of a row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	int32 Column = 0;

	/** Which way the flight climbs from its first cell. The approach is on the opposite side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	ELayoutClimb Climb = ELayoutClimb::Right;
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

	/**
	 *  Only for the typical storeys of a plan with bRepeatTypicalStorey: how well off this storey is, 1 (basic) to 3 (high
	 *  end). It decides how often the generator stacks this storey at each height (high standings go up, see
	 *  UHouseLayout::StandingFalloff) and how far the furniture of its rooms moves from the house's own standing: 2 changes
	 *  nothing, 3 leans one tier better, 1 one tier worse. Ignored on every other grid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "1", ClampMax = "3"))
	int32 Standing = 2;
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

	/** The plan's flights, checked: in the cells of the plan's own grid. */
	TArray<FLayoutStair> Stairs;
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

	/** The plan's flights moved into the grown grid: Row and Column are the first flight cell there, the approach and landing follow from Climb. */
	TArray<FLayoutStair> Stairs;

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

	/**
	 *  One grid per storey, ground floor first. The bungalow has one. With bRepeatTypicalStorey: the ground floor, then one
	 *  or more typical storeys (2 to 5 is the intended range) that the generator draws from for every storey above it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutLevel> Levels;

	/**
	 *  Apartment-block mode: Levels holds the ground floor, then the typical storeys (Levels[1], Levels[2]...), and the
	 *  generator fills every storey above the ground floor with one of them, up to the number of storeys the house is given
	 *  (UHouseArchetype::Floors, MinFloors..MaxFloors). One plan makes a block of 3, 5 or 10 storeys, each storey drawn
	 *  from the seed (DrawTypicalStoreys). Each copy gets its own letters (the plan uses one letter per room, unique across
	 *  all the grids; the copies take free characters), its own swap groups and its own links.
	 *
	 *  All the typical storeys share the same stair core. The flights alternate, so two stairwells never sit on top of each
	 *  other: Stairs holds exactly two entries, Level 0 and Level 1, and the flight leaving storey L is the entry with
	 *  Level == L % 2 (the ground floor takes the first, the first typical storey the second, the next one the first again).
	 *  Both flights' cells must lie in the hall of every typical storey, in different rows: see ThreeStoreyA, where they are
	 *  on rows 0 and 1. Parse checks every typical storey under each of the two flights, and every pair of neighbours.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Repeat")
	bool bRepeatTypicalStorey = false;

	/**
	 *  How strongly the height steers the draw of the typical storeys. Storey s of a block of F gets a target standing that
	 *  rises from 1 (lowest storey above the ground floor) to 3 (top storey); a typical storey is drawn with a weight of
	 *  StandingFalloff ^ (distance between its Standing and that target). 1 = the height does not matter, every typical
	 *  storey is as likely anywhere; 0.5 = the high-end storey is four times as likely as the basic one at the top, and the
	 *  other way round at the bottom.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Repeat", meta = (ClampMin = "0.01", ClampMax = "1", EditCondition = "bRepeatTypicalStorey"))
	float StandingFalloff = 0.5f;

	/**
	 *  Which typical storey stands at each storey above the ground floor of a block of Floors storeys: the returned array
	 *  has Floors - 1 entries, entry i is the index into Levels minus one (0 = Levels[1]) for storey i + 1. One draw per
	 *  storey, whatever the weights. Empty if the plan does not repeat or has no typical storey.
	 */
	TArray<int32> DrawTypicalStoreys(int32 Floors, FRandomStream& Stream) const;

	/**
	 *  A copy of this plan (bRepeatTypicalStorey) as a plain plan the generator can parse and resolve like any other: the
	 *  ground floor, then one typical storey per entry of TypicalPicks (the index of the typical storey, 0 = Levels[1]), so
	 *  the copy has TypicalPicks.Num() + 1 storeys. The copy is transient and owned by Outer, whose owner must keep it
	 *  referenced for as long as it is used. Null, with the reason in OutError, if this plan cannot be expanded (not in repeat
	 *  mode, no typical storey, a flight missing, a letter used by two grids, a link across grids, out of free letters).
	 */
	UHouseLayout* ExpandForFloors(const TArray<int32>& TypicalPicks, UObject* Outer, FString& OutError) const;

	/**
	 *  Set on a plan made by ExpandForFloors: for each storey, how many standing steps the furniture of its rooms moves from
	 *  the house's standing (the typical storey's Standing minus 2; 0 for the ground floor). Empty on every other plan.
	 */
	TArray<int32> LevelStandingShifts;

	/** Letter to room type, in the order the rooms are numbered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutRoomDef> Legend;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutLink> Links;

	/** Letter of the room whose outer wall gets the front door. It must be on the ground floor and touch the edge of the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FString EntranceRoom;

	/**
	 *  The flights of stairs between storeys, by hand: a house with several storeys needs at least one per
	 *  storey change. On the ground storey the approach cell and the two flight cells must be one room; on the
	 *  storey above, the two flight cells (an open stairwell, no floor) and the landing after them must be one
	 *  room. A flight is not a room, so it has no letter and no partition around it. Mirrors and stretches keep it valid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TArray<FLayoutStair> Stairs;

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
	 *  The footprints a parsed plan can come out at (X = rows, Y = columns): OutMin is its own size, OutMax the most its
	 *  stretch allows, never above FreeCells. A plan with no stretch has OutMin equal to OutMax.
	 */
	void GetSizeRange(const FParsedLayout& Parsed, FIntPoint FreeCells, FIntPoint& OutMin, FIntPoint& OutMax) const;

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
