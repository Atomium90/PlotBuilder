#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/ContractTexts.h"

/**
 *  The words of the contract screens, on small hand-made contracts. Run from the editor: Automation RunTests PlotBuilder.Contracts.Texts
 */
namespace PBTextsTests
{
	FContractDefinition MakeContract(const TCHAR* Archetype, EContractType Type)
	{
		FContractDefinition Contract;
		Contract.ArchetypeId = FName(Archetype);
		Contract.Type = Type;
		return Contract;
	}

	void AddCondition(FContractDefinition& Contract, EConditionType Type, int32 Count, const TCHAR* Param = TEXT(""))
	{
		FContractCondition& Condition = Contract.Conditions.AddDefaulted_GetRef();
		Condition.Type = Type;
		Condition.Count = Count;
		Condition.Param = FName(Param);
	}
}

// The archetype asset names turn into what a client would call the place.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBTextsSiteTest, "PlotBuilder.Contracts.Texts.Site", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBTextsSiteTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("bungalow"), ContractTexts::SiteName(PBTextsTests::MakeContract(TEXT("DA_Archetype_Bungalow"), EContractType::TotalDemolition)).ToString(), FString(TEXT("Bungalow")));
	TestEqual(TEXT("two storeys"), ContractTexts::SiteName(PBTextsTests::MakeContract(TEXT("DA_Archetype_TwoStorey"), EContractType::TotalDemolition)).ToString(), FString(TEXT("Two-storey house")));
	TestEqual(TEXT("three storeys"), ContractTexts::SiteName(PBTextsTests::MakeContract(TEXT("DA_Archetype_ThreeStorey"), EContractType::TotalDemolition)).ToString(), FString(TEXT("Three-storey house")));
	TestEqual(TEXT("apartment"), ContractTexts::SiteName(PBTextsTests::MakeContract(TEXT("DA_Archetype_Apartment"), EContractType::TotalDemolition)).ToString(), FString(TEXT("Apartment block")));
	TestEqual(TEXT("unknown"), ContractTexts::SiteName(PBTextsTests::MakeContract(TEXT("DA_Something"), EContractType::TotalDemolition)).ToString(), FString(TEXT("House")));
	TestEqual(TEXT("none"), ContractTexts::SiteName(FContractDefinition()).ToString(), FString(TEXT("House")));

	return true;
}

// The letters on the square that stands for a portrait.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBTextsInitialsTest, "PlotBuilder.Contracts.Texts.Initials", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBTextsInitialsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("two words and an ampersand"), ContractTexts::Initials(TEXT("Brandt & Sons")), FString(TEXT("BS")));
	TestEqual(TEXT("two words"), ContractTexts::Initials(TEXT("Walter Hale")), FString(TEXT("WH")));
	TestEqual(TEXT("one word"), ContractTexts::Initials(TEXT("amara")), FString(TEXT("A")));
	TestEqual(TEXT("three words"), ContractTexts::Initials(TEXT("Anne Marie Dupont")), FString(TEXT("AM")));
	TestEqual(TEXT("nothing"), ContractTexts::Initials(FString()), FString());

	return true;
}

// The time is shown to the half minute: 4.84 reads 5, 4.456 reads 4.5, 10.5 stays, 3.2 reads 3.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBTextsMinutesTest, "PlotBuilder.Contracts.Texts.Minutes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBTextsMinutesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("4.84"), ContractTexts::MinutesString(4.84f), FString(TEXT("5")));
	TestEqual(TEXT("4.456"), ContractTexts::MinutesString(4.456f), FString(TEXT("4.5")));
	TestEqual(TEXT("10.5"), ContractTexts::MinutesString(10.5f), FString(TEXT("10.5")));
	TestEqual(TEXT("3.2"), ContractTexts::MinutesString(3.2f), FString(TEXT("3")));
	TestEqual(TEXT("zero"), ContractTexts::MinutesString(0.f), FString(TEXT("0")));
	TestEqual(TEXT("negative"), ContractTexts::MinutesString(-2.f), FString(TEXT("0")));

	return true;
}

// The sentence of the job follows the type and names the site; the chips and the details follow the conditions in order.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBTextsConditionsTest, "PlotBuilder.Contracts.Texts.Conditions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBTextsConditionsTest::RunTest(const FString& Parameters)
{
	FContractDefinition Contract = PBTextsTests::MakeContract(TEXT("DA_Archetype_TwoStorey"), EContractType::TotalDemolition);
	PBTextsTests::AddCondition(Contract, EConditionType::SalvageFurniture, 3);
	PBTextsTests::AddCondition(Contract, EConditionType::ProtectedZone, 1);
	PBTextsTests::AddCondition(Contract, EConditionType::KeepStanding, 1, TEXT("LivingRoom"));
	PBTextsTests::AddCondition(Contract, EConditionType::ForbiddenTool, 1, TEXT("Jackhammer"));

	TestEqual(TEXT("job"), ContractTexts::Job(Contract).ToString(), FString(TEXT("Take the whole two-storey house down.")));

	const TArray<ContractTexts::FConditionText> Texts = ContractTexts::Conditions(Contract);

	if (!TestEqual(TEXT("four conditions"), Texts.Num(), 4))
	{
		return false;
	}

	TestEqual(TEXT("salvage chip"), Texts[0].Chip.ToString(), FString(TEXT("3 pieces")));
	TestEqual(TEXT("salvage title"), Texts[0].Title.ToString(), FString(TEXT("Pieces to recover")));
	TestEqual(TEXT("zone chip"), Texts[1].Chip.ToString(), FString(TEXT("1 zone")));
	TestEqual(TEXT("keep chip"), Texts[2].Chip.ToString(), FString(TEXT("Keep living room")));
	TestEqual(TEXT("keep detail"), Texts[2].Detail.ToString(), FString(TEXT("The living room must still stand at the end.")));
	TestEqual(TEXT("tool chip"), Texts[3].Chip.ToString(), FString(TEXT("No jackhammer")));
	TestEqual(TEXT("tool detail"), Texts[3].Detail.ToString(), FString(TEXT("The jackhammer is not allowed on this site.")));

	// One piece and two zones: the singular and the plural.
	FContractDefinition Small = PBTextsTests::MakeContract(TEXT("DA_Archetype_Bungalow"), EContractType::Renovation);
	PBTextsTests::AddCondition(Small, EConditionType::SalvageFurniture, 1);
	PBTextsTests::AddCondition(Small, EConditionType::ProtectedZone, 2);

	const TArray<ContractTexts::FConditionText> SmallTexts = ContractTexts::Conditions(Small);

	if (TestEqual(TEXT("two conditions"), SmallTexts.Num(), 2))
	{
		TestEqual(TEXT("one piece"), SmallTexts[0].Chip.ToString(), FString(TEXT("1 piece")));
		TestEqual(TEXT("two zones"), SmallTexts[1].Chip.ToString(), FString(TEXT("2 zones")));
	}

	TestEqual(TEXT("renovation job"), ContractTexts::Job(Small).ToString(), FString(TEXT("Strip the inside of the bungalow. The shell has to stay.")));
	TestEqual(TEXT("partial job"), ContractTexts::Job(PBTextsTests::MakeContract(TEXT("DA_Archetype_TwoStorey"), EContractType::PartialDemolition)).ToString(), FString(TEXT("The upper storeys come down. The storeys below have to stay.")));
	TestEqual(TEXT("no condition"), ContractTexts::Conditions(FContractDefinition()).Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
