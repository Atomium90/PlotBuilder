#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Contracts/ContractEvaluator.h"
#include "Contracts/ContractPool.h"

/**
 *  The contract pool without an editor asset: templates built by hand with explicit ranges (no archetype), drawn with a
 *  random stream. Run from the editor: Automation RunTests PlotBuilder.Contracts.Pool
 */
namespace PBPoolTests
{
	/** A template with no archetype: a 5 x 5 footprint (100 m2 on one storey), total demolition only, no condition. */
	FContractTemplate MakeTemplate(int32 Tier, float Weight)
	{
		FContractTemplate Template;
		Template.Id = FName(*FString::Printf(TEXT("Template%d"), Tier));
		Template.Tier = Tier;
		Template.Weight = Weight;
		Template.MinFootprintCells = FIntPoint(5, 5);
		Template.MaxFootprintCells = FIntPoint(5, 5);
		Template.MinFloors = 1;
		Template.MaxFloors = 1;
		return Template;
	}

	UContractPool* MakePool(const TArray<FContractTemplate>& Templates)
	{
		UContractPool* Pool = NewObject<UContractPool>(GetTransientPackage());
		Pool->Templates = Templates;
		return Pool;
	}
}

// 5 x 5 cells on one storey is 100 m2: a total demolition shows 6 minutes and pays 1200, and a 1.5 multiplier makes it 1800.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBPoolEstimatesTest, "PlotBuilder.Contracts.Pool.Estimates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBPoolEstimatesTest::RunTest(const FString& Parameters)
{
	UContractPool* Pool = PBPoolTests::MakePool({});
	FContractTemplate Template = PBPoolTests::MakeTemplate(0, 1.f);

	FRandomStream Stream(1);
	const FContractDefinition Plain = Pool->DrawContract(Template, Stream);

	TestNearlyEqual(TEXT("area"), Plain.AreaM2, 100.f, 0.001f);
	TestNearlyEqual(TEXT("minutes"), Plain.EstimatedMinutes, 6.f, 0.001f);
	TestEqual(TEXT("payout"), Plain.BasePayout, 1200);
	TestTrue(TEXT("total demolition"), Plain.Type == EContractType::TotalDemolition);

	Template.PayoutMultiplier = 1.5f;
	FRandomStream OtherStream(1);
	const FContractDefinition Better = Pool->DrawContract(Template, OtherStream);

	TestEqual(TEXT("payout with a 1.5 multiplier"), Better.BasePayout, 1800);

	return true;
}

// The same stream seed gives the same contract; the footprint and storeys stay in the template's range and the area follows them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBPoolDeterminismTest, "PlotBuilder.Contracts.Pool.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBPoolDeterminismTest::RunTest(const FString& Parameters)
{
	UContractPool* Pool = PBPoolTests::MakePool({});
	FContractTemplate Template = PBPoolTests::MakeTemplate(0, 1.f);
	Template.MinFootprintCells = FIntPoint(4, 3);
	Template.MaxFootprintCells = FIntPoint(6, 5);
	Template.MinFloors = 2;
	Template.MaxFloors = 3;
	Template.PartialWeight = 1.f;
	Template.RenovationWeight = 1.f;

	for (int32 Seed = 1; Seed <= 30; ++Seed)
	{
		FRandomStream First(Seed);
		FRandomStream Second(Seed);
		const FContractDefinition A = Pool->DrawContract(Template, First);
		const FContractDefinition B = Pool->DrawContract(Template, Second);

		TestEqual(TEXT("same seed, same house seed"), A.Seed, B.Seed);
		TestTrue(TEXT("same seed, same footprint"), A.FootprintCells == B.FootprintCells);
		TestEqual(TEXT("same seed, same storeys"), A.Floors, B.Floors);
		TestTrue(TEXT("same seed, same type"), A.Type == B.Type);
		TestEqual(TEXT("same seed, same pay"), A.BasePayout, B.BasePayout);

		TestTrue(TEXT("width in range"), A.FootprintCells.X >= 4 && A.FootprintCells.X <= 6);
		TestTrue(TEXT("depth in range"), A.FootprintCells.Y >= 3 && A.FootprintCells.Y <= 5);
		TestTrue(TEXT("storeys in range"), A.Floors >= 2 && A.Floors <= 3);
		TestNearlyEqual(TEXT("area follows the request"), A.AreaM2, static_cast<float>(A.FootprintCells.X * A.FootprintCells.Y * A.Floors) * 4.f, 0.001f);
	}

	return true;
}

