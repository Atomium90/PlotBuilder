#include "InventoryWidget.h"
#include "ItemSlotWidget.h"
#include "UIStyle.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "GameFramework/Pawn.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"
#include "PlotBuilder.h"

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UInventoryWidget::NativeDestruct()
{
	Unbind();

	Super::NativeDestruct();
}

void UInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBound)
	{
		TryBind();
	}
}

bool UInventoryWidget::TryBind()
{
	if (bBound)
	{
		return true;
	}

	const APawn* Pawn = GetOwningPlayerPawn();
	Hotbar = Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;
	Brush = Pawn ? Pawn->FindComponentByClass<UPaintBrushComponent>() : nullptr;

	if (!Hotbar)
	{
		return false;
	}

	if (!Style || !SlotClass)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UInventoryWidget: style or slot class is not set, the inventory is not drawn"));
		return false;
	}

	Hotbar->OnInventoryChanged.AddUniqueDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	Hotbar->OnSelectedSlotChanged.AddUniqueDynamic(this, &UInventoryWidget::HandleInventoryChanged);
	Hotbar->OnMoneyChanged.AddUniqueDynamic(this, &UInventoryWidget::HandleMoneyChanged);

	if (Slots.IsEmpty())
	{
		BuildSlots();
	}

	bBound = true;

	RefreshSlots();
	RefreshInfo();
	return true;
}

void UInventoryWidget::Unbind()
{
	if (Hotbar)
	{
		Hotbar->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
		Hotbar->OnSelectedSlotChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventoryChanged);
		Hotbar->OnMoneyChanged.RemoveDynamic(this, &UInventoryWidget::HandleMoneyChanged);
	}

	bBound = false;
}

void UInventoryWidget::BuildSlots()
{
	HotbarGrid->ClearChildren();
	BackpackGrid->ClearChildren();

	const int32 Columns = FMath::Max(1, Hotbar->NumColumns);
	const int32 Total = Hotbar->GetTotalSlotCount();

	for (int32 Index = 0; Index < Total; ++Index)
	{
		UItemSlotWidget* SlotWidget = CreateWidget<UItemSlotWidget>(this, SlotClass);

		if (!SlotWidget)
		{
			continue;
		}

		SlotWidget->Setup(Style, Hotbar, Brush, Index, SlotSize, true);

		const int32 Row = Index / Columns;
		const int32 Column = Index % Columns;

		// Row 0 is the hotbar, the rest is the backpack.
		if (Row == 0)
		{
			HotbarGrid->AddChildToUniformGrid(SlotWidget, 0, Column);
		}
		else
		{
			BackpackGrid->AddChildToUniformGrid(SlotWidget, Row - 1, Column);
		}

		Slots.Add(SlotWidget);
	}
}

void UInventoryWidget::RefreshSlots()
{
	for (UItemSlotWidget* SlotWidget : Slots)
	{
		SlotWidget->Refresh();
	}
}

void UInventoryWidget::RefreshInfo()
{
	if (!Hotbar)
	{
		return;
	}

	if (BalanceText)
	{
		BalanceText->SetText(Hotbar->IsInfiniteMoney() ? FText::FromString(FString(TEXT("∞"))) : FText::AsNumber(Hotbar->GetMoney()));
	}

	if (BackpackUsedText)
	{
		const int32 BackpackTotal = FMath::Max(0, Hotbar->GetTotalSlotCount() - Hotbar->NumColumns);
		int32 BackpackUsed = 0;

		for (int32 Index = Hotbar->NumColumns; Index < Hotbar->GetSlots().Num(); ++Index)
		{
			BackpackUsed += Hotbar->GetSlots()[Index].IsEmpty() ? 0 : 1;
		}

		BackpackUsedText->SetText(FText::Format(NSLOCTEXT("Inventory", "BackpackUsed", "{0} of {1} slots used"),
			FText::AsNumber(BackpackUsed), FText::AsNumber(BackpackTotal)));
	}
}

FReply UInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape || Key == EKeys::I)
	{
		// The key that opened the bag may still be held: its auto-repeat must not close it again (see UShopWidget).
		if (!InKeyEvent.IsRepeat())
		{
			OnCloseRequested.Broadcast();
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UInventoryWidget::HandleInventoryChanged()
{
	RefreshSlots();
	RefreshInfo();
}

void UInventoryWidget::HandleMoneyChanged()
{
	RefreshInfo();
}
