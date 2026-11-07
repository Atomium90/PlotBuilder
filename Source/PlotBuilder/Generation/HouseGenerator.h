#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FurnishingRuleRow.h"
#include "HouseArchetype.h"
#include "HouseGenerator.generated.h"

class ABuildablePiece;
class APhysicsProp;
class APlot;
class UHouseArchetype;

/** Block of room cells, inclusive on both ends. Room cell i covers grid X from 2i - 1 to 2i + 1 (same on Y). */
struct FRoomRect
{
	int32 MinI = 0;
	int32 MinJ = 0;
	int32 MaxI = 0;
	int32 MaxJ = 0;

	int32 Width() const { return MaxI - MinI + 1; }
	int32 Depth() const { return MaxJ - MinJ + 1; }
	int32 Area() const { return Width() * Depth(); }
};

/** What stands on one edge of the room grid: the furnishing step reads this to keep the way in front of doors clear. */
enum class EEdgeKind : uint8
{
	ExteriorWall,
	ExteriorDoor,
	Partition,
	PartitionDoor
};

/** One room of the generated house: a rectangle of room cells on one storey. */
struct FRoom
{
	FRoomRect Rect;
	int32 Level = 0;

	/** What the room is for: the plan's legend (after the seed's swaps), or for the random split the room's rank by area. */
	ERoomType Type = ERoomType::LivingRoom;

	/** The plan's letter for this room, empty for a room of the random split. */
	FString Label;
};

/**
 *  One flight of stairs of the generated house, in world room cells. Along the climb the cells go Approach,
 *  First, Second, Landing: the flight (BP_Stairs, 2 cells long) covers First and Second on Level, climbs one
 *  storey, and leaves an open stairwell in the floor above over those same two cells. The Approach is the
 *  cell in front of its foot, the Landing the floor cell the top steps onto.
 */
struct FStairRun
{
	int32 Level = 0;
	FIntPoint Approach = FIntPoint::ZeroValue;
	FIntPoint First = FIntPoint::ZeroValue;
	FIntPoint Second = FIntPoint::ZeroValue;
	FIntPoint Landing = FIntPoint::ZeroValue;

	/** Where BP_Stairs goes: the grid edge between Approach and First, on Level. */
	FIntVector Pivot = FIntVector::ZeroValue;

	uint8 QuarterTurns = 0;
};

/**
 *  Builds a house in a plot from a UHouseArchetype and a seed (GDD §9). It places every piece through
 *  APlot::PlacePiece, the same function build mode uses, so a generated house is a normal house for the
 *  structural graph: it can be demolished, it collapses, it shows in the heat-map.
 *
 *  Deterministic: one FRandomStream seeded from Seed drives every choice (never the global random
 *  functions, which a seed can't control), so the
 *  same seed and archetype always give the same house. GetLayoutHash() is a fingerprint of the result
 *  (class, cell, orientation of every piece, then class, position, yaw of every prop, in emission
 *  order) to check that.
 *
 *  Once the house stands, FurnishRooms fills each room with physics props (APhysicsProp) from the
 *  archetype's DT_FurnishingRules. Props are plain physics actors, outside the structural graph.
 *
 *  Place one in the level, give it the plot (or leave it empty to use the first APlot found) and an
 *  archetype, then call Generate from Blueprint or with the pb.GenerateHouse console command.
 */
UCLASS()
class AHouseGenerator : public AActor
{
	GENERATED_BODY()

public:

	AHouseGenerator();

