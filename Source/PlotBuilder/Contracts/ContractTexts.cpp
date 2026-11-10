#include "Contracts/ContractTexts.h"
#include "Contracts/MissionNotebook.h"

FText ContractTexts::TypeLabel(const EContractType Type)
{
	switch (Type)
	{
	case EContractType::PartialDemolition:
		return NSLOCTEXT("Contract", "TypePartial", "Partial demolition");
	case EContractType::Renovation:
		return NSLOCTEXT("Contract", "TypeRenovation", "Renovation");
	default:
		return NSLOCTEXT("Contract", "TypeTotal", "Total demolition");
	}
}

FText ContractTexts::SiteName(const FContractDefinition& Contract)
{
	const FString Id = Contract.ArchetypeId.ToString();

	if (Id.Contains(TEXT("Apartment")))
	{
		return NSLOCTEXT("Contract", "SiteApartment", "Apartment block");
	}

	if (Id.Contains(TEXT("ThreeStorey")))
	{
		return NSLOCTEXT("Contract", "SiteThreeStorey", "Three-storey house");
	}

	if (Id.Contains(TEXT("TwoStorey")))
	{
		return NSLOCTEXT("Contract", "SiteTwoStorey", "Two-storey house");
	}

	if (Id.Contains(TEXT("Bungalow")))
	{
		return NSLOCTEXT("Contract", "SiteBungalow", "Bungalow");
	}

	return NSLOCTEXT("Contract", "SiteHouse", "House");
}

FString ContractTexts::Initials(const FString& Name)
{
	TArray<FString> Words;
	Name.ParseIntoArray(Words, TEXT(" "), true);

	FString Result;

	for (const FString& Word : Words)
	{
		// "&" and the like are not words.
		if (Result.Len() < 2 && !Word.IsEmpty() && FChar::IsAlpha(Word[0]))
		{
			Result.AppendChar(FChar::ToUpper(Word[0]));
		}
	}

	return Result;
}

FString ContractTexts::MinutesString(const float Minutes)
{
	const float Half = FMath::RoundToFloat(FMath::Max(0.f, Minutes) * 2.f) / 2.f;

	// A whole number reads "5", a half reads "10.5".
	return FMath::IsNearlyEqual(Half, FMath::RoundToFloat(Half)) ? FString::Printf(TEXT("%d"), FMath::RoundToInt(Half)) : FString::Printf(TEXT("%.1f"), Half);
}

FText ContractTexts::Job(const FContractDefinition& Contract)
{
	const FText Site = SiteName(Contract).ToLower();

	switch (Contract.Type)
	{
	case EContractType::PartialDemolition:
		return NSLOCTEXT("Contract", "JobPartial", "The upper storeys come down. The storeys below have to stay.");
	case EContractType::Renovation:
		return FText::Format(NSLOCTEXT("Contract", "JobRenovation", "Strip the inside of the {0}. The shell has to stay."), Site);
	default:
		return FText::Format(NSLOCTEXT("Contract", "JobTotal", "Take the whole {0} down."), Site);
	}
}

FText ContractTexts::Summary(const FContractDefinition& Contract)
{
	return FText::Format(NSLOCTEXT("Contract", "Summary", "{0}, {1} m². {2}"), SiteName(Contract), FText::AsNumber(FMath::RoundToInt(Contract.AreaM2)), Job(Contract));
}

TArray<ContractTexts::FConditionText> ContractTexts::Conditions(const FContractDefinition& Contract)
{
	TArray<FConditionText> Result;

	for (const FContractCondition& Condition : Contract.Conditions)
	{
		FConditionText& Text = Result.AddDefaulted_GetRef();
		Text.Type = Condition.Type;

		switch (Condition.Type)
		{
		case EConditionType::SalvageFurniture:
			Text.Chip = Condition.Count == 1
				? NSLOCTEXT("Contract", "ChipSalvageOne", "1 piece")
				: FText::Format(NSLOCTEXT("Contract", "ChipSalvage", "{0} pieces"), Condition.Count);
			Text.Title = NSLOCTEXT("Contract", "TitleSalvage", "Pieces to recover");
			Text.Detail = Condition.Count == 1
				? NSLOCTEXT("Contract", "DetailSalvageOne", "Carry the listed piece to the client zone. A piece left inside counts as lost.")
				: FText::Format(NSLOCTEXT("Contract", "DetailSalvage", "Carry the {0} listed pieces to the client zone. A piece left inside counts as lost."), Condition.Count);
			break;

		case EConditionType::ProtectedZone:
			Text.Chip = Condition.Count == 1
				? NSLOCTEXT("Contract", "ChipZoneOne", "1 zone")
				: FText::Format(NSLOCTEXT("Contract", "ChipZone", "{0} zones"), Condition.Count);
			Text.Title = NSLOCTEXT("Contract", "TitleZone", "Protected zone");
			Text.Detail = Condition.Count == 1
				? NSLOCTEXT("Contract", "DetailZoneOne", "What stands in the marked zone must not be damaged, by a tool or by falling pieces.")
				: FText::Format(NSLOCTEXT("Contract", "DetailZone", "What stands in the {0} marked zones must not be damaged, by a tool or by falling pieces."), Condition.Count);
			break;

		case EConditionType::KeepStanding:
		{
			const FText Room = FText::FromString(MissionNotebook::RoomName(Condition.Param).ToLower());
			Text.Chip = FText::Format(NSLOCTEXT("Contract", "ChipKeep", "Keep {0}"), Room);
			Text.Title = NSLOCTEXT("Contract", "TitleKeep", "Keep standing");
			Text.Detail = FText::Format(NSLOCTEXT("Contract", "DetailKeep", "The {0} must still stand at the end."), Room);
			break;
		}

		case EConditionType::ForbiddenTool:
		{
			const FText Tool = FText::FromString(Condition.Param.ToString().ToLower());
			Text.Chip = FText::Format(NSLOCTEXT("Contract", "ChipTool", "No {0}"), Tool);
			Text.Title = NSLOCTEXT("Contract", "TitleTool", "Forbidden tool");
			Text.Detail = FText::Format(NSLOCTEXT("Contract", "DetailTool", "The {0} is not allowed on this site."), Tool);
			break;
		}
		}
	}

	return Result;
}
