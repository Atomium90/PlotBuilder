#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Contracts/ContractTypes.h"
#include "ContractEvaluator.generated.h"

/**
 *  Turns the result of a mission into the client's report and the money (GDD §6.5). Pure functions: the input is plain
 *  numbers (FMissionSnapshot, FScoringParams), the output is a FDebriefResult, nothing reads the world. Whoever ends the
 *  mission fills the snapshot from the level; the tests fill it by hand, which is what makes the formula testable and
 *  explainable term by term.
 *
 *  The payment is always made: there is no failure state, a sloppy job only pays less.
 *
 *  Completion    = destroyed weight of the target set / weight of the target set (1 if there is nothing to destroy)
 *  Collateral    = destroyed weight of the must-stay set / weight of the must-stay set (0 if nothing must stay)
 *  ZoneDamage    = sum over the active protected zones of min(1, tool damage ratio + fallen weight / house weight x ZoneFallFactor), at most 1
 *  FurnitureLoss = 1 - value kept / value asked, an item being kept only if delivered, for its value times the HP curve
 *  Safety        = min(SafetyCap, crush events x CrushPenalty + explosion events x ExplosionPenalty)
 *  Forbidden     = ForbiddenToolPenalty if the tool the contract forbade was used at all, 0 otherwise (once for the mission)
 *  Penalty       = min(PenaltyCap, kCollateral x Collateral + kZone x ZoneDamage + kFurniture x FurnitureLoss + Safety + Forbidden)
 *  Payout        = BasePayout x Completion x (1 - Penalty), in whole money
 *
 *  Money is rounded term by term (Earned, then each penalty term, then the cap), so the lines of the report always add
 *  up to the payout to the unit.
 */
UCLASS()
class UContractEvaluator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** The report for a mission that ended as Snapshot says, under the contract's base payout and the scoring parameters. */
	UFUNCTION(BlueprintPure, Category = "Contracts")
	static FDebriefResult Evaluate(const FContractDefinition& Contract, const FMissionSnapshot& Snapshot, const FScoringParams& Params);

	/** Share of an item's value that is left at that HP ratio (see FScoringParams::FurnitureValuePoints). */
	UFUNCTION(BlueprintPure, Category = "Contracts")
	static float FurnitureValueMultiplier(const FScoringParams& Params, float HPRatio);

	/** Damage to the protected zones, 0 to 1: the zones' damages summed and capped at 1. */
	UFUNCTION(BlueprintPure, Category = "Contracts")
	static float ComputeZoneDamage(const FMissionSnapshot& Snapshot, const FScoringParams& Params);

	/** Estimated duration of a job in minutes (GDD §6.4): 0.06 x area x the type's factor (total 1.0, partial 0.6, renovation 0.5). */
	UFUNCTION(BlueprintPure, Category = "Contracts")
	static float EstimateMinutes(EContractType Type, float AreaM2);

	/** Money for a perfect job (GDD §11.2): 12 per m2 x the type's factor, rounded to the nearest 10. */
	UFUNCTION(BlueprintPure, Category = "Contracts")
	static int32 EstimateBasePayout(EContractType Type, float AreaM2);

	/** 1.0 for a total demolition, 0.6 for a partial one, 0.5 for a renovation: how much of the work (and of the pay) the type represents. */
	UFUNCTION(BlueprintPure, Category = "Contracts")
	static float TypeFactor(EContractType Type);
};
