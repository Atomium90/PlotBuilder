#pragma once

#include "CoreMinimal.h"
#include "Contracts/ContractTypes.h"
#include "Contracts/MissionNotebook.h"
#include "DebriefView.generated.h"

/** What a step of the waterfall does to the running total. */
UENUM(BlueprintType)
enum class EDebriefStepKind : uint8
{
	/** A whole bar from 0: the base payout at the top, the payout at the bottom. */
	Total,
	/** Money lost to a mistake or an accident: the running total goes down from From to To. */
	Loss,
	/** Money not earned because the job was not finished (a part of the house left standing). A loss in the figures, but not a fault: it is drawn in a neutral colour. */
	Unfinished,
	/** A term that applies to the contract and cost nothing (a forbidden tool that was not used). */
	Nothing,
	/** What the tools cost to keep. Money gone, but the cost of working and not a mistake: drawn in a neutral colour, and never the biggest loss. Appended last so the serialized values stay valid. */
	Upkeep
};

/** One row of the waterfall: where the running total starts, where it ends, and what to say about it. */
USTRUCT(BlueprintType)
struct FDebriefStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	FString Name;

	/** What happened, in words ("92 % of the house came down"). */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	FString Detail;

	/** The running total before and after the step. A Total goes from 0 to its figure. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 From = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 To = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	EDebriefStepKind Kind = EDebriefStepKind::Loss;

	/** The money the step moves, always positive: what a Loss took, or the figure of a Total. */
	int32 Amount() const { return Kind == EDebriefStepKind::Total ? To : FMath::Abs(From - To); }
};

/**
 *  The report turned into what the debrief draws: the waterfall from the base payout down to what is paid, with one step per
 *  cause that applies to the contract, and the cause that cost the most. Plain data, built from the report and the final notebook
 *  (MissionDebrief::Build), so what the screen says is tested without a level.
 *
 *  The steps always add up: the running total at the end is the payout, to the unit, because the report rounds its money term by term.
 *  When the cap on the penalty steps in, the causes are shown for their share of what was really taken (never under zero).
 */
USTRUCT(BlueprintType)
struct FDebriefView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 BasePayout = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 Payout = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	TArray<FDebriefStep> Steps;

	/** Index in Steps of the loss that cost the most, or INDEX_NONE when nothing was lost. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 BiggestLossIndex = INDEX_NONE;
};

namespace MissionDebrief
{
	/**
	 *  The waterfall for Result. Notebook is the final notebook of the mission (it says which terms apply: the zones, the furniture,
	 *  the forbidden tool, and what happened to each); an empty one gives the money steps only.
	 */
	FDebriefView Build(const FDebriefResult& Result, const FNotebookData& Notebook);
}
