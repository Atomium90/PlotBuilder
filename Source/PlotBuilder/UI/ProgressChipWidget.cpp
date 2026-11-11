#include "ProgressChipWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Contracts/MissionResultSubsystem.h"
#include "Flow/GameFlowSubsystem.h"

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

void UProgressChipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this))
	{
		// The flow only looks a few times a second and still holds the state of the level before this one: look now, so a chip built at
		// the start of a mission is right from its first frame.
		Flow->Refresh();

		Flow->OnStateChanged.Remove(FlowChangedHandle);
		FlowChangedHandle = Flow->OnStateChanged.AddUObject(this, &UProgressChipWidget::HandleFlowChanged);
	}

	ApplyVisibility();
}

void UProgressChipWidget::NativeDestruct()
{
	if (UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this))
	{
		Flow->OnStateChanged.Remove(FlowChangedHandle);
	}

	FlowChangedHandle.Reset();

	Super::NativeDestruct();
}

void UProgressChipWidget::HandleFlowChanged(EGameFlowState /*OldState*/, EGameFlowState /*NewState*/)
{
	ApplyVisibility();
}

void UProgressChipWidget::ApplyVisibility()
{
	// The chip only means something in a mission; elsewhere it takes no room.
	const UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);
	const bool bInMission = Flow && Flow->GetState() == EGameFlowState::OnSite && UMissionResultSubsystem::Get(this);

	SetVisibility(bInMission ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UProgressChipWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Visibility is not decided here: a collapsed widget is never ticked again (see ApplyVisibility).
	UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this);

	if (!Results)
	{
		return;
	}

	const float Completion = Results->GetLiveNotebook().Completion;

	Ring->SetRatio(Completion);

	// Rounded down: the ring never reads 100 while a piece is still standing.
	PercentText->SetText(FText::AsNumber(FMath::FloorToInt(Completion * 100.f)));
}
