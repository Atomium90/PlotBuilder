#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/ContractEvaluator.h"
#include "Contracts/DebriefView.h"

/**
 *  The waterfall of the debrief, on the job of the mock-up worked out by hand (1 800 down to 685). The report comes from the real
 *  evaluator, so the steps are checked against the money it really gives. Run from the editor: Automation RunTests PlotBuilder.Contracts.Debrief
 */
namespace PBDebriefTests
{
	FContractDefinition MakeContract()
	{
		FContractDefinition Contract;
		Contract.BasePayout = 1800;

		FContractCondition& Salvage = Contract.Conditions.AddDefaulted_GetRef();
		Salvage.Type = EConditionType::SalvageFurniture;
		Salvage.Count = 3;

		FContractCondition& Zone = Contract.Conditions.AddDefaulted_GetRef();
		Zone.Type = EConditionType::ProtectedZone;
		Zone.Count = 1;

		FContractCondition& Tool = Contract.Conditions.AddDefaulted_GetRef();
		Tool.Type = EConditionType::ForbiddenTool;
		Tool.Param = FName(TEXT("Jackhammer"));

		return Contract;
	}

	FSalvageResult MakePiece(const TCHAR* Name, int32 Value, float HPRatio, bool bDelivered)
	{
		FSalvageResult Piece;
		Piece.Name = Name;
		Piece.Value = Value;
		Piece.HPRatio = HPRatio;
		Piece.bDelivered = bDelivered;
		Piece.bOnSite = !bDelivered;
		return Piece;
	}

	/** 92 % of the house down, 6 % of what had to stay hurt, the shed hit by a tool for 10 %, a sofa at 90 %, a TV intact, a lamp left inside, one crush, the jackhammer not used. */
	FMissionSnapshot MakeSnapshot()
	{
		FMissionSnapshot Snapshot;
		Snapshot.TargetWeight = 1000;
		Snapshot.TargetDestroyedWeight = 920;
		Snapshot.MustStayWeight = 400;
		Snapshot.MustStayDestroyedWeight = 24;
		Snapshot.Salvage.Add(MakePiece(TEXT("Sofa"), 400, 0.9f, true));
		Snapshot.Salvage.Add(MakePiece(TEXT("TV"), 250, 1.f, true));
		Snapshot.Salvage.Add(MakePiece(TEXT("Floor lamp"), 80, 1.f, false));

		FZoneResult& Zone = Snapshot.Zones.AddDefaulted_GetRef();
		Zone.Name = TEXT("Zone");
		Zone.Label = TEXT("Neighbour's shed");
		Zone.ToolDamageRatio = 0.1f;

		Snapshot.CrushEvents = 1;
		Snapshot.ForbiddenToolName = FName(TEXT("Jackhammer"));
		return Snapshot;
	}
}

