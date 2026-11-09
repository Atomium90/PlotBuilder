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

	/** A prop's own value (APhysicsProp::Value): what the client counts for a piece of furniture it asked to get back. 0 for a piece. */
	int32 Value = 0;

	/** A name to show for the item, set for the salvage furniture only: the report names it even after it was destroyed and the actor is gone. */
	FName Label;

	/**
	 *  Set by whoever picks the furniture the client wants (the salvage condition): the item must be carried out intact,
	 *  so it is neither to destroy nor to leave alone. Compute puts it in FMissionSets::Salvage and in neither of the two sets.
	 */
	bool bSalvage = false;

	/** A piece placed on the grid (a Legacy-anchored piece has no cell): its grid coordinate, X and Y on the grid, Z the storey. Meaningless for a prop and a Legacy piece. */
	FIntVector GridCoord = FIntVector::ZeroValue;
	bool bGridPiece = false;

	/**
	 *  Set by whoever picks the part the contract asks to keep standing (the keep-standing condition): the item stays, whatever
	 *  the contract type would have said. Compute puts it in the must-stay set, so destroying it is collateral.
	 */
	bool bKeep = false;
};

/**
 *  One room of the house on the grid: its storey and the grid coordinates of its perimeter. A room of cells MinI..MaxI by
 *  MinJ..MaxJ has its walls on the odd coordinates 2*MinI-1 and 2*MaxI+1 (and the same on the other axis), see FRoomRect.
 */
struct FGridRoom
{
	int32 Level = 0;
	int32 MinX = 0;
	int32 MaxX = 0;
	int32 MinY = 0;
	int32 MaxY = 0;
};

/**
 *  What a contract asks of the house: the target set (to destroy) and the must-stay set (to leave alone). Every
 *  item of the house is in exactly one of the two, except the furniture the client asked to get back (the salvage
 *  condition): that is in a third set of its own, so destroying it is not a success and not collateral, only a loss
 *  of furniture value. APlot holds the current one; the evaluator reads it.
 */
struct FMissionSets
{
	EContractType Type = EContractType::TotalDemolition;

	/** The storey a partial demolition starts from, as it was applied. 0 for the other types, and for a partial that fell back to Total. */
	int32 PartialFromLevel = 0;

	TArray<FBindingItem> Targets;
	TArray<FBindingItem> MustStay;

	/** The furniture to carry out intact to the client drop zone. Empty without a salvage condition. */
	TArray<FBindingItem> Salvage;

	int64 TargetWeight = 0;
	int64 MustStayWeight = 0;
	int64 SalvageWeight = 0;

	bool IsEmpty() const { return Targets.IsEmpty() && MustStay.IsEmpty() && Salvage.IsEmpty(); }

	/** Whether Actor is in the target set (false for anything that is not an item of the house). */
	bool IsTarget(AActor* Actor) const { return TargetActors.Contains(Actor); }

	/** Whether Actor is in the must-stay set. */
	bool IsMustStay(AActor* Actor) const { return MustStayActors.Contains(Actor); }

	/** Whether Actor is furniture the client asked to get back. */
	bool IsSalvage(AActor* Actor) const { return SalvageActors.Contains(Actor); }

	/** What Actor weighs in the report (its DT_Shop cost, see FBindingItem::Weight), whichever set it is in. 0 for anything that is not an item of the house. */
	int32 WeightOf(AActor* Actor) const
	{
		const int32* Weight = WeightByActor.Find(Actor);
		return Weight ? *Weight : 0;
	}

	/** Filled by MissionBinding::Compute, so a weight is found without scanning the three arrays (a falling piece asks for its own when it breaks). */
	TMap<TWeakObjectPtr<AActor>, int32> WeightByActor;

	/** Filled by MissionBinding::Compute, so the three queries above do not scan the arrays. */
	TSet<TWeakObjectPtr<AActor>> TargetActors;
	TSet<TWeakObjectPtr<AActor>> MustStayActors;
	TSet<TWeakObjectPtr<AActor>> SalvageActors;
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
	 *  Whatever the type, an item flagged bSalvage goes to FMissionSets::Salvage and to neither side.
	 *  Items whose actor is gone are skipped. OutNote, if given, receives a line when something was corrected.
	 */
	FMissionSets Compute(EContractType Type, int32 PartialFromLevel, int32 StoreyCount, const TArray<FBindingItem>& Items, FString* OutNote = nullptr);

	/**
	 *  Whether a piece is part of Room, for the keep-standing condition: what stands on the room's storey inside its perimeter
	 *  (its walls, the posts at its corners, its floor slab), and the slab right above it, its ceiling. Nothing else: not the walls
	 *  of the storey above, not the furniture. A prop, or a piece with no cell, is never part of a room. Coordinates only, no actor.
	 */
	bool IsPartOfRoom(const FBindingItem& Item, const FGridRoom& Room);

	/** A short readable name for an item of the house: its class name without the Blueprint prefixes ("BP_Prop_Sofa_C" gives "Sofa"). */
	FString ItemName(const AActor* Actor);

	/** Multi-line text for the log and the screen: the totals, then the count and weight of each side by piece class (props on their own line), then the furniture to salvage. */
	FString Describe(const FMissionSets& Sets);
}
