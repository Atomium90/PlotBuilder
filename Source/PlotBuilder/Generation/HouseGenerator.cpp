#include "HouseGenerator.h"
#include "HouseArchetype.h"
#include "HouseLayout.h"
#include "PlotBuilder.h"
#include "Buildables/BuildablePiece.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/Plot.h"
#include "Environment/StructuralRules.h"
#include "Environment/StructuralSupportComponent.h"
#include "Props/PhysicsProp.h"

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

	/**
	 *  A block of a plan's text grid, in world room cells. Row 0 of the text is the highest X and column 0
	 *  the lowest Y (the Top viewport), so row r is cell OriginI + Rows - 1 - r and column c is cell
	 *  OriginJ + c. Mirroring reverses the rows (X) and/or the columns (Y) first.
	 */
	FRoomRect MapLayoutRect(const FLayoutRect& Rect, int32 Rows, int32 Columns, bool bMirrorX, bool bMirrorY, int32 OriginI, int32 OriginJ)
	{
		const int32 MinRow = bMirrorX ? Rows - 1 - Rect.MaxRow : Rect.MinRow;
		const int32 MaxRow = bMirrorX ? Rows - 1 - Rect.MinRow : Rect.MaxRow;
		const int32 MinCol = bMirrorY ? Columns - 1 - Rect.MaxCol : Rect.MinCol;
		const int32 MaxCol = bMirrorY ? Columns - 1 - Rect.MinCol : Rect.MaxCol;

		return FRoomRect{OriginI + Rows - 1 - MaxRow, OriginJ + MinCol, OriginI + Rows - 1 - MinRow, OriginJ + MaxCol};
	}

	FString RoomTypeName(ERoomType Type)
	{
		return StaticEnum<ERoomType>()->GetNameStringByValue(static_cast<int64>(Type));
	}

	/** "Bathroom 1, Kitchen 1, Bedroom 2": how many rooms of each type the house has, for the result line. Types with no room are left out. */
	FString RoomTypeTally(const TArray<FRoom>& Rooms)
	{
		const ERoomType AllTypes[] = {ERoomType::Bathroom, ERoomType::Kitchen, ERoomType::LivingRoom, ERoomType::Bedroom, ERoomType::Office};
		FString Tally;

		for (const ERoomType Type : AllTypes)
		{
			int32 Count = 0;

			for (const FRoom& Room : Rooms)
			{
				Count += Room.Type == Type ? 1 : 0;
			}

			if (Count > 0)
			{
				Tally += FString::Printf(TEXT("%s%s %d"), Tally.IsEmpty() ? TEXT("") : TEXT(", "), *RoomTypeName(Type), Count);
			}
		}

		return Tally;
	}

	/**
	 *  Types the rooms of a random split. No seed is drawn (the hash of a house does not move): the rooms are
	 *  ranked by area, the biggest is the living room, the smallest the bathroom, the second biggest the
	 *  kitchen, the third a bedroom; any room left gets the office first, then bedrooms. A tie goes to the room
	 *  emitted first. Four rooms or more always give the four core types; the office only exists when a room is spare.
	 *  Returns one type per leaf, in the leaves' order.
	 */
	TArray<ERoomType> TypeRooms(const TArray<FRoomRect>& Leaves)
	{
		TArray<int32> Ranking;
		Ranking.Reserve(Leaves.Num());

		for (int32 Index = 0; Index < Leaves.Num(); ++Index)
		{
			Ranking.Add(Index);
		}

		Ranking.Sort([&Leaves](int32 A, int32 B)
		{
			const int32 AreaA = Leaves[A].Area();
			const int32 AreaB = Leaves[B].Area();
			return AreaA != AreaB ? AreaA > AreaB : A < B;
		});

		TArray<ERoomType> Types;
		Types.Init(ERoomType::Bedroom, Leaves.Num());
		bool bOfficeGiven = false;

		for (int32 Rank = 0; Rank < Ranking.Num(); ++Rank)
		{
			ERoomType Type = ERoomType::Bedroom;

			if (Rank == 0)
			{
				Type = ERoomType::LivingRoom;
			}
			else if (Rank == Ranking.Num() - 1)
			{
				Type = ERoomType::Bathroom;
			}
			else if (Rank == 1)
			{
				Type = ERoomType::Kitchen;
			}
			else if (Rank > 2 && !bOfficeGiven)
			{
				Type = ERoomType::Office;
				bOfficeGiven = true;
			}

			Types[Ranking[Rank]] = Type;
		}

		return Types;
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

	/** Gap kept between a prop's back and the room-side face of a wall, so a fresh physics body never starts inside it (cm). */
	constexpr float WallGap = 2.f;

	/** Gap kept between two props (cm). */
	constexpr float PropGap = 2.f;

	/** How far into the room the floor in front of a door stays empty (cm). The zone is as wide as the door segment. */
	constexpr float DoorClearDepth = 100.f;

	/** Spacing of the positions tried along a wall and around the middle of a room (cm). */
	constexpr float SlideStep = 50.f;

	/** Walls on the X sides run along Y, walls on the Y sides run along X. */
	bool IsXSide(EFootprintSide Side)
	{
		return Side == EFootprintSide::MinusX || Side == EFootprintSide::PlusX;
	}

	/** World yaw a prop must face when its back is to the wall on Wall: its front points into the room. */
	float FacingYawOf(EFootprintSide Wall)
	{
		switch (Wall)
		{
		case EFootprintSide::MinusX: return 0.f;
		case EFootprintSide::PlusX: return 180.f;
		case EFootprintSide::MinusY: return 90.f;
		default: return 270.f;
		}
	}

	/** Grid key of the wall edge on Side of Rect, at room cell Along (a J for the two X sides, an I for the two Y sides). */
	FIntVector SideEdgeKey(const FRoomRect& Rect, EFootprintSide Side, int32 Along, int32 Level)
	{
		switch (Side)
		{
		case EFootprintSide::MinusX: return FIntVector(2 * Rect.MinI - 1, 2 * Along, Level);
		case EFootprintSide::PlusX: return FIntVector(2 * Rect.MaxI + 1, 2 * Along, Level);
		case EFootprintSide::MinusY: return FIntVector(2 * Along, 2 * Rect.MinJ - 1, Level);
		default: return FIntVector(2 * Along, 2 * Rect.MaxJ + 1, Level);
		}
	}

	/** One place for one prop: the world XY box its rotated footprint covers, and the yaws that produce it. */
	struct FSpot
	{
		FBox2D Box;

		/** Direction the front is meant to point, before the row's FrontYawOffset. Drawn as an arrow, so a mesh that faces the wrong way is easy to see. */
		float FacingYaw = 0.f;

		/** Yaw of the actor: FacingYaw + FrontYawOffset. */
		float Yaw = 0.f;

		/** Distance from the middle of the room (Centred spots only). */
		float Score = 0.f;
	};

	/** The floor of one room that furniture can use: inside the walls, minus what is already taken. */
	struct FRoomFloor
	{
		FBox2D Free;
		TArray<FBox2D> Taken;

		bool IsFree(const FBox2D& Box) const
		{
			constexpr float Tolerance = 0.5f;

			if (Box.Min.X < Free.Min.X - Tolerance || Box.Min.Y < Free.Min.Y - Tolerance || Box.Max.X > Free.Max.X + Tolerance || Box.Max.Y > Free.Max.Y + Tolerance)
			{
				return false;
			}

			for (const FBox2D& Other : Taken)
			{
				if (Box.Min.X < Other.Max.X && Other.Min.X < Box.Max.X && Box.Min.Y < Other.Max.Y && Other.Min.Y < Box.Max.Y)
				{
					return false;
				}
			}

			return true;
		}
	};

	/** A prop picked for a room, with what placing it needs from its mesh. */
	struct FChosenProp
	{
		const FFurnishingRuleRow* Row = nullptr;

		/** Mesh bounds in the actor's space, scale included, pivot wherever the artist left it. */
		FBox Local = FBox(ForceInit);

		/** Scale the Blueprint gives its mesh root: a spawn transform replaces the root's own, so it is passed back. */
		FVector Scale = FVector::OneVector;

		/** Footprint area before rotation, used to place the biggest props first. */
		float Area = 0.f;
	};

	/** Reads the mesh bounds and scale from the Blueprint's default object. False if the Blueprint has no mesh. */
	bool ReadProp(const FFurnishingRuleRow& Row, FChosenProp& OutProp)
	{
		const APhysicsProp* Default = Row.PropClass ? Row.PropClass.GetDefaultObject() : nullptr;
		const UStaticMeshComponent* Mesh = Default ? Default->GetMeshComponent() : nullptr;
		const UStaticMesh* StaticMesh = Mesh ? Mesh->GetStaticMesh() : nullptr;

		if (!StaticMesh)
		{
			return false;
		}

		OutProp.Row = &Row;
		OutProp.Scale = Mesh->GetRelativeScale3D();
		OutProp.Local = StaticMesh->GetBounds().GetBox().TransformBy(FTransform(FQuat::Identity, FVector::ZeroVector, OutProp.Scale));
		OutProp.Area = OutProp.Local.GetSize().X * OutProp.Local.GetSize().Y;
		return true;
	}

	/** Start positions of a block of length Size inside [Lo, Hi]: Lo, then every SlideStep, then the last that fits. EndsOnly keeps just the two ends. Empty if it does not fit. */
	void SlidePositions(float Lo, float Hi, float Size, bool bEndsOnly, TArray<float>& OutPositions)
	{
		OutPositions.Reset();
		const float Last = Hi - Size;

		if (Last < Lo - UE_KINDA_SMALL_NUMBER)
		{
			return;
		}

		OutPositions.Add(Lo);

		if (Last > Lo + UE_KINDA_SMALL_NUMBER)
		{
			for (float Position = Lo + SlideStep; !bEndsOnly && Position < Last - 1.f; Position += SlideStep)
			{
				OutPositions.Add(Position);
			}

			OutPositions.Add(Last);
		}
	}

	/**
	 *  Every place Prop can go in Floor under its row's rule. Against a wall (and NearPlumbing, which is the
	 *  same for now): back to one of the four walls, front into the room, sliding along it. Corner: only the
	 *  ends of that slide. Centred: around the middle of the room, in the four directions, keeping only the
	 *  spots nearest to the middle that are free.
	 */
	void CollectSpots(const FChosenProp& Prop, const FRoomFloor& Floor, TArray<FSpot>& OutSpots)
	{
		const EFurniturePlacement Placement = Prop.Row->Placement;
		const float YawOffset = Prop.Row->FrontYawOffset;

		const auto SizeAt = [&Prop](float Yaw)
		{
			const FVector Size = Prop.Local.TransformBy(FTransform(FRotator(0.f, Yaw, 0.f))).GetSize();
			return FVector2D(Size.X, Size.Y);
		};

		if (Placement == EFurniturePlacement::Centred)
		{
			const FVector2D Middle = Floor.Free.GetCenter();
			const FVector2D Half = Floor.Free.GetExtent();
			const int32 Steps = FMath::CeilToInt(FMath::Max(Half.X, Half.Y) / SlideStep);

			for (int32 Turn = 0; Turn < 4; ++Turn)
			{
				const float FacingYaw = 90.f * Turn;
				const FVector2D Size = SizeAt(FacingYaw + YawOffset);

				for (int32 StepX = -Steps; StepX <= Steps; ++StepX)
				{
					for (int32 StepY = -Steps; StepY <= Steps; ++StepY)
					{
						const FVector2D Offset(StepX * SlideStep, StepY * SlideStep);
						const FBox2D Box(Middle + Offset - Size * 0.5, Middle + Offset + Size * 0.5);

						if (Floor.IsFree(Box))
						{
							OutSpots.Add(FSpot{Box, FacingYaw, FacingYaw + YawOffset, static_cast<float>(Offset.Size())});
						}
					}
				}
			}

			float Nearest = TNumericLimits<float>::Max();

			for (const FSpot& Spot : OutSpots)
			{
				Nearest = FMath::Min(Nearest, Spot.Score);
			}

			OutSpots.RemoveAll([Nearest](const FSpot& Spot) { return Spot.Score > Nearest + 1.f; });
			return;
		}

		const EFootprintSide Walls[] = {EFootprintSide::MinusX, EFootprintSide::PlusX, EFootprintSide::MinusY, EFootprintSide::PlusY};
		TArray<float> Along;

		for (const EFootprintSide Wall : Walls)
		{
			const float FacingYaw = FacingYawOf(Wall);
			const FVector2D Size = SizeAt(FacingYaw + YawOffset);
			const bool bXWall = IsXSide(Wall);

			// Along the wall: the footprint slides between the two ends of the free floor, a gap short of each.
			const float AlongLo = (bXWall ? Floor.Free.Min.Y : Floor.Free.Min.X) + WallGap;
			const float AlongHi = (bXWall ? Floor.Free.Max.Y : Floor.Free.Max.X) - WallGap;
			SlidePositions(AlongLo, AlongHi, bXWall ? Size.Y : Size.X, Placement == EFurniturePlacement::Corner, Along);

			for (const float Position : Along)
			{
				FVector2D Min;

				switch (Wall)
				{
				case EFootprintSide::MinusX: Min = FVector2D(Floor.Free.Min.X + WallGap, Position); break;
				case EFootprintSide::PlusX: Min = FVector2D(Floor.Free.Max.X - WallGap - Size.X, Position); break;
				case EFootprintSide::MinusY: Min = FVector2D(Position, Floor.Free.Min.Y + WallGap); break;
				default: Min = FVector2D(Position, Floor.Free.Max.Y - WallGap - Size.Y); break;
				}

				const FBox2D Box(Min, Min + Size);

				if (Floor.IsFree(Box))
				{
					OutSpots.Add(FSpot{Box, FacingYaw, FacingYaw + YawOffset, 0.f});
				}
			}
		}
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

		// Debug: the rejected house was kept for inspection, a re-roll would clear it.
		if (bKeptUnstableHouse)
		{
			return false;
		}
	}

	AttemptNote = FString::Printf(TEXT(" (requested %d, fallback seed after %d failed attempts)"), RequestedSeed, Attempts);

	if (Try(Archetype->FallbackSeed))
	{
		return true;
	}

	if (bKeptUnstableHouse)
	{
		return false;
	}

	AttemptNote.Reset();
	ReportResult(false, FString::Printf(TEXT("Requested seed %d: no valid house after %d attempts and the fallback seed %d"), RequestedSeed, Attempts, Archetype->FallbackSeed));
	return false;
}

