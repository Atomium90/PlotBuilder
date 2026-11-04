#include "HouseGenerator.h"
#include "HouseArchetype.h"
#include "PlotBuilder.h"
#include "Buildables/BuildablePiece.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/Plot.h"
#include "Environment/StructuralRules.h"
#include "Environment/StructuralSupportComponent.h"

namespace
{
	/** Fixed key so each result replaces the previous line instead of stacking. */
	constexpr int32 ResultMessageKey = 7100;

	/** How long the red boxes around unstable pieces stay up after a failed generation (the pieces themselves are cleared at once). */
	constexpr float UnstableMarkerSeconds = 15.f;

	/** The four faces of the footprint, named by the grid axis they are perpendicular to. */
	enum class EFootprintSide : uint8
	{
		MinusX,
		PlusX,
		MinusY,
		PlusY
	};

	const TCHAR* SideLabel(EFootprintSide Side)
	{
		switch (Side)
		{
		case EFootprintSide::MinusX: return TEXT("-X");
		case EFootprintSide::PlusX: return TEXT("+X");
		case EFootprintSide::MinusY: return TEXT("-Y");
		default: return TEXT("+Y");
		}
	}

	/** One exterior wall slot: its grid (X, Y) and the face of the footprint it is on. */
	struct FPerimeterEdge
	{
		FIntPoint Cell;
		EFootprintSide Side;
	};

	/** One straight partition wall dividing a block of rooms in two: a run of edge segments along one line, one of them a door. */
	struct FPartitionCut
	{
		/** True: the wall runs along Y (a vertical edge, odd grid X). False: it runs along X (odd grid Y). */
		bool bRunsAlongY = false;

		/** Grid coordinate of the line: the odd X (or odd Y) every segment sits on. */
		int32 LineCoord = 0;

		/** Room cells the wall spans along its own axis (J for a wall along Y, I for a wall along X), inclusive. */
		int32 FirstCell = 0;
		int32 LastCell = 0;

		/** The cell, within FirstCell..LastCell, whose segment is the door. */
		int32 DoorCell = 0;
	};

	/**
	 *  Cuts Rect in two across its longer side (a square flips a coin), at a seeded position kept near the
	 *  middle: a side of length L is cut after k cells with k in [ceil(L/3), floor(2L/3)], so neither half
	 *  is a thin strip. Draw order (axis if tied, position, door) is part of the determinism.
	 *  False, with the outputs untouched, if Rect is a single cell.
	 */
	bool SplitOnce(const FRoomRect& Rect, FRandomStream& Stream, FRoomRect& OutFirst, FRoomRect& OutSecond, FPartitionCut& OutCut)
	{
		const int32 Width = Rect.Width();
		const int32 Depth = Rect.Depth();

		if (Width < 2 && Depth < 2)
		{
			return false;
		}

		// Cut across X (the wall then runs along Y) when the block is wider than deep.
		const bool bCutAcrossX = Width == Depth ? Stream.RandRange(0, 1) == 0 : Width > Depth;
		const int32 Length = bCutAcrossX ? Width : Depth;
		const int32 CutAfter = Stream.RandRange((Length + 2) / 3, (2 * Length) / 3);

		OutFirst = Rect;
		OutSecond = Rect;
		OutCut.bRunsAlongY = bCutAcrossX;

		if (bCutAcrossX)
		{
			OutFirst.MaxI = Rect.MinI + CutAfter - 1;
			OutSecond.MinI = Rect.MinI + CutAfter;
			OutCut.LineCoord = 2 * OutSecond.MinI - 1;
			OutCut.FirstCell = Rect.MinJ;
			OutCut.LastCell = Rect.MaxJ;
		}
		else
		{
			OutFirst.MaxJ = Rect.MinJ + CutAfter - 1;
			OutSecond.MinJ = Rect.MinJ + CutAfter;
			OutCut.LineCoord = 2 * OutSecond.MinJ - 1;
			OutCut.FirstCell = Rect.MinI;
			OutCut.LastCell = Rect.MaxI;
		}

		OutCut.DoorCell = Stream.RandRange(OutCut.FirstCell, OutCut.LastCell);
		return true;
	}

