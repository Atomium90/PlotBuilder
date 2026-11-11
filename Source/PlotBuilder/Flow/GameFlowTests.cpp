#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Flow/GameFlow.h"

/**
 *  The state of the loop worked out from the facts, with no level and no widget. Run them from the editor:
 *  Automation RunTests PlotBuilder.Flow
 */
namespace PBGameFlowTests
{
	FGameFlowFacts MakeFacts(bool bInMission, bool bInHome, bool bDebriefOpen, bool bHasActiveContract)
	{
		FGameFlowFacts Facts;
		Facts.bInMission = bInMission;
		Facts.bInHome = bInHome;
		Facts.bDebriefOpen = bDebriefOpen;
		Facts.bHasActiveContract = bHasActiveContract;
		return Facts;
	}
}

// The loop of a mission: nothing picked is the hub, a contract taken is ContractSelected, the mission level is OnSite (the contract is still active
// there), the report open on arrival is Debrief (the contract is closed by then), and closing it is the hub again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBGameFlowLoopTest, "PlotBuilder.Flow.Loop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBGameFlowLoopTest::RunTest(const FString& Parameters)
{
	using namespace PBGameFlowTests;

	TestTrue(TEXT("nothing picked: the hub"), GameFlow::Resolve(MakeFacts(false, false, false, false)) == EGameFlowState::InHub);
	TestTrue(TEXT("contract taken: selected"), GameFlow::Resolve(MakeFacts(false, false, false, true)) == EGameFlowState::ContractSelected);
	TestTrue(TEXT("in the mission level: on site"), GameFlow::Resolve(MakeFacts(true, false, false, true)) == EGameFlowState::OnSite);
	TestTrue(TEXT("back at the hub with the report open: debrief"), GameFlow::Resolve(MakeFacts(false, false, true, false)) == EGameFlowState::Debrief);
	TestTrue(TEXT("report closed: the hub"), GameFlow::Resolve(MakeFacts(false, false, false, false)) == EGameFlowState::InHub);

	return true;
}

// What wins when several facts are true: the mission level beats everything (a mission started from travel options has no contract, and a
// debrief cannot be open on site), an open debrief beats the home and a taken contract, the home beats a taken contract.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBGameFlowPriorityTest, "PlotBuilder.Flow.Priority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBGameFlowPriorityTest::RunTest(const FString& Parameters)
{
	using namespace PBGameFlowTests;

	TestTrue(TEXT("on site without a contract"), GameFlow::Resolve(MakeFacts(true, false, false, false)) == EGameFlowState::OnSite);
	TestTrue(TEXT("on site beats an open debrief"), GameFlow::Resolve(MakeFacts(true, false, true, true)) == EGameFlowState::OnSite);
	TestTrue(TEXT("debrief beats a taken contract"), GameFlow::Resolve(MakeFacts(false, false, true, true)) == EGameFlowState::Debrief);
	TestTrue(TEXT("debrief beats the home"), GameFlow::Resolve(MakeFacts(false, true, true, false)) == EGameFlowState::Debrief);
	TestTrue(TEXT("the home"), GameFlow::Resolve(MakeFacts(false, true, false, false)) == EGameFlowState::InHome);
	TestTrue(TEXT("the home beats a taken contract"), GameFlow::Resolve(MakeFacts(false, true, false, true)) == EGameFlowState::InHome);

	return true;
}

// The names the debug readout prints.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBGameFlowNamesTest, "PlotBuilder.Flow.Names", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBGameFlowNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("hub"), FString(GameFlow::ToText(EGameFlowState::InHub)), FString(TEXT("InHub")));
	TestEqual(TEXT("selected"), FString(GameFlow::ToText(EGameFlowState::ContractSelected)), FString(TEXT("ContractSelected")));
	TestEqual(TEXT("on site"), FString(GameFlow::ToText(EGameFlowState::OnSite)), FString(TEXT("OnSite")));
	TestEqual(TEXT("debrief"), FString(GameFlow::ToText(EGameFlowState::Debrief)), FString(TEXT("Debrief")));
	TestEqual(TEXT("home"), FString(GameFlow::ToText(EGameFlowState::InHome)), FString(TEXT("InHome")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
