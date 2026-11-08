#include "ToolTileWidget.h"
#include "InfoTooltipWidget.h"
#include "UIStyle.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Interaction/ToolbeltComponent.h"

void UToolTileWidget::Setup(UUIStyle* InStyle, UToolbeltComponent* InBelt, int32 InSlot)
{
	Style = InStyle;
	Belt = InBelt;
	ToolSlot = InSlot;

	Refresh();
}

void UToolTileWidget::Refresh()
{
	if (!Style || !Belt)
	{
		return;
	}

	const bool bMissingTile = ToolSlot == INDEX_NONE;
	const bool bEquipped = !bMissingTile && Belt->GetEquippedIndex() == ToolSlot;
	const bool bUsable = !bMissingTile && Belt->IsSlotAvailable(ToolSlot);

	FLinearColor Outline = Style->SlotBorder;
	float OutlineWidth = 1.f;
	FLinearColor IconColour = Style->TextPrimary;

	if (bMissingTile)
	{
		IconColour = Style->TextMuted;
	}
	else if (bEquipped)
	{
		Outline = Style->Accent;
		OutlineWidth = 2.f;
		IconColour = Style->Accent;
	}

	Frame->SetBrush(UUIStyle::MakeRounded(bMissingTile ? FLinearColor::Transparent : Style->SlotBackground, 8.f, Outline, OutlineWidth));
	Icon->SetBrushFromTexture(bMissingTile ? Style->LockIcon.Get() : Style->GetToolIcon(ToolSlot));
	Icon->SetColorAndOpacity(IconColour);

	// A tool the player owns but the map does not allow stays visible and fades back.
	SetRenderOpacity((bMissingTile || bUsable) ? 1.f : 0.4f);

	if (NameText)
	{
		NameText->SetText(bMissingTile
			? FText::Format(NSLOCTEXT("Belt", "MissingShort", "{0} missing"), FText::AsNumber(Belt->CountMissingTools()))
			: Belt->GetSlotName(ToolSlot));
		NameText->SetColorAndOpacity(FSlateColor(Style->TextPrimary));
	}

	if (StatusText)
	{
		FText Status = NSLOCTEXT("Belt", "Ready", "Ready");

		if (bMissingTile)
		{
			Status = NSLOCTEXT("Belt", "ToBuy", "Tools to buy");
		}
		else if (bEquipped)
		{
			Status = NSLOCTEXT("Belt", "Equipped", "Equipped");
		}
		else if (!bUsable)
		{
			Status = NSLOCTEXT("Belt", "NotHere", "Not usable here");
		}

		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(FSlateColor(Style->TextMuted));
	}
}

void UToolTileWidget::SetLabelVisible(bool bVisible)
{
	if (!LabelRoot)
	{
		return;
	}

	if (bVisible && Belt && ToolSlot != INDEX_NONE)
	{
		if (LabelNameText)
		{
			LabelNameText->SetText(Belt->GetSlotName(ToolSlot));
		}

		if (LabelHintText)
		{
			LabelHintText->SetText(Belt->GetSlotHint(ToolSlot));
			LabelHintText->SetColorAndOpacity(FSlateColor(Style ? Style->TextMuted : FLinearColor::Gray));
		}

		if (LabelNameText && Style)
		{
			LabelNameText->SetColorAndOpacity(FSlateColor(Style->TextPrimary));
		}
	}

	LabelRoot->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UToolTileWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	// Only the padlock tile has something to explain. The tooltip is rebuilt on each hover so the count is current.
	if (ToolSlot != INDEX_NONE || !Belt || !TooltipClass)
	{
		SetToolTip(nullptr);
		return;
	}

	if (UInfoTooltipWidget* Tooltip = CreateWidget<UInfoTooltipWidget>(this, TooltipClass))
	{
		Tooltip->SetContent(
			FText::Format(NSLOCTEXT("Belt", "MissingTitle", "{0} missing tools"), FText::AsNumber(Belt->CountMissingTools())),
			NSLOCTEXT("Belt", "MissingBody", "Buy them at the terminal in the hub."));
		SetToolTip(Tooltip);
	}
}