	/** Recursive cut: a block already within MaxArea (or a single cell) becomes a room, any other is cut in two and both halves go again. */
	void SplitRoom(const FRoomRect& Rect, int32 MaxArea, FRandomStream& Stream, TArray<FRoomRect>& OutRooms, TArray<FPartitionCut>& OutCuts)
	{
		FRoomRect First;
		FRoomRect Second;
		FPartitionCut Cut;

		if (Rect.Area() <= MaxArea || !SplitOnce(Rect, Stream, First, Second, Cut))
		{
			OutRooms.Add(Rect);
			return;
		}

		OutCuts.Add(Cut);
		SplitRoom(First, MaxArea, Stream, OutRooms, OutCuts);
		SplitRoom(Second, MaxArea, Stream, OutRooms, OutCuts);
	}
}

AHouseGenerator::AHouseGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// A root so the generator can be dragged around the level; its location means nothing, the house goes where the plot is.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AHouseGenerator::Generate(int32 InSeed)
{
	AttemptNote.Reset();
	return TryGenerate(InSeed, false);
}

bool AHouseGenerator::GenerateValid(int32 RequestedSeed, int32 MaxAttempts)
{
	if (!Archetype)
	{
		// Nothing to re-roll with: Generate reports the missing archetype once instead of once per attempt.
		return Generate(RequestedSeed);
	}

	const int32 Attempts = FMath::Max(1, MaxAttempts);
	int32 ForcedLeft = FMath::Max(0, DebugForcedFailures);

	const auto Try = [this, &ForcedLeft](int32 AttemptSeed)
	{
		const bool bForceFailure = ForcedLeft > 0;
		ForcedLeft = FMath::Max(0, ForcedLeft - 1);
		return TryGenerate(AttemptSeed, bForceFailure);
	};

	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		// Attempt 0 is the requested seed itself, so a seed that already works gives exactly what Generate would.
		const int32 AttemptSeed = Attempt == 0 ? RequestedSeed : static_cast<int32>(HashCombine(GetTypeHash(RequestedSeed), GetTypeHash(Attempt)));
		AttemptNote = FString::Printf(TEXT(" (requested %d, attempt %d of %d)"), RequestedSeed, Attempt + 1, Attempts);

		if (Try(AttemptSeed))
		{
			return true;
		}
	}

	AttemptNote = FString::Printf(TEXT(" (requested %d, fallback seed after %d failed attempts)"), RequestedSeed, Attempts);

	if (Try(Archetype->FallbackSeed))
	{
		return true;
	}

	AttemptNote.Reset();
	ReportResult(false, FString::Printf(TEXT("Requested seed %d: no valid house after %d attempts and the fallback seed %d"), RequestedSeed, Attempts, Archetype->FallbackSeed));
	return false;
}

bool AHouseGenerator::TryGenerate(int32 InSeed, bool bForceFailure)
{
	Clear();
	Seed = InSeed;

	if (!Plot)
	{
		for (TActorIterator<APlot> It(GetWorld()); It; ++It)
		{
			Plot = *It;
			break;
		}
	}

	if (!Plot || !Plot->GetStructuralSupport() || !Archetype || !Archetype->FoundationClass || !Archetype->WallClass)
	{
		ReportResult(false, FString::Printf(TEXT("House seed %d: no plot in the level, or the archetype has no foundation or wall class"), Seed));
		return false;
	}

	if (!Archetype->DoorWallClass)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': archetype '%s' has no DoorWallClass, the house will have no door"), *GetName(), *Archetype->GetName());
	}

	FRandomStream Stream(Seed);
	const bool bPlaced = EmitHouse(Stream);
	const int32 PlacedCount = Pieces.Num();

	if (!bPlaced)
	{
		Clear();
		ReportResult(false, FString::Printf(TEXT("House seed %d: a piece could not be placed (%d placed before it), cleared"), Seed, PlacedCount));
		return false;
	}

	// Every piece is registered, but the structural graph has not recomputed yet (that happens on the
	// next tick), so each piece's own Stability is still 0. FindUnstablePieces relaxes the whole
	// registered set on a scratch map instead, which is what the real recompute will conclude.
	TArray<ABuildablePiece*> Unstable;
	Plot->GetStructuralSupport()->FindUnstablePieces(Unstable);

	if (!Unstable.IsEmpty())
	{
		UWorld* World = GetWorld();

		for (const ABuildablePiece* Piece : Unstable)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': seed %d, unstable %s at cell (%d, %d, %d)"),
				*GetName(), Seed, *Piece->GetClass()->GetName(), Piece->GetGridCoord().X, Piece->GetGridCoord().Y, Piece->GetGridCoord().Z);

			if (bShowDebug && World)
			{
				const FBox Bounds = Piece->GetComponentsBoundingBox(true);
				DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), FColor::Red, false, UnstableMarkerSeconds, 0, 5.f);
			}
		}

		const int32 UnstableCount = Unstable.Num();
		Clear();
		ReportResult(false, FString::Printf(TEXT("House seed %d: %d of %d pieces unstable, cleared"), Seed, UnstableCount, PlacedCount));
		return false;
	}

	if (bForceFailure)
	{
		Clear();
		ReportResult(false, FString::Printf(TEXT("House seed %d: built and stable, but this attempt is a forced failure (DebugForcedFailures), cleared"), Seed));
		return false;
	}

	DrawRoomsDebug();
	ReportResult(true, FString::Printf(TEXT("House seed %d: %d pieces, %d x %d cells, %d storey(s), %d room(s), door %s, stable, hash %08X"),
		Seed, PlacedCount, LastSize.X, LastSize.Y, FMath::Max(1, Archetype->Floors), Rooms.Num(), *LastDoorSide, LayoutHash));
	return true;
}

