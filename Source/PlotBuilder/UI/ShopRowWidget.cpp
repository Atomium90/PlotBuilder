#include "ShopRowWidget.h"
#include "ItemLook.h"
#include "UIStyle.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"
#include "Interaction/ShopCartComponent.h"

void UShopRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	MinusButton->OnClicked.AddDynamic(this, &UShopRowWidget::HandleMinus);
	PlusButton->OnClicked.AddDynamic(this, &UShopRowWidget::HandlePlus);
}

void UShopRowWidget::Setup(UUIStyle* InStyle, FName InRowName, const FShopItemRow& InRow, UShopCartComponent* InCart, UHotbarComponent* InHotbar, UPaintBrushComponent* InBrush)
{
	Style = InStyle;
	RowName = InRowName;
	Row = InRow;
	Cart = InCart;
	Hotbar = InHotbar;

	const FItemLook Look = FItemLook::FromRow(Style, Row, InBrush);

	NameText->SetText(Row.DisplayName);
	PriceText->SetText(FText::AsNumber(Row.Cost));

	if (DetailText)
	{
		DetailText->SetText(FText::Format(NSLOCTEXT("Shop", "RowDetail", "{0}  |  stack of {1}"),
			UUIStyle::GetCategoryLabel(Row.Category), FText::AsNumber(Row.StackSize)));
	}

	if (Icon)
	{
		Icon->SetBrushFromTexture(Look.Icon);
		Icon->SetColorAndOpacity(Look.Ink);
	}

	if (IconBackground)
	{
		IconBackground->SetColorAndOpacity(Look.ChipFill);
	}

	if (Style)
	{
		NameText->SetColorAndOpacity(FSlateColor(Style->TextPrimary));
		PriceText->SetColorAndOpacity(FSlateColor(Style->TextPrimary));

		if (DetailText)
		{
			DetailText->SetColorAndOpacity(FSlateColor(Style->TextMuted));
		}

		if (OwnedText)
		{
			OwnedText->SetColorAndOpacity(FSlateColor(Style->TextMuted));
		}
	}

	RefreshOwned();
	RefreshCart();
}

void UShopRowWidget::RefreshOwned()
{
	if (!OwnedText)
	{
		return;
	}

	const int32 Owned = Hotbar ? Hotbar->CountOf(Row.ActorClass) : 0;
	OwnedText->SetText(Owned > 0 ? FText::Format(NSLOCTEXT("Shop", "Owned", "x{0}"), FText::AsNumber(Owned)) : FText::GetEmpty());
}

void UShopRowWidget::RefreshCart()
{
	const int32 Quantity = Cart ? Cart->GetQuantity(RowName) : 0;
	bInCart = Quantity > 0;

	QuantityText->SetText(FText::AsNumber(Quantity));
	MinusButton->SetIsEnabled(bInCart);

	// No + once the player holds all the item allows (a paint bucket): nothing more can be bought.
	PlusButton->SetIsEnabled(!Hotbar || Quantity < Hotbar->GetPurchaseLimit(Row));

	if (Style)
	{
		QuantityText->SetColorAndOpacity(FSlateColor(bInCart ? Style->Accent : Style->TextMuted));
	}

	UpdateBackground();
}

void UShopRowWidget::UpdateBackground()
{
	if (!Background || !Style)
	{
		return;
	}

	FLinearColor Tint = FLinearColor::Transparent;

	if (bInCart)
	{
		Tint = UUIStyle::WithAlpha(Style->Accent, 0.1f);
	}
	else if (bHovered)
	{
		Tint = Style->RowHover;
	}

	Background->SetColorAndOpacity(Tint);
}

void UShopRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	bHovered = true;
	UpdateBackground();
}

void UShopRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bHovered = false;
	UpdateBackground();
}

void UShopRowWidget::HandleMinus()
{
	if (Cart)
	{
		Cart->StepQuantity(RowName, -1);
	}
}

void UShopRowWidget::HandlePlus()
{
	if (Cart)
	{
		Cart->StepQuantity(RowName, 1);
	}
}
