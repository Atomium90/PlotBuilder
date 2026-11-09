#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/ContractEvaluator.h"

/**
 *  The contract evaluator on numbers worked out by hand (T7): each case below states its arithmetic, so the formula of
 *  GDD §6.5 can be checked line by line. Run them from the editor: Automation RunTests PlotBuilder.Contracts
 *  (or Tools > Session Frontend > Automation, filter "PlotBuilder").
 */
namespace PBContractTests
{
	FContractDefinition MakeContract(int32 BasePayout)
	{
		FContractDefinition Contract;
		Contract.BasePayout = BasePayout;
		return Contract;
	}

	/** A job where the whole target set (weight 1000) is destroyed and a must-stay set of 400 is untouched. */
	FMissionSnapshot MakeCleanSnapshot()
	{
		FMissionSnapshot Snapshot;
		Snapshot.TargetWeight = 1000;
		Snapshot.TargetDestroyedWeight = 1000;
		Snapshot.MustStayWeight = 400;
		Snapshot.MustStayDestroyedWeight = 0;
		return Snapshot;
	}

	FSalvageResult MakeSalvage(int32 Value, float HPRatio, bool bDelivered)
	{
		FSalvageResult Item;
		Item.Name = TEXT("Item");
		Item.Value = Value;
		Item.HPRatio = HPRatio;
		Item.bDelivered = bDelivered;
		return Item;
	}

	/** The money of every line but the last (the payout itself): it must add up to the payout. */
	int32 SumOfLinesBeforePayout(const FDebriefResult& Result)
	{
		int32 Sum = 0;

		for (int32 Index = 0; Index < Result.Lines.Num() - 1; ++Index)
		{
			Sum += Result.Lines[Index].Money;
		}

		return Sum;
	}
}

// Everything destroyed, nothing broken, every item delivered whole: the full base payout, no penalty.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorPerfectJobTest, "PlotBuilder.Contracts.Evaluator.PerfectJob", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorPerfectJobTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();
	Snapshot.Salvage.Add(PBContractTests::MakeSalvage(300, 1.f, true));
	Snapshot.Salvage.Add(PBContractTests::MakeSalvage(200, 1.f, true));

	const FDebriefResult Result = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("completion"), Result.Completion, 1.f, 0.001f);
	TestNearlyEqual(TEXT("collateral"), Result.Collateral, 0.f, 0.001f);
	TestNearlyEqual(TEXT("furniture loss"), Result.FurnitureLoss, 0.f, 0.001f);
	TestNearlyEqual(TEXT("penalty"), Result.Penalty, 0.f, 0.001f);
	TestEqual(TEXT("earned"), Result.Earned, 1000);
	TestEqual(TEXT("payout"), Result.Payout, 1000);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Result), Result.Payout);

	return true;
}

// 800 of 1000 destroyed: completion 0.8, earned 2000 x 0.8 = 1600. 100 of the 400 that had to stay is gone:
// collateral 0.25, penalty 1.5 x 0.25 = 0.375, so 1600 x 0.375 = 600 off. Payout 1000.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorCollateralTest, "PlotBuilder.Contracts.Evaluator.Collateral", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorCollateralTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();
	Snapshot.TargetDestroyedWeight = 800;
	Snapshot.MustStayDestroyedWeight = 100;

	const FDebriefResult Result = UContractEvaluator::Evaluate(PBContractTests::MakeContract(2000), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("completion"), Result.Completion, 0.8f, 0.001f);
	TestNearlyEqual(TEXT("collateral"), Result.Collateral, 0.25f, 0.001f);
	TestNearlyEqual(TEXT("penalty"), Result.Penalty, 0.375f, 0.001f);
	TestTrue(TEXT("not capped"), !Result.bPenaltyCapped);
	TestEqual(TEXT("earned"), Result.Earned, 1600);
	TestEqual(TEXT("payout"), Result.Payout, 1000);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Result), Result.Payout);

	return true;
}

