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

	// The stepper stays between 1 and what the stock can still take (10 by default, so it goes past 9).
	const int32 Room = ToolShop::Room(Charge, 0);

	TestEqual(TEXT("an empty stock has room for the row's limit"), Room, 10);
	TestEqual(TEXT("clamp low"), ToolShop::ClampQuantity(0, Room), 1);
	TestEqual(TEXT("clamp negative"), ToolShop::ClampQuantity(-4, Room), 1);
	TestEqual(TEXT("ten can be asked"), ToolShop::ClampQuantity(10, Room), 10);
	TestEqual(TEXT("clamp high"), ToolShop::ClampQuantity(40, Room), 10);
	TestEqual(TEXT("an asked quantity past the maximum is priced at the maximum"), ToolShop::Evaluate(Charge, false, 99999, false, 40).Total, 150 * 10);

	return true;
}

// The stock limit of a consumable: what the player holds counts. With 7 of 10 held, three more at most, and asking for five is priced
// at three; with 10 held the card says the stock is full and nothing can be bought, however rich. A limit of 0 means no limit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBToolShopStockLimitTest, "PlotBuilder.Progress.ToolShop.StockLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBToolShopStockLimitTest::RunTest(const FString& Parameters)
{
	const FToolOfferRow Charge = PBToolShopTests::MakeRow(150, true);

	TestEqual(TEXT("room with 7 held"), ToolShop::Room(Charge, 7), 3);

	const FOfferStatus Five = ToolShop::Evaluate(Charge, false, 99999, false, 5, 7);
	TestTrue(TEXT("five asked with 7 held can still buy"), Five.State == EOfferState::CanBuy);
	TestEqual(TEXT("priced at the three that fit"), Five.Total, 450);
	TestEqual(TEXT("the stepper stops at the room left"), ToolShop::ClampQuantity(5, ToolShop::Room(Charge, 7)), 3);

	const FOfferStatus Full = ToolShop::Evaluate(Charge, false, 99999, false, 1, 10);
	TestTrue(TEXT("10 held is the limit"), Full.State == EOfferState::AtLimit);
	TestTrue(TEXT("full, even with infinite money"), ToolShop::Evaluate(Charge, false, 0, true, 1, 10).State == EOfferState::AtLimit);
	TestEqual(TEXT("nothing to pay"), Full.Total, 0);
	TestEqual(TEXT("the stepper still shows 1"), ToolShop::ClampQuantity(5, ToolShop::Room(Charge, 10)), 1);
	TestTrue(TEXT("more than the limit held is full too"), ToolShop::Evaluate(Charge, false, 99999, false, 1, 14).State == EOfferState::AtLimit);

	FToolOfferRow Unlimited = PBToolShopTests::MakeRow(150, true);
	Unlimited.MaxStock = 0;

	TestEqual(TEXT("no limit, a lot of room"), ToolShop::Room(Unlimited, 500), ToolShop::MaxQuantityUnlimited);
	TestEqual(TEXT("no limit, asked 40"), ToolShop::Evaluate(Unlimited, false, 99999, false, 40, 500).Total, 150 * 40);

	// A tool has no stock: the limit means nothing for it.
	FToolOfferRow Crowbar = PBToolShopTests::MakeRow(300, false);
	TestTrue(TEXT("a tool ignores the stock"), ToolShop::Evaluate(Crowbar, false, 1000, false, 1, 50).State == EOfferState::CanBuy);

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
