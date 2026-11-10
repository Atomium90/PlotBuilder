#include "PCTerminalWidget.h"
#include "MissionBoardWidget.h"
#include "ParcelTabWidget.h"
#include "ToolsTabWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HotbarComponent.h"

void UPCTerminalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ContractsTab->OnClicked.AddUniqueDynamic(this, &UPCTerminalWidget::HandleContractsTab);
	ToolsTab->OnClicked.AddUniqueDynamic(this, &UPCTerminalWidget::HandleToolsTab);
	ParcelTab->OnClicked.AddUniqueDynamic(this, &UPCTerminalWidget::HandleParcelTab);

	// A purchase changes the money: the balance in the tab bar follows.
	if (ToolsPage)
	{
		ToolsPage->OnPurchased.AddUObject(this, &UPCTerminalWidget::RefreshBalance);
	}

	if (ParcelPage)
	{
		ParcelPage->OnPurchased.AddUObject(this, &UPCTerminalWidget::RefreshBalance);
	}
}

void UPCTerminalWidget::OnScreenOpened()
{
	Super::OnScreenOpened();

	// Always the Contracts tab, with the board as it is now.
	SelectTab(0);
	MissionBoard->Refresh();
	RefreshBalance();
}

void UPCTerminalWidget::SelectTab(const int32 Index)
{
	TabSwitcher->SetActiveWidgetIndex(Index);

	const auto Mark = [Index](UWidget* Widget, const int32 Own)
	{
		if (Widget)
		{
			Widget->SetVisibility(Index == Own ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	Mark(ContractsMark, 0);
	Mark(ToolsMark, 1);
	Mark(ParcelMark, 2);

	// A tab shows the state of the game as it is when it is picked.
	if (Index == 1 && ToolsPage)
	{
		ToolsPage->Refresh();
	}
	else if (Index == 2 && ParcelPage)
	{
		ParcelPage->Refresh();
	}
}

void UPCTerminalWidget::RefreshBalance()
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const UHotbarComponent* Hotbar = Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;

	if (Hotbar)
	{
		BalanceText->SetText(Hotbar->IsInfiniteMoney() ? FText::FromString(FString(TEXT("∞"))) : FText::AsNumber(Hotbar->GetMoney()));
	}
}

void UPCTerminalWidget::HandleContractsTab()
{
	SelectTab(0);
}

void UPCTerminalWidget::HandleToolsTab()
{
	SelectTab(1);
}

void UPCTerminalWidget::HandleParcelTab()
{
	SelectTab(2);
}
