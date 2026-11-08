#include "HotbarWidget.h"
#include "ItemLook.h"
#include "ItemSlotWidget.h"
#include "UIStyle.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "UI/PlayerMessages.h"
#include "PlotBuilder.h"
#include "TimerManager.h"

void UHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UHotbarWidget::NativeDestruct()
{
	Unbind();

	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->ClearBottomLimit();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LabelTimer);
	}

	Super::NativeDestruct();
}

void UHotbarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBound)
	{
		TryBind();
	}

	PublishBottomLimit();
}

void UHotbarWidget::PublishBottomLimit()
{
	UPlayerMessages* Messages = UPlayerMessages::Get(this);

	if (!Messages)
	{
		return;
	}

	// The prompt and the messages sit just above the top of the hotbar (and the room kept for its name label).
	if (ContentRoot && ContentRoot->IsVisible())
	{
		FVector2D PixelPosition;
		FVector2D ViewportPosition;
		USlateBlueprintLibrary::AbsoluteToViewport(this, FVector2D(ContentRoot->GetCachedGeometry().GetAbsolutePosition()), PixelPosition, ViewportPosition);
		Messages->SetBottomLimit(static_cast<float>(PixelPosition.Y));
	}
	else
	{
		Messages->ClearBottomLimit();
	}
}

bool UHotbarWidget::TryBind()
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
		UE_LOG(LogPlotBuilder, Warning, TEXT("UHotbarWidget: style or slot class is not set, the hotbar is not drawn"));
		return false;
	}

	Hotbar->OnInventoryChanged.AddUniqueDynamic(this, &UHotbarWidget::HandleInventoryChanged);
	Hotbar->OnSelectedSlotChanged.AddUniqueDynamic(this, &UHotbarWidget::HandleSelectionChanged);
	Hotbar->OnAllowedChanged.AddUniqueDynamic(this, &UHotbarWidget::HandleAllowedChanged);

	if (Slots.IsEmpty())
	{
		BuildSlots();
	}

	bBound = true;

	HideLabel();
	RefreshSlots();
	RefreshVisibility();
	return true;
}

void UHotbarWidget::Unbind()
{
	if (Hotbar)
	{
		Hotbar->OnInventoryChanged.RemoveDynamic(this, &UHotbarWidget::HandleInventoryChanged);
		Hotbar->OnSelectedSlotChanged.RemoveDynamic(this, &UHotbarWidget::HandleSelectionChanged);
		Hotbar->OnAllowedChanged.RemoveDynamic(this, &UHotbarWidget::HandleAllowedChanged);
	}

	bBound = false;
}

void UHotbarWidget::BuildSlots()
{
	SlotRow->ClearChildren();

	// The hotbar is the first row of the grid: slots 0 to NumColumns - 1.
	for (int32 Index = 0; Index < Hotbar->NumColumns; ++Index)
	{
		if (UItemSlotWidget* SlotWidget = CreateWidget<UItemSlotWidget>(this, SlotClass))
		{
			SlotWidget->Setup(Style, Hotbar, Brush, Index, SlotSize, false);
			SlotRow->AddChild(SlotWidget);
			Slots.Add(SlotWidget);
		}
	}
}

void UHotbarWidget::RefreshSlots()
{
	for (UItemSlotWidget* SlotWidget : Slots)
	{
		SlotWidget->Refresh();
	}
}

void UHotbarWidget::RefreshVisibility()
{
	if (ContentRoot && Hotbar)
	{
		ContentRoot->SetVisibility(Hotbar->IsAllowed() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHotbarWidget::ShowLabel()
{
	if (!LabelRoot || !LabelText || !Hotbar || LabelSeconds <= 0.f)
	{
		return;
	}

	FInventorySlot Selected;

	// An empty slot has nothing to name.
	if (!Hotbar->GetSelectedEntry(Selected))
	{
		HideLabel();
		return;
	}

	LabelText->SetText(FItemLook::FromSlot(Style, Hotbar, Selected, Brush).Name);
	LabelRoot->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LabelTimer, FTimerDelegate::CreateUObject(this, &UHotbarWidget::HideLabel), LabelSeconds, false);
	}
}

void UHotbarWidget::HideLabel()
{
	// Hidden, not collapsed: the label keeps its room, so the top of the hotbar does not jump when a name shows up.
	if (LabelRoot)
	{
		LabelRoot->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UHotbarWidget::HandleInventoryChanged()
{
	RefreshSlots();
}

void UHotbarWidget::HandleSelectionChanged()
{
	RefreshSlots();
	ShowLabel();
}

void UHotbarWidget::HandleAllowedChanged()
{
	RefreshVisibility();
}
