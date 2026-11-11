#include "GameHUDWidget.h"
#include "ControlsHintWidget.h"
#include "CursorStackWidget.h"
#include "DebriefWidget.h"
#include "GameScreenWidget.h"
#include "InventoryWidget.h"
#include "MissionNotebookWidget.h"
#include "PCTerminalWidget.h"
#include "ShopWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Contracts/ClientRow.h"
#include "Contracts/ContractSubsystem.h"
#include "FocusCardWidget.h"
#include "Flow/GameFlowSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/HotbarComponent.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSubsystem.h"

const FName UGameHUDWidget::ShopMenuName(TEXT("Shop"));
const FName UGameHUDWidget::InventoryMenuName(TEXT("Inventory"));
const FName UGameHUDWidget::TerminalMenuName(TEXT("Terminal"));
const FName UGameHUDWidget::DebriefMenuName(TEXT("Debrief"));
const FName UGameHUDWidget::NotebookMenuName(TEXT("Notebook"));
const FName UGameHUDWidget::LeaveMenuName(TEXT("Leave"));

UGameHUDWidget* UGameHUDWidget::Find(const UObject* WorldContext)
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContext, Widgets, UGameHUDWidget::StaticClass(), false);

	return Widgets.IsEmpty() ? nullptr : Cast<UGameHUDWidget>(Widgets[0]);
}

void UGameHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Every screen starts closed, whatever the Blueprint left visible.
	ShopMenu->SetVisibility(ESlateVisibility::Collapsed);
	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);

	ShopMenu->OnCloseRequested.AddUniqueDynamic(this, &UGameHUDWidget::HandleCloseRequested);
	InventoryMenu->OnCloseRequested.AddUniqueDynamic(this, &UGameHUDWidget::HandleCloseRequested);

	// The screens of the contract loop.
	for (UGameScreenWidget* Screen : {static_cast<UGameScreenWidget*>(TerminalMenu), static_cast<UGameScreenWidget*>(DebriefMenu), static_cast<UGameScreenWidget*>(NotebookMenu), static_cast<UGameScreenWidget*>(LeaveMenu)})
	{
		if (Screen)
		{
			Screen->SetVisibility(ESlateVisibility::Collapsed);
			Screen->OnCloseRequested.AddUniqueDynamic(this, &UGameHUDWidget::HandleCloseRequested);
		}
	}

	// The stack is placed by its top left corner by the Blueprint; it is centred on the pointer here, so it does not depend on its size.
	if (UCanvasPanelSlot* StackSlot = Cast<UCanvasPanelSlot>(CursorStackWidget->Slot))
	{
		StackSlot->SetAnchors(FAnchors(0.f, 0.f));
		StackSlot->SetAlignment(FVector2D(0.5, 0.5));
		StackSlot->SetAutoSize(true);
	}
}

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentMenu = NAME_None;
	TryBind();

	// The card of what the crosshair looks at lives over the game, beside this HUD, not inside it.
	if (!FocusCard && FocusCardClass)
	{
		FocusCard = CreateWidget<UFocusCardWidget>(GetOwningPlayer(), FocusCardClass);

		if (FocusCard)
		{
			FocusCard->AddToViewport(FocusCardZOrder);
		}
	}
}

void UGameHUDWidget::NativeDestruct()
{
	Unbind();

	if (FocusCard)
	{
		FocusCard->RemoveFromParent();
		FocusCard = nullptr;
	}

	Super::NativeDestruct();
}

void UGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The pawn may not exist yet when the HUD is built.
	if (!bBound)
	{
		TryBind();
	}

	if (IsMenuOpen())
	{
		FollowPointer();
	}

	// A screen over the game: the card steps aside.
	if (FocusCard)
	{
		FocusCard->SetSuppressed(IsMenuOpen());
	}
}

bool UGameHUDWidget::TryBind()
{
	if (bBound)
	{
		return true;
	}

	Hotbar = FindHotbar();

	if (!Hotbar)
	{
		return false;
	}

	Hotbar->OnMoneyChanged.AddUniqueDynamic(this, &UGameHUDWidget::HandleMoneyChanged);
	bBound = true;

	UpdateMoney();
	return true;
}

void UGameHUDWidget::Unbind()
{
	if (Hotbar)
	{
		Hotbar->OnMoneyChanged.RemoveDynamic(this, &UGameHUDWidget::HandleMoneyChanged);
	}

	bBound = false;
}

UHotbarComponent* UGameHUDWidget::FindHotbar() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;
}

void UGameHUDWidget::HandleMoneyChanged()
{
	UpdateMoney();
}

void UGameHUDWidget::UpdateMoney()
{
	const UHotbarComponent* Source = Hotbar ? Hotbar.Get() : FindHotbar();

	if (!Source || !MoneyText)
	{
		return;
	}

	MoneyText->SetText(Source->IsInfiniteMoney() ? FText::FromString(FString(TEXT("∞"))) : FText::AsNumber(Source->GetMoney()));
}

