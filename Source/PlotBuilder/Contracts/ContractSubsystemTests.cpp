#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Contracts/ContractPool.h"
#include "Contracts/ContractSubsystem.h"
#include "Engine/GameInstance.h"

/**
 *  How a finished mission is paid and handed to the hub, on a contract subsystem made by hand (no game instance, no level).
 *  The subsystem only says how much is due; the caller adds it to the money. Run from the editor:
 *  Automation RunTests PlotBuilder.Contracts.Subsystem
 */
namespace PBSubsystemTests
{
	/** A subsystem with nothing in it. It can only live under a game instance (its ClassWithin), so one is made to hold it. */
	UContractSubsystem* MakeBareSubsystem()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		return NewObject<UContractSubsystem>(GameInstance);
	}

	/** A subsystem with a board of one kind of offer (a 5 x 5 total demolition), ready to accept offer 0. */
	UContractSubsystem* MakeSubsystemWithBoard()
	{
		UContractPool* Pool = NewObject<UContractPool>(GetTransientPackage());

		FContractTemplate& Template = Pool->Templates.AddDefaulted_GetRef();
		Template.Id = FName(TEXT("Test"));
		Template.Weight = 1.f;
		Template.MinFootprintCells = FIntPoint(5, 5);
		Template.MaxFootprintCells = FIntPoint(5, 5);
		Template.MinFloors = 1;
		Template.MaxFloors = 1;

		UContractSubsystem* Contracts = MakeBareSubsystem();
		Contracts->SetPool(Pool);
		Contracts->RefreshOffers(1);
		return Contracts;
	}

	/** A report worth Payout. The subsystem reads nothing else from it. */
	FDebriefResult MakeReport(int32 Payout)
	{
		FDebriefResult Report;
		Report.Payout = Payout;
		return Report;
	}
}

// A contract the player took is paid once: the first completion says 274 is due, the second says nothing is, because the contract is closed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBSubsystemPaidOnceTest, "PlotBuilder.Contracts.Subsystem.PaidOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBSubsystemPaidOnceTest::RunTest(const FString& Parameters)
{
	UContractSubsystem* Contracts = PBSubsystemTests::MakeSubsystemWithBoard();

	if (!TestTrue(TEXT("offer taken"), Contracts->AcceptOffer(0)))
	{
		return false;
	}

	TestTrue(TEXT("contract is active"), Contracts->HasActiveContract());

	const FDebriefResult Report = PBSubsystemTests::MakeReport(274);

	TestEqual(TEXT("first completion: the payout is due"), Contracts->CompleteActiveContract(Report), 274);
	TestFalse(TEXT("contract is closed"), Contracts->HasActiveContract());
	TestEqual(TEXT("second completion: nothing is due"), Contracts->CompleteActiveContract(Report), 0);

	return true;
}

// No contract (a mission started from travel options): the report is kept for the debrief, but there is no client to pay.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBSubsystemNoContractTest, "PlotBuilder.Contracts.Subsystem.NoContractNoPay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBSubsystemNoContractTest::RunTest(const FString& Parameters)
{
	UContractSubsystem* Contracts = PBSubsystemTests::MakeSubsystemWithBoard();

	TestFalse(TEXT("no contract taken"), Contracts->HasActiveContract());
	TestEqual(TEXT("nothing is due"), Contracts->CompleteActiveContract(PBSubsystemTests::MakeReport(500)), 0);
	TestTrue(TEXT("the report is kept"), Contracts->HasLastResult());
	TestEqual(TEXT("the report is the one given"), Contracts->GetLastResult().Payout, 500);
	TestTrue(TEXT("and not shown yet"), Contracts->HasUnseenResult());

	return true;
}

// The hub shows the report once: the first read hands it out, the next ones say it was shown, a new mission makes a new one to show.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBSubsystemShownOnceTest, "PlotBuilder.Contracts.Subsystem.ReportShownOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBSubsystemShownOnceTest::RunTest(const FString& Parameters)
{
	UContractSubsystem* Contracts = PBSubsystemTests::MakeSubsystemWithBoard();

	FDebriefResult Shown;
	TestFalse(TEXT("nothing to show before a mission ends"), Contracts->ConsumeUnseenResult(Shown));

	Contracts->AcceptOffer(0);
	Contracts->CompleteActiveContract(PBSubsystemTests::MakeReport(274));

	TestTrue(TEXT("first read hands the report out"), Contracts->ConsumeUnseenResult(Shown));
	TestEqual(TEXT("it is the right report"), Shown.Payout, 274);
	TestFalse(TEXT("second read: already shown"), Contracts->ConsumeUnseenResult(Shown));
	TestTrue(TEXT("the report is still there for the console"), Contracts->HasLastResult());
	TestEqual(TEXT("still the same"), Contracts->GetLastResult().Payout, 274);

	Contracts->RefreshOffers(2);
	Contracts->AcceptOffer(0);
	Contracts->CompleteActiveContract(PBSubsystemTests::MakeReport(90));

	TestTrue(TEXT("a new mission, a new report to show"), Contracts->ConsumeUnseenResult(Shown));
	TestEqual(TEXT("the new one"), Shown.Payout, 90);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
