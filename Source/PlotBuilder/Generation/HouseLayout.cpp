#include "HouseLayout.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	/**
	 *  Decides how many copies each row (or column) of an axis gets. The size is drawn in MinCells..MaxCells,
	 *  clamped to [Base, the most Stretch allows] and to Free; the extra cells then go to rows picked at
	 *  random from a pool that holds each row as many times as its Stretch (a row with 2 may grow by 2).
	 *  OutExtra gets one entry per row: how many copies to add after it.
	 */
	void DrawGrowth(int32 Base, int32 MinCells, int32 MaxCells, int32 Free, const TArray<int32>& Stretch, FRandomStream& Stream, TArray<int32>& OutExtra)
	{
		OutExtra.Init(0, Base);

		TArray<int32> Pool;

		if (Stretch.Num() == Base)
		{
			for (int32 Index = 0; Index < Base; ++Index)
			{
				for (int32 Copy = 0; Copy < Stretch[Index]; ++Copy)
				{
					Pool.Add(Index);
				}
			}
		}

		const int32 Reach = FMath::Min(Free, Base + Pool.Num());

		if (Reach <= Base)
		{
			return;
		}

		const int32 Low = FMath::Clamp(MinCells, Base, Reach);
		const int32 High = FMath::Clamp(MaxCells, Low, Reach);
		const int32 Extra = Stream.RandRange(Low, High) - Base;

		// Partial shuffle: the first Extra entries of the pool are the rows that grow.
		for (int32 Draw = 0; Draw < Extra; ++Draw)
		{
			const int32 Pick = Stream.RandRange(Draw, Pool.Num() - 1);
			Pool.Swap(Draw, Pick);
			++OutExtra[Pool[Draw]];
		}
	}

	/** Where each original row (or column) starts in the grown grid: every row takes 1 + its copies. */
	TArray<int32> StartsFromExtra(const TArray<int32>& Extra)
	{
		TArray<int32> Starts;
		int32 Next = 0;

		for (const int32 Copies : Extra)
		{
			Starts.Add(Next);
			Next += 1 + Copies;
		}

		return Starts;
	}
}