	/** Plot the house is built in. Empty = the first APlot found in the level when Generate runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	TObjectPtr<APlot> Plot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	TObjectPtr<UHouseArchetype> Archetype;

	/** Archetypes pb.SpawnHouse can switch to by name (a part of the asset name is enough: "bungalow", "storey"). Fill with the project's archetypes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	TArray<TObjectPtr<UHouseArchetype>> KnownArchetypes;

	/** Seed of the last Generate call (also the value used by the editor-placed default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 Seed = 1;

	/**
	 *  Storeys the next houses must have, 0 = not forced (the archetype's default, or the seed's draw if bRandomFloors).
	 *  Clamped to the archetype's MinFloors..MaxFloors. An archetype with no range ignores it, since its plans have a fixed
	 *  number of storeys. The mission sets it from its Floors option (Floors=8); pb.SpawnHouse from its third argument.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0", ClampMax = "30"))
	int32 FloorsOverride = 0;

	/**
	 *  The seed draws the number of storeys in the archetype's MinFloors..MaxFloors, so the same archetype gives blocks of
	 *  different heights. FloorsOverride wins over it. Off = the archetype's default count. The draw happens only for an
	 *  archetype with a range, so the houses of every other archetype stay exactly as they were.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	bool bRandomFloors = false;

	/**
	 *  Where the player arrives from (the truck drop-off in a mission). When set, the door goes on the
	 *  side of the house that faces it: the face the line from the house centre to this actor leaves
	 *  through. Empty, or standing inside the footprint, the door can be on any side.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	TObjectPtr<AActor> FacingTarget;

	/** Prints a one-line result on screen after Generate, and boxes the pieces that failed validation for a few seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Debug")
	bool bShowDebug = true;

	/**
	 *  Debug: when a house fails the stability check it is left in the world instead of being cleared, and
	 *  GenerateValid stops there without re-rolling, so the rejected house can be looked at (red boxes on the
	 *  unstable pieces for a few seconds, pb.ShowStability for the values). The unstable pieces then fall as
	 *  they would in the game. The next generation clears it. pb.GenerateHouse sets it from its third argument.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Debug")
	bool bKeepUnstableHouse = false;

	/**
	 *  Debug, for testing GenerateValid: the first N attempts are declared failed after the house was
	 *  built and checked (so Clear and the re-roll path really run). Counts the fallback attempt too:
	 *  with MaxAttempts 5, 2 = built on the 3rd try, 5 = the fallback seed, 6 = nothing is built.
	 *  pb.GenerateHouse sets it from its second argument.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Debug", meta = (ClampMin = "0"))
	int32 DebugForcedFailures = 0;

	/**
	 *  Debug: build ForcedRoof on every house instead of the shape the seed rolls. The roll is still drawn, so the rest of
	 *  the house is exactly the one the seed gives. A pitched shape the footprint or the kit cannot make still falls back to flat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Debug")
	bool bForceRoof = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Debug", meta = (EditCondition = "bForceRoof"))
	EHouseRoof ForcedRoof = EHouseRoof::Gable;

	/**
	 *  Clear(), then builds a house from InSeed. Once everything is placed, the whole structure is
	 *  checked with UStructuralSupportComponent::FindUnstablePieces. Returns true if the house stands;
	 *  false if it could not be placed or would collapse, in which case nothing is left in the world.
	 *  One attempt, no retry: use GenerateValid for a house that must exist.
	 */
	UFUNCTION(BlueprintCallable, Category = "Generation")
	bool Generate(int32 InSeed);

	/**
	 *  A house that is certain to exist (GDD §9.5): tries RequestedSeed, then up to MaxAttempts - 1
	 *  seeds derived from it (hash of the requested seed and the attempt number), then the archetype's
	 *  FallbackSeed. The derivation is deterministic, so the same requested seed always ends on the same
	 *  house, whatever it took to get there. Returns false only if every attempt failed, leaving the world empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Generation")
	bool GenerateValid(int32 RequestedSeed, int32 MaxAttempts = 5);

	/**
	 *  Debug, for the Gate 2 checks: runs GenerateValid on every seed from FirstSeed to LastSeed (at most
	 *  100), each one twice, and prints a summary: how many gave a house, how many needed a re-roll or the
	 *  fallback, how many gave the same hash both times (T6), how many houses had each room type, and the
	 *  range of prop counts. The debug boxes are off during the run. Leaves the last house in the world.
	 *  pb.TestHouses calls it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Generation|Debug")
	void TestSeeds(int32 FirstSeed, int32 LastSeed);

	/** Seed the current house was really built from: the requested one, a derived one after a re-roll, or the fallback. */
	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetEffectiveSeed() const { return Seed; }

