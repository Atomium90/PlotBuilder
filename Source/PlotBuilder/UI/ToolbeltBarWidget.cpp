#include "ToolbeltBarWidget.h"
#include "ToolTileWidget.h"
#include "UIStyle.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interaction/ToolbeltComponent.h"
#include "PlotBuilder.h"
#include "TimerManager.h"

void UToolbeltBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UToolbeltBarWidget::NativeDestruct()
{
	Unbind();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LabelTimer);
	}

	Super::NativeDestruct();
}

void UToolbeltBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The pawn may not exist yet when the HUD is built.
	if (!bBound)
	{
		TryBind();
	}
}

bool UToolbeltBarWidget::TryBind()
{
	if (bBound)
	{
		return true;
	}

	const APawn* Pawn = GetOwningPlayerPawn();
	Belt = Pawn ? Pawn->FindComponentByClass<UToolbeltComponent>() : nullptr;

	if (!Belt)
	{
		return false;
	}

	if (!Style || !TileClass)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UToolbeltBarWidget: style or tile class is not set, the tool belt is not drawn"));
		return false;
	}

	Belt->OnBeltChanged.AddUniqueDynamic(this, &UToolbeltBarWidget::HandleBeltChanged);

	if (Tiles.IsEmpty())
	{
		BuildTiles();
	}

	LastEquipped = Belt->GetEquippedIndex();
	bBound = true;

	HideLabel();
	RefreshTiles();
	return true;
}

void UToolbeltBarWidget::Unbind()
{
	if (Belt)
	{
		Belt->OnBeltChanged.RemoveDynamic(this, &UToolbeltBarWidget::HandleBeltChanged);
	}

	bBound = false;
}

void UToolbeltBarWidget::BuildTiles()
{
	TileHost->ClearChildren();

	// In a row (the bag drawer) the tiles share the whole width; in a column (the HUD) they keep their own size.
	auto AddTile = [this](int32 ToolSlot)
	{
		UToolTileWidget* Tile = CreateWidget<UToolTileWidget>(this, TileClass);

		if (!Tile)
		{
			return;
		}

		Tile->Setup(Style, Belt, ToolSlot);

		if (UHorizontalBoxSlot* RowSlot = Cast<UHorizontalBoxSlot>(TileHost->AddChild(Tile)))
		{
			RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		Tiles.Add(Tile);
	};

	// One tile for every tool the player owns, in belt order.
	for (int32 ToolSlot = 0; ToolSlot < Belt->NumSlots; ++ToolSlot)
	{
		if (Belt->IsSlotOwned(ToolSlot))
		{
			AddTile(ToolSlot);
		}
	}

	// The padlock: only when something is missing, so a player who owns everything does not see it.
	if (Belt->CountMissingTools() > 0)
	{
		AddTile(INDEX_NONE);
	}
}

void UToolbeltBarWidget::RefreshTiles()
{
	for (UToolTileWidget* Tile : Tiles)
	{
		Tile->Refresh();
	}
}

void UToolbeltBarWidget::HandleBeltChanged()
{
	RefreshTiles();

	// The label is for a switch, not for a map turning a switch on or off.
	const int32 Equipped = Belt ? Belt->GetEquippedIndex() : INDEX_NONE;

	if (Equipped != LastEquipped)
	{
		LastEquipped = Equipped;
		ShowLabel();
	}
}

void UToolbeltBarWidget::ShowLabel()
{
	if (!Belt || LabelSeconds <= 0.f)
	{
		return;
	}

	// The chip is on the tile of the tool in hand, so it sits right next to it whatever the layout.
	for (UToolTileWidget* Tile : Tiles)
	{
		Tile->SetLabelVisible(Tile->GetToolSlot() == LastEquipped);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LabelTimer, FTimerDelegate::CreateUObject(this, &UToolbeltBarWidget::HideLabel), LabelSeconds, false);
	}
}

void UToolbeltBarWidget::HideLabel()
{
	for (UToolTileWidget* Tile : Tiles)
	{
		Tile->SetLabelVisible(false);
	}
}