void AHouseGenerator::TestSeeds(int32 FirstSeed, int32 LastSeed)
{
	// Every build spawns a whole house and its furniture, so the batch stays small enough not to freeze the editor.
	constexpr int32 MaxSeeds = 100;
	constexpr int32 TypeCount = 5;
	const int32 Count = FMath::Clamp(LastSeed - FirstSeed + 1, 1, MaxSeeds);

	const bool bSavedShowDebug = bShowDebug;
	const int32 SavedForcedFailures = DebugForcedFailures;
	const bool bSavedKeepUnstable = bKeepUnstableHouse;
	bShowDebug = false;
	DebugForcedFailures = 0;

	// A batch has to re-roll to go on: keeping a rejected house would stop it at the first one.
	bKeepUnstableHouse = false;

	TArray<int32> Failed;
	TArray<int32> Rerolled;
	TArray<int32> HashDiffers;
	int32 HousesWithType[TypeCount] = {};

	// Per standing (1 to 3): houses, and the tiers of the furniture picked from groups that have several rows.
	constexpr int32 StandingCount = 3;
	int32 HousesAtStanding[StandingCount] = {};
	int32 VariantTierSumAt[StandingCount] = {};
	int32 VariantPropsAt[StandingCount] = {};
	int32 MinProps = MAX_int32;
	int32 MaxProps = 0;
	int32 MinPosts = MAX_int32;
	int32 MaxPosts = 0;
	int32 ValidCount = 0;

	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 TestSeed = FirstSeed + Offset;

		if (!GenerateValid(TestSeed))
		{
			Failed.Add(TestSeed);
			continue;
		}

		++ValidCount;
		const int32 FirstHash = GetLayoutHash();
		const int32 FirstEffectiveSeed = Seed;

		if (FirstEffectiveSeed != TestSeed)
		{
			Rerolled.Add(TestSeed);
		}

		bool bHasType[TypeCount] = {};

		for (const FRoom& Room : Rooms)
		{
			bHasType[static_cast<int32>(Room.Type)] = true;
		}

		for (int32 Type = 0; Type < TypeCount; ++Type)
		{
			HousesWithType[Type] += bHasType[Type] ? 1 : 0;
		}

		if (Standing >= 1 && Standing <= StandingCount)
		{
			++HousesAtStanding[Standing - 1];
			VariantTierSumAt[Standing - 1] += VariantTierSum;
			VariantPropsAt[Standing - 1] += VariantPropCount;
		}

		MinProps = FMath::Min(MinProps, Props.Num());
		MaxProps = FMath::Max(MaxProps, Props.Num());
		MinPosts = FMath::Min(MinPosts, PostVertices.Num());
		MaxPosts = FMath::Max(MaxPosts, PostVertices.Num());

		// T6: asking again for the same seed must give the same house, furniture included.
		if (!GenerateValid(TestSeed) || GetLayoutHash() != FirstHash || Seed != FirstEffectiveSeed)
		{
			HashDiffers.Add(TestSeed);
		}
	}

	bShowDebug = bSavedShowDebug;
	DebugForcedFailures = SavedForcedFailures;
	bKeepUnstableHouse = bSavedKeepUnstable;

	const auto Join = [](const TArray<int32>& Seeds)
	{
		FString Text;

		for (const int32 Value : Seeds)
		{
			Text += (Text.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::FromInt(Value);
		}

		return Text.IsEmpty() ? FString(TEXT("none")) : Text;
	};

	FString TypeLine;
	bool bAllTypesSeen = true;

	for (int32 Type = 0; Type < TypeCount; ++Type)
	{
		TypeLine += FString::Printf(TEXT("%s%s %d"), Type == 0 ? TEXT("") : TEXT(", "), *RoomTypeName(static_cast<ERoomType>(Type)), HousesWithType[Type]);
		bAllTypesSeen = bAllTypesSeen && HousesWithType[Type] > 0;
	}

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Seed test %d..%d: %d of %d valid (failed: %s), re-rolled or fallback: %s"), FirstSeed, FirstSeed + Count - 1, ValidCount, Count, *Join(Failed), *Join(Rerolled)));
	Lines.Add(FString::Printf(TEXT("Same seed twice (T6): %d of %d gave the same hash (differs: %s)"), ValidCount - HashDiffers.Num(), ValidCount, *Join(HashDiffers)));
	Lines.Add(FString::Printf(TEXT("Houses with each room type (of %d): %s"), ValidCount, *TypeLine));
	Lines.Add(ValidCount > 0 ? FString::Printf(TEXT("Props per house: %d to %d, support posts per house: %d to %d"), MinProps, MaxProps, MinPosts, MaxPosts) : FString(TEXT("Props per house: no valid house")));

	// The standing leans the furniture: the average tier of the props that had a choice should rise with it.
	FString StandingLine;

	for (int32 Index = 0; Index < StandingCount; ++Index)
	{
		const FString Average = VariantPropsAt[Index] > 0 ? FString::Printf(TEXT("%.2f"), static_cast<float>(VariantTierSumAt[Index]) / VariantPropsAt[Index]) : FString(TEXT("-"));
		StandingLine += FString::Printf(TEXT("%sstanding %d: %d house(s), average tier %s"), Index == 0 ? TEXT("") : TEXT(" | "), Index + 1, HousesAtStanding[Index], *Average);
	}

	Lines.Add(StandingLine);

	// Red is a real failure (a seed that gave no house, a house that changed the second time). A room type nobody
	// has is yellow: it only means this archetype's plans do not contain it (a two-storey plan with no office, say).
	const FColor SummaryColor = (!Failed.IsEmpty() || !HashDiffers.IsEmpty()) ? FColor::Red : (bAllTypesSeen ? FColor::Green : FColor::Yellow);

	for (const FString& Line : Lines)
	{
		UE_LOG(LogPlotBuilder, Log, TEXT("AHouseGenerator '%s': %s"), *GetName(), *Line);
	}

	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(ResultMessageKey + 1, 60.f, SummaryColor, FString::Join(Lines, TEXT("\n")));
	}
}