// Asked: a 300 item delivered whole and a 100 item not delivered. Kept 300 of 400, loss 0.25, kFurniture 1.0: 250 off a
// 1000 base, payout 750. The default is the GDD curve (0 HP 0.05, 0.8 HP 0.5, full 1.0): with the 300 item at 0.8 HP, kept
// 150 of 400, loss 0.625, payout 375. The curve itself: 0.9 HP is 0.75, 0.4 HP is 0.275, 0 HP is 0.05. With no points at
// all the value goes down in a straight line with the HP.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorFurnitureTest, "PlotBuilder.Contracts.Evaluator.Furniture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorFurnitureTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot;
	Snapshot.TargetWeight = 500;
	Snapshot.TargetDestroyedWeight = 500;
	Snapshot.Salvage.Add(PBContractTests::MakeSalvage(300, 1.f, true));
	Snapshot.Salvage.Add(PBContractTests::MakeSalvage(100, 1.f, false));

	const FScoringParams Defaults = FScoringParams();
	const FDebriefResult Whole = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, Defaults);

	TestNearlyEqual(TEXT("furniture loss"), Whole.FurnitureLoss, 0.25f, 0.001f);
	TestEqual(TEXT("payout"), Whole.Payout, 750);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Whole), Whole.Payout);

	Snapshot.Salvage[0].HPRatio = 0.8f;

	const FDebriefResult Damaged = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, Defaults);

	TestNearlyEqual(TEXT("furniture loss with the default curve"), Damaged.FurnitureLoss, 0.625f, 0.001f);
	TestEqual(TEXT("payout with the default curve"), Damaged.Payout, 375);

	TestNearlyEqual(TEXT("curve at 0.9"), UContractEvaluator::FurnitureValueMultiplier(Defaults, 0.9f), 0.75f, 0.001f);
	TestNearlyEqual(TEXT("curve at 0.4"), UContractEvaluator::FurnitureValueMultiplier(Defaults, 0.4f), 0.275f, 0.001f);
	TestNearlyEqual(TEXT("curve at 0"), UContractEvaluator::FurnitureValueMultiplier(Defaults, 0.f), 0.05f, 0.001f);

	FScoringParams Straight;
	Straight.FurnitureValuePoints.Reset();

	TestNearlyEqual(TEXT("no points, straight line"), UContractEvaluator::FurnitureValueMultiplier(Straight, 0.3f), 0.3f, 0.001f);

	return true;
}

// Three items asked, a 100 one delivered, a 100 one destroyed, a 100 one left standing on the plot: kept 100 of 300, and the
// report says so in words (1 of 3 delivered, 1 destroyed, 1 left on the plot). The words change nothing to the money.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorFurnitureFateTest, "PlotBuilder.Contracts.Evaluator.FurnitureFate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorFurnitureFateTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();

	Snapshot.Salvage.Add(PBContractTests::MakeSalvage(100, 1.f, true));

	FSalvageResult Destroyed = PBContractTests::MakeSalvage(100, 0.f, false);
	Destroyed.bDestroyed = true;
	Snapshot.Salvage.Add(Destroyed);

	FSalvageResult Standing = PBContractTests::MakeSalvage(100, 1.f, false);
	Standing.bOnSite = true;
	Snapshot.Salvage.Add(Standing);

	const FDebriefResult Result = UContractEvaluator::Evaluate(PBContractTests::MakeContract(900), Snapshot, FScoringParams());

	// Kept 100 of 300, loss 2/3, so 600 off a 900 base.
	TestNearlyEqual(TEXT("furniture loss"), Result.FurnitureLoss, 2.f / 3.f, 0.001f);
	TestEqual(TEXT("payout"), Result.Payout, 300);

	const FString& Detail = Result.Lines[3].Detail;
	TestTrue(TEXT("says 1 of 3 delivered"), Detail.Contains(TEXT("1 of 3 delivered")));
	TestTrue(TEXT("says 1 destroyed"), Detail.Contains(TEXT("1 destroyed")));
	TestTrue(TEXT("says 1 left on the plot"), Detail.Contains(TEXT("1 left on the plot")));

	return true;
}