UHouseLayout* UHouseLayout::ExpandForFloors(int32 Floors, UObject* Outer, FString& OutError) const
{
	if (!bRepeatTypicalStorey)
	{
		OutError = TEXT("bRepeatTypicalStorey is off: this plan is used as written");
		return nullptr;
	}

	if (Levels.Num() != 2)
	{
		OutError = FString::Printf(TEXT("bRepeatTypicalStorey needs exactly 2 grids in Levels (the ground floor, then the typical storey), there are %d"), Levels.Num());
		return nullptr;
	}

	Floors = FMath::Max(1, Floors);

	// The flight leaving storey L is the entry authored for Level L % 2.
	const auto FlightOfParity = [this](int32 Parity) -> const FLayoutStair*
	{
		return Stairs.FindByPredicate([Parity](const FLayoutStair& Stair) { return Stair.Level == Parity; });
	};

	if (Floors >= 2 && !FlightOfParity(0))
	{
		OutError = TEXT("bRepeatTypicalStorey needs a flight in Stairs with Level 0 (the one leaving the ground floor)");
		return nullptr;
	}

	if (Floors >= 3 && !FlightOfParity(1))
	{
		OutError = TEXT("bRepeatTypicalStorey needs a flight in Stairs with Level 1 (the one leaving a typical storey, on other cells than the Level 0 flight)");
		return nullptr;
	}

	// Letters are keyed by their character code: TCHAR has no portable hash.
	const auto KeyOf = [](TCHAR Letter) { return static_cast<int32>(Letter); };

	// Every character a grid or the legend already uses is taken: the copies get the free ones.
	TSet<int32> Used;
	TSet<int32> InGround;
	TSet<int32> InTypical;

	for (const FString& Row : Levels[0].Rows)
	{
		for (const TCHAR Letter : Row)
		{
			Used.Add(KeyOf(Letter));
			InGround.Add(KeyOf(Letter));
		}
	}

	for (const FString& Row : Levels[1].Rows)
	{
		for (const TCHAR Letter : Row)
		{
			Used.Add(KeyOf(Letter));
			InTypical.Add(KeyOf(Letter));
		}
	}

	for (const FLayoutRoomDef& Def : Legend)
	{
		if (Def.Letter.Len() == 1)
		{
			Used.Add(KeyOf(Def.Letter[0]));
		}
	}

	// The legend entries of the typical storey, by index: the letters its grid draws.
	TArray<int32> TypicalLegend;

	for (int32 Index = 0; Index < Legend.Num(); ++Index)
	{
		const FString& Letter = Legend[Index].Letter;

		if (Letter.Len() == 1 && InTypical.Contains(KeyOf(Letter[0])))
		{
			if (InGround.Contains(KeyOf(Letter[0])))
			{
				OutError = FString::Printf(TEXT("The letter '%s' is used on both the ground floor and the typical storey: use a different letter for each room"), *Letter);
				return nullptr;
			}

			TypicalLegend.Add(Index);
		}
	}

	const auto IsTypicalLetter = [&InTypical, &KeyOf](const FString& Letter) { return Letter.Len() == 1 && InTypical.Contains(KeyOf(Letter[0])); };

	UHouseLayout* Out = NewObject<UHouseLayout>(Outer, MakeUniqueObjectName(Outer, UHouseLayout::StaticClass(), FName(*FString::Printf(TEXT("%s_x%d"), *GetName(), Floors))), RF_Transient);
	Out->bAllowMirrorX = bAllowMirrorX;
	Out->bAllowMirrorY = bAllowMirrorY;
	Out->EntranceRoom = EntranceRoom;
	Out->RowStretch = RowStretch;
	Out->ColumnStretch = ColumnStretch;
	Out->Levels.Add(Levels[0]);

	for (int32 Index = 0; Index < Legend.Num(); ++Index)
	{
		if (!TypicalLegend.Contains(Index))
		{
			Out->Legend.Add(Legend[Index]);
		}
	}

	// A link that does not name two typical rooms is the ground floor's (or a mistake Parse will report): kept as written, once.
	for (const FLayoutLink& Link : Links)
	{
		if (!(IsTypicalLetter(Link.RoomA) && IsTypicalLetter(Link.RoomB)))
		{
			Out->Links.Add(Link);
		}
	}

	TCHAR NextLetter = TEXT('!');

	const auto TakeLetter = [&Used, &NextLetter, &KeyOf]() -> TCHAR
	{
		while (NextLetter <= TEXT('~') && (Used.Contains(KeyOf(NextLetter)) || NextLetter == TEXT('.')))
		{
			++NextLetter;
		}

		if (NextLetter > TEXT('~'))
		{
			return TEXT('\0');
		}

		Used.Add(KeyOf(NextLetter));
		return NextLetter++;
	};

	for (int32 Storey = 1; Storey < Floors; ++Storey)
	{
		// The first typical storey keeps the plan's letters, the copies take free ones.
		TMap<int32, TCHAR> Remap;

		for (const int32 Index : TypicalLegend)
		{
			const TCHAR Original = Legend[Index].Letter[0];
			const TCHAR Letter = Storey == 1 ? Original : TakeLetter();

			if (Letter == TEXT('\0'))
			{
				OutError = FString::Printf(TEXT("Out of free characters for the letters of %d storeys: the plan has too many rooms per storey for this many storeys"), Floors);
				return nullptr;
			}

			Remap.Add(KeyOf(Original), Letter);

			FLayoutRoomDef Def = Legend[Index];
			Def.Letter = FString::Chr(Letter);

			// A swap group stays inside its storey.
			if (!Def.SwapGroup.IsNone())
			{
				Def.SwapGroup = FName(*FString::Printf(TEXT("%s.%d"), *Def.SwapGroup.ToString(), Storey));
			}

			Out->Legend.Add(Def);
		}

		FLayoutLevel Grid;

		for (const FString& Row : Levels[1].Rows)
		{
			FString Copy = Row;

			for (TCHAR& Letter : Copy)
			{
				if (const TCHAR* Mapped = Remap.Find(KeyOf(Letter)))
				{
					Letter = *Mapped;
				}
			}

			Grid.Rows.Add(Copy);
		}

		Out->Levels.Add(Grid);

		for (const FLayoutLink& Link : Links)
		{
			if (IsTypicalLetter(Link.RoomA) && IsTypicalLetter(Link.RoomB))
			{
				FLayoutLink Copy;
				Copy.RoomA = FString::Chr(Remap[KeyOf(Link.RoomA[0])]);
				Copy.RoomB = FString::Chr(Remap[KeyOf(Link.RoomB[0])]);
				Out->Links.Add(Copy);
			}
		}
	}

	for (int32 Storey = 0; Storey + 1 < Floors; ++Storey)
	{
		FLayoutStair Flight = *FlightOfParity(Storey % 2);
		Flight.Level = Storey;
		Out->Stairs.Add(Flight);
	}

	return Out;
}

