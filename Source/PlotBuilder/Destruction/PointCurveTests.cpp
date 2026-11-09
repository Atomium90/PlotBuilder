#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Destruction/DemolitionRules.h"
#include "Destruction/PointCurve.h"

/**
 *  The curves given as lists of points, and c(h) read from them. Run from the editor:
 *  Automation RunTests PlotBuilder.Destruction.PointCurve
 */

// Straight between two points, flat beyond the ends, a single point is a constant, two points at the same x make a step.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBPointCurveShapeTest, "PlotBuilder.Destruction.PointCurve.Shape", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBPointCurveShapeTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Points = {FVector2D(0.0, 2.0), FVector2D(0.5, 4.0), FVector2D(1.0, 4.0)};

	TestNearlyEqual(TEXT("on the first point"), PointCurve::Evaluate(Points, 0.f), 2.f, 0.0001f);
	TestNearlyEqual(TEXT("halfway to the second"), PointCurve::Evaluate(Points, 0.25f), 3.f, 0.0001f);
	TestNearlyEqual(TEXT("on the second point"), PointCurve::Evaluate(Points, 0.5f), 4.f, 0.0001f);
	TestNearlyEqual(TEXT("flat between two equal points"), PointCurve::Evaluate(Points, 0.8f), 4.f, 0.0001f);
	TestNearlyEqual(TEXT("flat before the first point"), PointCurve::Evaluate(Points, -3.f), 2.f, 0.0001f);
	TestNearlyEqual(TEXT("flat after the last point"), PointCurve::Evaluate(Points, 7.f), 4.f, 0.0001f);

	const TArray<FVector2D> One = {FVector2D(0.3, 9.0)};

	TestNearlyEqual(TEXT("one point, before"), PointCurve::Evaluate(One, 0.f), 9.f, 0.0001f);
	TestNearlyEqual(TEXT("one point, after"), PointCurve::Evaluate(One, 1.f), 9.f, 0.0001f);

	const TArray<FVector2D> Step = {FVector2D(0.0, 1.0), FVector2D(0.5, 1.0), FVector2D(0.5, 5.0), FVector2D(1.0, 5.0)};

	TestNearlyEqual(TEXT("step, below"), PointCurve::Evaluate(Step, 0.4f), 1.f, 0.0001f);
	TestNearlyEqual(TEXT("step, above"), PointCurve::Evaluate(Step, 0.6f), 5.f, 0.0001f);

	return true;
}

// c(h) of the GDD: 0.3 at no hit points, 0.6 at 0.35, 1.0 from 0.75 up, a straight line between. The same numbers as the curve asset
// it replaced and as the code that stood in for it. Fewer than two points is not a curve: the default stands in.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBStabilityCouplingTest, "PlotBuilder.Destruction.PointCurve.StabilityCoupling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBStabilityCouplingTest::RunTest(const FString& Parameters)
{
	UDemolitionRules* Rules = NewObject<UDemolitionRules>(GetTransientPackage());

	TestNearlyEqual(TEXT("no hit points"), Rules->ComputeStabilityCoupling(0.f), 0.3f, 0.0001f);
	TestNearlyEqual(TEXT("halfway to critical"), Rules->ComputeStabilityCoupling(0.175f), 0.45f, 0.0001f);
	TestNearlyEqual(TEXT("critical"), Rules->ComputeStabilityCoupling(0.35f), 0.6f, 0.0001f);
	TestNearlyEqual(TEXT("between critical and sound"), Rules->ComputeStabilityCoupling(0.55f), 0.8f, 0.0001f);
	TestNearlyEqual(TEXT("sound"), Rules->ComputeStabilityCoupling(0.75f), 1.f, 0.0001f);
	TestNearlyEqual(TEXT("untouched"), Rules->ComputeStabilityCoupling(1.f), 1.f, 0.0001f);

	// Another curve set in the asset takes over.
	Rules->StabilityTransmission = {FVector2D(0.0, 0.0), FVector2D(1.0, 1.0)};

	TestNearlyEqual(TEXT("a straight line"), Rules->ComputeStabilityCoupling(0.4f), 0.4f, 0.0001f);

	// Empty or a single point: the default, not a piece that transmits nothing or everything.
	Rules->StabilityTransmission.Reset();

	TestNearlyEqual(TEXT("empty list, default curve"), Rules->ComputeStabilityCoupling(0.55f), 0.8f, 0.0001f);

	Rules->StabilityTransmission = {FVector2D(0.5, 0.1)};

	TestNearlyEqual(TEXT("one point, default curve"), Rules->ComputeStabilityCoupling(0.55f), 0.8f, 0.0001f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
