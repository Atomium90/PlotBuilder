#include "ToolCardWidget.h"
#include "UIStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/WrapBoxSlot.h"
#include "ContractChipWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UToolCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuyButton->OnClicked.AddUniqueDynamic(this, &UToolCardWidget::HandleBuy);

	if (MinusButton)
	{
		MinusButton->OnClicked.AddUniqueDynamic(this, &UToolCardWidget::HandleMinus);
	}

	if (PlusButton)
	{
		PlusButton->OnClicked.AddUniqueDynamic(this, &UToolCardWidget::HandlePlus);
	}
}

void UToolCardWidget::SetOffer(const FName InToolId, const FToolOfferRow& InRow, const bool bInOwned, const int32 InMoney, const bool bInInfiniteMoney, const int32 InStock, const FToolUpkeep& InUpkeep)
{
	ToolId = InToolId;
	Row = InRow;
	Upkeep = InUpkeep;
	bOwned = bInOwned;
	Money = InMoney;
	bInfiniteMoney = bInInfiniteMoney;
	Stock = InStock;

	BuildChips();
	Redraw();
}

void UToolCardWidget::BuildChips()
{
	if (bChipsBuilt || !ChipHost || !ChipClass)
	{
		return;
	}

	bChipsBuilt = true;
	ChipHost->ClearChildren();

	for (const FText& Material : Row.GoodAgainst)
	{
		if (UContractChipWidget* Chip = CreateWidget<UContractChipWidget>(this, ChipClass))
		{
			Chip->SetChip(Material);
			ChipHost->AddChild(Chip);

			// Side by side with a small gap, a second line when there is no room.
			if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Chip->Slot))
			{
				WrapSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 4.f));
			}
		}
	}
}

void UToolCardWidget::Redraw()
{
	const FOfferStatus Status = ToolShop::Evaluate(Row, bOwned, Money, bInfiniteMoney, Quantity);
	const bool bIsOwned = Status.State == EOfferState::Owned;
	const bool bCanBuy = Status.State == EOfferState::CanBuy;

	NameText->SetText(Row.DisplayName);
	BlurbText->SetText(Row.Blurb);

	if (ClicksText)
	{
		ClicksText->SetText(Row.Clicks);
	}

	if (PriceRow)
	{
		PriceRow->SetVisibility(bIsOwned ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (Row.bConsumable)
	{
		PriceText->SetText(FText::Format(NSLOCTEXT("ToolCard", "PriceEach", "{0} each"), FText::AsNumber(Row.Price)));
	}
	else
	{
		PriceText->SetText(Row.Price > 0 ? FText::AsNumber(Row.Price) : NSLOCTEXT("ToolCard", "Included", "Included"));
	}

	if (UpkeepText)
	{
		UpkeepText->SetText(FText::Format(NSLOCTEXT("ToolCard", "Upkeep", "Upkeep: {0}"), FText::FromString(Upkeep.Describe())));
		UpkeepText->SetVisibility(Upkeep.IsFree() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (StockText)
	{
		StockText->SetText(FText::Format(NSLOCTEXT("ToolCard", "Stock", "In stock: {0}"), FText::AsNumber(Stock)));
		StockText->SetVisibility(Row.bConsumable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (StepperRow)
	{
		StepperRow->SetVisibility(Row.bConsumable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
	}

	if (OwnedBadge)
	{
		OwnedBadge->SetVisibility(bIsOwned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// The button: "On your belt" (quiet) for what is owned, the price when it can be bought, the shortfall when it cannot.
	BuyButton->SetIsEnabled(bCanBuy);

	FText Label;

	if (bIsOwned)
	{
		Label = NSLOCTEXT("ToolCard", "OnBelt", "On your belt");
	}
	else if (bCanBuy)
	{
		Label = Row.bConsumable
			? FText::Format(NSLOCTEXT("ToolCard", "BuyMany", "Buy {0} · {1}"), FText::AsNumber(Quantity), FText::AsNumber(Status.Total))
			: FText::Format(NSLOCTEXT("ToolCard", "Buy", "Buy {0}"), FText::AsNumber(Status.Total));
	}
	else
	{
		Label = FText::Format(NSLOCTEXT("ToolCard", "Short", "{0} short"), FText::AsNumber(Status.Missing));
	}

	BuyLabelText->SetText(Label);

	if (!Style)
	{
		return;
	}

	if (UTexture2D* Icon = Style->GetToolIcon(Row.BeltSlot))
	{
		IconImage->SetBrushFromTexture(Icon);
	}

	IconImage->SetColorAndOpacity(bIsOwned ? Style->Success : Style->TextPrimary);

	Frame->SetBrush(UUIStyle::MakeRounded(Style->PanelBackground, 14.f, bIsOwned ? Style->Success : Style->SlotBorder, bIsOwned ? 2.f : 1.f));

	BuyLabelText->SetColorAndOpacity(FSlateColor(bCanBuy ? Style->AccentText : Style->TextMuted));

	if (BuyFrame)
	{
		BuyFrame->SetBrush(UUIStyle::MakeRounded(bCanBuy ? Style->Accent : Style->RowActive, 10.f));
	}
}

void UToolCardWidget::HandleBuy()
{
	const FOfferStatus Status = ToolShop::Evaluate(Row, bOwned, Money, bInfiniteMoney, Quantity);

	if (Status.State != EOfferState::CanBuy)
	{
		return;
	}

	const int32 Asked = Row.bConsumable ? ToolShop::ClampQuantity(Quantity) : 1;

	// Back to one for the next time; the tab redraws the card from the new state.
	Quantity = 1;

	OnBuyRequested.ExecuteIfBound(ToolId, Asked);
}

void UToolCardWidget::HandleMinus()
{
	Quantity = ToolShop::ClampQuantity(Quantity - 1);
	Redraw();
}

void UToolCardWidget::HandlePlus()
{
	Quantity = ToolShop::ClampQuantity(Quantity + 1);
	Redraw();
}
