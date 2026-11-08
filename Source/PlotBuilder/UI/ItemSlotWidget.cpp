#include "ItemSlotWidget.h"
#include "InfoTooltipWidget.h"
#include "ItemLook.h"
#include "UIStyle.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"

void UItemSlotWidget::Setup(UUIStyle* InStyle, UHotbarComponent* InHotbar, UPaintBrushComponent* InBrush, int32 InIndex, float Size, bool bInInteractive)
{
	Style = InStyle;
	Hotbar = InHotbar;
	Brush = InBrush;
	SlotIndex = InIndex;
	bInteractive = bInInteractive;

	if (SizeRoot)
	{
		SizeRoot->SetWidthOverride(Size);
		SizeRoot->SetHeightOverride(Size);
	}

	// In the HUD the slot is only drawn: it must not catch the pointer.
	SetVisibility(bInteractive ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);

	if (Style)
	{
		CountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	Refresh();
}

void UItemSlotWidget::Refresh()
{
	if (!Hotbar || !Style)
	{
		return;
	}

	FInventorySlot Stack;

	bFilled = ResolveStack(Stack) && !Stack.IsEmpty();
	bSelected = SlotIndex == Hotbar->GetSelectedIndex() && SlotIndex < Hotbar->NumColumns;

	const ESlateVisibility ContentVisibility = bFilled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	Icon->SetVisibility(ContentVisibility);
	CountText->SetVisibility(ContentVisibility);

	if (CategoryBar)
	{
		CategoryBar->SetVisibility(ContentVisibility);
	}

	if (bFilled)
	{
		const FItemLook Look = FItemLook::FromSlot(Style, Hotbar, Stack, Brush);

		Icon->SetBrushFromTexture(Look.Icon);
		Icon->SetColorAndOpacity(Look.Color);
		CountText->SetText(FText::AsNumber(Stack.Count));

		if (CategoryBar)
		{
			CategoryBar->SetColorAndOpacity(Look.Color);
		}
	}

	UpdateFrame();
}

bool UItemSlotWidget::ResolveStack(FInventorySlot& OutStack) const
{
	const TArray<FInventorySlot>& Slots = Hotbar->GetSlots();

	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	OutStack = Slots[SlotIndex];
	return true;
}

void UItemSlotWidget::UpdateFrame()
{
	if (!Style)
	{
		return;
	}

	FLinearColor Outline = Style->SlotBorder;
	float OutlineWidth = 1.f;

	if (bSelected)
	{
		Outline = Style->Accent;
		OutlineWidth = 2.f;
	}
	else if (bHovered && bInteractive)
	{
		Outline = Style->TextMuted;
		OutlineWidth = 2.f;
	}

	Frame->SetBrush(UUIStyle::MakeRounded(Style->SlotBackground, 6.f, Outline, OutlineWidth));
}

FReply UItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bInteractive || !Hotbar)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Hotbar->OnSlotLeftClicked(SlotIndex);
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Hotbar->OnSlotRightClicked(SlotIndex);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UItemSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	bHovered = true;
	UpdateFrame();

	// Built on each hover so it always shows the stack as it is now; null (nothing to say) clears the previous card.
	if (bInteractive)
	{
		SetToolTip(BuildTooltip());
	}
}

void UItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bHovered = false;
	UpdateFrame();
}

UWidget* UItemSlotWidget::BuildTooltip()
{
	if (!bInteractive || !bFilled || !Hotbar || !Style || !TooltipClass)
	{
		return nullptr;
	}

	const TArray<FInventorySlot>& Slots = Hotbar->GetSlots();

	if (!Slots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	const FInventorySlot& Stack = Slots[SlotIndex];
	const FItemLook Look = FItemLook::FromSlot(Style, Hotbar, Stack, Brush);

	FText Body = FText::Format(NSLOCTEXT("Slot", "TooltipBody", "{0}  |  stack of {1}\nIn this slot  x{2}"),
		UUIStyle::GetCategoryLabel(Look.Category), FText::AsNumber(Stack.StackSize), FText::AsNumber(Stack.Count));

	if (const FShopItemRow* Row = Hotbar->FindRowForClass(Stack.ActorClass))
	{
		Body = FText::Format(NSLOCTEXT("Slot", "TooltipBodyPriced", "{0}\nShop price  {1}"), Body, FText::AsNumber(Row->Cost));
	}

	UInfoTooltipWidget* Tooltip = CreateWidget<UInfoTooltipWidget>(this, TooltipClass);

	if (Tooltip)
	{
		Tooltip->SetContent(Look.Name, Body);
	}

	return Tooltip;
}
