#include "CartLineWidget.h"
#include "ItemLook.h"
#include "UIStyle.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Interaction/ShopCartComponent.h"

void UCartLineWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RemoveButton->OnClicked.AddDynamic(this, &UCartLineWidget::HandleRemove);
}

void UCartLineWidget::Setup(UUIStyle* InStyle, FName InRowName, const FShopItemRow& Row, int32 Quantity, UShopCartComponent* InCart, UPaintBrushComponent* InBrush)
{
	Cart = InCart;
	RowName = InRowName;

	const FItemLook Look = FItemLook::FromRow(InStyle, Row, InBrush);

	NameText->SetText(Row.DisplayName);
	DetailText->SetText(FText::Format(NSLOCTEXT("Shop", "CartLineDetail", "x{0}  |  {1} each"), FText::AsNumber(Quantity), FText::AsNumber(Row.Cost)));
	TotalText->SetText(FText::AsNumber(Row.Cost * Quantity));

	if (Icon)
	{
		Icon->SetBrushFromTexture(Look.Icon);
		Icon->SetColorAndOpacity(Look.Ink);
	}

	if (IconBackground)
	{
		IconBackground->SetColorAndOpacity(Look.ChipFill);
	}

	if (InStyle)
	{
		NameText->SetColorAndOpacity(FSlateColor(InStyle->TextPrimary));
		DetailText->SetColorAndOpacity(FSlateColor(InStyle->TextMuted));
		TotalText->SetColorAndOpacity(FSlateColor(InStyle->TextPrimary));
	}
}

void UCartLineWidget::HandleRemove()
{
	if (Cart)
	{
		Cart->SetQuantity(RowName, 0);
	}
}