bool AHouseGenerator::TryGenerate(int32 InSeed, bool bForceFailure)
{
	Clear();
	Seed = InSeed;
	bKeptUnstableHouse = false;

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

		if (bKeepUnstableHouse)
		{
			// Debug: the rejected house stays so it can be looked at; GenerateValid sees the flag and does not re-roll.
			bKeptUnstableHouse = true;
			ReportResult(false, FString::Printf(TEXT("House seed %d: %d of %d pieces unstable, KEPT for inspection (bKeepUnstableHouse), no re-roll"), Seed, UnstableCount, PlacedCount));
			return false;
		}

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

	// Only a house that stands gets furniture, and it draws after the structure, so the walls never depend on it.
	FurnishRooms(Stream);

	DrawRoomsDebug();
	ReportResult(true, FString::Printf(TEXT("House seed %d: %d pieces, %d x %d cells, %d storey(s), %d room(s) [%s] (%s), %d prop(s), standing %d, %d post(s), %d stair flight(s), door %s, stable, hash %08X"),
		Seed, PlacedCount, LastSize.X, LastSize.Y, FMath::Max(1, Archetype->Floors), Rooms.Num(), *RoomTypeTally(Rooms), *LastLayoutNote, Props.Num(), Standing, PostVertices.Num(), StairRuns.Num(), *LastDoorSide, LayoutHash));
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

	// A prop the player already broke is gone (HandleBroken destroys it): its weak pointer is stale, like a demolished piece's.
	for (const TWeakObjectPtr<APhysicsProp>& WeakProp : Props)
	{
		if (APhysicsProp* Prop = WeakProp.Get())
		{
			Prop->Destroy();
		}
	}

	Props.Reset();
	Pieces.Reset();
	Rooms.Reset();
	Edges.Reset();
	PostVertices.Reset();
	StairRuns.Reset();
	Standing = 0;
	VariantTierSum = 0;
	VariantPropCount = 0;
	LayoutHash = 0;
}

int32 AHouseGenerator::RollStanding(FRandomStream& Stream) const
{
	// Drawn before anything is read, so the number of draws never depends on the weights.
	const float Roll = Stream.FRand();
	float Total = 0.f;

	for (const float Weight : Archetype->StandingWeights)
	{
		Total += FMath::Max(0.f, Weight);
	}

	if (Total <= 0.f)
	{
		return 2;
	}

	float Remaining = Roll * Total;
	int32 Picked = 1;

	for (int32 Index = 0; Index < Archetype->StandingWeights.Num(); ++Index)
	{
		const float Weight = FMath::Max(0.f, Archetype->StandingWeights[Index]);

		if (Weight > 0.f)
		{
			Picked = Index + 1;

			if (Remaining < Weight)
			{
				break;
			}

			Remaining -= Weight;
		}
	}

	return Picked;
}

bool AHouseGenerator::IsFlightCell(int32 I, int32 J, int32 Level) const
{
	for (const FStairRun& Run : StairRuns)
	{
		if (Run.Level == Level && (Run.First == FIntPoint(I, J) || Run.Second == FIntPoint(I, J)))
		{
			return true;
		}
	}

	return false;
}

bool AHouseGenerator::IsStairwellCell(int32 I, int32 J, int32 Level) const
{
	// The flight starts one storey below the floor it opens a stairwell in.
	return IsFlightCell(I, J, Level - 1);
}

bool AHouseGenerator::IsOnFlight(const FIntVector& Vertex) const
{
	for (const FStairRun& Run : StairRuns)
	{
		if (Run.Level != Vertex.Z)
		{
			continue;
		}

		// The flight's two room cells cover grid X 2 * min - 1 .. 2 * max + 1 (same on Y), edges included.
		const int32 MinX = 2 * FMath::Min(Run.First.X, Run.Second.X) - 1;
		const int32 MaxX = 2 * FMath::Max(Run.First.X, Run.Second.X) + 1;
		const int32 MinY = 2 * FMath::Min(Run.First.Y, Run.Second.Y) - 1;
		const int32 MaxY = 2 * FMath::Max(Run.First.Y, Run.Second.Y) + 1;

		if (Vertex.X >= MinX && Vertex.X <= MaxX && Vertex.Y >= MinY && Vertex.Y <= MaxY)
		{
			return true;
		}
	}

	return false;
}

