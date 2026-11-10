#include "Contracts/MissionNotebook.h"
#include "Contracts/ContractEvaluator.h"

namespace
{
	FString NotebookPercent(float Ratio)
	{
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Ratio * 100.f));
	}

	/** Below this a zone counts as untouched (rounding noise from the hit points). */
	constexpr float NotebookDamageEpsilon = 0.005f;
}

FString FNotebookData::ToString() const
{
	FString Text = FString::Printf(TEXT("Notebook: house demolished %s, job %s"), *NotebookPercent(Completion), bJobComplete ? TEXT("complete") : TEXT("not complete"));

	for (const FNotebookSection& Section : Sections)
	{
		Text += FString::Printf(TEXT("\n  %s%s%s"), *Section.Title, Section.Count.IsEmpty() ? TEXT("") : TEXT("  "), *Section.Count);

		for (const FNotebookLine& Line : Section.Lines)
		{
			Text += FString::Printf(TEXT("\n    [%s] %s: %s%s"),
				*StaticEnum<ENotebookState>()->GetNameStringByValue(static_cast<int64>(Line.Status)),
				*Line.Name, *Line.State,
				Line.Damage >= 0.f ? *FString::Printf(TEXT(" (bar %s)"), *NotebookPercent(Line.Damage)) : TEXT(""));
		}
	}

	return Text;
}

FString MissionNotebook::RoomName(FName RoomType)
{
	const FString Source = RoomType.ToString();
	FString Result;

	for (int32 Index = 0; Index < Source.Len(); ++Index)
	{
		const TCHAR Character = Source[Index];

		if (Index > 0 && FChar::IsUpper(Character))
		{
			Result.AppendChar(TEXT(' '));
			Result.AppendChar(FChar::ToLower(Character));
		}
		else
		{
			Result.AppendChar(Character);
		}
	}

	return Result;
}

