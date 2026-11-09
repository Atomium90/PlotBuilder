#include "Contracts/MissionBinding.h"
#include "GameFramework/Actor.h"

namespace
{
	/** The partial demolition rule for one item, K being the first storey that goes. */
	bool IsInDemolishedStoreys(const FBindingItem& Item, int32 FromLevel)
	{
		if (Item.Level < FromLevel)
		{
			return false;
		}

		// The floor slab of storey K is also the ceiling of storey K - 1, which stays: it stays too.
		const bool bFloorOfFirstDemolishedStorey = !Item.bIsProp && Item.PieceClass == EPieceClass::FloorSlab && Item.Level == FromLevel;
		return !bFloorOfFirstDemolishedStorey;
	}
}

FMissionSets MissionBinding::Compute(EContractType Type, int32 PartialFromLevel, int32 StoreyCount, const TArray<FBindingItem>& Items, FString* OutNote)
{
	if (Type == EContractType::PartialDemolition && (StoreyCount < 2 || PartialFromLevel < 1 || PartialFromLevel >= StoreyCount))
	{
		if (OutNote)
		{
			*OutNote = FString::Printf(TEXT("Partial demolition from storey %d does not fit a house of %d storey(s): treated as a total demolition."), PartialFromLevel, StoreyCount);
		}

		Type = EContractType::TotalDemolition;
	}

	FMissionSets Sets;
	Sets.Type = Type;
	Sets.PartialFromLevel = Type == EContractType::PartialDemolition ? PartialFromLevel : 0;

	for (const FBindingItem& Item : Items)
	{
		AActor* Actor = Item.Actor.Get();
		if (!Actor)
		{
			continue;
		}

		// The furniture the client wants back is neither destroyed nor left alone: it has its own set.
		if (Item.bSalvage)
		{
			Sets.Salvage.Add(Item);
			Sets.SalvageActors.Add(Actor);
			Sets.SalvageWeight += Item.Weight;
			continue;
		}

		bool bTarget = true;
		switch (Type)
		{
		case EContractType::TotalDemolition:
			break;
		case EContractType::PartialDemolition:
			bTarget = IsInDemolishedStoreys(Item, PartialFromLevel);
			break;
		case EContractType::Renovation:
			bTarget = Item.bIsProp || !Item.bLoadBearing;
			break;
		}

		if (bTarget)
		{
			Sets.Targets.Add(Item);
			Sets.TargetActors.Add(Actor);
			Sets.TargetWeight += Item.Weight;
		}
		else
		{
			Sets.MustStay.Add(Item);
			Sets.MustStayActors.Add(Actor);
			Sets.MustStayWeight += Item.Weight;
		}
	}

	return Sets;
}

FString MissionBinding::ItemName(const AActor* Actor)
{
	if (!Actor)
	{
		return TEXT("(gone)");
	}

	FString Name = Actor->GetClass()->GetName();
	Name.RemoveFromStart(TEXT("BP_Prop_"));
	Name.RemoveFromStart(TEXT("BP_"));
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

FString MissionBinding::Describe(const FMissionSets& Sets)
{
	const UEnum* TypeEnum = StaticEnum<EContractType>();
	const UEnum* ClassEnum = StaticEnum<EPieceClass>();

	const FString FromText = Sets.Type == EContractType::PartialDemolition ? FString::Printf(TEXT(" from storey %d"), Sets.PartialFromLevel) : FString();

	FString Text = FString::Printf(TEXT("Contract %s%s: %d to destroy (weight %lld), %d to keep (weight %lld)"),
		*TypeEnum->GetNameStringByValue(static_cast<int64>(Sets.Type)), *FromText,
		Sets.Targets.Num(), Sets.TargetWeight, Sets.MustStay.Num(), Sets.MustStayWeight);

	if (!Sets.Salvage.IsEmpty())
	{
		Text += FString::Printf(TEXT(", %d to carry out (weight %lld)"), Sets.Salvage.Num(), Sets.SalvageWeight);
	}

	// One line per kind of item: piece classes in enum order, then the props (key -1). Count and weight, target side then keep side.
	struct FTally
	{
		int32 TargetCount = 0;
		int64 TargetWeight = 0;
		int32 KeepCount = 0;
		int64 KeepWeight = 0;
	};

	constexpr int32 PropKey = -1;
	TMap<int32, FTally> Tallies;

	auto AddTo = [&Tallies](const FBindingItem& Item, bool bTarget)
	{
		FTally& Tally = Tallies.FindOrAdd(Item.bIsProp ? PropKey : static_cast<int32>(Item.PieceClass));
		(bTarget ? Tally.TargetCount : Tally.KeepCount) += 1;
		(bTarget ? Tally.TargetWeight : Tally.KeepWeight) += Item.Weight;
	};

	for (const FBindingItem& Item : Sets.Targets)
	{
		AddTo(Item, true);
	}
	for (const FBindingItem& Item : Sets.MustStay)
	{
		AddTo(Item, false);
	}

	auto AddLine = [&Text, &Tallies](int32 Key, const FString& Name)
	{
		if (const FTally* Tally = Tallies.Find(Key))
		{
			Text += FString::Printf(TEXT("\n  %-12s destroy %3d (%lld)   keep %3d (%lld)"), *Name, Tally->TargetCount, Tally->TargetWeight, Tally->KeepCount, Tally->KeepWeight);
		}
	};

	// NumEnums() counts the hidden _MAX entry last: leave it out.
	for (int32 Index = 0; Index < ClassEnum->NumEnums() - 1; ++Index)
	{
		AddLine(static_cast<int32>(ClassEnum->GetValueByIndex(Index)), ClassEnum->GetNameStringByIndex(Index));
	}
	AddLine(PropKey, TEXT("Prop"));

	// The furniture the client wants back, one per line with its storey and value.
	for (const FBindingItem& Item : Sets.Salvage)
	{
		Text += FString::Printf(TEXT("\n  Salvage      %s (storey %d, value %d, weight %d)"), Item.Label.IsNone() ? *ItemName(Item.Actor.Get()) : *Item.Label.ToString(), Item.Level, Item.Value, Item.Weight);
	}

	return Text;
}