bool AHouseGenerator::SelectArchetype(const FString& NameFragment)
{
	for (const TObjectPtr<UHouseArchetype>& Candidate : KnownArchetypes)
	{
		if (Candidate && Candidate->GetName().Contains(NameFragment, ESearchCase::IgnoreCase))
		{
			Archetype = Candidate;
			return true;
		}
	}

	FString Known;

	for (const TObjectPtr<UHouseArchetype>& Candidate : KnownArchetypes)
	{
		Known += Candidate ? FString::Printf(TEXT(" %s"), *Candidate->GetName()) : FString();
	}

	UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': no known archetype matches '%s' (known:%s)"), *GetName(), *NameFragment, Known.IsEmpty() ? TEXT(" none, fill KnownArchetypes on the generator") : *Known);
	return false;
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

	const int32 Floors = FMath::Max(1, Archetype->Floors);

	// A house of several storeys is only worth building if the upper ones can be reached: every storey change needs a flight.
	const auto HasFlightForEveryStorey = [Floors](const FParsedLayout& Parsed)
	{
		for (int32 Level = 0; Level + 1 < Floors; ++Level)
		{
			if (!Parsed.Stairs.ContainsByPredicate([Level](const FLayoutStair& Stair) { return Stair.Level == Level; }))
			{
				return false;
			}
		}

		return true;
	};

	// Hand-written plans the archetype offers that can be built here: valid, one grid per storey, small enough for the plot.
	TArray<TPair<const UHouseLayout*, FParsedLayout>> UsableLayouts;

	for (const TObjectPtr<UHouseLayout>& Candidate : Archetype->Layouts)
	{
		if (!Candidate)
		{
			continue;
		}

		FParsedLayout Parsed;
		TArray<FString> Errors;

		if (!Candidate->Parse(Parsed, Errors))
		{
			for (const FString& Error : Errors)
			{
				UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': layout '%s' skipped: %s"), *GetName(), *Candidate->GetName(), *Error);
			}
		}
		else if (Candidate->Levels.Num() != Floors)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': layout '%s' skipped: it has %d storey(s), the archetype has %d"), *GetName(), *Candidate->GetName(), Candidate->Levels.Num(), Floors);
		}
		else if (!HasFlightForEveryStorey(Parsed))
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': layout '%s' skipped: the archetype has %d storeys and the plan lacks a flight of stairs for one of the storey changes"), *GetName(), *Candidate->GetName(), Floors);
		}
		else if (Parsed.Rows > FreeX || Parsed.Columns > FreeY)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': layout '%s' skipped: it is %d x %d cells, the plot holds %d x %d"), *GetName(), *Candidate->GetName(), Parsed.Rows, Parsed.Columns, FreeX, FreeY);
		}
		else
		{
			UsableLayouts.Emplace(Candidate.Get(), MoveTemp(Parsed));
		}
	}

	// Without a plan the random split has no stairs to give, so a house of several storeys cannot be built.
	if (Floors > 1 && (UsableLayouts.IsEmpty() || !Archetype->StairsClass))
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': archetype '%s' has %d storeys: it needs a usable plan with stairs and a StairsClass, nothing is built"), *GetName(), *Archetype->GetName(), Floors);
		return false;
	}

	if (!Archetype->Layouts.IsEmpty() && UsableLayouts.IsEmpty())
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': archetype '%s' has layouts but none is usable, cutting the footprint at random instead"), *GetName(), *Archetype->GetName());
	}

	// Draw order is part of the determinism. With a plan: which plan, then its own draws (mirrors, size, growth,
	// swaps), then the X then Y offset, door. Without: width, depth, then the same.
	const UHouseLayout* Layout = nullptr;
	FResolvedLayout Resolved;
	int32 Width = 0;
	int32 Depth = 0;

	if (!UsableLayouts.IsEmpty())
	{
		const int32 Pick = Stream.RandRange(0, UsableLayouts.Num() - 1);
		Layout = UsableLayouts[Pick].Key;
		Layout->Resolve(UsableLayouts[Pick].Value, Archetype->MinRoomCells, Archetype->MaxRoomCells, FIntPoint(FreeX, FreeY), Stream, Resolved);
		Width = Resolved.Rows;
		Depth = Resolved.Columns;
	}
	else
	{
		// The archetype's bounds, clamped to what the plot can hold.
		const auto PickSize = [&Stream](int32 MinCells, int32 MaxCells, int32 Free)
		{
			const int32 Low = FMath::Clamp(MinCells, 1, Free);
			const int32 High = FMath::Clamp(MaxCells, Low, Free);
			return Stream.RandRange(Low, High);
		};

		Width = PickSize(Archetype->MinRoomCells.X, Archetype->MaxRoomCells.X, FreeX);
		Depth = PickSize(Archetype->MinRoomCells.Y, Archetype->MaxRoomCells.Y, FreeY);
	}

	// Centred on the plot (the spare cell goes to the lower side when it doesn't divide evenly), then moved by up to PlacementJitter cells.
	const auto PickOrigin = [&Stream](int32 Min, int32 Max, int32 Cells, int32 Jitter)
	{
		const int32 Centred = Min + (Max - Min + 1 - Cells) / 2;
		return FMath::Clamp(Centred + Stream.RandRange(-Jitter, Jitter), Min, Max - Cells + 1);
	};

	const int32 OriginI = PickOrigin(MinI, MaxI, Width, FMath::Max(0, Archetype->PlacementJitter.X));
	const int32 OriginJ = PickOrigin(MinJ, MaxJ, Depth, FMath::Max(0, Archetype->PlacementJitter.Y));
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

	// With a plan: the rooms in world cells, the mirror, and the room the front door belongs to.
	TArray<TPair<int32, int32>> LayoutLinks;

	if (Layout)
	{
		bool bMirrorX = Resolved.bMirrorX;
		bool bMirrorY = Resolved.bMirrorY;
		const FLayoutRect& EntranceRect = Resolved.Rooms[Resolved.EntranceRoom].Rect;

		const auto Faces = [&](bool bTryX, bool bTryY)
		{
			const FRoomRect Entrance = MapLayoutRect(EntranceRect, Resolved.Rows, Resolved.Columns, bTryX, bTryY, OriginI, OriginJ);

			switch (FacingSide)
			{
			case EFootprintSide::MinusX: return Entrance.MinI == OriginI;
			case EFootprintSide::PlusX: return Entrance.MaxI == OriginI + Width - 1;
			case EFootprintSide::MinusY: return Entrance.MinJ == OriginJ;
			default: return Entrance.MaxJ == OriginJ + Depth - 1;
			}
		};

		// The seed picked the mirror; if that leaves the entrance room off the face the player arrives at, try the flips the plan allows.
		if (bHasFacingSide && !Faces(bMirrorX, bMirrorY))
		{
			const bool Flips[3][2] = {{true, false}, {false, true}, {true, true}};

			for (const bool (&Flip)[2] : Flips)
			{
				const bool bTryX = bMirrorX != Flip[0];
				const bool bTryY = bMirrorY != Flip[1];

				if ((!Flip[0] || Layout->bAllowMirrorX) && (!Flip[1] || Layout->bAllowMirrorY) && Faces(bTryX, bTryY))
				{
					bMirrorX = bTryX;
					bMirrorY = bTryY;
					break;
				}
			}
		}

		for (const FParsedLayoutRoom& Room : Resolved.Rooms)
		{
			Rooms.Add(FRoom{MapLayoutRect(Room.Rect, Resolved.Rows, Resolved.Columns, bMirrorX, bMirrorY, OriginI, OriginJ), Room.Level, Room.Type, Room.Letter});
		}

		LayoutLinks = Resolved.Links;

		// The plan's flights as world room cells. Row 0 of the plan is the highest X and column 0 the lowest Y, so Up is +I
		// and Left is -J; a mirror flips its axis. The foot of the flight is the grid edge between the approach and the first cell.
		for (const FLayoutStair& Stair : Resolved.Stairs)
		{
			const FRoomRect FirstCell = MapLayoutRect(FLayoutRect{Stair.Row, Stair.Column, Stair.Row, Stair.Column}, Resolved.Rows, Resolved.Columns, bMirrorX, bMirrorY, OriginI, OriginJ);
			const FIntPoint PlanStep = LayoutClimbStep(Stair.Climb);
			const FIntPoint Step(bMirrorX ? PlanStep.X : -PlanStep.X, bMirrorY ? -PlanStep.Y : PlanStep.Y);

			FStairRun Run;
			Run.Level = Stair.Level;
			Run.First = FIntPoint(FirstCell.MinI, FirstCell.MinJ);
			Run.Approach = Run.First - Step;
			Run.Second = Run.First + Step;
			Run.Landing = Run.First + Step * 2;
			Run.Pivot = FIntVector(2 * Run.First.X - Step.X, 2 * Run.First.Y - Step.Y, Stair.Level);

			// BP_Stairs climbs towards its local +X: +X is 0 quarter turns, +Y 1, -X 2, -Y 3.
			Run.QuarterTurns = Step.X > 0 ? 0 : (Step.Y > 0 ? 1 : (Step.X < 0 ? 2 : 3));
			StairRuns.Add(Run);
		}

		LastLayoutNote = FString::Printf(TEXT("plan %s%s%s"), *Layout->GetName(), bMirrorX ? TEXT(" mirrored X") : TEXT(""), bMirrorY ? TEXT(" mirrored Y") : TEXT(""));
	}
	else
	{
		LastLayoutNote = TEXT("random split");
	}

	// The cell just inside a perimeter edge: with a plan, the door can only go on an edge of the entrance room.
	const auto InsideCellOf = [&](const FPerimeterEdge& Edge)
	{
		switch (Edge.Side)
		{
		case EFootprintSide::MinusX: return FIntPoint(OriginI, Edge.Cell.Y / 2);
		case EFootprintSide::PlusX: return FIntPoint(OriginI + Width - 1, Edge.Cell.Y / 2);
		case EFootprintSide::MinusY: return FIntPoint(Edge.Cell.X / 2, OriginJ);
		default: return FIntPoint(Edge.Cell.X / 2, OriginJ + Depth - 1);
		}
	};

	const FRoomRect EntranceWorld = Layout ? Rooms[Resolved.EntranceRoom].Rect : FRoomRect();

	const auto IsEntranceEdge = [&](const FPerimeterEdge& Edge)
	{
		const FIntPoint Inside = InsideCellOf(Edge);

		// The front door never opens onto the side of a flight of stairs.
		if (IsFlightCell(Inside.X, Inside.Y, 0))
		{
			return false;
		}

		if (!Layout)
		{
			return true;
		}

		return Inside.X >= EntranceWorld.MinI && Inside.X <= EntranceWorld.MaxI && Inside.Y >= EntranceWorld.MinJ && Inside.Y <= EntranceWorld.MaxJ;
	};

	TArray<int32> DoorCandidates;

	for (int32 Index = 0; Index < Perimeter.Num(); ++Index)
	{
		if ((!bHasFacingSide || Perimeter[Index].Side == FacingSide) && IsEntranceEdge(Perimeter[Index]))
		{
			DoorCandidates.Add(Index);
		}
	}

	// A plan whose entrance room cannot face the target (even mirrored): the door goes on the entrance room's own wall anyway.
	const bool bDoorFacesTarget = bHasFacingSide && !DoorCandidates.IsEmpty();

	if (DoorCandidates.IsEmpty())
	{
		for (int32 Index = 0; Index < Perimeter.Num(); ++Index)
		{
			if (IsEntranceEdge(Perimeter[Index]))
			{
				DoorCandidates.Add(Index);
			}
		}
	}

	if (DoorCandidates.IsEmpty())
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': no exterior edge left for the front door (every edge of the entrance room is the side of a flight of stairs)"), *GetName());
		return false;
	}

	const int32 DoorIndex = DoorCandidates[Stream.RandRange(0, DoorCandidates.Num() - 1)];
	LastDoorSide = FString::Printf(TEXT("%s%s"), SideLabel(Perimeter[DoorIndex].Side), bDoorFacesTarget ? TEXT(" toward target") : (bHasFacingSide ? TEXT(" (entrance room does not face the target)") : TEXT("")));

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
				// The open stairwell above a flight of stairs: no floor there.
				if (IsStairwellCell(OriginI + I, OriginJ + J, Level))
				{
					continue;
				}

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

		if (Layout ? !EmitLayoutPartitions(Level, LayoutLinks, Stream) : !EmitPartitions(Level, Footprint, Stream))
		{
			return false;
		}

		// The flights that start on this storey, on its floor (they rest on the slabs of the cells they cross).
		// The plan checked that no partition crosses their foot, so the edge they stand on is free.
		for (const FStairRun& Run : StairRuns)
		{
			if (Run.Level == Level && !Place(Archetype->StairsClass, Run.Pivot, Run.QuarterTurns))
			{
				return false;
			}
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

	// Slabs too far from an exterior wall to stand get a post (see UHouseArchetype::SupportPostClass).
	if (Archetype->SupportPostClass && !AddSupportPosts())
	{
		return false;
	}

	// One ceiling lamp per room, under the slab above the room's middle cell. Placed last, once every
	// slab exists, and drawing nothing from the seed: the lamps follow from the rooms. The lamp is in the
	// structural graph (it hangs from the slab and falls with it) but costs no stability, so it cannot make
	// a house fail validation by itself.
	if (Archetype->CeilingLightClass)
	{
		for (const FRoom& Room : Rooms)
		{
			// The middle cell, unless its ceiling is an open stairwell (nothing to hang from): then the first cell
			// of the room, in cell order, that has a floor above it; a room with none gets no lamp.
			FIntPoint LampCell((Room.Rect.MinI + Room.Rect.MaxI) / 2, (Room.Rect.MinJ + Room.Rect.MaxJ) / 2);
			bool bHasCeiling = !IsStairwellCell(LampCell.X, LampCell.Y, Room.Level + 1);

			for (int32 I = Room.Rect.MinI; I <= Room.Rect.MaxI && !bHasCeiling; ++I)
			{
				for (int32 J = Room.Rect.MinJ; J <= Room.Rect.MaxJ && !bHasCeiling; ++J)
				{
					if (!IsStairwellCell(I, J, Room.Level + 1))
					{
						LampCell = FIntPoint(I, J);
						bHasCeiling = true;
					}
				}
			}

			if (!bHasCeiling)
			{
				continue;
			}

			if (!Place(Archetype->CeilingLightClass, FIntVector(2 * LampCell.X, 2 * LampCell.Y, Room.Level), 0))
			{
				return false;
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

	const TArray<ERoomType> Types = TypeRooms(Leaves);

	for (int32 Index = 0; Index < Leaves.Num(); ++Index)
	{
		Rooms.Add(FRoom{Leaves[Index], Level, Types[Index]});
	}

	return true;
}

bool AHouseGenerator::EmitLayoutPartitions(int32 Level, const TArray<TPair<int32, int32>>& Links, FRandomStream& Stream)
{
	if (!Archetype->InteriorWallClass || !Archetype->InteriorDoorWallClass)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': archetype '%s' lacks InteriorWallClass or InteriorDoorWallClass, the plan's rooms get no walls"), *GetName(), *Archetype->GetName());
		return true;
	}

	// Which room owns each cell of this storey.
	TMap<FIntPoint, int32> OwnerOf;

	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		if (Rooms[Index].Level != Level)
		{
			continue;
		}

		const FRoomRect& Rect = Rooms[Index].Rect;

		for (int32 I = Rect.MinI; I <= Rect.MaxI; ++I)
		{
			for (int32 J = Rect.MinJ; J <= Rect.MaxJ; ++J)
			{
				OwnerOf.Add(FIntPoint(I, J), Index);
			}
		}
	}

	// Every boundary between two rooms, grouped by the pair of rooms it separates. Each boundary is met once,
	// from the cell on its low side (looking at the +X and +Y neighbours), in room then cell order: a fixed order, so the seed decides alone.
	struct FWallSegment
	{
		FIntVector Edge;
		uint8 QuarterTurns = 0;
	};

	TMap<FIntPoint, TArray<FWallSegment>> SegmentsOf;
	TArray<FIntPoint> PairOrder;

	const auto AddSegment = [&SegmentsOf, &PairOrder](int32 RoomA, int32 RoomB, const FWallSegment& Segment)
	{
		const FIntPoint Pair(FMath::Min(RoomA, RoomB), FMath::Max(RoomA, RoomB));
		TArray<FWallSegment>* Found = SegmentsOf.Find(Pair);

		if (!Found)
		{
			PairOrder.Add(Pair);
			Found = &SegmentsOf.Add(Pair);
		}

		Found->Add(Segment);
	};

	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		if (Rooms[Index].Level != Level)
		{
			continue;
		}

		const FRoomRect& Rect = Rooms[Index].Rect;

		for (int32 I = Rect.MinI; I <= Rect.MaxI; ++I)
		{
			for (int32 J = Rect.MinJ; J <= Rect.MaxJ; ++J)
			{
				// Cell (I, J) spans grid 2I - 1 .. 2I + 1: its +X boundary is the vertical edge at 2I + 1 (a wall along Y, turned 0),
				// its +Y boundary the horizontal edge at 2J + 1 (a wall along X, turned 1), as for the random split's cuts.
				const int32* NextInX = OwnerOf.Find(FIntPoint(I + 1, J));

				if (NextInX && *NextInX != Index)
				{
					AddSegment(Index, *NextInX, FWallSegment{FIntVector(2 * I + 1, 2 * J, Level), 0});
				}

				const int32* NextInY = OwnerOf.Find(FIntPoint(I, J + 1));

				if (NextInY && *NextInY != Index)
				{
					AddSegment(Index, *NextInY, FWallSegment{FIntVector(2 * I, 2 * J + 1, Level), 1});
				}
			}
		}
	}

	// One door per link of this storey, on a seeded segment of the shared wall, drawn in the plan's link order.
	TMap<FIntPoint, int32> DoorOf;

	for (const TPair<int32, int32>& Link : Links)
	{
		if (!Rooms.IsValidIndex(Link.Key) || !Rooms.IsValidIndex(Link.Value) || Rooms[Link.Key].Level != Level)
		{
			continue;
		}

		const FIntPoint Pair(FMath::Min(Link.Key, Link.Value), FMath::Max(Link.Key, Link.Value));
		const TArray<FWallSegment>* Segments = SegmentsOf.Find(Pair);

		if (!Segments || Segments->IsEmpty())
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': rooms %d and %d are linked but share no wall, no door between them"), *GetName(), Link.Key, Link.Value);
			continue;
		}

		// A door must not open onto the side of a flight of stairs: the stairs are solid, so a link whose only door
		// did would cut two rooms apart. The other segments of the shared wall are preferred; the draw is still one,
		// and without stairs every segment is usable, so it is the same draw as before.
		TArray<int32> UsableSegments;

		for (int32 SegmentIndex = 0; SegmentIndex < Segments->Num(); ++SegmentIndex)
		{
			const FIntVector& Edge = (*Segments)[SegmentIndex].Edge;
			const bool bVerticalEdge = FMath::Abs(Edge.X) % 2 == 1;
			const FIntPoint CellA = bVerticalEdge ? FIntPoint((Edge.X - 1) / 2, Edge.Y / 2) : FIntPoint(Edge.X / 2, (Edge.Y - 1) / 2);
			const FIntPoint CellB = bVerticalEdge ? FIntPoint((Edge.X + 1) / 2, Edge.Y / 2) : FIntPoint(Edge.X / 2, (Edge.Y + 1) / 2);

			if (!IsFlightCell(CellA.X, CellA.Y, Level) && !IsFlightCell(CellB.X, CellB.Y, Level))
			{
				UsableSegments.Add(SegmentIndex);
			}
		}

		DoorOf.Add(Pair, UsableSegments.IsEmpty() ? Stream.RandRange(0, Segments->Num() - 1) : UsableSegments[Stream.RandRange(0, UsableSegments.Num() - 1)]);
	}

	for (const FIntPoint& Pair : PairOrder)
	{
		const TArray<FWallSegment>& Segments = SegmentsOf[Pair];
		const int32* Door = DoorOf.Find(Pair);

		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			const bool bIsDoor = Door && *Door == Index;

			if (!Place(bIsDoor ? Archetype->InteriorDoorWallClass : Archetype->InteriorWallClass, Segments[Index].Edge, Segments[Index].QuarterTurns))
			{
				return false;
			}

			Edges.Add(Segments[Index].Edge, bIsDoor ? EEdgeKind::PartitionDoor : EEdgeKind::Partition);
		}
	}

	return true;
}