	/**
	 *  Debug: makes the first of KnownArchetypes whose asset name contains NameFragment (any case) the current
	 *  Archetype. False, with the names logged, if none matches. pb.SpawnHouse calls it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Generation|Debug")
	bool SelectArchetype(const FString& NameFragment);

	/** Unregisters from the structural graph, then destroys, every piece this generator created and still owns. */
	UFUNCTION(BlueprintCallable, Category = "Generation")
	void Clear();

	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetPieceCount() const { return Pieces.Num(); }

	/** Storeys of the current house (the ground floor included): the count it was really built with, 0 when there is none. */
	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetFloorCount() const { return ActiveFloors; }

	/** Reads "random" (the seed draws the count) or a number (forced) into the two floor settings. False, nothing changed, for anything else. */
	bool SetFloorsFromText(const FString& Text);

	/** Furniture the current house has (props are not pieces: they are not in the structural graph). */
	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetPropCount() const { return Props.Num(); }

	/** Every piece this generator placed and still owns (a stale entry is a piece already demolished: check the pointer). The contract binding reads them. */
	const TArray<TWeakObjectPtr<ABuildablePiece>>& GetPieces() const { return Pieces; }

	/** Every prop this generator spawned and still owns (a stale entry is a prop already broken). */
	const TArray<TWeakObjectPtr<APhysicsProp>>& GetProps() const { return Props; }

	/** Storey of the room Prop was placed in, 0 for a prop this generator did not place. */
	int32 GetPropLevel(APhysicsProp* Prop) const;

	/**
	 *  The current house's standing, 1 (basic) to 3 (high end), rolled from the seed when it was furnished: it
	 *  tilts every room's furniture towards rows of that quality tier. 0 for an empty house, or one with no
	 *  furnishing table. The contract's tier can read it later.
	 */
	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetStanding() const { return Standing; }

	/** Roof shape the current house really got (a pitched roll that could not be built shows as Flat). None for an empty house or a roofless one. */
	UFUNCTION(BlueprintPure, Category = "Generation")
	EHouseRoof GetRoofShape() const { return RoofShape; }

	/** Fingerprint of the current house (0 when empty): same seed and archetype, same value. */
	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetLayoutHash() const { return static_cast<int32>(LayoutHash); }

private:

	/** One generation attempt: Generate and every GenerateValid try go through here. bForceFailure = the DebugForcedFailures case. */
	bool TryGenerate(int32 InSeed, bool bForceFailure);

	/** Whole emission: footprint, foundation, then storey by storey the slabs and walls, then the roof. False if a piece could not be placed. */
	bool EmitHouse(FRandomStream& Stream);

	/**
	 *  How many storeys this house gets: FloorsOverride if set, else a draw in the archetype's range if bRandomFloors, else
	 *  the archetype's Floors. Clamped to the range when there is one. The only draw it makes, and only for an archetype
	 *  with a range and bRandomFloors, so every other house keeps its stream as it was.
	 */
	int32 ResolveFloors(FRandomStream& Stream) const;

	/**
	 *  Cuts one storey's footprint into rooms and places the partition walls (GDD §9.2, step 4). The
	 *  footprint is cut in two along its longer axis, then each half again, until no room is bigger than
	 *  the archetype's MaxRoomArea; if that leaves fewer than MinRooms, the biggest rooms are cut again.
	 *  Every cut gets one door, so the rooms form a tree and all of them are reachable. Without both
	 *  interior classes in the archetype the storey stays one room. The rooms are then typed by area
	 *  (biggest living room, smallest bathroom, second kitchen, third bedroom, then office and bedrooms).
	 *  False if a piece could not be placed.
	 */
	bool EmitPartitions(int32 Level, const FRoomRect& Footprint, FRandomStream& Stream);

