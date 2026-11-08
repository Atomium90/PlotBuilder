#include "ShopCategoryButton.h"
#include "UIStyle.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UShopCategoryButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button->OnClicked.AddDynamic(this, &UShopCategoryButton::HandleClicked);
}

void UShopCategoryButton::Setup(UUIStyle* InStyle, int32 InFilterIndex, bool bInAll, EShopItemCategory InCategory, int32 ItemCount)
{
	Style = InStyle;
	FilterIndex = InFilterIndex;

	LabelText->SetText(bInAll ? NSLOCTEXT("Shop", "AllItems", "All items") : UUIStyle::GetCategoryLabel(InCategory));

	if (CountText)
	{
		CountText->SetText(FText::AsNumber(ItemCount));
	}

	if (!Style)
	{
		return;
	}

	// The "all" entry has no colour of its own: it reads like plain text, with the structure icon.
	const FLinearColor Colour = bInAll ? Style->TextPrimary : Style->GetCategoryColor(InCategory);
	UTexture2D* IconTexture = Style->GetCategoryIcon(bInAll ? EShopItemCategory::Structure : InCategory);

	if (Icon)
	{
		Icon->SetBrushFromTexture(IconTexture);
		Icon->SetColorAndOpacity(Colour);
	}

	if (IconBackground)
	{
		IconBackground->SetColorAndOpacity(UUIStyle::WithAlpha(Colour, 0.18f));
	}

	LabelText->SetColorAndOpacity(FSlateColor(Style->TextPrimary));

	if (CountText)
	{
		CountText->SetColorAndOpacity(FSlateColor(Style->TextMuted));
	}

	SetActive(false);
}

void UShopCategoryButton::SetActive(bool bActive)
{
	if (!Style)
	{
		return;
	}

	if (ActiveBar)
	{
		ActiveBar->SetColorAndOpacity(bActive ? Style->Accent : FLinearColor::Transparent);
	}

	if (Background)
	{
		Background->SetColorAndOpacity(bActive ? Style->RowActive : FLinearColor::Transparent);
	}
}

void UShopCategoryButton::HandleClicked()
{
	OnPicked.Broadcast(FilterIndex);
}