bool AHouseGenerator::AddSupportPosts()
{
	UStructuralSupportComponent* Structural = Plot->GetStructuralSupport();

	// A house within the plot never needs more than a handful; the cap only stops a post that fixes nothing from looping.
	constexpr int32 MaxPosts = 24;

	for (int32 Added = 0; Added < MaxPosts; ++Added)
	{
		TArray<ABuildablePiece*> Unstable;
		Structural->FindUnstablePieces(Unstable);

		// Slabs only: a wall or a lamp above an unstable slab is unstable because of it. Lowest storey first,
		// since a slab above another unstable one may only be unstable because of it.
		int32 LowestLevel = MAX_int32;

		for (const ABuildablePiece* Piece : Unstable)
		{
			if (Piece->PieceClass == EPieceClass::FloorSlab)
			{
				LowestLevel = FMath::Min(LowestLevel, Piece->GetGridCoord().Z);
			}
		}

		// Nothing a post can fix (a slab on the ground floor is a foundation, an anchor): validation decides the rest.
		if (LowestLevel == MAX_int32 || LowestLevel < 1)
		{
			return true;
		}

		// Candidate vertices: the corners of the unstable slabs of that storey, each with how many of those slabs it would hold up.
		TMap<FIntPoint, int32> Coverage;

		for (const ABuildablePiece* Piece : Unstable)
		{
			if (Piece->PieceClass != EPieceClass::FloorSlab || Piece->GetGridCoord().Z != LowestLevel)
			{
				continue;
			}

			for (int32 SignX = -1; SignX <= 1; SignX += 2)
			{
				for (int32 SignY = -1; SignY <= 1; SignY += 2)
				{
					Coverage.FindOrAdd(FIntPoint(Piece->GetGridCoord().X + SignX, Piece->GetGridCoord().Y + SignY)) += 1;
				}
			}
		}

		// The post stands on the slab below, one storey down from the slabs it holds up.
		const int32 PostLevel = LowestLevel - 1;

		// A vertex on an interior wall wins over one in the open, so the post reads as part of the partition;
		// then the vertex that holds up the most slabs; then the lowest grid position, so the choice never
		// depends on the order the map is walked in.
		const auto OnPartition = [this, PostLevel](const FIntPoint& Vertex)
		{
			const FIntVector Around[] = {
				FIntVector(Vertex.X, Vertex.Y - 1, PostLevel), FIntVector(Vertex.X, Vertex.Y + 1, PostLevel),
				FIntVector(Vertex.X - 1, Vertex.Y, PostLevel), FIntVector(Vertex.X + 1, Vertex.Y, PostLevel)};

			for (const FIntVector& Edge : Around)
			{
				const EEdgeKind* Kind = Edges.Find(Edge);

				if (Kind && *Kind == EEdgeKind::Partition)
				{
					return true;
				}
			}

			return false;
		};

		FIntPoint Best = FIntPoint::ZeroValue;
		int32 BestScore = INDEX_NONE;

		for (const TPair<FIntPoint, int32>& Candidate : Coverage)
		{
			if (!Structural->IsSlotFree(EBuildAnchor::Vertex, FIntVector(Candidate.Key.X, Candidate.Key.Y, PostLevel)) || IsOnFlight(FIntVector(Candidate.Key.X, Candidate.Key.Y, PostLevel)))
			{
				continue;
			}

			const int32 Score = Candidate.Value + (OnPartition(Candidate.Key) ? 10 : 0);
			const bool bLowerKey = Candidate.Key.X < Best.X || (Candidate.Key.X == Best.X && Candidate.Key.Y < Best.Y);

			if (Score > BestScore || (Score == BestScore && bLowerKey))
			{
				Best = Candidate.Key;
				BestScore = Score;
			}
		}

		if (BestScore == INDEX_NONE)
		{
			return true;
		}

		const FIntVector PostCell(Best.X, Best.Y, PostLevel);

		if (!Place(Archetype->SupportPostClass, PostCell, 0))
		{
			return false;
		}

		PostVertices.Add(PostCell);
	}

	return true;
}

