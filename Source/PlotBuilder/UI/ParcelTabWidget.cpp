#include "ParcelTabWidget.h"
#include "PlayerMessages.h"
#include "UIStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HotbarComponent.h"
#include "Progress/ProgressSubsystem.h"
#include "Progress/ToolShop.h"

namespace
{
	UHotbarComponent* FindHotbar(const UUserWidget& Widget)
	{
		const APawn* Pawn = Widget.GetOwningPlayerPawn();
		return Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;
	}
}

void UParcelTabWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuyButton->OnClicked.AddUniqueDynamic(this, &UParcelTabWidget::HandleBuy);
}

void UParcelTabWidget::Refresh()
{
	const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	const UHotbarComponent* Hotbar = FindHotbar(*this);

	if (!Progress || !Hotbar)
	{
		return;
	}

	const int32 Money = Hotbar->GetMoney();
	const bool bInfinite = Hotbar->IsInfiniteMoney();
	const FParcelStatus Status = ToolShop::EvaluateParcel(Price, Progress->IsParcelOwned(), Money, bInfinite);

	PriceText->SetText(FText::AsNumber(Price));
	SavingBar->SetPercent(Status.Fraction);

	SavingText->SetText(Status.State == EParcelState::Saving
		? FText::Format(NSLOCTEXT("Parcel", "Saving", "{0} to go"), FText::AsNumber(Status.Missing))
		: NSLOCTEXT("Parcel", "CanAfford", "You can afford it"));

	if (ForSaleBlock)
	{
		ForSaleBlock->SetVisibility(Status.State == EParcelState::Owned ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (OwnedBlock)
	{
		OwnedBlock->SetVisibility(Status.State == EParcelState::Owned ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	FText Label;

	switch (Status.State)
	{
	case EParcelState::Owned:
		Label = NSLOCTEXT("Parcel", "Owned", "Owned");
		break;
	case EParcelState::CanBuy:
		Label = NSLOCTEXT("Parcel", "Buy", "Buy the parcel");
		break;
	default:
		Label = FText::Format(NSLOCTEXT("Parcel", "ToGo", "{0} to go"), FText::AsNumber(Status.Missing));
		break;
	}

	BuyLabelText->SetText(Label);
	BuyButton->SetIsEnabled(Status.State == EParcelState::CanBuy);

	if (!Style)
	{
		return;
	}

	const FLinearColor Fill = Status.State == EParcelState::Owned ? Style->Success : (Status.State == EParcelState::CanBuy ? Style->Accent : Style->RowActive);
	BuyLabelText->SetColorAndOpacity(FSlateColor(Status.State == EParcelState::Saving ? Style->TextMuted : Style->AccentText));

	if (BuyFrame)
	{
		BuyFrame->SetBrush(UUIStyle::MakeRounded(Fill, 10.f));
	}
}

void UParcelTabWidget::HandleBuy()
{
	UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	UHotbarComponent* Hotbar = FindHotbar(*this);

	if (!Progress || !Hotbar)
	{
		return;
	}

	// Judged again here, not trusted from the last drawing.
	const FParcelStatus Status = ToolShop::EvaluateParcel(Price, Progress->IsParcelOwned(), Hotbar->GetMoney(), Hotbar->IsInfiniteMoney());

	if (Status.State != EParcelState::CanBuy || !Hotbar->TrySpend(Price))
	{
		return;
	}

	Progress->SetParcelOwned(true);

	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(NSLOCTEXT("Parcel", "Bought", "You bought the parcel. The access point is open in the hub."), EMessageTone::Info);
	}

	Refresh();
	OnPurchased.Broadcast();
}
