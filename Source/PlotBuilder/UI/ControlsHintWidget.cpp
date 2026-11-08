#include "ControlsHintWidget.h"
#include "Components/Widget.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/ToolbeltComponent.h"

void UControlsHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Folded at the start, whatever the Blueprint says.
	SetOpen(false);
}

void UControlsHintWidget::SetVisibility(ESlateVisibility InVisibility)
{
	const bool bFold = InVisibility == ESlateVisibility::Collapsed || InVisibility == ESlateVisibility::Hidden;

	SetOpen(!bFold);

	Super::SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UControlsHintWidget::SetOpen(const bool bInOpen)
{
	bOpen = bInOpen;

	// SetVisibility can be called before the widgets of the Blueprint exist.
	if (!Panel || !Reminder)
	{
		return;
	}

	if (bOpen)
	{
		RefreshGroups();
	}

	Panel->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Reminder->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UControlsHintWidget::RefreshGroups()
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const UToolbeltComponent* Belt = Pawn ? Pawn->FindComponentByClass<UToolbeltComponent>() : nullptr;
	const UHotbarComponent* Hotbar = Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;

	// Without a pawn yet every group stays, so the card is never empty.
	if (ToolsGroup)
	{
		ToolsGroup->SetVisibility(Belt && Belt->IsLocked() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (BuildGroup)
	{
		BuildGroup->SetVisibility(Hotbar && !Hotbar->IsAllowed() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}
