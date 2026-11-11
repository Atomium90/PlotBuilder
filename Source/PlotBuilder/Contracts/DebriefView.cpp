#include "Contracts/DebriefView.h"

namespace
{
	/** The line of the report with that label (the evaluator's own wording), or nullptr. */
	const FDebriefLine* DebriefFindLine(const FDebriefResult& Result, const TCHAR* Label)
	{
		return Result.Lines.FindByPredicate([Label](const FDebriefLine& Line) { return Line.Label == Label; });
	}

	/** The money a penalty line took, as a positive number (its Money is negative). 0 if the report has no such line. */
	int32 DebriefLoss(const FDebriefResult& Result, const TCHAR* Label)
	{
		const FDebriefLine* Line = DebriefFindLine(Result, Label);
		return Line ? FMath::Max(0, -Line->Money) : 0;
	}

	FString DebriefPercent(const float Ratio)
	{
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Ratio * 100.f));
	}

	FString DebriefTimes(const int32 Count)
	{
		return Count == 1 ? FString(TEXT("once")) : FString::Printf(TEXT("%d times"), Count);
	}

	/** The penalty terms in the order the waterfall lists them. */
	enum EDebriefTerm : int32
	{
		DebriefTermCollateral,
		DebriefTermZones,
		DebriefTermFurniture,
		DebriefTermSafety,
		DebriefTermTool,
		DebriefTermCount
	};
}