// Three offers, never the same kind twice; a kind without weight never comes; two kinds for three offers come back only after both were used.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBPoolOffersTest, "PlotBuilder.Contracts.Pool.Offers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBPoolOffersTest::RunTest(const FString& Parameters)
{
	// The tier tells the kinds apart: kind 9 has no weight.
	TArray<FContractTemplate> Templates;

	for (int32 Tier = 1; Tier <= 5; ++Tier)
	{
		Templates.Add(PBPoolTests::MakeTemplate(Tier, 1.f));
	}

	Templates.Add(PBPoolTests::MakeTemplate(9, 0.f));

	const UContractPool* Pool = PBPoolTests::MakePool(Templates);

	for (int32 Seed = 1; Seed <= 50; ++Seed)
	{
		FRandomStream Stream(Seed);
		const TArray<FContractDefinition> Offers = Pool->DrawOffers(3, Stream);

		TestEqual(TEXT("three offers"), Offers.Num(), 3);

		TSet<int32> Tiers;

		for (const FContractDefinition& Offer : Offers)
		{
			Tiers.Add(Offer.ReputationRequired);
			TestTrue(TEXT("a kind without weight never comes"), Offer.ReputationRequired != 9);
		}

		TestEqual(TEXT("never the same kind twice"), Tiers.Num(), 3);
	}

	// Two kinds, three offers: the third repeats one of the first two.
	const UContractPool* Short = PBPoolTests::MakePool({PBPoolTests::MakeTemplate(1, 1.f), PBPoolTests::MakeTemplate(2, 1.f)});
	FRandomStream ShortStream(7);
	const TArray<FContractDefinition> ShortOffers = Short->DrawOffers(3, ShortStream);

	TestEqual(TEXT("still three offers"), ShortOffers.Num(), 3);
	TestTrue(TEXT("the first two are different kinds"), ShortOffers.Num() == 3 && ShortOffers[0].ReputationRequired != ShortOffers[1].ReputationRequired);

	// Nothing with a weight: no offers.
	const UContractPool* Empty = PBPoolTests::MakePool({PBPoolTests::MakeTemplate(1, 0.f)});
	FRandomStream EmptyStream(1);

	TestEqual(TEXT("no weight, no offer"), Empty->DrawOffers(3, EmptyStream).Num(), 0);

	return true;
}

// A partial demolition needs two storeys; a type with all the weight is always the one drawn; conditions come with their count.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBPoolTypesTest, "PlotBuilder.Contracts.Pool.TypesAndConditions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBPoolTypesTest::RunTest(const FString& Parameters)
{
	UContractPool* Pool = PBPoolTests::MakePool({});

	FContractTemplate OneStorey = PBPoolTests::MakeTemplate(0, 1.f);
	OneStorey.TotalWeight = 0.f;
	OneStorey.PartialWeight = 1.f;

	FContractTemplate TwoStoreys = OneStorey;
	TwoStoreys.MinFloors = 2;
	TwoStoreys.MaxFloors = 2;

	FContractTemplate Strip = PBPoolTests::MakeTemplate(0, 1.f);
	Strip.TotalWeight = 0.f;
	Strip.RenovationWeight = 1.f;
	Strip.MinSalvage = 2;
	Strip.MaxSalvage = 2;
	Strip.MinZones = 1;
	Strip.MaxZones = 1;

	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		FRandomStream A(Seed);
		FRandomStream B(Seed);
		FRandomStream C(Seed);

		TestTrue(TEXT("no partial demolition on one storey"), Pool->DrawContract(OneStorey, A).Type == EContractType::TotalDemolition);
		TestTrue(TEXT("partial demolition on two storeys"), Pool->DrawContract(TwoStoreys, B).Type == EContractType::PartialDemolition);

		const FContractDefinition Renovation = Pool->DrawContract(Strip, C);

		TestTrue(TEXT("renovation"), Renovation.Type == EContractType::Renovation);
		TestEqual(TEXT("salvage count"), Renovation.CountOf(EConditionType::SalvageFurniture), 2);
		TestEqual(TEXT("zone count"), Renovation.CountOf(EConditionType::ProtectedZone), 1);
		TestEqual(TEXT("no other condition"), Renovation.Conditions.Num(), 2);
	}

	return true;
}