FNotebookData MissionNotebook::Build(const FContractDefinition& Contract, const FMissionSnapshot& Snapshot, const FScoringParams& Params)
{
	FNotebookData Data;
	Data.Contract = Contract;
	Data.Completion = UContractEvaluator::ComputeCompletion(Snapshot);

	// The furniture the client asked for.
	if (!Snapshot.Salvage.IsEmpty())
	{
		FNotebookSection& Section = Data.Sections.AddDefaulted_GetRef();
		Section.Kind = ENotebookSection::Furniture;
		Section.Title = TEXT("Furniture to recover");
		Section.SummaryName = Section.Title;

		// In hands, still to do, delivered, lost: the lines that need the player come first.
		TArray<FNotebookLine> Ranked[4];
		int32 Delivered = 0;
		int32 Lost = 0;
		int32 ValueKept = 0;
		int32 ValueAsked = 0;
		FString LastPending;

		for (const FSalvageResult& Item : Snapshot.Salvage)
		{
			FNotebookLine Line;
			Line.Name = Item.Name;
			int32 Rank = 1;

			// What the client counts for the piece: its value through the health curve if it was delivered, nothing otherwise.
			const int32 Kept = Item.bDelivered ? FMath::RoundToInt(static_cast<float>(Item.Value) * UContractEvaluator::FurnitureValueMultiplier(Params, Item.HPRatio)) : 0;
			Line.Figure = FString::Printf(TEXT("%d / %d"), Kept, Item.Value);
			ValueKept += Kept;
			ValueAsked += Item.Value;

			if (Item.bDelivered)
			{
				Line.Status = ENotebookState::Done;
				Line.State = FString::Printf(TEXT("Delivered, %s"), *NotebookPercent(Item.HPRatio));
				++Delivered;
				Rank = 2;
			}
			else if (Item.bDestroyed)
			{
				Line.Status = ENotebookState::Lost;
				Line.State = TEXT("Destroyed");
				++Lost;
				Rank = 3;
			}
			else
			{
				++Data.PendingFurniture;
				LastPending = Item.Name;

				if (Item.bCarried)
				{
					Line.Status = ENotebookState::Carried;
					Line.State = TEXT("In your hands");
					Rank = 0;
				}
				else
				{
					Line.Status = ENotebookState::Todo;
					Line.State = Item.bOnSite ? TEXT("Still on the plot") : TEXT("Left outside the plot");
				}
			}

			Ranked[Rank].Add(MoveTemp(Line));
		}

		for (TArray<FNotebookLine>& Group : Ranked)
		{
			Section.Lines.Append(MoveTemp(Group));
		}

		const int32 Total = Snapshot.Salvage.Num();
		Section.Figure = FString::Printf(TEXT("%d kept of %d"), ValueKept, ValueAsked);
		Section.Count = FString::Printf(TEXT("%d / %d"), Delivered, Total);
		Section.Summary = FString::Printf(TEXT("%d of %d delivered"), Delivered, Total);

		if (Data.PendingFurniture == 1)
		{
			Section.Summary += FString::Printf(TEXT(", %s still to deliver"), *LastPending);
		}
		else if (Data.PendingFurniture > 1)
		{
			Section.Summary += FString::Printf(TEXT(", %d still to deliver"), Data.PendingFurniture);
		}

		if (Lost > 0)
		{
			Section.Summary += FString::Printf(TEXT(", %d destroyed"), Lost);
		}

		Section.Status = (Data.PendingFurniture > 0 || Lost > 0) ? ENotebookState::Warning : ENotebookState::Ok;
	}

	// The protected zones the contract switched on.
	if (!Snapshot.Zones.IsEmpty())
	{
		FNotebookSection& Section = Data.Sections.AddDefaulted_GetRef();
		Section.Kind = ENotebookSection::Zones;
		Section.Title = Snapshot.Zones.Num() == 1 ? TEXT("Protected zone") : TEXT("Protected zones");
		Section.Count = FString::Printf(TEXT("%d active"), Snapshot.Zones.Num());

		int32 Damaged = 0;

		for (const FZoneResult& Zone : Snapshot.Zones)
		{
			const float Damage = UContractEvaluator::ComputeSingleZoneDamage(Snapshot, Zone, Params);

			FNotebookLine Line;
			Line.Name = Zone.Label.IsEmpty() ? FString(TEXT("Protected zone")) : Zone.Label;

			if (Damage < NotebookDamageEpsilon)
			{
				Line.Status = ENotebookState::Ok;
				Line.State = TEXT("Untouched");
			}
			else
			{
				Line.Status = ENotebookState::Warning;
				Line.State = FString::Printf(TEXT("Damaged, %s"), *NotebookPercent(Damage));
				Line.Damage = Damage;
				++Damaged;
			}

			Section.Lines.Add(MoveTemp(Line));
		}

		Section.Status = Damaged > 0 ? ENotebookState::Warning : ENotebookState::Ok;

		if (Section.Lines.Num() == 1)
		{
			Section.SummaryName = Section.Lines[0].Name;
			Section.Summary = Section.Lines[0].State;
		}
		else
		{
			Section.SummaryName = Section.Title;
			Section.Summary = Damaged > 0 ? FString::Printf(TEXT("%d of %d damaged"), Damaged, Section.Lines.Num()) : FString(TEXT("Untouched"));
		}
	}

	// The room the client wants left standing. It reads the whole must-stay set: that set holds the room, and a room cannot be hurt without the set being hurt.
	if (const FContractCondition* Keep = Contract.FindCondition(EConditionType::KeepStanding))
	{
		FNotebookSection& Section = Data.Sections.AddDefaulted_GetRef();
		Section.Kind = ENotebookSection::KeepStanding;
		Section.Title = TEXT("Keep standing");

		const float Down = Snapshot.MustStayWeight > 0
			? FMath::Clamp(static_cast<float>(Snapshot.MustStayDestroyedWeight) / static_cast<float>(Snapshot.MustStayWeight), 0.f, 1.f)
			: 0.f;

		FNotebookLine Line;
		Line.Name = RoomName(Keep->Param);

		if (Snapshot.MustStayDestroyedWeight <= 0)
		{
			Line.Status = ENotebookState::Ok;
			Line.State = TEXT("Still standing");
		}
		else
		{
			Line.Status = ENotebookState::Warning;
			Line.State = FString::Printf(TEXT("%s of what had to stay is down"), *NotebookPercent(Down));
			Line.Damage = Down;
		}

		Section.Status = Line.Status;
		Section.SummaryName = Line.Name;
		Section.Summary = Line.State;
		Section.Lines.Add(MoveTemp(Line));
	}

	// The tool the client forbade.
	const FContractCondition* Forbidden = Contract.FindCondition(EConditionType::ForbiddenTool);
	const FName Tool = !Snapshot.ForbiddenToolName.IsNone() ? Snapshot.ForbiddenToolName : (Forbidden ? Forbidden->Param : NAME_None);

	if (!Tool.IsNone())
	{
		FNotebookSection& Section = Data.Sections.AddDefaulted_GetRef();
		Section.Kind = ENotebookSection::ForbiddenTool;
		Section.Title = TEXT("Forbidden tool");

		FNotebookLine Line;
		Line.Name = Tool.ToString();

		if (Snapshot.bForbiddenToolUsed)
		{
			Line.Status = ENotebookState::Warning;
			Line.State = TEXT("Used, it will show in the report");
		}
		else
		{
			Line.Status = ENotebookState::Ok;
			Line.State = TEXT("Not used");
		}

		Section.Status = Line.Status;
		Section.SummaryName = Line.Name;
		Section.Summary = Line.State;
		Section.Lines.Add(MoveTemp(Line));
	}

	Data.bJobComplete = Snapshot.TargetDestroyedWeight >= Snapshot.TargetWeight && Data.PendingFurniture == 0;

	return Data;
}
