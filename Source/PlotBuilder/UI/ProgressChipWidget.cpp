#include "ProgressChipWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Contracts/MissionDirector.h"
#include "Contracts/MissionResultSubsystem.h"

void UProgressRingWidget::SetRatio(const float NewRatio)
{
	Ratio = FMath::Clamp(NewRatio, 0.f, 1.f);
}

int32 UProgressRingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 SuperLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FPaintContext InContext(AllottedGeometry, MyCullingRect, OutDrawElements, SuperLayer, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Centre = Size * 0.5;
	const double Radius = FMath::Max(1.0, FMath::Min(Size.X, Size.Y) * 0.5 - static_cast<double>(Thickness) * 0.5);

	// Angles run clockwise from the top of the ring.
	const auto PointAt = [&Centre, Radius](const double Turns)
	{
		const double Angle = Turns * 2.0 * UE_DOUBLE_PI - UE_DOUBLE_HALF_PI;
		return Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	};

	const auto DrawArc = [this, &InContext, &PointAt](const double FromTurns, const double ToTurns, const FLinearColor& Color)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(static_cast<double>(Segments) * (ToTurns - FromTurns)));

		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const double A = FromTurns + (ToTurns - FromTurns) * static_cast<double>(Step) / static_cast<double>(Steps);
			const double B = FromTurns + (ToTurns - FromTurns) * static_cast<double>(Step + 1) / static_cast<double>(Steps);
			UWidgetBlueprintLibrary::DrawLine(InContext, PointAt(A), PointAt(B), Color, true, Thickness);
		}
	};

	DrawArc(0.0, 1.0, TrackColor);

	if (Ratio > 0.001f)
	{
		DrawArc(0.0, static_cast<double>(Ratio), FillColor);
	}

	return FMath::Max(SuperLayer, InContext.MaxLayer);
}

void UProgressChipWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The chip only means something in a mission; elsewhere it takes no room.
	UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this);
	const bool bInMission = Results && AMissionDirector::Find(this);

	SetVisibility(bInMission ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	if (!bInMission)
	{
		return;
	}

	const float Completion = Results->GetLiveNotebook().Completion;

	Ring->SetRatio(Completion);

	// Rounded down: the ring never reads 100 while a piece is still standing.
	PercentText->SetText(FText::AsNumber(FMath::FloorToInt(Completion * 100.f)));
}
