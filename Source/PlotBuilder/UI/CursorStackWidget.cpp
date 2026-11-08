#include "CursorStackWidget.h"
#include "UIStyle.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"
#include "PlotBuilder.h"

void UCursorStackWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UCursorStackWidget::NativeDestruct()
{
	Unbind();

	Super::NativeDestruct();
}

void UCursorStackWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBound)
	{
		TryBind();
	}
}

bool UCursorStackWidget::ResolveStack(FInventorySlot& OutStack) const
{
	if (!CursorHotbar)
	{
		return false;
	}

	OutStack = CursorHotbar->GetCursorSlot();
	return true;
}

bool UCursorStackWidget::TryBind()
{
	if (bBound)
	{
		return true;
	}

	const APawn* Pawn = GetOwningPlayerPawn();
	CursorHotbar = Pawn ? Pawn->FindComponentByClass<UHotbarComponent>() : nullptr;

	if (!CursorHotbar)
	{
		return false;
	}

	if (!StackStyle)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UCursorStackWidget: the style is not set, the carried stack is not drawn"));
		return false;
	}

	Setup(StackStyle, CursorHotbar, Pawn->FindComponentByClass<UPaintBrushComponent>(), INDEX_NONE, StackSize, false);

	CursorHotbar->OnCursorSlotChanged.AddUniqueDynamic(this, &UCursorStackWidget::HandleCursorChanged);
	bBound = true;

	Show();
	return true;
}

void UCursorStackWidget::Unbind()
{
	if (CursorHotbar)
	{
		CursorHotbar->OnCursorSlotChanged.RemoveDynamic(this, &UCursorStackWidget::HandleCursorChanged);
	}

	bBound = false;
}

void UCursorStackWidget::Show()
{
	Refresh();

	// Never hit-testable: the stack follows the pointer, so it would sit under every click.
	const bool bCarrying = CursorHotbar && !CursorHotbar->GetCursorSlot().IsEmpty();
	SetVisibility(bCarrying ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UCursorStackWidget::HandleCursorChanged()
{
	Show();
}
