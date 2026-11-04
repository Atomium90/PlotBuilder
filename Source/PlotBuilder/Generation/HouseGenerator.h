#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseGenerator.generated.h"

class ABuildablePiece;
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
};

/**
 *  Builds a house in a plot from a UHouseArchetype and a seed (GDD §9). It places every piece through
 *  APlot::PlacePiece, the same function build mode uses, so a generated house is a normal house for the
 *  structural graph: it can be demolished, it collapses, it shows in the heat-map.
 *
 *  Deterministic: one FRandomStream seeded from Seed drives every choice (never the global random
 *  functions, which a seed can't control), so the
 *  same seed and archetype always give the same house. GetLayoutHash() is a fingerprint of the result
 *  (class, cell, orientation of every piece, in emission order) to check that.
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

	/** Seed of the last Generate call (also the value used by the editor-placed default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 Seed = 1;

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
	 *  Clear(), then builds a house from InSeed. Once everything is placed, the whole structure is
	 *  checked with UStructuralSupportComponent::FindUnstablePieces. Returns true if the house stands;
	 *  false if it could not be placed or would collapse, in which case nothing is left in the world
	 *  (retrying with another seed is the caller's job for now).
	 */
	UFUNCTION(BlueprintCallable, Category = "Generation")
	bool Generate(int32 InSeed);

	/** Unregisters from the structural graph, then destroys, every piece this generator created and still owns. */
	UFUNCTION(BlueprintCallable, Category = "Generation")
	void Clear();

	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetPieceCount() const { return Pieces.Num(); }

	/** Fingerprint of the current house (0 when empty): same seed and archetype, same value. */
	UFUNCTION(BlueprintPure, Category = "Generation")
	int32 GetLayoutHash() const { return static_cast<int32>(LayoutHash); }

private:

	/** Whole emission: footprint, foundation, then storey by storey the slabs and walls, then the roof. False if a piece could not be placed. */
	bool EmitHouse(FRandomStream& Stream);

	/**
	 *  Cuts one storey's footprint into rooms and places the partition walls (GDD §9.2, step 4). The
	 *  footprint is cut in two along its longer axis, then each half again, until no room is bigger than
	 *  the archetype's MaxRoomArea; if that leaves fewer than MinRooms, the biggest rooms are cut again.
	 *  Every cut gets one door, so the rooms form a tree and all of them are reachable. Without both
	 *  interior classes in the archetype the storey stays one room. False if a piece could not be placed.
	 */
	bool EmitPartitions(int32 Level, const FRoomRect& Footprint, FRandomStream& Stream);

	/** Draws every room as a coloured flat box with its number and size, for UnstableMarkerSeconds. */
	void DrawRoomsDebug() const;

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

	/** Rooms of the current house, every storey, in emission order. Cleared with the house. */
	TArray<FRoom> Rooms;

	/** Kind of every wall edge placed, keyed by grid (X, Y, storey). Cleared with the house. */
	TMap<FIntVector, EEdgeKind> Edges;

	uint32 LayoutHash = 0;

	/** Footprint chosen by the last EmitHouse (room cells), for the result line. */
	FIntPoint LastSize = FIntPoint::ZeroValue;

	/** Side the last door went on ("+X", "-X", "+Y", "-Y"), and whether FacingTarget chose it, for the result line. */
	FString LastDoorSide;
};
