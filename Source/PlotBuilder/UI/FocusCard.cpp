#include "FocusCard.h"

FString FocusCard::MaterialName(const EDemolitionMaterial Material)
{
	switch (Material)
	{
	case EDemolitionMaterial::Plaster:
		return TEXT("Plaster");
	case EDemolitionMaterial::Brick:
		return TEXT("Brick");
	case EDemolitionMaterial::Concrete:
		return TEXT("Concrete");
	case EDemolitionMaterial::Glass:
		return TEXT("Glass");
	case EDemolitionMaterial::Metal:
		return TEXT("Metal");
	default:
		return TEXT("Wood");
	}
}

bool FocusCard::ShownIn(const EGameFlowState State)
{
	return State == EGameFlowState::OnSite || State == EGameFlowState::InHome;
}

FFocusCardView FocusCard::Describe(const FFocusCardFacts& Facts)
{
	FFocusCardView View;

	if (!Facts.bHasTarget)
	{
		return View;
	}

	View.bVisible = true;
	View.Name = Facts.Name;

	// Material first, then where it is (a storey for a piece of the house) or what it is for (furniture).
	View.Sub = MaterialName(Facts.Material);

	if (Facts.Storey == 0)
	{
		View.Sub += TEXT(" · ground floor");
	}
	else if (Facts.Storey > 0)
	{
		View.Sub += FString::Printf(TEXT(" · storey %d"), Facts.Storey);
	}

	if (!Facts.Role.IsEmpty())
	{
		View.Sub += TEXT(" · ") + Facts.Role;
	}

	const float Ratio = FMath::Clamp(Facts.HPRatio, 0.f, 1.f);

	if (Facts.bCompromised)
	{
		View.StateText = TEXT("Compromised");
		View.StateTone = EFocusTone::Bad;
	}
	else if (Facts.State == EDurabilityState::Damaged)
	{
		View.StateText = TEXT("Damaged");
		View.StateTone = EFocusTone::Warn;
	}
	else if (Facts.State == EDurabilityState::Critical || Facts.State == EDurabilityState::Destroyed)
	{
		View.StateText = TEXT("Critical");
		View.StateTone = EFocusTone::Bad;
	}
	else
	{
		View.StateText = TEXT("Intact");
		View.StateTone = EFocusTone::Good;
	}

	// Outside a mission the bar says nothing at full health: it appears with the first knock. Rounded down, so it never reads 100 while hurt.
	View.bShowBar = Facts.bInMission || Facts.State != EDurabilityState::Intact || Ratio < 0.999f;
	View.Fill = Ratio;
	// The small margin absorbs the float error of a ratio such as 0.62, which times 100 lands a hair under 62.
	View.FillText = FString::Printf(TEXT("%d %%"), FMath::FloorToInt(Ratio * 100.f + 0.001f));

	if (Facts.bInMission && Facts.bHasToolMatch)
	{
		const FString Material = MaterialName(Facts.Material).ToLower();

		View.bShowTool = true;

		if (Facts.ToolMultiplier >= GoodMultiplier)
		{
			View.ToolTone = EFocusTone::Good;
			View.ToolText = FString::Printf(TEXT("%s: good on %s"), *Facts.ToolName, *Material);
		}
		else if (Facts.ToolMultiplier >= SlowMultiplier)
		{
			View.ToolTone = EFocusTone::Warn;
			View.ToolText = FString::Printf(TEXT("%s: slow on %s"), *Facts.ToolName, *Material);
		}
		else
		{
			View.ToolTone = EFocusTone::Bad;
			View.ToolText = FString::Printf(TEXT("%s: barely scratches %s"), *Facts.ToolName, *Material);
		}
	}

	if (Facts.bCompromised)
	{
		View.bShowWarning = true;
		View.WarningText = TEXT("Nothing holds it up: about to fall");
	}

	if (Facts.bIsSalvage && Facts.Value > 0)
	{
		const int32 Now = FMath::RoundToInt(static_cast<float>(Facts.Value) * FMath::Clamp(Facts.ValueShare, 0.f, 1.f));

		View.bShowValue = true;
		View.ValueNow = FString::FromInt(Now);
		View.ValueText = FString::Printf(TEXT("of %d at this health"), Facts.Value);
	}

	// Never in a mission: a house only comes down through the tools there.
	View.bShowTakeBack = Facts.bCanTakeBack && !Facts.bInMission;

	return View;
}
