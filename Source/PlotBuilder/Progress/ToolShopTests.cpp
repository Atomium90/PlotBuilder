#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progress/ToolShop.h"

/**
 *  What the Tools and Parcel tabs say, on hand-made rows. Run from the editor: Automation RunTests PlotBuilder.Progress
 */
namespace PBToolShopTests
{
	FToolOfferRow MakeRow(const int32 Price, const bool bConsumable)
	{
		FToolOfferRow Row;
		Row.Price = Price;
		Row.bConsumable = bConsumable;
		return Row;
	}
}

// A tool is bought once: owned, then can buy or too expensive with the exact shortfall.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBToolShopToolTest, "PlotBuilder.Progress.ToolShop.Tool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBToolShopToolTest::RunTest(const FString& Parameters)
{
	const FToolOfferRow Crowbar = PBToolShopTests::MakeRow(300, false);

	TestTrue(TEXT("owned"), ToolShop::Evaluate(Crowbar, true, 5000, false).State == EOfferState::Owned);
	TestTrue(TEXT("owned stays owned however rich"), ToolShop::Evaluate(Crowbar, true, 0, false).State == EOfferState::Owned);

	const FOfferStatus Exact = ToolShop::Evaluate(Crowbar, false, 300, false);
	TestTrue(TEXT("exactly enough can buy"), Exact.State == EOfferState::CanBuy);
	TestEqual(TEXT("exact total"), Exact.Total, 300);
	TestEqual(TEXT("nothing missing"), Exact.Missing, 0);

	const FOfferStatus Short = ToolShop::Evaluate(Crowbar, false, 120, false);
	TestTrue(TEXT("short is too expensive"), Short.State == EOfferState::TooExpensive);
	TestEqual(TEXT("180 short"), Short.Missing, 180);

	// A quantity means nothing for a tool.
	TestEqual(TEXT("quantity ignored for a tool"), ToolShop::Evaluate(Crowbar, false, 1000, false, 5).Total, 300);

	// Infinite money (tests and the demo): never too expensive.
	TestTrue(TEXT("infinite money"), ToolShop::Evaluate(Crowbar, false, 0, true).State == EOfferState::CanBuy);

	return true;
}

// A consumable is bought by the unit, priced by the quantity, and never owned.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBToolShopConsumableTest, "PlotBuilder.Progress.ToolShop.Consumable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBToolShopConsumableTest::RunTest(const FString& Parameters)
{
	const FToolOfferRow Charge = PBToolShopTests::MakeRow(150, true);

	// "Buy 3 for 450" with 500: enough. The same with 400: 50 short.
	const FOfferStatus Three = ToolShop::Evaluate(Charge, false, 500, false, 3);
	TestTrue(TEXT("three can buy"), Three.State == EOfferState::CanBuy);
	TestEqual(TEXT("three cost 450"), Three.Total, 450);

	const FOfferStatus ShortThree = ToolShop::Evaluate(Charge, false, 400, false, 3);
	TestTrue(TEXT("three with 400 is too expensive"), ShortThree.State == EOfferState::TooExpensive);
	TestEqual(TEXT("50 short"), ShortThree.Missing, 50);

	// "Owned" does not apply: a consumable can always be bought again.
	TestTrue(TEXT("never owned"), ToolShop::Evaluate(Charge, true, 500, false, 1).State == EOfferState::CanBuy);

	// The stepper stays between 1 and 9.
	TestEqual(TEXT("clamp low"), ToolShop::ClampQuantity(0), 1);
	TestEqual(TEXT("clamp negative"), ToolShop::ClampQuantity(-4), 1);
	TestEqual(TEXT("clamp high"), ToolShop::ClampQuantity(40), ToolShop::MaxQuantity);
	TestEqual(TEXT("an asked quantity past the maximum is priced at the maximum"), ToolShop::Evaluate(Charge, false, 99999, false, 40).Total, 150 * ToolShop::MaxQuantity);

	return true;
}

// The parcel: saving (with how far along), can buy, owned.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBToolShopParcelTest, "PlotBuilder.Progress.ToolShop.Parcel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBToolShopParcelTest::RunTest(const FString& Parameters)
{
	const FParcelStatus Saving = ToolShop::EvaluateParcel(40000, false, 10000, false);
	TestTrue(TEXT("saving"), Saving.State == EParcelState::Saving);
	TestEqual(TEXT("30 000 to go"), Saving.Missing, 30000);
	TestTrue(TEXT("a quarter of the way"), FMath::IsNearlyEqual(Saving.Fraction, 0.25f));

	TestTrue(TEXT("exactly the price can buy"), ToolShop::EvaluateParcel(40000, false, 40000, false).State == EParcelState::CanBuy);
	TestTrue(TEXT("infinite money can buy"), ToolShop::EvaluateParcel(40000, false, 0, true).State == EParcelState::CanBuy);
	TestTrue(TEXT("owned"), ToolShop::EvaluateParcel(40000, true, 0, false).State == EParcelState::Owned);
	TestTrue(TEXT("owned fills the bar"), FMath::IsNearlyEqual(ToolShop::EvaluateParcel(40000, true, 0, false).Fraction, 1.f));
	TestTrue(TEXT("a free parcel is not a division by zero"), ToolShop::EvaluateParcel(0, false, 0, false).State == EParcelState::CanBuy);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