// One crush: 0.10 off a 1000 base, payout 900. Three crushes and two explosions would be 0.5, held at the safety cap 0.25: payout 750.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorSafetyTest, "PlotBuilder.Contracts.Evaluator.Safety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorSafetyTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();
	Snapshot.CrushEvents = 1;

	const FDebriefResult One = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("safety"), One.Safety, 0.1f, 0.001f);
	TestEqual(TEXT("payout"), One.Payout, 900);

	Snapshot.CrushEvents = 3;
	Snapshot.ExplosionEvents = 2;

	const FDebriefResult Many = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("safety is capped"), Many.Safety, 0.25f, 0.001f);
	TestEqual(TEXT("payout"), Many.Payout, 750);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Many), Many.Payout);

	return true;
}

// All the 400 that had to stay is gone: collateral 1.0, 1.5 off a 1000 base on paper (1500), held at the penalty cap 0.8:
// 800 off, payout 200. The report gives the 700 back on its own line so the lines still add up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorPenaltyCapTest, "PlotBuilder.Contracts.Evaluator.PenaltyCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorPenaltyCapTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();
	Snapshot.MustStayDestroyedWeight = 400;

	const FDebriefResult Result = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, FScoringParams());

	TestTrue(TEXT("capped"), Result.bPenaltyCapped);
	TestNearlyEqual(TEXT("penalty"), Result.Penalty, 0.8f, 0.001f);
	TestEqual(TEXT("earned"), Result.Earned, 1000);
	TestEqual(TEXT("payout"), Result.Payout, 200);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Result), Result.Payout);

	return true;
}

// Nothing to destroy, nothing that must stay, nothing asked: completion 1, no penalty, the base payout in full.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorEmptyTest, "PlotBuilder.Contracts.Evaluator.ZeroDenominators", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorEmptyTest::RunTest(const FString& Parameters)
{
	const FDebriefResult Result = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1500), FMissionSnapshot(), FScoringParams());

	TestNearlyEqual(TEXT("completion"), Result.Completion, 1.f, 0.001f);
	TestNearlyEqual(TEXT("collateral"), Result.Collateral, 0.f, 0.001f);
	TestNearlyEqual(TEXT("furniture loss"), Result.FurnitureLoss, 0.f, 0.001f);
	TestEqual(TEXT("payout"), Result.Payout, 1500);

	return true;
}

// House weight 1400. Zone A: tools took 0.10, 70 of weight fell in, 70 / 1400 x 3 = 0.15, so 0.25. Zone B: untouched.
// Zone damage 0.25, kZone 1.5: 0.375 of a 2000 base, 750 off, payout 1250. Two bad zones add to more than 1 and are held at 1.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorZonesTest, "PlotBuilder.Contracts.Evaluator.ProtectedZones", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorZonesTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();

	FZoneResult ZoneA;
	ZoneA.ToolDamageRatio = 0.1f;
	ZoneA.FallenWeight = 70;
	FZoneResult ZoneB;
	Snapshot.Zones = {ZoneA, ZoneB};

	const FDebriefResult Result = UContractEvaluator::Evaluate(PBContractTests::MakeContract(2000), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("zone damage"), Result.ZoneDamage, 0.25f, 0.001f);
	TestEqual(TEXT("payout"), Result.Payout, 1250);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Result), Result.Payout);

	// Zone C: 0.9 from the tools and 140 / 1400 x 3 = 0.3 fallen, 1.2 held at 1 for the zone. Zone D: 0.5. Together 1.5, held at 1.
	FZoneResult ZoneC;
	ZoneC.ToolDamageRatio = 0.9f;
	ZoneC.FallenWeight = 140;
	FZoneResult ZoneD;
	ZoneD.ToolDamageRatio = 0.5f;
	Snapshot.Zones = {ZoneC, ZoneD};

	const FDebriefResult Ruined = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("zone damage is held at 1"), Ruined.ZoneDamage, 1.f, 0.001f);
	TestTrue(TEXT("capped"), Ruined.bPenaltyCapped);
	TestEqual(TEXT("payout"), Ruined.Payout, 200);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Ruined), Ruined.Payout);

	return true;
}