FDebriefView MissionDebrief::Build(const FDebriefResult& Result, const FNotebookData& Notebook)
{
	FDebriefView View;
	View.BasePayout = Result.BasePayout;
	View.Payout = Result.Payout;

	int32 Running = Result.BasePayout;

	const auto AddTotal = [&View](const TCHAR* Name, const FString& Detail, const int32 Figure)
	{
		FDebriefStep& Step = View.Steps.AddDefaulted_GetRef();
		Step.Name = Name;
		Step.Detail = Detail;
		Step.From = 0;
		Step.To = Figure;
		Step.Kind = EDebriefStepKind::Total;
	};

	const auto AddLoss = [&View, &Running](const TCHAR* Name, const FString& Detail, const int32 Amount)
	{
		FDebriefStep& Step = View.Steps.AddDefaulted_GetRef();
		Step.Name = Name;
		Step.Detail = Detail;
		Step.From = Running;
		Step.To = Running - Amount;
		Step.Kind = Amount > 0 ? EDebriefStepKind::Loss : EDebriefStepKind::Nothing;
		Running -= Amount;
	};

	// What each term took, then what the player really lost to penalties. They differ only when the cap on the penalty stepped in: the
	// terms alone would have taken more than the player had earned, so each is shown for its share of what was really taken, and the
	// waterfall never goes under zero.
	int32 Taken[DebriefTermCount] = {
		DebriefLoss(Result, TEXT("Collateral")),
		DebriefLoss(Result, TEXT("Protected zones")),
		DebriefLoss(Result, TEXT("Furniture")),
		DebriefLoss(Result, TEXT("Safety")),
		DebriefLoss(Result, TEXT("Forbidden tool"))};

	int32 Terms = 0;
	int32 Largest = 0;

	for (int32 Index = 0; Index < DebriefTermCount; ++Index)
	{
		Terms += Taken[Index];
		Largest = Taken[Index] > Taken[Largest] ? Index : Largest;
	}

	// The upkeep of the tools is taken after the penalty and is not one of the terms: only what is left of the earned money counts here.
	const int32 Applied = FMath::Max(0, Result.Earned - Result.Payout - Result.Upkeep);
	const bool bReduced = Terms > Applied;

	if (bReduced && Terms > 0)
	{
		int32 Sum = 0;

		for (int32 Index = 0; Index < DebriefTermCount; ++Index)
		{
			Taken[Index] = FMath::FloorToInt(static_cast<double>(Taken[Index]) * static_cast<double>(Applied) / static_cast<double>(Terms));
			Sum += Taken[Index];
		}

		// The rounding leaves a few units: they go to the biggest term, so the steps add up to the payout to the unit.
		Taken[Largest] += Applied - Sum;
	}

	const FString Capped = bReduced ? FString(TEXT(" (reduced by the cap)")) : FString();

	AddTotal(TEXT("Base payout"), TEXT("What a perfect job pays"), Result.BasePayout);

	// What was not brought down is paid for in proportion.
	const int32 NotDone = Result.BasePayout - Result.Earned;

	if (NotDone > 0)
	{
		AddLoss(TEXT("Left standing"), FString::Printf(TEXT("%s of the house came down"), *DebriefPercent(Result.Completion)), NotDone);
		View.Steps.Last().Kind = EDebriefStepKind::Unfinished;
	}

	if (Taken[DebriefTermCollateral] > 0)
	{
		AddLoss(TEXT("Collateral"), FString::Printf(TEXT("%s of what had to stay was destroyed%s"), *DebriefPercent(Result.Collateral), *Capped), Taken[DebriefTermCollateral]);
	}

	// The terms of the contract are listed even when they cost nothing, so the player sees they were checked.
	if (const FNotebookSection* Zones = Notebook.FindSection(ENotebookSection::Zones))
	{
		const FString What = Zones->SummaryName == Zones->Title ? Zones->Summary : FString::Printf(TEXT("%s, %s"), *Zones->SummaryName, *Zones->Summary.ToLower());
		AddLoss(TEXT("Protected zone"), What + Capped, Taken[DebriefTermZones]);
	}
	else if (Taken[DebriefTermZones] > 0)
	{
		AddLoss(TEXT("Protected zone"), FString::Printf(TEXT("%s damage%s"), *DebriefPercent(Result.ZoneDamage), *Capped), Taken[DebriefTermZones]);
	}

	if (const FNotebookSection* Furniture = Notebook.FindSection(ENotebookSection::Furniture))
	{
		AddLoss(TEXT("Furniture"), Furniture->Summary + Capped, Taken[DebriefTermFurniture]);
	}
	else if (Taken[DebriefTermFurniture] > 0)
	{
		AddLoss(TEXT("Furniture"), FString::Printf(TEXT("%s of the furniture value lost%s"), *DebriefPercent(Result.FurnitureLoss), *Capped), Taken[DebriefTermFurniture]);
	}

	if (Taken[DebriefTermSafety] > 0)
	{
		FString What;

		if (Result.CrushEvents > 0)
		{
			What = FString::Printf(TEXT("The collapse caught you %s"), *DebriefTimes(Result.CrushEvents));
		}

		if (Result.ExplosionEvents > 0)
		{
			What += FString::Printf(TEXT("%sAn explosion caught you %s"), What.IsEmpty() ? TEXT("") : TEXT(". "), *DebriefTimes(Result.ExplosionEvents));
		}

		AddLoss(TEXT("Safety"), What + Capped, Taken[DebriefTermSafety]);
	}

	if (const FNotebookSection* Tool = Notebook.FindSection(ENotebookSection::ForbiddenTool))
	{
		const bool bUsed = Tool->Status == ENotebookState::Warning;
		AddLoss(TEXT("Forbidden tool"),
			(bUsed ? FString::Printf(TEXT("%s was used, once for the whole mission"), *Tool->SummaryName) : FString::Printf(TEXT("%s not used"), *Tool->SummaryName)) + (bUsed ? Capped : FString()),
			Taken[DebriefTermTool]);
	}
	else if (Taken[DebriefTermTool] > 0)
	{
		AddLoss(TEXT("Forbidden tool"), FString(TEXT("A forbidden tool was used")) + Capped, Taken[DebriefTermTool]);
	}

	// The tools that cost something to keep, when one of them worked.
	if (const FDebriefLine* UpkeepLine = DebriefFindLine(Result, TEXT("Upkeep")))
	{
		AddLoss(TEXT("Upkeep"), UpkeepLine->Detail, Result.Upkeep);

		if (Result.Upkeep > 0)
		{
			View.Steps.Last().Kind = EDebriefStepKind::Upkeep;
		}
	}

	AddTotal(TEXT("Paid to you"), TEXT("What reaches your account"), Result.Payout);

	// The loss that cost the most (the "Left standing" step counts: not finishing the job is a way to lose money).
	int32 Biggest = 0;

	for (int32 Index = 0; Index < View.Steps.Num(); ++Index)
	{
		const FDebriefStep& Step = View.Steps[Index];

		const bool bCounts = Step.Kind == EDebriefStepKind::Loss || Step.Kind == EDebriefStepKind::Unfinished;

		if (bCounts && Step.Amount() > Biggest)
		{
			Biggest = Step.Amount();
			View.BiggestLossIndex = Index;
		}
	}

	return View;
}
