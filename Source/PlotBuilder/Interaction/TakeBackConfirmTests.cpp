#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/TakeBackConfirm.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"

/**
 *  The second press that confirms a risky take-back, with no level. Run them from the editor:
 *  Automation RunTests PlotBuilder.Hotbar
 */

// First press arms and refuses, a second press on the same target in time takes it, and the one after starts over.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBTakeBackConfirmTest, "PlotBuilder.Hotbar.TakeBackConfirm", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBTakeBackConfirmTest::RunTest(const FString& Parameters)
{
	UDataTable* Wall = NewObject<UDataTable>(GetTransientPackage());

	FTakeBackConfirm Confirm;

	TestFalse(TEXT("the first press only arms"), Confirm.Confirm(Wall, 10.0, 2.0));
	TestTrue(TEXT("the second press in time confirms"), Confirm.Confirm(Wall, 11.5, 2.0));
	TestFalse(TEXT("a confirmed take-back disarms: the next press arms again"), Confirm.Confirm(Wall, 11.6, 2.0));

	return true;
}

// A press that comes too late, or on another piece, never confirms: it arms the new one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBTakeBackConfirmWindowTest, "PlotBuilder.Hotbar.TakeBackConfirmWindow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBTakeBackConfirmWindowTest::RunTest(const FString& Parameters)
{
	UDataTable* Wall = NewObject<UDataTable>(GetTransientPackage());
	UDataTable* Floor = NewObject<UDataTable>(GetTransientPackage());

	FTakeBackConfirm Confirm;

	Confirm.Confirm(Wall, 10.0, 2.0);
	TestFalse(TEXT("a press after the window arms again, it does not confirm"), Confirm.Confirm(Wall, 12.5, 2.0));
	TestTrue(TEXT("and that new arming can be confirmed in time"), Confirm.Confirm(Wall, 13.0, 2.0));

	Confirm.Confirm(Wall, 20.0, 2.0);
	TestFalse(TEXT("another piece does not borrow the first one's arming"), Confirm.Confirm(Floor, 20.5, 2.0));
	TestFalse(TEXT("the first piece lost its arming to the other one"), Confirm.Confirm(Wall, 21.0, 2.0));

	TestFalse(TEXT("no target never confirms"), Confirm.Confirm(nullptr, 30.0, 2.0));
	TestFalse(TEXT("even twice"), Confirm.Confirm(nullptr, 30.1, 2.0));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