void AHouseGenerator::Clear()
{
	UStructuralSupportComponent* Structural = Plot ? Plot->GetStructuralSupport() : nullptr;

	for (const TWeakObjectPtr<ABuildablePiece>& WeakPiece : Pieces)
	{
		// A piece the player already demolished is gone (and already unregistered): the weak pointer is stale.
		if (ABuildablePiece* Piece = WeakPiece.Get())
		{
			if (Structural)
			{
				Structural->UnregisterPiece(Piece);
			}

			Piece->Destroy();
		}
	}

	Pieces.Reset();
	Rooms.Reset();
	Edges.Reset();
	LayoutHash = 0;
}

bool AHouseGenerator::EmitHouse(FRandomStream& Stream)
{
	const UStructuralRules* Rules = Plot->GetStructuralSupport()->Rules;
	const float CellSize = Rules ? Rules->CellSize : 100.f;
	const FVector Extent = Plot->GetBoundsVolume()->GetScaledBoxExtent();

	// Room cell i covers grid X from 2i - 1 to 2i + 1 (a room cell is two grid units wide, centred on
	// the even coordinate), so it fits the plot while both ends are inside +-Extent. Same on Y.
	const int32 MinI = FMath::CeilToInt((1.f - Extent.X / CellSize) / 2.f - KINDA_SMALL_NUMBER);
	const int32 MaxI = FMath::FloorToInt((Extent.X / CellSize - 1.f) / 2.f + KINDA_SMALL_NUMBER);
	const int32 MinJ = FMath::CeilToInt((1.f - Extent.Y / CellSize) / 2.f - KINDA_SMALL_NUMBER);
	const int32 MaxJ = FMath::FloorToInt((Extent.Y / CellSize - 1.f) / 2.f + KINDA_SMALL_NUMBER);
	const int32 FreeX = MaxI - MinI + 1;
	const int32 FreeY = MaxJ - MinJ + 1;

	if (FreeX < 1 || FreeY < 1)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': plot '%s' is smaller than one room cell"), *GetName(), *Plot->GetName());
		return false;
	}

	// The archetype's bounds, clamped to what the plot can hold. Draw order is part of the determinism: width, depth, X then Y offset, door.
	const auto PickSize = [&Stream](int32 MinCells, int32 MaxCells, int32 Free)
	{
		const int32 Low = FMath::Clamp(MinCells, 1, Free);
		const int32 High = FMath::Clamp(MaxCells, Low, Free);
		return Stream.RandRange(Low, High);
	};

	const int32 Width = PickSize(Archetype->MinRoomCells.X, Archetype->MaxRoomCells.X, FreeX);
	const int32 Depth = PickSize(Archetype->MinRoomCells.Y, Archetype->MaxRoomCells.Y, FreeY);
	// Centred on the plot (the spare cell goes to the lower side when it doesn't divide evenly), then moved by up to PlacementJitter cells.
	const auto PickOrigin = [&Stream](int32 Min, int32 Max, int32 Cells, int32 Jitter)
	{
		const int32 Centred = Min + (Max - Min + 1 - Cells) / 2;
		return FMath::Clamp(Centred + Stream.RandRange(-Jitter, Jitter), Min, Max - Cells + 1);
	};

	const int32 OriginI = PickOrigin(MinI, MaxI, Width, FMath::Max(0, Archetype->PlacementJitter.X));
	const int32 OriginJ = PickOrigin(MinJ, MaxJ, Depth, FMath::Max(0, Archetype->PlacementJitter.Y));
	const int32 Floors = FMath::Max(1, Archetype->Floors);
	LastSize = FIntPoint(Width, Depth);

	// Perimeter edges as grid (X, Y): a vertical edge sits at an odd X, a horizontal one at an odd Y.
	const int32 WestX = 2 * OriginI - 1;
	const int32 EastX = 2 * (OriginI + Width - 1) + 1;
	const int32 SouthY = 2 * OriginJ - 1;
	const int32 NorthY = 2 * (OriginJ + Depth - 1) + 1;

	TArray<FPerimeterEdge> Perimeter;

	for (int32 J = 0; J < Depth; ++J)
	{
		Perimeter.Add(FPerimeterEdge{FIntPoint(WestX, 2 * (OriginJ + J)), EFootprintSide::MinusX});
		Perimeter.Add(FPerimeterEdge{FIntPoint(EastX, 2 * (OriginJ + J)), EFootprintSide::PlusX});
	}

	for (int32 I = 0; I < Width; ++I)
	{
		Perimeter.Add(FPerimeterEdge{FIntPoint(2 * (OriginI + I), SouthY), EFootprintSide::MinusY});
		Perimeter.Add(FPerimeterEdge{FIntPoint(2 * (OriginI + I), NorthY), EFootprintSide::PlusY});
	}

	// The face the player arrives at: the one the line from the house centre to FacingTarget leaves
	// through. Offsets are divided by the half sizes so a long house doesn't favour its long sides.
	// A target standing inside the footprint (or none at all) leaves every face open.
	bool bHasFacingSide = false;
	EFootprintSide FacingSide = EFootprintSide::MinusX;

	if (FacingTarget)
	{
		const FVector GridOrigin = Plot->GetGridOrigin();
		const float CentreX = GridOrigin.X + (2 * OriginI + Width - 1) * CellSize;
		const float CentreY = GridOrigin.Y + (2 * OriginJ + Depth - 1) * CellSize;
		const FVector TargetLocation = FacingTarget->GetActorLocation();
		const float ScaledX = (TargetLocation.X - CentreX) / (Width * CellSize);
		const float ScaledY = (TargetLocation.Y - CentreY) / (Depth * CellSize);

		if (FMath::Max(FMath::Abs(ScaledX), FMath::Abs(ScaledY)) > 1.f)
		{
			bHasFacingSide = true;
			FacingSide = FMath::Abs(ScaledX) >= FMath::Abs(ScaledY)
				? (ScaledX > 0.f ? EFootprintSide::PlusX : EFootprintSide::MinusX)
				: (ScaledY > 0.f ? EFootprintSide::PlusY : EFootprintSide::MinusY);
		}
	}

	TArray<int32> DoorCandidates;

	for (int32 Index = 0; Index < Perimeter.Num(); ++Index)
	{
		if (!bHasFacingSide || Perimeter[Index].Side == FacingSide)
		{
			DoorCandidates.Add(Index);
		}
	}

	const int32 DoorIndex = DoorCandidates[Stream.RandRange(0, DoorCandidates.Num() - 1)];
	LastDoorSide = FString::Printf(TEXT("%s%s"), SideLabel(Perimeter[DoorIndex].Side), bHasFacingSide ? TEXT(" toward target") : TEXT(""));

	for (int32 Level = 0; Level < Floors; ++Level)
	{
		// The foundation is the ground floor; every storey above gets a slab, a reinforcement one every SubAnchorInterval.
		TSubclassOf<ABuildablePiece> FloorClass = Archetype->FoundationClass;

		if (Level > 0)
		{
			const bool bReinforce = Archetype->SubAnchorInterval > 0 && Level % Archetype->SubAnchorInterval == 0 && Archetype->SubAnchorSlabClass;
			FloorClass = bReinforce ? Archetype->SubAnchorSlabClass : Archetype->FloorSlabClass;
		}

		if (!FloorClass)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': archetype '%s' has no floor slab class for storey %d"), *GetName(), *Archetype->GetName(), Level);
			return false;
		}

		// Floors first: a wall's side is read from the occupancy of the cells next to it.
		for (int32 J = 0; J < Depth; ++J)
		{
			for (int32 I = 0; I < Width; ++I)
			{
				if (!Place(FloorClass, FIntVector(2 * (OriginI + I), 2 * (OriginJ + J), Level), 0))
				{
					return false;
				}
			}
		}

		for (int32 Index = 0; Index < Perimeter.Num(); ++Index)
		{
			const FIntVector Edge(Perimeter[Index].Cell.X, Perimeter[Index].Cell.Y, Level);
			const bool bIsDoor = Level == 0 && Index == DoorIndex && Archetype->DoorWallClass;
			const TSubclassOf<ABuildablePiece> WallToPlace = bIsDoor ? Archetype->DoorWallClass : PickWallClass(Stream);

			if (!Place(WallToPlace, Edge, ResolveWallQuarterTurns(Edge)))
			{
				return false;
			}

			Edges.Add(Edge, bIsDoor ? EEdgeKind::ExteriorDoor : EEdgeKind::ExteriorWall);
		}

		// Partitions after the exterior walls, still on this storey's floor.
		const FRoomRect Footprint{OriginI, OriginJ, OriginI + Width - 1, OriginJ + Depth - 1};

		if (!EmitPartitions(Level, Footprint, Stream))
		{
			return false;
		}
	}

	if (Archetype->RoofVariant == EHouseRoof::Flat && Archetype->FlatRoofClass)
	{
		for (int32 J = 0; J < Depth; ++J)
		{
			for (int32 I = 0; I < Width; ++I)
			{
				if (!Place(Archetype->FlatRoofClass, FIntVector(2 * (OriginI + I), 2 * (OriginJ + J), Floors), 0))
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool AHouseGenerator::EmitPartitions(int32 Level, const FRoomRect& Footprint, FRandomStream& Stream)
{
	if (!Archetype->InteriorWallClass || !Archetype->InteriorDoorWallClass)
	{
		if (Archetype->InteriorWallClass || Archetype->InteriorDoorWallClass)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': archetype '%s' sets only one of InteriorWallClass / InteriorDoorWallClass, the storey stays one room"), *GetName(), *Archetype->GetName());
		}

		Rooms.Add(FRoom{Footprint, Level});
		return true;
	}

	TArray<FRoomRect> Leaves;
	TArray<FPartitionCut> Cuts;
	SplitRoom(Footprint, FMath::Max(1, Archetype->MaxRoomArea), Stream, Leaves, Cuts);

	// Too few rooms (a small footprint, a generous MaxRoomArea): cut the biggest one again, first of the biggest on a tie.
	while (Leaves.Num() < Archetype->MinRooms)
	{
		int32 Biggest = INDEX_NONE;

		for (int32 Index = 0; Index < Leaves.Num(); ++Index)
		{
			if (Leaves[Index].Area() >= 2 && (Biggest == INDEX_NONE || Leaves[Index].Area() > Leaves[Biggest].Area()))
			{
				Biggest = Index;
			}
		}

		FRoomRect First;
		FRoomRect Second;
		FPartitionCut Cut;

		if (Biggest == INDEX_NONE || !SplitOnce(Leaves[Biggest], Stream, First, Second, Cut))
		{
			break;
		}

		Leaves[Biggest] = First;
		Leaves.Insert(Second, Biggest + 1);
		Cuts.Add(Cut);
	}

	for (const FPartitionCut& Cut : Cuts)
	{
		// A wall along Y sits on a vertical edge (turned 0), one along X on a horizontal edge (turned 1): the
		// exterior walls' base orientation, without their +2, because a partition is centred on its edge.
		const uint8 QuarterTurns = Cut.bRunsAlongY ? 0 : 1;

		for (int32 Cell = Cut.FirstCell; Cell <= Cut.LastCell; ++Cell)
		{
			const bool bIsDoor = Cell == Cut.DoorCell;
			const FIntVector Edge = Cut.bRunsAlongY ? FIntVector(Cut.LineCoord, 2 * Cell, Level) : FIntVector(2 * Cell, Cut.LineCoord, Level);

			if (!Place(bIsDoor ? Archetype->InteriorDoorWallClass : Archetype->InteriorWallClass, Edge, QuarterTurns))
			{
				return false;
			}

			Edges.Add(Edge, bIsDoor ? EEdgeKind::PartitionDoor : EEdgeKind::Partition);
		}
	}

	for (const FRoomRect& Leaf : Leaves)
	{
		Rooms.Add(FRoom{Leaf, Level});
	}

	return true;
}

void AHouseGenerator::DrawRoomsDebug() const
{
	UWorld* World = GetWorld();

	if (!bShowDebug || !World || !Plot || !Plot->GetStructuralSupport())
	{
		return;
	}

	const UStructuralRules* Rules = Plot->GetStructuralSupport()->Rules;
	const float CellSize = Rules ? Rules->CellSize : 100.f;
	const FVector GridOrigin = Plot->GetGridOrigin();
	const FColor Palette[] = {FColor::Cyan, FColor::Orange, FColor::Magenta, FColor::Yellow, FColor::Emerald, FColor::Turquoise, FColor::Purple};

	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		const FRoom& Room = Rooms[Index];
		const FColor Colour = Palette[Index % UE_ARRAY_COUNT(Palette)];

		// A room cell is 2 grid units wide centred on 2i, so a block spans MinI + MaxI grid units either side of the origin, Width cells * CellSize half-wide.
		const FVector Centre(
			GridOrigin.X + (Room.Rect.MinI + Room.Rect.MaxI) * CellSize,
			GridOrigin.Y + (Room.Rect.MinJ + Room.Rect.MaxJ) * CellSize,
			Plot->GetPlaneZ(Room.Level, 0, 0.f) + 10.f);
		const FVector Extent(Room.Rect.Width() * CellSize - 15.f, Room.Rect.Depth() * CellSize - 15.f, 2.f);

		DrawDebugBox(World, Centre, Extent, Colour, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugString(World, Centre + FVector(0.f, 0.f, 40.f), FString::Printf(TEXT("Room %d (%d x %d)"), Index, Room.Rect.Width(), Room.Rect.Depth()), nullptr, Colour, UnstableMarkerSeconds, true);
	}
}

ABuildablePiece* AHouseGenerator::Place(TSubclassOf<ABuildablePiece> PieceClass, const FIntVector& GridCoord, uint8 QuarterTurns, int32 RoofStep)
{
	ABuildablePiece* Piece = Plot->PlacePiece(PieceClass, GridCoord, QuarterTurns, RoofStep);

	if (!Piece)
	{
		// PlacePiece already logged why.
		return nullptr;
	}

	Pieces.Add(Piece);

	LayoutHash = HashCombine(LayoutHash, GetTypeHash(PieceClass->GetPathName()));
	LayoutHash = HashCombine(LayoutHash, GetTypeHash(GridCoord));
	LayoutHash = HashCombine(LayoutHash, GetTypeHash(QuarterTurns));
	LayoutHash = HashCombine(LayoutHash, GetTypeHash(RoofStep));

	return Piece;
}

TSubclassOf<ABuildablePiece> AHouseGenerator::PickWallClass(FRandomStream& Stream) const
{
	const float Roll = Stream.FRand();
	TSubclassOf<ABuildablePiece> Picked = Archetype->WallClass;

	float Threshold = Archetype->WideWindowChance;

	if (Roll < Threshold)
	{
		Picked = Archetype->WideWindowWallClass;
	}
	else
	{
		Threshold += Archetype->LargeWindowChance;

		if (Roll < Threshold)
		{
			Picked = Archetype->LargeWindowWallClass;
		}
		else
		{
			Threshold += Archetype->WindowChance;

			if (Roll < Threshold)
			{
				Picked = Archetype->WindowWallClass;
			}
		}
	}

	// An opening class left empty in the archetype falls back to a plain wall.
	return Picked ? Picked : Archetype->WallClass;
}

uint8 AHouseGenerator::ResolveWallQuarterTurns(const FIntVector& Edge) const
{
	const bool bVertical = FMath::Abs(Edge.X) % 2 == 1;
	const FIntVector MinusNeighbour = bVertical ? FIntVector(Edge.X - 1, Edge.Y, Edge.Z) : FIntVector(Edge.X, Edge.Y - 1, Edge.Z);
	const bool bMinusHasFloor = Plot->GetStructuralSupport()->GetPieceAt(EBuildAnchor::Cell, MinusNeighbour) != nullptr;

	// Base 0 (vertical) or 1 (horizontal) faces the -X / -Y side; +2 turns the wall to the other side.
	const uint8 Base = bVertical ? 0 : 1;
	return (Base + (bMinusHasFloor ? 0 : 2)) % 4;
}

void AHouseGenerator::ReportResult(bool bSuccess, const FString& Line) const
{
	const FString FullLine = Line + AttemptNote;

	if (bSuccess)
	{
		UE_LOG(LogPlotBuilder, Log, TEXT("AHouseGenerator '%s': %s"), *GetName(), *FullLine);
	}
	else
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': %s"), *GetName(), *FullLine);
	}

	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(ResultMessageKey, UnstableMarkerSeconds, bSuccess ? FColor::Green : FColor::Red, FullLine);
	}
}
