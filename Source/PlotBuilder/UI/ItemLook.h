#pragma once

#include "CoreMinimal.h"
#include "Interaction/ShopItemRow.h"

class UUIStyle;
class UTexture2D;
class UHotbarComponent;
class UPaintBrushComponent;
struct FInventorySlot;

/**
 *  How one item is drawn, wherever it shows up (shop row, cart line, inventory slot): its name, its category, the colour
 *  it wears, the chip behind its icon. One place, so the shop and the bag always agree. A paint bucket wears the colour
 *  of its paint when the plot has a palette, and the Paint category colour otherwise.
 */
struct FItemLook
{
	FText Name;
	EShopItemCategory Category = EShopItemCategory::Structure;

	/** The colour of the item: its category colour, or its paint. Used for thin bars and text. */
	FLinearColor Color = FLinearColor::White;

	/** The chip behind the icon. */
	FLinearColor ChipFill = FLinearColor::Transparent;

	/** The icon colour on the chip. */
	FLinearColor Ink = FLinearColor::White;

	UTexture2D* Icon = nullptr;

	/** From a catalog row. Brush may be null (no palette: the bucket keeps the category colour). */
	static FItemLook FromRow(const UUIStyle* Style, const FShopItemRow& Row, const UPaintBrushComponent* Brush);

	/** From a stack of the inventory: its catalog row when it has one, a guess from its class when it has none. */
	static FItemLook FromSlot(const UUIStyle* Style, const UHotbarComponent* Hotbar, const FInventorySlot& Slot, const UPaintBrushComponent* Brush);
};
