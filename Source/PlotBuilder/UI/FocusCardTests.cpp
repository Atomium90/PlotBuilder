#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/FocusCard.h"

/**
 *  What the focus card says about the thing under the crosshair, on hand-made facts. Run from the editor: Automation RunTests PlotBuilder.UI.FocusCard
 */
namespace PBFocusCardTests
{
	FFocusCardFacts MakeWall()
	{
		FFocusCardFacts Facts;
		Facts.bHasTarget = true;
		Facts.Name = TEXT("Exterior wall");
		Facts.Material = EDemolitionMaterial::Brick;
		Facts.Storey = 1;
		return Facts;
	}
}

// Nothing under the crosshair: no card. A wall of the house: its name, the material and the storey, and a state that follows the health.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBFocusCardPieceTest, "PlotBuilder.UI.FocusCard.Piece", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBFocusCardPieceTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("no target, no card"), !FocusCard::Describe(FFocusCardFacts()).bVisible);

	FFocusCardFacts Wall = PBFocusCardTests::MakeWall();
	Wall.State = EDurabilityState::Damaged;
	Wall.HPRatio = 0.62f;

	const FFocusCardView View = FocusCard::Describe(Wall);

	TestTrue(TEXT("shown"), View.bVisible);
	TestEqual(TEXT("name"), View.Name, FString(TEXT("Exterior wall")));
	TestEqual(TEXT("material and storey"), View.Sub, FString(TEXT("Brick · storey 1")));
	TestEqual(TEXT("damaged"), View.StateText, FString(TEXT("Damaged")));
	TestTrue(TEXT("amber"), View.StateTone == EFocusTone::Warn);
	TestTrue(TEXT("the bar shows once it is hurt"), View.bShowBar);
	TestEqual(TEXT("62 %"), View.FillText, FString(TEXT("62 %")));

	Wall.Storey = 0;
	TestEqual(TEXT("ground floor"), FocusCard::Describe(Wall).Sub, FString(TEXT("Brick · ground floor")));

	Wall.State = EDurabilityState::Critical;
	Wall.HPRatio = 0.14f;
	TestTrue(TEXT("critical is coral"), FocusCard::Describe(Wall).StateTone == EFocusTone::Bad);

	return true;
}

// The bar is left out at full health outside a mission, and is always there in one. It never reads 100 % while hurt.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBFocusCardBarTest, "PlotBuilder.UI.FocusCard.Bar", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBFocusCardBarTest::RunTest(const FString& Parameters)
{
	FFocusCardFacts Wall = PBFocusCardTests::MakeWall();

	TestTrue(TEXT("home, whole: no bar"), !FocusCard::Describe(Wall).bShowBar);

	Wall.bInMission = true;
	TestTrue(TEXT("mission, whole: bar"), FocusCard::Describe(Wall).bShowBar);

	Wall.bInMission = false;
	Wall.HPRatio = 0.999f;
	Wall.State = EDurabilityState::Damaged;
	TestTrue(TEXT("home, barely hurt: bar"), FocusCard::Describe(Wall).bShowBar);
	TestEqual(TEXT("99 % and not 100"), FocusCard::Describe(Wall).FillText, FString(TEXT("99 %")));

	return true;
}

// The line on the tool in hand: good at 1 or more, slow from 0.5, barely scratching under that. Only in a mission.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBFocusCardToolTest, "PlotBuilder.UI.FocusCard.Tool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBFocusCardToolTest::RunTest(const FString& Parameters)
{
	FFocusCardFacts Wall = PBFocusCardTests::MakeWall();
	Wall.bInMission = true;
	Wall.bHasToolMatch = true;
	Wall.ToolName = TEXT("Sledgehammer");

	Wall.ToolMultiplier = 1.5f;
	FFocusCardView View = FocusCard::Describe(Wall);
	TestEqual(TEXT("good"), View.ToolText, FString(TEXT("Sledgehammer: good on brick")));
	TestTrue(TEXT("teal"), View.ToolTone == EFocusTone::Good);

	Wall.ToolMultiplier = 0.6f;
	View = FocusCard::Describe(Wall);
	TestEqual(TEXT("slow"), View.ToolText, FString(TEXT("Sledgehammer: slow on brick")));
	TestTrue(TEXT("amber"), View.ToolTone == EFocusTone::Warn);

	Wall.ToolMultiplier = 0.2f;
	View = FocusCard::Describe(Wall);
	TestEqual(TEXT("barely"), View.ToolText, FString(TEXT("Sledgehammer: barely scratches brick")));
	TestTrue(TEXT("coral"), View.ToolTone == EFocusTone::Bad);

	Wall.bInMission = false;
	TestTrue(TEXT("no tool line outside a mission"), !FocusCard::Describe(Wall).bShowTool);

	Wall.bInMission = true;
	Wall.bHasToolMatch = false;
	TestTrue(TEXT("no tool in hand, no line"), !FocusCard::Describe(Wall).bShowTool);

	return true;
}

