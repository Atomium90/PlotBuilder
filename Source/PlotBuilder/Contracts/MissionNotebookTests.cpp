#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/MissionNotebook.h"

/**
 *  The notebook data on small hand-made snapshots (the notebook, the progress ring and the leave-the-site card all read it,
 *  so what they say is checked here, without a level). Run them from the editor: Automation RunTests PlotBuilder.Contracts.Notebook
 */
namespace PBNotebookTests
{
	FMissionSnapshot MakeFinishedHouse()
	{
		FMissionSnapshot Snapshot;
		Snapshot.TargetWeight = 1000;
		Snapshot.TargetDestroyedWeight = 1000;
		Snapshot.MustStayWeight = 400;
		return Snapshot;
	}

	FSalvageResult MakeItem(const TCHAR* Name, bool bDelivered, bool bDestroyed, bool bCarried, bool bOnSite, float HPRatio = 1.f)
	{
		FSalvageResult Item;
		Item.Name = Name;
		Item.Value = 100;
		Item.bDelivered = bDelivered;
		Item.bDestroyed = bDestroyed;
		Item.bCarried = bCarried;
		Item.bOnSite = bOnSite;
		Item.HPRatio = HPRatio;
		return Item;
	}

	FZoneResult MakeZone(const TCHAR* Label, float ToolDamage)
	{
		FZoneResult Zone;
		Zone.Name = TEXT("Zone");
		Zone.Label = Label;
		Zone.ToolDamageRatio = ToolDamage;
		return Zone;
	}
}

// Five pieces in five states. The list puts the ones that need the player first: in hands, then the two still in the house
// (one on the plot, one outside it), then delivered, then destroyed. 1 of 5 delivered; 3 could still be delivered.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBNotebookFurnitureTest, "PlotBuilder.Contracts.Notebook.Furniture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBNotebookFurnitureTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBNotebookTests::MakeFinishedHouse();
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("Sofa"), true, false, false, false, 0.9f));
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("TV"), false, false, true, true));
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("Lamp"), false, false, false, true));
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("Fridge"), false, true, false, false));
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("Desk"), false, false, false, false));

	const FNotebookData Data = MissionNotebook::Build(FContractDefinition(), Snapshot, FScoringParams());
	const FNotebookSection* Section = Data.FindSection(ENotebookSection::Furniture);

	if (!TestNotNull(TEXT("furniture section"), Section) || !TestEqual(TEXT("five lines"), Section->Lines.Num(), 5))
	{
		return false;
	}

	TestEqual(TEXT("first: in hands"), Section->Lines[0].Name, FString(TEXT("TV")));
	TestEqual(TEXT("first state"), Section->Lines[0].State, FString(TEXT("In your hands")));
	TestTrue(TEXT("first status"), Section->Lines[0].Status == ENotebookState::Carried);
	TestEqual(TEXT("second: to do, on the plot"), Section->Lines[1].State, FString(TEXT("Still on the plot")));
	TestEqual(TEXT("third: to do, outside"), Section->Lines[2].State, FString(TEXT("Left outside the plot")));
	TestEqual(TEXT("fourth: delivered"), Section->Lines[3].State, FString(TEXT("Delivered, 90 %")));
	TestTrue(TEXT("last: lost"), Section->Lines[4].Status == ENotebookState::Lost);
	TestEqual(TEXT("count"), Section->Count, FString(TEXT("1 / 5")));
	TestEqual(TEXT("summary"), Section->Summary, FString(TEXT("1 of 5 delivered, 3 still to deliver, 1 destroyed")));
	TestTrue(TEXT("section status"), Section->Status == ENotebookState::Warning);
	TestEqual(TEXT("pending"), Data.PendingFurniture, 3);
	TestFalse(TEXT("job not complete"), Data.bJobComplete);

	return true;
}