UUserWidget* UGameHUDWidget::FindScreen(FName Menu) const
{
	if (Menu == ShopMenuName)
	{
		return ShopMenu;
	}

	if (Menu == InventoryMenuName)
	{
		return InventoryMenu;
	}

	if (Menu == TerminalMenuName)
	{
		return TerminalMenu;
	}

	if (Menu == DebriefMenuName)
	{
		return DebriefMenu;
	}

	if (Menu == NotebookMenuName)
	{
		return NotebookMenu;
	}

	if (Menu == LeaveMenuName)
	{
		return LeaveMenu;
	}

	return nullptr;
}

bool UGameHUDWidget::AskLeave(const FNotebookData& Data, const FSimpleDelegate& OnLeave)
{
	if (!LeaveMenu)
	{
		return false;
	}

	LeaveMenu->Show(Data, OnLeave);
	OpenMenu(LeaveMenuName);

	return true;
}

bool UGameHUDWidget::ShowDebrief(const bool bOnlyIfUnseen)
{
	UContractSubsystem* Contracts = UContractSubsystem::Get(this);

	if (!DebriefMenu || !Contracts)
	{
		return false;
	}

	FDebriefResult Result;

	if (bOnlyIfUnseen)
	{
		if (!Contracts->ConsumeUnseenResult(Result))
		{
			return false;
		}
	}
	else if (Contracts->HasLastResult())
	{
		Result = Contracts->GetLastResult();
	}
	else
	{
		return false;
	}

	const UHotbarComponent* Source = FindHotbar();
	const FNotebookData& Notebook = Contracts->GetLastNotebook();

	// The account is the progress, which the payment was credited to; the hotbar only holds a copy of it for the map.
	const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	const int32 Balance = Progress ? Progress->GetMoney() : (Source ? Source->GetMoney() : 0);

	DebriefMenu->ShowReport(Result, Notebook, Contracts->FindClient(Notebook.Contract.ClientId), Contracts->GetLastPaid(), Balance, Source && Source->IsInfiniteMoney());
	OpenMenu(DebriefMenuName);

	return true;
}

void UGameHUDWidget::ToggleMenu(FName Menu)
{
	if (CurrentMenu == Menu)
	{
		CloseMenu();
		return;
	}

	// The notebook is the mission's: outside a mission there is nothing to write in it.
	if (Menu == NotebookMenuName)
	{
		const UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);

		if (!Flow || Flow->GetState() != EGameFlowState::OnSite)
		{
			return;
		}
	}

	// The shop and the bag belong to building: a map that has switched it off (the hub, a mission) has neither.
	if (Menu == ShopMenuName || Menu == InventoryMenuName)
	{
		const UHotbarComponent* Source = FindHotbar();

		if (Source && !Source->IsAllowed())
		{
			return;
		}
	}

	OpenMenu(Menu);
}

void UGameHUDWidget::OpenMenu(FName Menu)
{
	UUserWidget* Screen = FindScreen(Menu);
	APlayerController* Controller = GetOwningPlayer();

	if (!Screen || !Controller)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UGameHUDWidget::OpenMenu: no screen named '%s' (or no player controller), nothing opened."), *Menu.ToString());
		return;
	}

	// One screen at a time.
	if (!CurrentMenu.IsNone() && CurrentMenu != Menu)
	{
		CloseMenu();
	}

	if (CurrentMenu == Menu)
	{
		return;
	}

	CurrentMenu = Menu;
	Screen->SetVisibility(ESlateVisibility::Visible);

	if (UGameScreenWidget* GameScreen = Cast<UGameScreenWidget>(Screen))
	{
		GameScreen->OnScreenOpened();
	}

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Controller, Screen, EMouseLockMode::DoNotLock);
	Controller->bShowMouseCursor = true;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);
}

void UGameHUDWidget::CloseMenu()
{
	if (CurrentMenu.IsNone())
	{
		return;
	}

	UUserWidget* Screen = FindScreen(CurrentMenu);
	CurrentMenu = NAME_None;

	if (Screen)
	{
		if (UGameScreenWidget* GameScreen = Cast<UGameScreenWidget>(Screen))
		{
			GameScreen->OnScreenClosed();
		}

		Screen->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Whatever the cursor still carries goes back to the bag, so a stack is never lost with the menu.
	if (UHotbarComponent* Source = FindHotbar())
	{
		Source->ReturnCursorSlot();
	}

	if (APlayerController* Controller = GetOwningPlayer())
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(Controller);
		Controller->bShowMouseCursor = false;
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}
}

void UGameHUDWidget::HandleCloseRequested()
{
	CloseMenu();
}

void UGameHUDWidget::ToggleHelp()
{
	if (HelpWidget)
	{
		HelpWidget->SetOpen(!HelpWidget->IsOpen());
	}
}

void UGameHUDWidget::FollowPointer() const
{
	if (!CursorStackWidget || CursorStackWidget->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	if (UCanvasPanelSlot* StackSlot = Cast<UCanvasPanelSlot>(CursorStackWidget->Slot))
	{
		StackSlot->SetPosition(UWidgetLayoutLibrary::GetMousePositionOnViewport(this));
	}
}
