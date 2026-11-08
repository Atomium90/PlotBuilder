#include "ItemLook.h"
#include "UIStyle.h"
#include "Buildables/BuildablePiece.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"

FItemLook FItemLook::FromRow(const UUIStyle* Style, const FShopItemRow& Row, const UPaintBrushComponent* Brush)
{
	FItemLook Look;
	Look.Name = Row.DisplayName;
	Look.Category = Row.Category;

	if (!Style)
	{
		return Look;
	}

	Look.Color = Style->GetCategoryColor(Row.Category);
	Look.Icon = Style->GetCategoryIcon(Row.Category);
	Look.ChipFill = UUIStyle::WithAlpha(Look.Color, 0.2f);
	Look.Ink = Look.Color;

	FLinearColor PaintColor;
	FText PaintName;

	if (Row.Category == EShopItemCategory::Paint && Brush && Brush->GetBucketColor(Row.ActorClass, PaintColor, PaintName))
	{
		Look.Color = PaintColor;
		Look.ChipFill = PaintColor;
		Look.Ink = UUIStyle::InkFor(PaintColor);
	}

	return Look;
}

FItemLook FItemLook::FromSlot(const UUIStyle* Style, const UHotbarComponent* Hotbar, const FInventorySlot& Slot, const UPaintBrushComponent* Brush)
{
	if (const FShopItemRow* Row = Hotbar ? Hotbar->FindRowForClass(Slot.ActorClass) : nullptr)
	{
		return FromRow(Style, *Row, Brush);
	}

	// An item the shop does not sell (picked up in the world): a piece is structure, anything else is a prop.
	FShopItemRow Guess;
	Guess.DisplayName = Slot.ActorClass ? FText::FromString(Slot.ActorClass->GetName()) : FText::GetEmpty();
	Guess.ActorClass = Slot.ActorClass;
	Guess.Category = (Slot.ActorClass && Slot.ActorClass->IsChildOf(ABuildablePiece::StaticClass())) ? EShopItemCategory::Structure : EShopItemCategory::Prop;

	return FromRow(Style, Guess, Brush);
}
