#include "ToolsTabWidget.h"
#include "PlayerMessages.h"
#include "ToolCardWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/ToolbeltComponent.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSubsystem.h"
#include "Progress/ToolShop.h"

namespace
{
	UHotbarComponent* ToolsTabFindHotbar(const UUserWidget& Widget)
	{
		const APawn* Pawn = Widget.GetOwningPlayerPawn();
		return Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;
	}

	const UToolbeltComponent* ToolsTabFindBelt(const UUserWidget& Widget)
	{
		const APawn* Pawn = Widget.GetOwningPlayerPawn();
		return Pawn ? Pawn->FindComponentByClass<UToolbeltComponent>() : nullptr;
	}
}

void UToolsTabWidget::Refresh()
{
	UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	const UHotbarComponent* Hotbar = ToolsTabFindHotbar(*this);

	if (!Offers || !CardClass || !Progress || !Hotbar)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UToolsTabWidget: needs a DT_ToolShop, a card class, the progress and the player's hotbar."));
		return;
	}

	const TArray<FName> RowNames = Offers->GetRowNames();

	// The cards are made once; a purchase only redraws them.
	if (Cards.Num() != RowNames.Num())
	{
		CardHost->ClearChildren();
		Cards.Reset();

		for (int32 Index = 0; Index < RowNames.Num(); ++Index)
		{
			UToolCardWidget* Card = CreateWidget<UToolCardWidget>(this, CardClass);

			if (!Card)
			{
				continue;
			}

			Card->OnBuyRequested.BindUObject(this, &UToolsTabWidget::HandleBuy);
			CardHost->AddChild(Card);

			// Side by side, with a gap between the cards and not before the first.
			if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Card->Slot))
			{
				BoxSlot->SetPadding(FMargin(Index == 0 ? 0.f : 14.f, 0.f, 0.f, 0.f));
			}

			Cards.Add(Card);
		}
	}

	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const FToolOfferRow* Row = Offers->FindRow<FToolOfferRow>(RowNames[Index], TEXT("UToolsTabWidget::Refresh"));

		if (!Row)
		{
			continue;
		}

		// What costs nothing (the hands) is always owned, even in a save older than the shop.
		const bool bOwned = Row->Price == 0 || Progress->OwnsTool(RowNames[Index]);

		// The row names are the tool ids; what a tool costs to keep is on its data asset, which the belt reaches through the tool class of its slot.
		const UToolbeltComponent* Belt = ToolsTabFindBelt(*this);
		const FToolUpkeep Upkeep = Belt ? Belt->GetUpkeepForTool(RowNames[Index]) : FToolUpkeep();

		Cards[Index]->SetOffer(RowNames[Index], *Row, bOwned, Hotbar->GetMoney(), Hotbar->IsInfiniteMoney(), Progress->GetCharges(), Upkeep);
	}
}

void UToolsTabWidget::HandleBuy(const FName ToolId, const int32 Quantity)
{
	UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	UHotbarComponent* Hotbar = ToolsTabFindHotbar(*this);
	const FToolOfferRow* Row = Offers ? Offers->FindRow<FToolOfferRow>(ToolId, TEXT("UToolsTabWidget::HandleBuy")) : nullptr;

	if (!Progress || !Hotbar || !Row)
	{
		return;
	}

	// Judged again here, not trusted from the card: the money may have moved since it was drawn.
	const bool bOwned = Row->Price == 0 || Progress->OwnsTool(ToolId);
	const int32 Stock = Progress->GetCharges();
	const FOfferStatus Status = ToolShop::Evaluate(*Row, bOwned, Hotbar->GetMoney(), Hotbar->IsInfiniteMoney(), Quantity, Stock);

	if (Status.State != EOfferState::CanBuy || !Hotbar->TrySpend(Status.Total))
	{
		return;
	}

	const int32 Bought = Row->bConsumable ? ToolShop::ClampQuantity(Quantity, ToolShop::Room(*Row, Stock)) : 1;

	// The only consumable is the explosive charge, kept as a stock in the progress.
	if (Row->bConsumable)
	{
		Progress->AddCharges(Bought);
	}
	else
	{
		Progress->GrantTool(ToolId);
	}

	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(Row->bConsumable
			? FText::Format(NSLOCTEXT("Tools", "BoughtMany", "Bought {0} x {1}."), FText::AsNumber(Bought), Row->DisplayName)
			: FText::Format(NSLOCTEXT("Tools", "Bought", "Bought: {0}."), Row->DisplayName), EMessageTone::Info);
	}

	Refresh();
	OnPurchased.Broadcast();
}