// The report of the mock-up: 1 800, minus 144 left standing, 149 collateral, 248 for the shed, 408 for the furniture, 166 for the crush: 685.
// Every step starts where the one before ended, and the last total is the payout to the unit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBDebriefWaterfallTest, "PlotBuilder.Contracts.Debrief.Waterfall", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBDebriefWaterfallTest::RunTest(const FString& Parameters)
{
	const FContractDefinition Contract = PBDebriefTests::MakeContract();
	const FMissionSnapshot Snapshot = PBDebriefTests::MakeSnapshot();
	const FScoringParams Params;

	const FDebriefResult Result = UContractEvaluator::Evaluate(Contract, Snapshot, Params);
	const FNotebookData Notebook = MissionNotebook::Build(Contract, Snapshot, Params);
	const FDebriefView View = MissionDebrief::Build(Result, Notebook);

	TestEqual(TEXT("the payout of the mock-up"), Result.Payout, 685);

	if (!TestEqual(TEXT("eight steps"), View.Steps.Num(), 8))
	{
		return false;
	}

	const TArray<FString> Names = {TEXT("Base payout"), TEXT("Left standing"), TEXT("Collateral"), TEXT("Protected zone"), TEXT("Furniture"), TEXT("Safety"), TEXT("Forbidden tool"), TEXT("Paid to you")};
	const TArray<int32> Froms = {0, 1800, 1656, 1507, 1259, 851, 685, 0};
	const TArray<int32> Tos = {1800, 1656, 1507, 1259, 851, 685, 685, 685};

	for (int32 Index = 0; Index < 8; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("name of step %d"), Index), View.Steps[Index].Name, Names[Index]);
		TestEqual(*FString::Printf(TEXT("start of step %d"), Index), View.Steps[Index].From, Froms[Index]);
		TestEqual(*FString::Printf(TEXT("end of step %d"), Index), View.Steps[Index].To, Tos[Index]);
	}

	TestTrue(TEXT("base payout is a total"), View.Steps[0].Kind == EDebriefStepKind::Total);
	TestTrue(TEXT("what was left standing is not a fault"), View.Steps[1].Kind == EDebriefStepKind::Unfinished);
	TestTrue(TEXT("furniture is a loss"), View.Steps[4].Kind == EDebriefStepKind::Loss);
	TestTrue(TEXT("the unused forbidden tool costs nothing"), View.Steps[6].Kind == EDebriefStepKind::Nothing);
	TestTrue(TEXT("payout is a total"), View.Steps[7].Kind == EDebriefStepKind::Total);
	TestEqual(TEXT("the biggest loss is the furniture"), View.BiggestLossIndex, 4);
	TestEqual(TEXT("its amount"), View.Steps[4].Amount(), 408);

	TestEqual(TEXT("left standing, in words"), View.Steps[1].Detail, FString(TEXT("92 % of the house came down")));
	TestEqual(TEXT("the shed, in words"), View.Steps[3].Detail, FString(TEXT("Neighbour's shed, damaged, 10 %")));
	TestEqual(TEXT("the crush, in words"), View.Steps[5].Detail, FString(TEXT("The collapse caught you once")));
	TestEqual(TEXT("the tool, in words"), View.Steps[6].Detail, FString(TEXT("Jackhammer not used")));

	// Each step starts where the last one ended.
	for (int32 Index = 1; Index < 7; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("step %d continues step %d"), Index, Index - 1), View.Steps[Index].From, View.Steps[Index - 1].To);
	}

	return true;
}

// A perfect job loses nothing: only the two totals, no biggest loss. A contract with no zone, no furniture and no forbidden tool has no step for them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBDebriefPerfectTest, "PlotBuilder.Contracts.Debrief.PerfectJob", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBDebriefPerfectTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract;
	Contract.BasePayout = 768;

	FMissionSnapshot Snapshot;
	Snapshot.TargetWeight = 500;
	Snapshot.TargetDestroyedWeight = 500;

	const FScoringParams Params;
	const FDebriefResult Result = UContractEvaluator::Evaluate(Contract, Snapshot, Params);
	const FDebriefView View = MissionDebrief::Build(Result, MissionNotebook::Build(Contract, Snapshot, Params));

	TestEqual(TEXT("two steps"), View.Steps.Num(), 2);
	TestEqual(TEXT("no biggest loss"), View.BiggestLossIndex, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("paid in full"), View.Steps.Last().To, 768);

	return true;
}

// The cap on the penalty: everything that had to stay is gone, 1.5 x 1000 would take more than the 800 the cap allows. The collateral is
// shown for the 800 really taken, the waterfall goes from 1 000 to 200 and never under zero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBDebriefCapTest, "PlotBuilder.Contracts.Debrief.PenaltyCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBDebriefCapTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract;
	Contract.BasePayout = 1000;

	FMissionSnapshot Snapshot;
	Snapshot.TargetWeight = 100;
	Snapshot.TargetDestroyedWeight = 100;
	Snapshot.MustStayWeight = 100;
	Snapshot.MustStayDestroyedWeight = 100;

	const FScoringParams Params;
	const FDebriefResult Result = UContractEvaluator::Evaluate(Contract, Snapshot, Params);
	const FDebriefView View = MissionDebrief::Build(Result, MissionNotebook::Build(Contract, Snapshot, Params));

	TestTrue(TEXT("the cap was reached"), Result.bPenaltyCapped);
	TestEqual(TEXT("the payout is 200"), Result.Payout, 200);

	if (!TestEqual(TEXT("three steps"), View.Steps.Num(), 3))
	{
		return false;
	}

	TestEqual(TEXT("collateral starts at the base"), View.Steps[1].From, 1000);
	TestEqual(TEXT("collateral ends on the payout"), View.Steps[1].To, 200);
	TestTrue(TEXT("and says it was reduced"), View.Steps[1].Detail.EndsWith(TEXT("(reduced by the cap)")));
	TestEqual(TEXT("the biggest loss"), View.BiggestLossIndex, 1);
	TestEqual(TEXT("the last total is the payout"), View.Steps.Last().To, 200);

	return true;
}