// A piece that is destroyed cannot be recovered: asking about it on the way out would be noise. All the house is down, one piece
// delivered and one destroyed: the job is complete, but the section still reads as a warning (a loss).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBNotebookLostPieceTest, "PlotBuilder.Contracts.Notebook.LostPieceDoesNotBlock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBNotebookLostPieceTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBNotebookTests::MakeFinishedHouse();
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("Sofa"), true, false, false, false));
	Snapshot.Salvage.Add(PBNotebookTests::MakeItem(TEXT("TV"), false, true, false, false));

	const FNotebookData Data = MissionNotebook::Build(FContractDefinition(), Snapshot, FScoringParams());
	const FNotebookSection* Section = Data.FindSection(ENotebookSection::Furniture);

	TestTrue(TEXT("job complete"), Data.bJobComplete);
	TestEqual(TEXT("nothing pending"), Data.PendingFurniture, 0);

	if (TestNotNull(TEXT("furniture section"), Section))
	{
		TestTrue(TEXT("a loss is still a warning"), Section->Status == ENotebookState::Warning);
		TestEqual(TEXT("summary"), Section->Summary, FString(TEXT("1 of 2 delivered, 1 destroyed")));
	}

	return true;
}

// 800 of 1000 down: the ring says 80 %, the job is not complete, and a contract with no condition has no section.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBNotebookCompletionTest, "PlotBuilder.Contracts.Notebook.Completion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBNotebookCompletionTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBNotebookTests::MakeFinishedHouse();
	Snapshot.TargetDestroyedWeight = 800;

	const FNotebookData Data = MissionNotebook::Build(FContractDefinition(), Snapshot, FScoringParams());

	TestNearlyEqual(TEXT("completion"), Data.Completion, 0.8f, 0.001f);
	TestFalse(TEXT("job not complete"), Data.bJobComplete);
	TestEqual(TEXT("no section"), Data.Sections.Num(), 0);

	// Nothing to destroy at all (an empty target set) counts as done, like the report.
	const FNotebookData Empty = MissionNotebook::Build(FContractDefinition(), FMissionSnapshot(), FScoringParams());
	TestNearlyEqual(TEXT("empty completion"), Empty.Completion, 1.f, 0.001f);
	TestTrue(TEXT("empty job complete"), Empty.bJobComplete);

	return true;
}

// Two zones, one untouched, one that lost 10 % to a tool: the second shows its damage and a bar, the section says 1 of 2 damaged.
// A single zone is summed up by its own name.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBNotebookZonesTest, "PlotBuilder.Contracts.Notebook.Zones", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBNotebookZonesTest::RunTest(const FString& Parameters)
{
	FMissionSnapshot Snapshot = PBNotebookTests::MakeFinishedHouse();
	Snapshot.Zones.Add(PBNotebookTests::MakeZone(TEXT("Garage"), 0.f));
	Snapshot.Zones.Add(PBNotebookTests::MakeZone(TEXT("Neighbour's shed"), 0.1f));

	const FNotebookData Data = MissionNotebook::Build(FContractDefinition(), Snapshot, FScoringParams());
	const FNotebookSection* Section = Data.FindSection(ENotebookSection::Zones);

	if (!TestNotNull(TEXT("zones section"), Section) || !TestEqual(TEXT("two lines"), Section->Lines.Num(), 2))
	{
		return false;
	}

	TestEqual(TEXT("title"), Section->Title, FString(TEXT("Protected zones")));
	TestEqual(TEXT("count"), Section->Count, FString(TEXT("2 active")));
	TestEqual(TEXT("untouched"), Section->Lines[0].State, FString(TEXT("Untouched")));
	TestNearlyEqual(TEXT("no bar when untouched"), Section->Lines[0].Damage, -1.f, 0.001f);
	TestEqual(TEXT("damaged"), Section->Lines[1].State, FString(TEXT("Damaged, 10 %")));
	TestNearlyEqual(TEXT("bar"), Section->Lines[1].Damage, 0.1f, 0.001f);
	TestEqual(TEXT("summary"), Section->Summary, FString(TEXT("1 of 2 damaged")));
	TestTrue(TEXT("status"), Section->Status == ENotebookState::Warning);

	Snapshot.Zones.RemoveAt(0);
	const FNotebookData Single = MissionNotebook::Build(FContractDefinition(), Snapshot, FScoringParams());
	const FNotebookSection* SingleSection = Single.FindSection(ENotebookSection::Zones);

	if (TestNotNull(TEXT("single zone section"), SingleSection))
	{
		TestEqual(TEXT("title"), SingleSection->Title, FString(TEXT("Protected zone")));
		TestEqual(TEXT("name"), SingleSection->SummaryName, FString(TEXT("Neighbour's shed")));
		TestEqual(TEXT("summary"), SingleSection->Summary, FString(TEXT("Damaged, 10 %")));
	}

	// A zone with no label still reads as something.
	FMissionSnapshot NoLabel = PBNotebookTests::MakeFinishedHouse();
	NoLabel.Zones.Add(PBNotebookTests::MakeZone(TEXT(""), 0.f));
	const FNotebookData Unnamed = MissionNotebook::Build(FContractDefinition(), NoLabel, FScoringParams());

	if (const FNotebookSection* UnnamedSection = Unnamed.FindSection(ENotebookSection::Zones))
	{
		TestEqual(TEXT("fallback name"), UnnamedSection->Lines[0].Name, FString(TEXT("Protected zone")));
	}

	return true;
}