// A piece about to fall says so. Furniture the client wants shows what it is worth now against full health: a 400 sofa at the 0.5 share is 200.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBFocusCardWarningAndValueTest, "PlotBuilder.UI.FocusCard.WarningAndValue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBFocusCardWarningAndValueTest::RunTest(const FString& Parameters)
{
	FFocusCardFacts Slab = PBFocusCardTests::MakeWall();
	Slab.Name = TEXT("Floor slab");
	Slab.Material = EDemolitionMaterial::Concrete;
	Slab.bCompromised = true;
	Slab.HPRatio = 0.38f;
	Slab.State = EDurabilityState::Damaged;

	const FFocusCardView Falling = FocusCard::Describe(Slab);

	TestEqual(TEXT("compromised"), Falling.StateText, FString(TEXT("Compromised")));
	TestTrue(TEXT("coral"), Falling.StateTone == EFocusTone::Bad);
	TestTrue(TEXT("warning"), Falling.bShowWarning);

	FFocusCardFacts Sofa;
	Sofa.bHasTarget = true;
	Sofa.Name = TEXT("Sofa");
	Sofa.Material = EDemolitionMaterial::Wood;
	Sofa.Role = TEXT("the client wants it");
	Sofa.bInMission = true;
	Sofa.State = EDurabilityState::Damaged;
	Sofa.HPRatio = 0.8f;
	Sofa.bIsSalvage = true;
	Sofa.Value = 400;
	Sofa.ValueShare = 0.5f;

	const FFocusCardView Furniture = FocusCard::Describe(Sofa);

	TestEqual(TEXT("material and role"), Furniture.Sub, FString(TEXT("Wood · the client wants it")));
	TestTrue(TEXT("value shown"), Furniture.bShowValue);
	TestEqual(TEXT("worth 200 now"), Furniture.ValueNow, FString(TEXT("200")));
	TestEqual(TEXT("of 400"), Furniture.ValueText, FString(TEXT("of 400 at this health")));
	TestTrue(TEXT("no warning on a sofa"), !Furniture.bShowWarning);

	Sofa.bIsSalvage = false;
	TestTrue(TEXT("a piece of furniture nobody asked for has no value line"), !FocusCard::Describe(Sofa).bShowValue);

	return true;
}

// The card is for the mission and the home: not the hub (the PC and the truck have nothing to read), not the debrief.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBFocusCardWhereTest, "PlotBuilder.UI.FocusCard.Where", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBFocusCardWhereTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("in a mission"), FocusCard::ShownIn(EGameFlowState::OnSite));
	TestTrue(TEXT("at home"), FocusCard::ShownIn(EGameFlowState::InHome));
	TestTrue(TEXT("not in the hub"), !FocusCard::ShownIn(EGameFlowState::InHub));
	TestTrue(TEXT("not with a contract taken"), !FocusCard::ShownIn(EGameFlowState::ContractSelected));
	TestTrue(TEXT("not on the debrief"), !FocusCard::ShownIn(EGameFlowState::Debrief));

	return true;
}

// The take-back key shows where it does something, and never in a mission.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBFocusCardTakeBackTest, "PlotBuilder.UI.FocusCard.TakeBack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBFocusCardTakeBackTest::RunTest(const FString& Parameters)
{
	FFocusCardFacts Sofa;
	Sofa.bHasTarget = true;
	Sofa.Name = TEXT("Sofa");
	Sofa.Role = TEXT("placed in your home");

	TestTrue(TEXT("not possible, no key"), !FocusCard::Describe(Sofa).bShowTakeBack);

	Sofa.bCanTakeBack = true;
	TestTrue(TEXT("possible, key"), FocusCard::Describe(Sofa).bShowTakeBack);

	Sofa.bInMission = true;
	TestTrue(TEXT("never in a mission"), !FocusCard::Describe(Sofa).bShowTakeBack);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