// Two terms and a cap: the 800 really taken is shared between them in proportion, and the units left by the rounding go to the biggest, so the steps add up exactly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBDebriefCapSharedTest, "PlotBuilder.Contracts.Debrief.PenaltyCapShared", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBDebriefCapSharedTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract;
	Contract.BasePayout = 1000;

	FMissionSnapshot Snapshot;
	Snapshot.TargetWeight = 100;
	Snapshot.TargetDestroyedWeight = 100;
	Snapshot.MustStayWeight = 100;
	Snapshot.MustStayDestroyedWeight = 100;
	Snapshot.CrushEvents = 2;

	const FScoringParams Params;
	const FDebriefResult Result = UContractEvaluator::Evaluate(Contract, Snapshot, Params);
	const FDebriefView View = MissionDebrief::Build(Result, MissionNotebook::Build(Contract, Snapshot, Params));

	TestTrue(TEXT("the cap was reached"), Result.bPenaltyCapped);

	int32 Lost = 0;

	for (const FDebriefStep& Step : View.Steps)
	{
		Lost += (Step.Kind == EDebriefStepKind::Loss || Step.Kind == EDebriefStepKind::Unfinished) ? Step.Amount() : 0;
	}

	TestEqual(TEXT("the losses add up to what was earned minus what is paid"), Lost, Result.Earned - Result.Payout);
	TestEqual(TEXT("the running total ends on the payout"), View.Steps[View.Steps.Num() - 2].To, Result.Payout);

	return true;
}

// The jackhammer drilled 38 s (50 per job + 2 per second = 126) on a 1 000 job with one crush (100 off): the waterfall goes 1 000, minus the crush,
// minus the upkeep taken from the 900 that is left, down to 774. The upkeep step is neutral, does not count as the biggest loss, and the steps add up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBDebriefUpkeepTest, "PlotBuilder.Contracts.Debrief.Upkeep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBDebriefUpkeepTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract;
	Contract.BasePayout = 1000;

	FMissionSnapshot Snapshot;
	Snapshot.TargetWeight = 500;
	Snapshot.TargetDestroyedWeight = 500;
	Snapshot.CrushEvents = 1;

	FToolUsage& Usage = Snapshot.ToolUsage.AddDefaulted_GetRef();
	Usage.ToolId = FName(TEXT("Jackhammer"));
	Usage.ActiveSeconds = 38.f;
	Usage.Upkeep.PerJob = 50;
	Usage.Upkeep.PerSecond = 2.f;

	const FScoringParams Params;
	const FDebriefResult Result = UContractEvaluator::Evaluate(Contract, Snapshot, Params);
	const FDebriefView View = MissionDebrief::Build(Result, MissionNotebook::Build(Contract, Snapshot, Params));

	TestEqual(TEXT("payout"), Result.Payout, 774);

	if (!TestEqual(TEXT("base, safety, upkeep, paid"), View.Steps.Num(), 4))
	{
		return false;
	}

	TestEqual(TEXT("safety is shown for the crush alone"), View.Steps[1].Amount(), 100);
	TestEqual(TEXT("the upkeep step"), View.Steps[2].Name, FString(TEXT("Upkeep")));
	TestEqual(TEXT("it starts where the crush ended"), View.Steps[2].From, 900);
	TestEqual(TEXT("it ends on the payout"), View.Steps[2].To, 774);
	TestEqual(TEXT("its amount"), View.Steps[2].Amount(), 126);
	TestTrue(TEXT("it is an upkeep, not a loss"), View.Steps[2].Kind == EDebriefStepKind::Upkeep);
	TestTrue(TEXT("it says which tool and for how long"), View.Steps[2].Detail.Contains(TEXT("Jackhammer, 38 s")));
	TestEqual(TEXT("the biggest loss stays the crush"), View.BiggestLossIndex, 1);
	TestEqual(TEXT("the last total is the payout"), View.Steps.Last().To, 774);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
