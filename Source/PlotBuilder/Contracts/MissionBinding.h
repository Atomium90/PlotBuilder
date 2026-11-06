#pragma once

#include "CoreMinimal.h"
#include "Contracts/ContractTypes.h"
#include "Destruction/DemolitionTypes.h"

class AActor;

/**
 *  One thing of the house the contract can ask to destroy or to keep: a piece, or a prop. A plain description,
 *  so the sets can be computed (and tested) without a level: whoever gathers the items reads the actors, the
 *  rules below only read this.
 */
struct FBindingItem
{
	TWeakObjectPtr<AActor> Actor;

	/** True for a prop (furniture), false for a piece. PieceClass and bLoadBearing mean nothing for a prop. */
	bool bIsProp = false;

	EPieceClass PieceClass = EPieceClass::FloorSlab;

	bool bLoadBearing = true;

	/** Storey the item belongs to: a piece's build level, the room's level for a prop. A lamp counts for the storey it lights. */
	int32 Level = 0;

	/** What the item weighs in the completion and collateral ratios (GDD §6.5): the cost of its DT_Shop row. At least 1. */
	int32 Weight = 1;
};

/**
 *  What a contract asks of the house: the target set (to destroy) and the must-stay set (to leave alone). Every
 *  item of the house is in exactly one of the two. APlot holds the current one; the evaluator reads it.
 */
struct FMissionSets
{
	EContractType Type = EContractType::TotalDemolition;

	/** The storey a partial demolition starts from, as it was applied. 0 for the other types, and for a partial that fell back to Total. */
	int32 PartialFromLevel = 0;

	TArray<FBindingItem> Targets;
	TArray<FBindingItem> MustStay;

	int64 TargetWeight = 0;
	int64 MustStayWeight = 0;

	bool IsEmpty() const { return Targets.IsEmpty() && MustStay.IsEmpty(); }

	/** Whether Actor is in the target set (false for anything that is not an item of the house). */
	bool IsTarget(AActor* Actor) const { return TargetActors.Contains(Actor); }

	/** Whether Actor is in the must-stay set. */
	bool IsMustStay(AActor* Actor) const { return MustStayActors.Contains(Actor); }

	/** Filled by MissionBinding::Compute, so the two queries above do not scan the arrays. */
	TSet<TWeakObjectPtr<AActor>> TargetActors;
	TSet<TWeakObjectPtr<AActor>> MustStayActors;
};

/**
 *  The contract binding (GDD §9.2 step 7): turns a contract type into the target and must-stay sets of a
 *  generated house. Pure functions, no world access.
 */
namespace MissionBinding
{
	/**
	 *  Splits Items into targets and must-stay for Type (GDD §6.2):
	 *   - Total: everything is a target.
	 *   - Partial: every item on a storey from PartialFromLevel up, except the floor slab of that very storey
	 *     (it becomes the ceiling of the storey that stays). The roof is above, so it goes. Needs a house with
	 *     at least two storeys and 1 <= PartialFromLevel < StoreyCount; anything else becomes a Total
	 *     demolition and says so in OutNote.
	 *   - Renovation: the props and every piece that is not load-bearing (partitions, doors, lamps...).
	 *     The load-bearing pieces stay.
	 *  Items whose actor is gone are skipped. OutNote, if given, receives a line when something was corrected.
	 */
	FMissionSets Compute(EContractType Type, int32 PartialFromLevel, int32 StoreyCount, const TArray<FBindingItem>& Items, FString* OutNote = nullptr);

	/** Multi-line text for the log and the screen: the totals, then the count and weight of each side by piece class (props on their own line). */
	FString Describe(const FMissionSets& Sets);
}