bool UHouseLayout::Parse(FParsedLayout& OutLayout, TArray<FString>& OutErrors) const
{
	OutLayout = FParsedLayout();
	OutErrors.Reset();

	// A plan that repeats its typical storey is checked through a four-storey expansion (the ground floor and three typical
	// storeys), so both flights are tried. The result is that of the expansion: the generator expands to the real count first.
	if (bRepeatTypicalStorey)
	{
		constexpr int32 ProbeFloors = 4;
		FString ExpandError;
		const UHouseLayout* Probe = ExpandForFloors(ProbeFloors, GetTransientPackage(), ExpandError);

		if (!Probe)
		{
			OutErrors.Add(ExpandError);
			return false;
		}

		return Probe->Parse(OutLayout, OutErrors);
	}

	if (Levels.IsEmpty())
	{
		OutErrors.Add(TEXT("Levels is empty: add one grid per storey"));
		return false;
	}

	// Letters are keyed by their character code: TCHAR has no portable hash.
	const auto KeyOf = [](TCHAR Letter) { return static_cast<int32>(Letter); };

	// Legend: one character each, '.' and spaces reserved, unique. LegendIndex keeps the first definition of a letter.
	TMap<int32, int32> LegendIndex;

	for (int32 Index = 0; Index < Legend.Num(); ++Index)
	{
		const FString& Letter = Legend[Index].Letter;

		if (Letter.Len() != 1 || Letter[0] == TEXT('.') || FChar::IsWhitespace(Letter[0]))
		{
			OutErrors.Add(FString::Printf(TEXT("Legend[%d]: '%s' must be exactly one character, and not '.' or a space"), Index, *Letter));
		}
		else if (LegendIndex.Contains(KeyOf(Letter[0])))
		{
			OutErrors.Add(FString::Printf(TEXT("Legend[%d]: the letter '%s' is already defined above"), Index, *Letter));
		}
		else
		{
			LegendIndex.Add(KeyOf(Letter[0]), Index);
		}
	}

	// The grid: every storey has the size of the first one.
	const int32 RowCount = Levels[0].Rows.Num();
	const int32 ColumnCount = RowCount > 0 ? Levels[0].Rows[0].Len() : 0;

	if (RowCount == 0 || ColumnCount == 0)
	{
		OutErrors.Add(TEXT("Levels[0] has no rows, or its first row is empty"));
		return false;
	}

	OutLayout.Rows = RowCount;
	OutLayout.Columns = ColumnCount;

	struct FLetterCells
	{
		int32 Level = INDEX_NONE;
		FLayoutRect Rect;
		int32 Count = 0;
	};

	TMap<int32, FLetterCells> CellsOf;
	TSet<int32> ReportedLetters;
	bool bReportedDot = false;

	for (int32 Level = 0; Level < Levels.Num(); ++Level)
	{
		const TArray<FString>& Grid = Levels[Level].Rows;

		if (Grid.Num() != RowCount)
		{
			OutErrors.Add(FString::Printf(TEXT("Levels[%d] has %d rows, Levels[0] has %d: all storeys must be the same size"), Level, Grid.Num(), RowCount));
			continue;
		}

		for (int32 Row = 0; Row < RowCount; ++Row)
		{
			if (Grid[Row].Len() != ColumnCount)
			{
				OutErrors.Add(FString::Printf(TEXT("Levels[%d] row %d has %d characters, expected %d"), Level, Row, Grid[Row].Len(), ColumnCount));
				continue;
			}

			for (int32 Column = 0; Column < ColumnCount; ++Column)
			{
				const TCHAR Letter = Grid[Row][Column];
				const int32 Key = KeyOf(Letter);

				if (Letter == TEXT('.'))
				{
					if (!bReportedDot)
					{
						OutErrors.Add(TEXT("'.' (no room) is reserved for L-shaped footprints and is not supported yet: the plan must be a full rectangle"));
						bReportedDot = true;
					}

					continue;
				}

				if (!LegendIndex.Contains(Key))
				{
					if (!ReportedLetters.Contains(Key))
					{
						OutErrors.Add(FString::Printf(TEXT("Levels[%d] row %d: the letter '%c' is not in the Legend"), Level, Row, Letter));
						ReportedLetters.Add(Key);
					}

					continue;
				}

				FLetterCells& Cells = CellsOf.FindOrAdd(Key);

				if (Cells.Count == 0)
				{
					Cells.Level = Level;
					Cells.Rect = FLayoutRect{Row, Column, Row, Column};
				}
				else if (Cells.Level != Level)
				{
					if (!ReportedLetters.Contains(Key))
					{
						OutErrors.Add(FString::Printf(TEXT("The letter '%c' is used on Levels[%d] and Levels[%d]: use a different letter for each room"), Letter, Cells.Level, Level));
						ReportedLetters.Add(Key);
					}

					continue;
				}
				else
				{
					Cells.Rect.MinRow = FMath::Min(Cells.Rect.MinRow, Row);
					Cells.Rect.MaxRow = FMath::Max(Cells.Rect.MaxRow, Row);
					Cells.Rect.MinCol = FMath::Min(Cells.Rect.MinCol, Column);
					Cells.Rect.MaxCol = FMath::Max(Cells.Rect.MaxCol, Column);
				}

				++Cells.Count;
			}
		}
	}

	// Rooms, in Legend order (never in map order), each of them a filled rectangle.
	TMap<int32, int32> RoomOf;

	for (int32 Index = 0; Index < Legend.Num(); ++Index)
	{
		const FString& Letter = Legend[Index].Letter;
		const int32* First = Letter.Len() == 1 ? LegendIndex.Find(KeyOf(Letter[0])) : nullptr;
		const FLetterCells* Cells = First && *First == Index ? CellsOf.Find(KeyOf(Letter[0])) : nullptr;

		// A letter that is defined but never drawn is simply not a room.
		if (!Cells)
		{
			continue;
		}

		const int32 BoxArea = (Cells->Rect.MaxRow - Cells->Rect.MinRow + 1) * (Cells->Rect.MaxCol - Cells->Rect.MinCol + 1);

		if (Cells->Count != BoxArea)
		{
			OutErrors.Add(FString::Printf(TEXT("Room '%s' is not a filled rectangle: %d cells in a %d x %d block"),
				*Letter, Cells->Count, Cells->Rect.MaxRow - Cells->Rect.MinRow + 1, Cells->Rect.MaxCol - Cells->Rect.MinCol + 1));
		}

		FParsedLayoutRoom Room;
		Room.Letter = Letter;
		Room.Level = Cells->Level;
		Room.Rect = Cells->Rect;
		Room.Type = Legend[Index].Type;
		Room.SwapGroup = Legend[Index].SwapGroup;
		RoomOf.Add(KeyOf(Letter[0]), OutLayout.Rooms.Add(Room));
	}

	if (OutLayout.Rooms.IsEmpty())
	{
		OutErrors.Add(TEXT("No room: the grid uses no letter of the Legend"));
	}

	if (!RowStretch.IsEmpty() && RowStretch.Num() != RowCount)
	{
		OutErrors.Add(FString::Printf(TEXT("RowStretch has %d entries but the grid has %d rows (leave it empty for none)"), RowStretch.Num(), RowCount));
	}

	if (!ColumnStretch.IsEmpty() && ColumnStretch.Num() != ColumnCount)
	{
		OutErrors.Add(FString::Printf(TEXT("ColumnStretch has %d entries but the grid has %d columns (leave it empty for none)"), ColumnStretch.Num(), ColumnCount));
	}

	// Links: two different rooms of the same storey that share a wall, each pair once.
	TSet<FIntPoint> LinkKeys;

	for (int32 Index = 0; Index < Links.Num(); ++Index)
	{
		const FLayoutLink& Link = Links[Index];
		const int32* A = Link.RoomA.Len() == 1 ? RoomOf.Find(KeyOf(Link.RoomA[0])) : nullptr;
		const int32* B = Link.RoomB.Len() == 1 ? RoomOf.Find(KeyOf(Link.RoomB[0])) : nullptr;

		if (!A || !B)
		{
			OutErrors.Add(FString::Printf(TEXT("Links[%d] '%s' - '%s': names a letter that is not a room of the grid"), Index, *Link.RoomA, *Link.RoomB));
			continue;
		}

		if (*A == *B)
		{
			OutErrors.Add(FString::Printf(TEXT("Links[%d] joins room '%s' to itself"), Index, *Link.RoomA));
			continue;
		}

		const FParsedLayoutRoom& RoomA = OutLayout.Rooms[*A];
		const FParsedLayoutRoom& RoomB = OutLayout.Rooms[*B];

		if (RoomA.Level != RoomB.Level)
		{
			OutErrors.Add(FString::Printf(TEXT("Links[%d] '%s' - '%s': the rooms are on different storeys"), Index, *Link.RoomA, *Link.RoomB));
			continue;
		}

		if (!RoomA.Rect.TouchesAlongAWall(RoomB.Rect))
		{
			OutErrors.Add(FString::Printf(TEXT("Links[%d] '%s' - '%s': the rooms do not share a wall"), Index, *Link.RoomA, *Link.RoomB));
			continue;
		}

		const FIntPoint LinkKey(FMath::Min(*A, *B), FMath::Max(*A, *B));

		if (LinkKeys.Contains(LinkKey))
		{
			OutErrors.Add(FString::Printf(TEXT("Links[%d] '%s' - '%s' is listed twice"), Index, *Link.RoomA, *Link.RoomB));
			continue;
		}

		LinkKeys.Add(LinkKey);
		OutLayout.Links.Add(TPair<int32, int32>(*A, *B));
	}

	// Stair flights: along its climb a flight uses the approach, two flight cells and the landing, in one line.
	// The approach and the flight stand in one room on their storey (no partition may cross the foot of the
	// stairs), the flight's two cells (the open stairwell) and the landing in one room on the storey above.
	const auto RoomAt = [&](int32 Level, int32 Row, int32 Column) -> int32
	{
		if (!Levels.IsValidIndex(Level) || !Levels[Level].Rows.IsValidIndex(Row) || Column < 0 || Column >= Levels[Level].Rows[Row].Len())
		{
			return INDEX_NONE;
		}

		const int32* Room = RoomOf.Find(KeyOf(Levels[Level].Rows[Row][Column]));
		return Room ? *Room : INDEX_NONE;
	};

	TSet<FIntVector> FlightFeet;
	TSet<FIntVector> Stairwells;
	TArray<TPair<int32, FIntVector>> StairLandings;

	for (int32 Index = 0; Index < Stairs.Num(); ++Index)
	{
		const FLayoutStair& Stair = Stairs[Index];
		const FIntPoint Step = LayoutClimbStep(Stair.Climb);

		if (Stair.Level < 0 || Stair.Level + 1 >= Levels.Num())
		{
			OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: Level %d needs a storey above it, the plan has %d storey(s)"), Index, Stair.Level, Levels.Num()));
			continue;
		}

		const auto CellAt = [&](int32 FlightIndex) { return FIntPoint(Stair.Row + Step.X * FlightIndex, Stair.Column + Step.Y * FlightIndex); };
		const FIntPoint Approach = CellAt(-1);
		const FIntPoint First = CellAt(0);
		const FIntPoint Second = CellAt(1);
		const FIntPoint Landing = CellAt(2);
		const auto Inside = [&](const FIntPoint& Cell) { return Cell.X >= 0 && Cell.X < RowCount && Cell.Y >= 0 && Cell.Y < ColumnCount; };

		if (!Inside(Approach) || !Inside(Landing))
		{
			OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: the approach cell (row %d, column %d) and the landing cell (row %d, column %d) must be inside the grid"), Index, Approach.X, Approach.Y, Landing.X, Landing.Y));
			continue;
		}

		const int32 FootRoom = RoomAt(Stair.Level, Approach.X, Approach.Y);

		if (FootRoom == INDEX_NONE || RoomAt(Stair.Level, First.X, First.Y) != FootRoom || RoomAt(Stair.Level, Second.X, Second.Y) != FootRoom)
		{
			OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: the approach cell and the two flight cells must be one room on Levels[%d]"), Index, Stair.Level));
			continue;
		}

		const int32 TopRoom = RoomAt(Stair.Level + 1, First.X, First.Y);

		if (TopRoom == INDEX_NONE || RoomAt(Stair.Level + 1, Second.X, Second.Y) != TopRoom || RoomAt(Stair.Level + 1, Landing.X, Landing.Y) != TopRoom)
		{
			OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: the two flight cells and the landing cell must be one room on Levels[%d]"), Index, Stair.Level + 1));
			continue;
		}

		// A flight's cells (the approach too) belong to it alone on their storey, and the stairwell it opens upstairs has no floor.
		for (int32 FlightIndex = -1; FlightIndex <= 1; ++FlightIndex)
		{
			bool bAlreadyUsed = false;
			FlightFeet.Add(FIntVector(CellAt(FlightIndex).X, CellAt(FlightIndex).Y, Stair.Level), &bAlreadyUsed);

			if (bAlreadyUsed)
			{
				OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: shares a cell with another flight on Levels[%d]"), Index, Stair.Level));
				break;
			}
		}

		Stairwells.Add(FIntVector(First.X, First.Y, Stair.Level + 1));
		Stairwells.Add(FIntVector(Second.X, Second.Y, Stair.Level + 1));
		StairLandings.Emplace(Index, FIntVector(Landing.X, Landing.Y, Stair.Level + 1));
		OutLayout.Stairs.Add(Stair);
	}

	// No flight may start in, or land on, the open stairwell another flight leaves in the floor.
	for (const FLayoutStair& Stair : OutLayout.Stairs)
	{
		const FIntPoint Step = LayoutClimbStep(Stair.Climb);

		for (int32 FlightIndex = -1; FlightIndex <= 1; ++FlightIndex)
		{
			if (Stairwells.Contains(FIntVector(Stair.Row + Step.X * FlightIndex, Stair.Column + Step.Y * FlightIndex, Stair.Level)))
			{
				OutErrors.Add(FString::Printf(TEXT("A flight on Levels[%d] (row %d, column %d) stands on the open stairwell of a flight from the storey below"), Stair.Level, Stair.Row, Stair.Column));
				break;
			}
		}
	}

	for (const TPair<int32, FIntVector>& Landing : StairLandings)
	{
		if (Stairwells.Contains(Landing.Value))
		{
			OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: the landing is inside the open stairwell of another flight"), Landing.Key));
		}
	}

	// Every room of a storey must be reachable from the others through the Links, or the player could not walk to it.
	for (int32 Level = 0; Level < Levels.Num(); ++Level)
	{
		int32 RoomsOnLevel = 0;
		int32 FirstRoom = INDEX_NONE;

		for (int32 Index = 0; Index < OutLayout.Rooms.Num(); ++Index)
		{
			if (OutLayout.Rooms[Index].Level == Level)
			{
				++RoomsOnLevel;
				FirstRoom = FirstRoom == INDEX_NONE ? Index : FirstRoom;
			}
		}

		if (RoomsOnLevel < 2)
		{
			continue;
		}

		TSet<int32> Reached;
		TArray<int32> Pending;
		Reached.Add(FirstRoom);
		Pending.Add(FirstRoom);

		while (!Pending.IsEmpty())
		{
			const int32 Current = Pending.Pop();

			for (const TPair<int32, int32>& Link : OutLayout.Links)
			{
				const int32 Other = Link.Key == Current ? Link.Value : (Link.Value == Current ? Link.Key : INDEX_NONE);

				if (Other != INDEX_NONE && !Reached.Contains(Other))
				{
					Reached.Add(Other);
					Pending.Add(Other);
				}
			}
		}

		if (Reached.Num() != RoomsOnLevel)
		{
			OutErrors.Add(FString::Printf(TEXT("Levels[%d]: %d of %d rooms cannot be reached through the Links"), Level, RoomsOnLevel - Reached.Num(), RoomsOnLevel));
		}
	}

	// The front door: a room of the ground floor with a wall on the outside.
	const int32* Entrance = EntranceRoom.Len() == 1 ? RoomOf.Find(KeyOf(EntranceRoom[0])) : nullptr;

	if (!Entrance)
	{
		OutErrors.Add(FString::Printf(TEXT("EntranceRoom '%s' is not a room of the grid"), *EntranceRoom));
	}
	else
	{
		const FParsedLayoutRoom& Room = OutLayout.Rooms[*Entrance];
		const bool bOnTheEdge = Room.Rect.MinRow == 0 || Room.Rect.MaxRow == RowCount - 1 || Room.Rect.MinCol == 0 || Room.Rect.MaxCol == ColumnCount - 1;

		if (Room.Level != 0)
		{
			OutErrors.Add(FString::Printf(TEXT("EntranceRoom '%s' is on Levels[%d]: the front door is on the ground floor"), *EntranceRoom, Room.Level));
		}
		else if (!bOnTheEdge)
		{
			OutErrors.Add(FString::Printf(TEXT("EntranceRoom '%s' has no wall on the outside, so the door has nowhere to go"), *EntranceRoom));
		}
		else
		{
			OutLayout.EntranceRoom = *Entrance;
		}
	}

	// Walking. The flights and the stairwells they open take their cells, and that can cut a plan in two: a corridor
	// one cell wide with a stairwell in it, or a band two cells wide with the stairwell on one row and a flight on the
	// other. Every cell left must be reachable from the front door (ground floor) or from the first landing (upper
	// storeys), moving freely inside a room and from one room to another only where the two are linked, since that is
	// where a door can go. The plan is checked as drawn: stretching only copies rows and columns, it never takes a cell away.
	if (OutErrors.IsEmpty())
	{
		TSet<FIntPoint> LinkedRooms;

		for (const TPair<int32, int32>& Link : OutLayout.Links)
		{
			LinkedRooms.Add(FIntPoint(FMath::Min(Link.Key, Link.Value), FMath::Max(Link.Key, Link.Value)));
		}

		const FIntPoint Directions[] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};

		for (int32 Level = 0; Level < Levels.Num(); ++Level)
		{
			// Taken on this storey: the two cells of each flight that starts here, and the open stairwell of each flight from below.
			TSet<FIntPoint> Taken;
			FIntPoint Start(INDEX_NONE, INDEX_NONE);

			for (const FLayoutStair& Stair : OutLayout.Stairs)
			{
				const FIntPoint Step = LayoutClimbStep(Stair.Climb);

				if (Stair.Level == Level || Stair.Level + 1 == Level)
				{
					Taken.Add(FIntPoint(Stair.Row, Stair.Column));
					Taken.Add(FIntPoint(Stair.Row + Step.X, Stair.Column + Step.Y));
				}

				if (Stair.Level + 1 == Level && Start.X == INDEX_NONE)
				{
					Start = FIntPoint(Stair.Row + 2 * Step.X, Stair.Column + 2 * Step.Y);
				}
			}

			for (int32 Row = 0; Level == 0 && Row < RowCount && Start.X == INDEX_NONE; ++Row)
			{
				for (int32 Column = 0; Column < ColumnCount && Start.X == INDEX_NONE; ++Column)
				{
					if (RoomAt(0, Row, Column) == OutLayout.EntranceRoom && !Taken.Contains(FIntPoint(Row, Column)))
					{
						Start = FIntPoint(Row, Column);
					}
				}
			}

			// A storey no flight reaches is the generator's to refuse, not a walking problem.
			if (Start.X == INDEX_NONE || Taken.Contains(Start))
			{
				continue;
			}

			TSet<FIntPoint> Reached;
			TArray<FIntPoint> Pending;
			Reached.Add(Start);
			Pending.Add(Start);

			while (!Pending.IsEmpty())
			{
				const FIntPoint Cell = Pending.Pop();
				const int32 Room = RoomAt(Level, Cell.X, Cell.Y);

				for (const FIntPoint& Direction : Directions)
				{
					const FIntPoint Next = Cell + Direction;
					const int32 NextRoom = RoomAt(Level, Next.X, Next.Y);

					if (NextRoom == INDEX_NONE || Taken.Contains(Next) || Reached.Contains(Next))
					{
						continue;
					}

					if (NextRoom != Room && !LinkedRooms.Contains(FIntPoint(FMath::Min(Room, NextRoom), FMath::Max(Room, NextRoom))))
					{
						continue;
					}

					Reached.Add(Next);
					Pending.Add(Next);
				}
			}

			// Cells nobody can walk to, room by room (the first one is where to look).
			TMap<int32, FIntPoint> FirstCut;
			TMap<int32, int32> CutCount;

			for (int32 Row = 0; Row < RowCount; ++Row)
			{
				for (int32 Column = 0; Column < ColumnCount; ++Column)
				{
					const FIntPoint Cell(Row, Column);
					const int32 Room = RoomAt(Level, Row, Column);

					if (Room != INDEX_NONE && !Taken.Contains(Cell) && !Reached.Contains(Cell))
					{
						FirstCut.FindOrAdd(Room, Cell);
						++CutCount.FindOrAdd(Room);
					}
				}
			}

			for (int32 Room = 0; Room < OutLayout.Rooms.Num(); ++Room)
			{
				if (const FIntPoint* Cell = FirstCut.Find(Room))
				{
					OutErrors.Add(FString::Printf(TEXT("Levels[%d]: %d cell(s) of room '%s' (the first at row %d, column %d) cannot be walked to once the flights and stairwells take their cells: widen the passage or move a flight"),
						Level, CutCount[Room], *OutLayout.Rooms[Room].Letter, Cell->X, Cell->Y));
				}
			}

			for (int32 Index = 0; Index < OutLayout.Stairs.Num(); ++Index)
			{
				const FLayoutStair& Stair = OutLayout.Stairs[Index];
				const FIntPoint Step = LayoutClimbStep(Stair.Climb);
				const FIntPoint Approach(Stair.Row - Step.X, Stair.Column - Step.Y);
				const FIntPoint Landing(Stair.Row + 2 * Step.X, Stair.Column + 2 * Step.Y);

				if (Stair.Level == Level && !Reached.Contains(Approach))
				{
					OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: nobody can walk to its approach cell (row %d, column %d) on Levels[%d]"), Index, Approach.X, Approach.Y, Level));
				}
				else if (Stair.Level + 1 == Level && !Reached.Contains(Landing))
				{
					OutErrors.Add(FString::Printf(TEXT("Stairs[%d]: its landing cell (row %d, column %d) on Levels[%d] is cut off from the other flights and the doors"), Index, Landing.X, Landing.Y, Level));
				}
			}

			// A link needs two walkable cells facing each other across the shared wall, or its door has nowhere to go.
			for (const TPair<int32, int32>& Link : OutLayout.Links)
			{
				const FParsedLayoutRoom& RoomA = OutLayout.Rooms[Link.Key];

				if (RoomA.Level != Level)
				{
					continue;
				}

				bool bDoorPossible = false;

				for (int32 Row = RoomA.Rect.MinRow; Row <= RoomA.Rect.MaxRow && !bDoorPossible; ++Row)
				{
					for (int32 Column = RoomA.Rect.MinCol; Column <= RoomA.Rect.MaxCol && !bDoorPossible; ++Column)
					{
						if (!Reached.Contains(FIntPoint(Row, Column)))
						{
							continue;
						}

						for (const FIntPoint& Direction : Directions)
						{
							const FIntPoint Next = FIntPoint(Row, Column) + Direction;

							if (RoomAt(Level, Next.X, Next.Y) == Link.Value && Reached.Contains(Next))
							{
								bDoorPossible = true;
								break;
							}
						}
					}
				}

				if (!bDoorPossible)
				{
					OutErrors.Add(FString::Printf(TEXT("Levels[%d]: no door can go between rooms '%s' and '%s': every cell along their shared wall is a stairwell or a flight, or cut off"),
						Level, *RoomA.Letter, *OutLayout.Rooms[Link.Value].Letter));
				}
			}
		}
	}

	return OutErrors.IsEmpty();
}

