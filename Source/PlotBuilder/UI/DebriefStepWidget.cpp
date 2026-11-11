#include "DebriefStepWidget.h"
#include "UIStyle.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

namespace
{
	/** The colour of a state: green for what went well, red for what went wrong, orange for what is in hand, grey for what is still to do. */
	FLinearColor DebriefStateColor(const UUIStyle& Style, const ENotebookState State)
	{
		switch (State)
		{
		case ENotebookState::Done:
		case ENotebookState::Ok:
			return Style.Success;
		case ENotebookState::Lost:
		case ENotebookState::Warning:
			return Style.Danger;
		case ENotebookState::Carried:
			return Style.Accent;
		default:
			return Style.TextMuted;
		}
	}

	/** Puts a widget of a canvas panel at a point, with the given alignment (0 or 1 on each axis picks the corner that sits on the point). */
	void DebriefPlace(UWidget* Widget, const FVector2D& Position, const FVector2D& Alignment, const FVector2D* Size)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
			CanvasSlot->SetAlignment(Alignment);
			CanvasSlot->SetPosition(Position);

			if (Size)
			{
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetSize(*Size);
			}
			else
			{
				CanvasSlot->SetAutoSize(true);
			}
		}
	}
}

void UDebriefStepWidget::SetStep(const FDebriefStep& Step, const int32 Base, const bool bConnector, const bool bBiggest)
{
	NameText->SetText(FText::FromString(Step.Name));
	DetailText->SetText(FText::FromString(Step.Detail));

	// The track is as wide as the base payout: a figure is a distance from its left end.
	const auto X = [Base, this](const int32 Value)
	{
		return Base > 0 ? FMath::RoundToFloat(static_cast<float>(Value) / static_cast<float>(Base) * TrackWidth) : 0.f;
	};

	const float Left = X(FMath::Min(Step.From, Step.To));
	const float Right = X(FMath::Max(Step.From, Step.To));
	const float Width = Step.Kind == EDebriefStepKind::Nothing ? MinBarWidth : FMath::Max(MinBarWidth, Right - Left);
	const float MiddleY = BarTop + BarHeight * 0.5f;

	const FVector2D BarSize(Width, BarHeight);
	DebriefPlace(Bar, FVector2D(Left, BarTop), FVector2D(0.0, 0.0), &BarSize);

	const bool bTotal = Step.Kind == EDebriefStepKind::Total;
	const bool bPayout = bTotal && Step.Name != TEXT("Base payout");

	if (Style)
	{
		FLinearColor Fill = UUIStyle::Srgb(176, 100, 90);

		if (bTotal)
		{
			Fill = bPayout ? Style->Success : UUIStyle::Srgb(93, 107, 130);
		}
		else if (Step.Kind == EDebriefStepKind::Unfinished || Step.Kind == EDebriefStepKind::Upkeep)
		{
			Fill = UUIStyle::Srgb(122, 135, 156);
		}
		else if (bBiggest)
		{
			Fill = Style->Danger;
		}

		Bar->SetBrushColor(Fill);
		FigureText->SetColorAndOpacity(FSlateColor(bTotal ? Style->AccentText : Style->TextPrimary));
	}

	// A total prints its figure inside the bar, which is wide enough; a loss prints it to the left of the step, where there is room.
	if (bTotal)
	{
		FigureText->SetText(FText::AsNumber(Step.To));
		DebriefPlace(FigureText, FVector2D(Left + 8.f, MiddleY), FVector2D(0.0, 0.5), nullptr);
	}
	else
	{
		FigureText->SetText(Step.Kind == EDebriefStepKind::Nothing ? FText::FromString(FString(TEXT("0"))) : FText::FromString(FString(TEXT("-")) + FText::AsNumber(Step.Amount()).ToString()));
		DebriefPlace(FigureText, FVector2D(Left - 8.f, MiddleY), FVector2D(1.0, 0.5), nullptr);
	}

	if (Connector)
	{
		Connector->SetVisibility(bConnector ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		const FVector2D ConnectorSize(1.f, ConnectorHeight);
		DebriefPlace(Connector, FVector2D(X(Step.To), ConnectorTop), FVector2D(0.0, 0.0), &ConnectorSize);

		if (Style)
		{
			Connector->SetBrushColor(Style->SlotBorder);
		}
	}

	// A term that applies and cost nothing is shown, but quietly.
	SetRenderOpacity(Step.Kind == EDebriefStepKind::Nothing ? 0.6f : 1.f);
}

void UDebriefLineWidget::SetLine(const FNotebookLine& Line)
{
	SetPlain(Line.Name, Line.State, Line.Status);

	if (FigureText)
	{
		FigureText->SetText(FText::FromString(Line.Figure));
		FigureText->SetVisibility(Line.Figure.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (DamageBar)
	{
		DamageBar->SetPercent(FMath::Max(0.f, Line.Damage));
		DamageBar->SetVisibility(Line.Damage >= 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UDebriefLineWidget::SetPlain(const FString& Name, const FString& State, const ENotebookState Status)
{
	NameText->SetText(FText::FromString(Name));
	StateText->SetText(FText::FromString(State));

	if (Style)
	{
		StateDot->SetBrush(UUIStyle::MakeRounded(DebriefStateColor(*Style, Status), 5.f));
	}

	if (FigureText)
	{
		FigureText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (DamageBar)
	{
		DamageBar->SetVisibility(ESlateVisibility::Collapsed);
	}
}
