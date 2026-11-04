#include "HouseLayout.h"

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

bool UHouseLayout::Parse(FParsedLayout& OutLayout, TArray<FString>& OutErrors) const
{
	OutLayout = FParsedLayout();
	OutErrors.Reset();

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