	/**
	 *  Places the partition walls of one storey of a hand-written plan (GDD §9.2, step 4). Rooms must
	 *  already hold the plan's rooms, in world cells. Every boundary between two rooms gets a wall; each
	 *  pair in Links (indices into Rooms) gets one door, on a segment of their shared wall picked by the
	 *  seed. Without both interior classes in the archetype no wall is placed. False if a piece could not be placed.
	 */
	bool EmitLayoutPartitions(int32 Level, const TArray<TPair<int32, int32>>& Links, FRandomStream& Stream);

	/**
	 *  Adds support posts under the slabs that would fall (see UHouseArchetype::SupportPostClass). Runs after
	 *  the slabs and the roof are placed, before the lamps: it asks the structural graph which slabs are
	 *  unstable, puts a post at a corner of the most useful one's neighbourhood, and asks again, until
	 *  nothing a post can fix is left. Draws nothing from the seed. False only if a post could not be placed.
	 */
	bool AddSupportPosts();

	/**
	 *  Furnishes every room from Archetype->FurnishingRules (GDD §9.2, step 6), once the house is built and
	 *  validated (a rejected house never gets a prop). Draws from Stream after everything the structure used,
	 *  so furnishing never changes where a wall goes. Does nothing without a table.
	 */
	void FurnishRooms(FRandomStream& Stream);

	/**
	 *  One room: for each group of rows of the room's type it picks a row by Weight, rolls its Chance,
	 *  then places the picked props biggest first. A prop goes where its rule allows, inside the walls,
	 *  clear of the doors and of the props already placed; if nothing fits it is skipped silently.
	 *  Rows must already be in a fixed order (they are sorted by name).
	 */
	void FurnishRoom(const FRoom& Room, const TArray<const FFurnishingRuleRow*>& Rows, FRandomStream& Stream);

	/** Rolls the house's standing from Archetype->StandingWeights: always one draw, so the stream after it does not depend on the weights. */
	int32 RollStanding(FRandomStream& Stream) const;

	/**
	 *  The standing the furniture of a storey leans towards: the house's, moved by the standing of that storey's plan
	 *  (LevelStandingShifts), kept in 1..3. The house's own for a plan that does not repeat storeys, and for an empty house.
	 *  No draw: the shift is data, so the stream is the same with or without it.
	 */
	int32 StandingOfLevel(int32 Level) const;

	/**
	 *  Rolls the roof shape from the archetype's weights, tilted by the footprint (RoofShapeBias), then the axis a square
	 *  footprint's gable would take. Always two draws, so the stream after the roof does not depend on the weights or the
	 *  shape. bOutRidgeAlongX is the ridge's axis for a two-sided roof: the longer side, a draw on a square.
	 */
	EHouseRoof RollRoof(FRandomStream& Stream, int32 Width, int32 Depth, bool& bOutRidgeAlongX) const;

	/**
	 *  The ceiling slab on every cell above the last storey (a reinforcement slab, so the roof starts from a stability
	 *  reset), then the roof the seed rolled: nothing more for a flat roof, tiles and gables for a pitched one. A pitched
	 *  shape that cannot be built (no kit, a missing required piece, fewer than 2 cells across) becomes flat, with a log.
	 *  False only if a piece could not be placed.
	 */
	bool EmitRoof(FRandomStream& Stream, const FRoomRect& Footprint);

	/** Draws every room as a coloured flat box with its number and size, and every stair flight with its approach, stairwell and landing, for UnstableMarkerSeconds. */
	void DrawRoomsDebug() const;

	/** True if (I, J) on Level is a flight cell of a stair run starting on that storey (not the approach). */
	bool IsFlightCell(int32 I, int32 J, int32 Level) const;

	/** True if (I, J) on Level is the open stairwell a flight from the storey below leaves in the floor: no slab goes there. */
	bool IsStairwellCell(int32 I, int32 J, int32 Level) const;

	/** True if the grid vertex (X, Y) on Level touches a flight's footprint (closed): a post there would stand in the stairs. */
	bool IsOnFlight(const FIntVector& Vertex) const;