// The room to keep reads the must-stay set (100 of 400 down = 25 %), the forbidden tool reads the flag of the snapshot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBNotebookConditionsTest, "PlotBuilder.Contracts.Notebook.KeepAndForbidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBNotebookConditionsTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract;

	FContractCondition& Keep = Contract.Conditions.AddDefaulted_GetRef();
	Keep.Type = EConditionType::KeepStanding;
	Keep.Param = FName(TEXT("LivingRoom"));

	FContractCondition& Forbidden = Contract.Conditions.AddDefaulted_GetRef();
	Forbidden.Type = EConditionType::ForbiddenTool;
	Forbidden.Param = FName(TEXT("Jackhammer"));

	FMissionSnapshot Snapshot = PBNotebookTests::MakeFinishedHouse();

	const FNotebookData Clean = MissionNotebook::Build(Contract, Snapshot, FScoringParams());
	const FNotebookSection* CleanKeep = Clean.FindSection(ENotebookSection::KeepStanding);
	const FNotebookSection* CleanTool = Clean.FindSection(ENotebookSection::ForbiddenTool);

	if (!TestNotNull(TEXT("keep section"), CleanKeep) || !TestNotNull(TEXT("tool section"), CleanTool))
	{
		return false;
	}

	TestEqual(TEXT("room name"), CleanKeep->Lines[0].Name, FString(TEXT("Living room")));
	TestEqual(TEXT("still standing"), CleanKeep->Lines[0].State, FString(TEXT("Still standing")));
	TestTrue(TEXT("room ok"), CleanKeep->Status == ENotebookState::Ok);
	TestEqual(TEXT("tool name"), CleanTool->Lines[0].Name, FString(TEXT("Jackhammer")));
	TestEqual(TEXT("not used"), CleanTool->Lines[0].State, FString(TEXT("Not used")));

	Snapshot.MustStayDestroyedWeight = 100;
	Snapshot.ForbiddenToolName = FName(TEXT("Jackhammer"));
	Snapshot.bForbiddenToolUsed = true;

	const FNotebookData Hurt = MissionNotebook::Build(Contract, Snapshot, FScoringParams());
	const FNotebookSection* HurtKeep = Hurt.FindSection(ENotebookSection::KeepStanding);
	const FNotebookSection* HurtTool = Hurt.FindSection(ENotebookSection::ForbiddenTool);

	if (TestNotNull(TEXT("keep section"), HurtKeep) && TestNotNull(TEXT("tool section"), HurtTool))
	{
		TestEqual(TEXT("room hurt"), HurtKeep->Lines[0].State, FString(TEXT("25 % of what had to stay is down")));
		TestNearlyEqual(TEXT("room bar"), HurtKeep->Lines[0].Damage, 0.25f, 0.001f);
		TestTrue(TEXT("room warning"), HurtKeep->Status == ENotebookState::Warning);
		TestTrue(TEXT("tool warning"), HurtTool->Status == ENotebookState::Warning);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBNotebookRoomNameTest, "PlotBuilder.Contracts.Notebook.RoomName", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBNotebookRoomNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("one word"), MissionNotebook::RoomName(FName(TEXT("Bedroom"))), FString(TEXT("Bedroom")));
	TestEqual(TEXT("two words"), MissionNotebook::RoomName(FName(TEXT("LivingRoom"))), FString(TEXT("Living room")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