// The keep-standing and forbidden-tool conditions: always there with a chance of 1, never with 0; never keep-standing on a renovation;
// the room and the tool come from the template's lists, and a template with an empty list never asks.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBPoolNamedConditionsTest, "PlotBuilder.Contracts.Pool.NamedConditions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBPoolNamedConditionsTest::RunTest(const FString& Parameters)
{
	UContractPool* Pool = PBPoolTests::MakePool({});

	FContractTemplate Always = PBPoolTests::MakeTemplate(0, 1.f);
	Always.KeepStandingChance = 1.f;
	Always.KeepStandingRooms = {ERoomType::Bathroom};
	Always.ForbiddenToolChance = 1.f;
	Always.ForbiddenTools = {FName(TEXT("Jackhammer"))};

	FContractTemplate Never = Always;
	Never.KeepStandingChance = 0.f;
	Never.ForbiddenToolChance = 0.f;

	FContractTemplate NoList = Always;
	NoList.KeepStandingRooms.Reset();
	NoList.ForbiddenTools.Reset();

	FContractTemplate Strip = Always;
	Strip.TotalWeight = 0.f;
	Strip.RenovationWeight = 1.f;

	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		FRandomStream A(Seed), B(Seed), C(Seed), D(Seed);

		const FContractDefinition Yes = Pool->DrawContract(Always, A);
		TestEqual(TEXT("keep-standing is asked"), Yes.CountOf(EConditionType::KeepStanding), 1);
		TestTrue(TEXT("keep-standing room"), Yes.FindCondition(EConditionType::KeepStanding) && Yes.FindCondition(EConditionType::KeepStanding)->Param == FName(TEXT("Bathroom")));
		TestEqual(TEXT("a tool is forbidden"), Yes.CountOf(EConditionType::ForbiddenTool), 1);
		TestTrue(TEXT("the tool"), Yes.FindCondition(EConditionType::ForbiddenTool) && Yes.FindCondition(EConditionType::ForbiddenTool)->Param == FName(TEXT("Jackhammer")));

		const FContractDefinition No = Pool->DrawContract(Never, B);
		TestTrue(TEXT("a chance of 0 never asks"), !No.FindCondition(EConditionType::KeepStanding) && !No.FindCondition(EConditionType::ForbiddenTool));

		const FContractDefinition Empty = Pool->DrawContract(NoList, C);
		TestTrue(TEXT("an empty list never asks"), !Empty.FindCondition(EConditionType::KeepStanding) && !Empty.FindCondition(EConditionType::ForbiddenTool));

		const FContractDefinition Renovation = Pool->DrawContract(Strip, D);
		TestTrue(TEXT("renovation"), Renovation.Type == EContractType::Renovation);
		TestTrue(TEXT("no keep-standing on a renovation"), !Renovation.FindCondition(EConditionType::KeepStanding));
		TestEqual(TEXT("a renovation can still forbid a tool"), Renovation.CountOf(EConditionType::ForbiddenTool), 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