void AHouseGenerator::FurnishRooms(FRandomStream& Stream)
{
	const UDataTable* Table = Archetype ? Archetype->FurnishingRules.Get() : nullptr;

	if (!GetWorld() || !Plot || !Plot->GetStructuralSupport() || !Table)
	{
		return;
	}

	// Rows in name order, whatever order the table stores them in: the draws below depend on it.
	TArray<FName> RowNames = Table->GetRowNames();
	RowNames.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });

	TArray<const FFurnishingRuleRow*> Rows;

	for (const FName& RowName : RowNames)
	{
		const FFurnishingRuleRow* Row = Table->FindRow<FFurnishingRuleRow>(RowName, TEXT("AHouseGenerator::FurnishRooms"));

		if (Row && Row->PropClass)
		{
			Rows.Add(Row);
		}
		else if (Row)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': furnishing row '%s' has no PropClass, skipped"), *GetName(), *RowName.ToString());
		}
	}

	// One standing for the whole house, drawn before the first room so every room leans the same way.
	Standing = RollStanding(Stream);

	for (const FRoom& Room : Rooms)
	{
		FurnishRoom(Room, Rows, Stream);
	}
}

void AHouseGenerator::FurnishRoom(const FRoom& Room, const TArray<const FFurnishingRuleRow*>& Rows, FRandomStream& Stream)
{
	UWorld* World = GetWorld();
	const UStructuralRules* StructuralRules = Plot->GetStructuralSupport()->Rules;
	const float CellSize = StructuralRules ? StructuralRules->CellSize : 100.f;
	const FVector GridOrigin = Plot->GetGridOrigin();
	const FRoomRect& Rect = Room.Rect;
	const float FloorZ = Plot->GetPlaneZ(Room.Level, 0, 0.f);
	const EFootprintSide Sides[] = {EFootprintSide::MinusX, EFootprintSide::PlusX, EFootprintSide::MinusY, EFootprintSide::PlusY};

	// 1. What to place: per group of this room type, one row by Weight, kept if its Chance roll passes.
	TArray<FName> GroupOrder;
	TMap<FName, TArray<const FFurnishingRuleRow*>> Groups;

	for (const FFurnishingRuleRow* Row : Rows)
	{
		if (Row->RoomType == Room.Type)
		{
			TArray<const FFurnishingRuleRow*>& Variants = Groups.FindOrAdd(Row->Group);

			if (Variants.IsEmpty())
			{
				GroupOrder.Add(Row->Group);
			}

			Variants.Add(Row);
		}
	}

	TArray<FChosenProp> Chosen;

	// The row's Weight, scaled down by how many tiers its quality is from the house's standing. Only the odds between the
	// rows of one group move: a group with a single row keeps all of its weight and is furnished at any standing.
	const float TierFalloff = FMath::Clamp(Archetype->TierMismatchFalloff, 0.01f, 1.f);
	const auto EffectiveWeight = [this, TierFalloff](const FFurnishingRuleRow& Row)
	{
		return FMath::Max(0.f, Row.Weight) * FMath::Pow(TierFalloff, static_cast<float>(FMath::Abs(Row.QualityTier - Standing)));
	};

	for (const FName& Group : GroupOrder)
	{
		const TArray<const FFurnishingRuleRow*>& Variants = Groups[Group];
		float TotalWeight = 0.f;

		for (const FFurnishingRuleRow* Variant : Variants)
		{
			TotalWeight += EffectiveWeight(*Variant);
		}

		// Both rolls are drawn even when they decide nothing, so the number of draws depends on the table's groups, not on its weights.
		float WeightRoll = Stream.FRand() * TotalWeight;
		const float ChanceRoll = Stream.FRand();
		const FFurnishingRuleRow* Picked = nullptr;

		for (const FFurnishingRuleRow* Variant : Variants)
		{
			const float Weight = EffectiveWeight(*Variant);

			if (Weight > 0.f)
			{
				Picked = Variant;

				if (WeightRoll < Weight)
				{
					break;
				}

				WeightRoll -= Weight;
			}
		}

		if (!Picked || ChanceRoll >= Picked->Chance)
		{
			continue;
		}

		FChosenProp Prop;

		if (ReadProp(*Picked, Prop))
		{
			Chosen.Add(Prop);

			// Debug: only a group with several rows really chooses a tier, so only those say whether the standing leans the house.
			if (Variants.Num() > 1)
			{
				VariantTierSum += Picked->QualityTier;
				++VariantPropCount;
			}
		}
		else
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AHouseGenerator '%s': %s has no static mesh on its root component, cannot be furnished"), *GetName(), *Picked->PropClass->GetName());
		}
	}

	// Big props first: they are the hardest to fit, and the small ones then take what is left.
	Chosen.Sort([](const FChosenProp& A, const FChosenProp& B)
	{
		return A.Area != B.Area ? A.Area > B.Area : A.Row->Group.Compare(B.Row->Group) < 0;
	});

	// 2. The floor they can use: inside the walls, minus the zone in front of every door of the room.
	const auto Inset = [&](EFootprintSide Side)
	{
		const int32 First = IsXSide(Side) ? Rect.MinJ : Rect.MinI;
		const EEdgeKind* Kind = Edges.Find(SideEdgeKey(Rect, Side, First, Room.Level));

		if (!Kind)
		{
			return 0.f;
		}

		const bool bExterior = *Kind == EEdgeKind::ExteriorWall || *Kind == EEdgeKind::ExteriorDoor;
		return bExterior ? Archetype->ExteriorWallThickness : Archetype->InteriorWallThickness * 0.5f;
	};

	// An exterior wall is pivoted on its outer face, so its room-side face is a full thickness inside the edge line; a partition is centred, half a thickness each side.
	FRoomFloor Floor;
	Floor.Free = FBox2D(
		FVector2D(GridOrigin.X + (2 * Rect.MinI - 1) * CellSize + Inset(EFootprintSide::MinusX), GridOrigin.Y + (2 * Rect.MinJ - 1) * CellSize + Inset(EFootprintSide::MinusY)),
		FVector2D(GridOrigin.X + (2 * Rect.MaxI + 1) * CellSize - Inset(EFootprintSide::PlusX), GridOrigin.Y + (2 * Rect.MaxJ + 1) * CellSize - Inset(EFootprintSide::PlusY)));

	for (const EFootprintSide Side : Sides)
	{
		const bool bXSide = IsXSide(Side);

		for (int32 Along = bXSide ? Rect.MinJ : Rect.MinI; Along <= (bXSide ? Rect.MaxJ : Rect.MaxI); ++Along)
		{
			const EEdgeKind* Kind = Edges.Find(SideEdgeKey(Rect, Side, Along, Room.Level));

			if (!Kind || (*Kind != EEdgeKind::ExteriorDoor && *Kind != EEdgeKind::PartitionDoor))
			{
				continue;
			}

			// The door segment is two grid units wide, centred on the cell.
			const float Centre = (bXSide ? GridOrigin.Y : GridOrigin.X) + 2 * Along * CellSize;
			FBox2D Zone;

			switch (Side)
			{
			case EFootprintSide::MinusX: Zone = FBox2D(FVector2D(Floor.Free.Min.X, Centre - CellSize), FVector2D(Floor.Free.Min.X + DoorClearDepth, Centre + CellSize)); break;
			case EFootprintSide::PlusX: Zone = FBox2D(FVector2D(Floor.Free.Max.X - DoorClearDepth, Centre - CellSize), FVector2D(Floor.Free.Max.X, Centre + CellSize)); break;
			case EFootprintSide::MinusY: Zone = FBox2D(FVector2D(Centre - CellSize, Floor.Free.Min.Y), FVector2D(Centre + CellSize, Floor.Free.Min.Y + DoorClearDepth)); break;
			default: Zone = FBox2D(FVector2D(Centre - CellSize, Floor.Free.Max.Y - DoorClearDepth), FVector2D(Centre + CellSize, Floor.Free.Max.Y)); break;
			}

			Floor.Taken.Add(Zone);

			if (bShowDebug)
			{
				DrawDebugBox(World, FVector(Zone.GetCenter(), FloorZ + 5.f), FVector(Zone.GetExtent(), 3.f), FColor::Red, false, UnstableMarkerSeconds, 0, 2.f);
			}
		}
	}

	// The floor the stairs need clear. On the flight's own storey: the cell in front of its foot and the flight
	// itself. One storey up: the open stairwell and the landing, so nothing hangs over the edge or blocks the way off the top.
	for (const FStairRun& Run : StairRuns)
	{
		FIntPoint FromCell = FIntPoint::ZeroValue;
		FIntPoint ToCell = FIntPoint::ZeroValue;

		if (Run.Level == Room.Level)
		{
			FromCell = Run.Approach;
			ToCell = Run.Second;
		}
		else if (Run.Level + 1 == Room.Level)
		{
			FromCell = Run.First;
			ToCell = Run.Landing;
		}
		else
		{
			continue;
		}

		const FVector2D Low(GridOrigin.X + (2 * FMath::Min(FromCell.X, ToCell.X) - 1) * CellSize, GridOrigin.Y + (2 * FMath::Min(FromCell.Y, ToCell.Y) - 1) * CellSize);
		const FVector2D High(GridOrigin.X + (2 * FMath::Max(FromCell.X, ToCell.X) + 1) * CellSize, GridOrigin.Y + (2 * FMath::Max(FromCell.Y, ToCell.Y) + 1) * CellSize);
		Floor.Taken.Add(FBox2D(Low - FVector2D(PropGap, PropGap), High + FVector2D(PropGap, PropGap)));
	}

	// A support post inside or on the border of this room is solid: keep props off it. Its half width comes
	// from the Blueprint's mesh, so a thinner post needs no change here.
	float PostHalfWidth = 10.f;

	if (const ABuildablePiece* PostDefault = Archetype->SupportPostClass ? Archetype->SupportPostClass->GetDefaultObject<ABuildablePiece>() : nullptr)
	{
		const UStaticMesh* PostMesh = PostDefault->GetMeshComponent() ? PostDefault->GetMeshComponent()->GetStaticMesh() : nullptr;

		if (PostMesh)
		{
			PostHalfWidth = FMath::Max(PostMesh->GetBounds().BoxExtent.X, PostMesh->GetBounds().BoxExtent.Y);
		}
	}

	for (const FIntVector& Vertex : PostVertices)
	{
		const bool bInside = Vertex.Z == Room.Level
			&& Vertex.X >= 2 * Rect.MinI - 1 && Vertex.X <= 2 * Rect.MaxI + 1
			&& Vertex.Y >= 2 * Rect.MinJ - 1 && Vertex.Y <= 2 * Rect.MaxJ + 1;

		if (bInside)
		{
			const FVector2D Centre(GridOrigin.X + Vertex.X * CellSize, GridOrigin.Y + Vertex.Y * CellSize);
			const float Reach = PostHalfWidth + PropGap;
			Floor.Taken.Add(FBox2D(Centre - FVector2D(Reach, Reach), Centre + FVector2D(Reach, Reach)));
		}
	}

	// 3. Place them: one of the free spots the rule allows, drawn from the seed; none free = skipped.
	for (const FChosenProp& Item : Chosen)
	{
		TArray<FSpot> Spots;
		CollectSpots(Item, Floor, Spots);

		if (Spots.IsEmpty())
		{
			continue;
		}

		const FSpot& Spot = Spots[Stream.RandRange(0, Spots.Num() - 1)];

		// Where the actor goes so the rotated bounds land exactly on the spot: the pivot does not matter, the bottom of the bounds rests on the floor plus 1 cm.
		const FBox Rotated = Item.Local.TransformBy(FTransform(FRotator(0.f, Spot.Yaw, 0.f)));
		const FVector Location(Spot.Box.Min.X - Rotated.Min.X, Spot.Box.Min.Y - Rotated.Min.Y, FloorZ + 1.f - Rotated.Min.Z);
		const FTransform SpawnTransform(FRotator(0.f, Spot.Yaw, 0.f), Location, Item.Scale);

		APhysicsProp* Prop = World->SpawnActorDeferred<APhysicsProp>(Item.Row->PropClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (!Prop)
		{
			continue;
		}

		// Set before the prop starts: BeginPlay reads these. The Blueprints only carry the mesh.
		Prop->Material = Item.Row->Material;
		Prop->MaxHP = Item.Row->MaxHP;
		Prop->FragmentCount = Item.Row->FragmentCount;
		Prop->Value = Item.Row->Value;
		Prop->FinishSpawning(SpawnTransform);
		Prop->GetMeshComponent()->SetMassOverrideInKg(NAME_None, Item.Row->Mass, true);

		Props.Add(Prop);
		Floor.Taken.Add(FBox2D(Spot.Box.Min - FVector2D(PropGap, PropGap), Spot.Box.Max + FVector2D(PropGap, PropGap)));

		LayoutHash = HashCombine(LayoutHash, GetTypeHash(Item.Row->PropClass->GetPathName()));
		LayoutHash = HashCombine(LayoutHash, GetTypeHash(FIntVector(FMath::RoundToInt(Location.X), FMath::RoundToInt(Location.Y), FMath::RoundToInt(Location.Z))));
		LayoutHash = HashCombine(LayoutHash, GetTypeHash(FMath::RoundToInt(FRotator::NormalizeAxis(Spot.Yaw))));

		if (bShowDebug)
		{
			// The box is the footprint; the green arrow is where the front is meant to point, so a mesh that faces another way shows at once (fix: FrontYawOffset in the row).
			const float Height = Rotated.GetSize().Z;
			const FVector Centre(Spot.Box.GetCenter(), FloorZ + Height * 0.5f);
			const FVector Front = FRotator(0.f, Spot.FacingYaw, 0.f).Vector();

			DrawDebugBox(World, Centre, FVector(Spot.Box.GetExtent(), Height * 0.5f), FColor::White, false, UnstableMarkerSeconds, 0, 1.f);
			DrawDebugDirectionalArrow(World, Centre, Centre + Front * 60.f, 20.f, FColor::Green, false, UnstableMarkerSeconds, 0, 2.f);
		}
	}
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
		// Every room shows its type; a room of a plan also shows its letter, to check the plan reads the way it was written.
		const FString Info = Room.Label.IsEmpty() ? RoomTypeName(Room.Type) : FString::Printf(TEXT("'%s' %s"), *Room.Label, *RoomTypeName(Room.Type));
		DrawDebugString(World, Centre + FVector(0.f, 0.f, 40.f), FString::Printf(TEXT("Room %d %s (%d x %d)"), Index, *Info, Room.Rect.Width(), Room.Rect.Depth()), nullptr, Colour, UnstableMarkerSeconds, true);
	}

	// Stair flights: green where the flight and its approach are, red for the open stairwell upstairs, blue for the landing, and an arrow up the climb.
	const auto CellCentre = [&](const FIntPoint& Cell, int32 Level)
	{
		return FVector(GridOrigin.X + 2 * Cell.X * CellSize, GridOrigin.Y + 2 * Cell.Y * CellSize, Plot->GetPlaneZ(Level, 0, 0.f) + 12.f);
	};

	for (const FStairRun& Run : StairRuns)
	{
		const FVector CellHalf(CellSize - 10.f, CellSize - 10.f, 2.f);

		DrawDebugBox(World, CellCentre(Run.Approach, Run.Level), CellHalf, FColor::Silver, false, UnstableMarkerSeconds, 0, 2.f);
		DrawDebugBox(World, CellCentre(Run.First, Run.Level), CellHalf, FColor::Green, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugBox(World, CellCentre(Run.Second, Run.Level), CellHalf, FColor::Green, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugBox(World, CellCentre(Run.First, Run.Level + 1), CellHalf, FColor::Red, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugBox(World, CellCentre(Run.Second, Run.Level + 1), CellHalf, FColor::Red, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugBox(World, CellCentre(Run.Landing, Run.Level + 1), CellHalf, FColor::Blue, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugDirectionalArrow(World, CellCentre(Run.Approach, Run.Level), CellCentre(Run.Landing, Run.Level + 1), 40.f, FColor::Green, false, UnstableMarkerSeconds, 0, 3.f);
		DrawDebugString(World, CellCentre(Run.First, Run.Level) + FVector(0.f, 0.f, 60.f), FString::Printf(TEXT("Stairs %d -> %d"), Run.Level, Run.Level + 1), nullptr, FColor::Green, UnstableMarkerSeconds, true);
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
	// An open stairwell has no slab either, but it is inside the house: a wall beside it still faces the inside.
	const bool bMinusHasFloor = Plot->GetStructuralSupport()->GetPieceAt(EBuildAnchor::Cell, MinusNeighbour) != nullptr
		|| IsStairwellCell(MinusNeighbour.X / 2, MinusNeighbour.Y / 2, MinusNeighbour.Z);

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