void UHouseLayout::Resolve(const FParsedLayout& Parsed, FIntPoint MinCells, FIntPoint MaxCells, FIntPoint FreeCells, FRandomStream& Stream, FResolvedLayout& OutLayout) const
{
	OutLayout = FResolvedLayout();

	// Both mirrors are always drawn, so the rest of the stream does not depend on which flips a plan allows.
	OutLayout.bMirrorX = Stream.RandRange(0, 1) == 1 && bAllowMirrorX;
	OutLayout.bMirrorY = Stream.RandRange(0, 1) == 1 && bAllowMirrorY;

	TArray<int32> RowExtra;
	TArray<int32> ColumnExtra;
	DrawGrowth(Parsed.Rows, MinCells.X, MaxCells.X, FreeCells.X, RowStretch, Stream, RowExtra);
	DrawGrowth(Parsed.Columns, MinCells.Y, MaxCells.Y, FreeCells.Y, ColumnStretch, Stream, ColumnExtra);

	const TArray<int32> RowStart = StartsFromExtra(RowExtra);
	const TArray<int32> ColumnStart = StartsFromExtra(ColumnExtra);

	OutLayout.Rows = Parsed.Rows;
	OutLayout.Columns = Parsed.Columns;

	for (const int32 Copies : RowExtra)
	{
		OutLayout.Rows += Copies;
	}

	for (const int32 Copies : ColumnExtra)
	{
		OutLayout.Columns += Copies;
	}

	// A copied row repeats its letters, so a room that crosses it grows with it and stays a rectangle.
	OutLayout.Rooms = Parsed.Rooms;

	for (FParsedLayoutRoom& Room : OutLayout.Rooms)
	{
		const FLayoutRect Grown{
			RowStart[Room.Rect.MinRow],
			ColumnStart[Room.Rect.MinCol],
			RowStart[Room.Rect.MaxRow] + RowExtra[Room.Rect.MaxRow],
			ColumnStart[Room.Rect.MaxCol] + ColumnExtra[Room.Rect.MaxCol]};
		Room.Rect = Grown;
	}

	OutLayout.Links = Parsed.Links;
	OutLayout.EntranceRoom = Parsed.EntranceRoom;

	// A flight moves with the copies and stays valid: copies only repeat letters, so the cells next to each other
	// in the grown grid are still the same rooms. Along its climb the first flight cell is the copy of its row
	// (or column) the approach touches: the first copy when climbing towards a higher index, the last towards
	// a lower one. Across its climb any copy is in the same room, the first is taken. No draw.
	for (const FLayoutStair& Stair : Parsed.Stairs)
	{
		const FIntPoint Step = LayoutClimbStep(Stair.Climb);
		FLayoutStair Grown = Stair;
		Grown.Row = Step.X < 0 ? RowStart[Stair.Row] + RowExtra[Stair.Row] : RowStart[Stair.Row];
		Grown.Column = Step.Y < 0 ? ColumnStart[Stair.Column] + ColumnExtra[Stair.Column] : ColumnStart[Stair.Column];
		OutLayout.Stairs.Add(Grown);
	}

	// Type swaps, group by group in order of first appearance: a Fisher-Yates shuffle of the group's types.
	TArray<FName> Groups;

	for (const FParsedLayoutRoom& Room : OutLayout.Rooms)
	{
		if (!Room.SwapGroup.IsNone())
		{
			Groups.AddUnique(Room.SwapGroup);
		}
	}

	for (const FName& Group : Groups)
	{
		TArray<int32> Members;

		for (int32 Index = 0; Index < OutLayout.Rooms.Num(); ++Index)
		{
			if (OutLayout.Rooms[Index].SwapGroup == Group)
			{
				Members.Add(Index);
			}
		}

		for (int32 Last = Members.Num() - 1; Last > 0; --Last)
		{
			Swap(OutLayout.Rooms[Members[Last]].Type, OutLayout.Rooms[Members[Stream.RandRange(0, Last)]].Type);
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UHouseLayout::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);

	FParsedLayout Parsed;
	TArray<FString> Errors;

	if (Parse(Parsed, Errors))
	{
		return SuperResult == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
	}

	for (const FString& Error : Errors)
	{
		Context.AddError(FText::FromString(Error));
	}

	return EDataValidationResult::Invalid;
}
#endif