	/** PlacePiece plus bookkeeping (owned list, layout hash). nullptr if the plot refused it. */
	ABuildablePiece* Place(TSubclassOf<ABuildablePiece> PieceClass, const FIntVector& GridCoord, uint8 QuarterTurns, int32 RoofStep = 0);

	/** Wall class for one segment (not the door): rolls the archetype's opening chances. Always non-null once Generate validated the archetype. */
	TSubclassOf<ABuildablePiece> PickWallClass(FRandomStream& Stream) const;

	/**
	 *  Orientation of a wall on Edge at its level: the wall extends towards -X local from its pivot,
	 *  so it has to face the side that has a floor, the same rule as the build mode ghost. A vertical
	 *  edge (odd, even) is turned 0 quarter turns if the cell on its -X side has a floor, 2 if not;
	 *  a horizontal edge (even, odd) 1 or 3, with the -Y side as the reference.
	 */
	uint8 ResolveWallQuarterTurns(const FIntVector& Edge) const;

	/** Logs Line, and shows it on screen (green or red) if bShowDebug. */
	void ReportResult(bool bSuccess, const FString& Line) const;

	TArray<TWeakObjectPtr<ABuildablePiece>> Pieces;

	/** Furniture this generator spawned and still owns: Clear() destroys it with the pieces. */
	TArray<TWeakObjectPtr<APhysicsProp>> Props;

	/** Storey each prop was furnished on (the room's level), for the contract binding. Cleared with the house. */
	TMap<TWeakObjectPtr<APhysicsProp>, int32> PropLevels;

	/** Rooms of the current house, every storey, in emission order. Cleared with the house. */
	TArray<FRoom> Rooms;

	/** Kind of every wall edge placed, keyed by grid (X, Y, storey). Cleared with the house. */
	TMap<FIntVector, EEdgeKind> Edges;

	/** Grid vertex (odd X, odd Y, storey the post stands on) of every support post. The furnishing step keeps props off them. Cleared with the house. */
	TArray<FIntVector> PostVertices;

	/** Stair flights of the current house, from the plan's Stairs. Cleared with the house. */
	TArray<FStairRun> StairRuns;

	/** See GetFloorCount. Set by EmitHouse, cleared with the house. */
	int32 ActiveFloors = 0;

	/** The plans of this attempt that were expanded to ActiveFloors storeys (UHouseLayout::ExpandForFloors): kept here so they are not collected while the house is built. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHouseLayout>> ExpandedLayouts;

	/** See GetStanding. Cleared with the house. */
	int32 Standing = 0;

	/** Per storey, how many standing steps its furniture moves from Standing: copied from the chosen plan (UHouseLayout::LevelStandingShifts), empty when the plan has none. Cleared with the house. */
	TArray<int32> LevelStandingShifts;

	/** See GetRoofShape, and a few words on how it got built (ridge axis, a fallback), for the result line. Cleared with the house. */
	EHouseRoof RoofShape = EHouseRoof::None;
	FString RoofNote;

	/** Debug (pb.TestHouses): quality tiers of the furniture picked from groups that have several rows, and how many there were. Cleared with the house. */
	int32 VariantTierSum = 0;
	int32 VariantPropCount = 0;

	/** Set by TryGenerate when bKeepUnstableHouse left a rejected house in the world: GenerateValid then stops instead of re-rolling. */
	bool bKeptUnstableHouse = false;

	uint32 LayoutHash = 0;

	/** Appended to every result line of the current attempt: which attempt of a GenerateValid this is. Empty for a plain Generate. */
	FString AttemptNote;

	/** Footprint chosen by the last EmitHouse (room cells), for the result line. */
	FIntPoint LastSize = FIntPoint::ZeroValue;

	/** Side the last door went on ("+X", "-X", "+Y", "-Y"), and whether FacingTarget chose it, for the result line. */
	FString LastDoorSide;

	/** Which plan the last EmitHouse built, and how it was mirrored, or "random split", for the result line. */
	FString LastLayoutNote;
};