// A forbidden tool used even once costs ForbiddenToolPenalty (0.15) of what was earned, once whatever the number of uses: 150 off a
// 1000 base, payout 850. Not used: nothing, but the line is in the report because the contract forbade a tool.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorForbiddenToolTest, "PlotBuilder.Contracts.Evaluator.ForbiddenTool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorForbiddenToolTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract = PBContractTests::MakeContract(1000);
	FContractCondition& Condition = Contract.Conditions.AddDefaulted_GetRef();
	Condition.Type = EConditionType::ForbiddenTool;
	Condition.Param = FName(TEXT("Jackhammer"));

	FMissionSnapshot Snapshot = PBContractTests::MakeCleanSnapshot();
	Snapshot.ForbiddenToolName = FName(TEXT("Jackhammer"));
	Snapshot.bForbiddenToolUsed = false;

	const FDebriefResult Unused = UContractEvaluator::Evaluate(Contract, Snapshot, FScoringParams());

	TestEqual(TEXT("not used, full pay"), Unused.Payout, 1000);
	TestNearlyEqual(TEXT("not used, no penalty"), Unused.ForbiddenTool, 0.f, 0.001f);
	TestEqual(TEXT("not used, the line is there"), Unused.Lines.Num(), 7);

	Snapshot.bForbiddenToolUsed = true;

	const FDebriefResult Used = UContractEvaluator::Evaluate(Contract, Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("used, the share"), Used.ForbiddenTool, 0.15f, 0.001f);
	TestNearlyEqual(TEXT("used, the penalty"), Used.Penalty, 0.15f, 0.001f);
	TestEqual(TEXT("used, payout"), Used.Payout, 850);
	TestEqual(TEXT("lines add up"), PBContractTests::SumOfLinesBeforePayout(Used), Used.Payout);

	// A contract that forbade nothing has no such line.
	const FDebriefResult Plain = UContractEvaluator::Evaluate(PBContractTests::MakeContract(1000), PBContractTests::MakeCleanSnapshot(), FScoringParams());
	TestEqual(TEXT("no condition, no line"), Plain.Lines.Num(), 6);

	return true;
}

// 100 m2: a total demolition shows 6 minutes and pays 1200 (0.06 min and 12 money per m2), a partial one 3.6 and 720 (x0.6), a renovation 3 and 600 (x0.5).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBEvaluatorEstimatesTest, "PlotBuilder.Contracts.Evaluator.Estimates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBEvaluatorEstimatesTest::RunTest(const FString& Parameters)
{
	TestNearlyEqual(TEXT("total minutes"), UContractEvaluator::EstimateMinutes(EContractType::TotalDemolition, 100.f), 6.f, 0.001f);
	TestEqual(TEXT("total payout"), UContractEvaluator::EstimateBasePayout(EContractType::TotalDemolition, 100.f), 1200);
	TestNearlyEqual(TEXT("partial minutes"), UContractEvaluator::EstimateMinutes(EContractType::PartialDemolition, 100.f), 3.6f, 0.001f);
	TestEqual(TEXT("partial payout"), UContractEvaluator::EstimateBasePayout(EContractType::PartialDemolition, 100.f), 720);
	TestNearlyEqual(TEXT("renovation minutes"), UContractEvaluator::EstimateMinutes(EContractType::Renovation, 100.f), 3.f, 0.001f);
	TestEqual(TEXT("renovation payout"), UContractEvaluator::EstimateBasePayout(EContractType::Renovation, 100.f), 600);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
