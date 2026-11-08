#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemSlotWidget.generated.h"

class UImage;
class USizeBox;
class UTextBlock;
class UUIStyle;
class UHotbarComponent;
class UPaintBrushComponent;
class UInfoTooltipWidget;
struct FInventorySlot;

/**
 *  One slot of the inventory grid, as the hotbar row and the bag draw it: the item's icon on its category colour, how many,
 *  and a frame that shows whether it is the selected hotbar slot or under the pointer. In the inventory a click moves the
 *  stack (left picks up or drops it, right drops one), through UHotbarComponent; in the HUD the slot is only drawn.
 *  The widget Blueprint only lays it out.
 */
UCLASS(Abstract)
class UItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Index is the slot's place in UHotbarComponent::GetSlots. Size is the width and height in pixels. */
	void Setup(UUIStyle* InStyle, UHotbarComponent* InHotbar, UPaintBrushComponent* InBrush, int32 InIndex, float Size, bool bInteractive);

	/** Redraws the slot from the inventory. */
	void Refresh();

protected:

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** The stack this slot shows: the grid slot at its index here, the cursor's stack in the cursor widget. False when there is none. */
	virtual bool ResolveStack(FInventorySlot& OutStack) const;

	/** The card shown on hover. Leave empty for no tooltip. */
	UPROPERTY(EditDefaultsOnly, Category = "Slot")
	TSubclassOf<UInfoTooltipWidget> TooltipClass;

	/** Rounded frame: its fill and outline are drawn here, from the style. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Frame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountText;

	/** The thin bar along the top edge, in the item's colour. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CategoryBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeRoot;

private:

	UFUNCTION()
	UWidget* BuildTooltip();

	void UpdateFrame();

	UPROPERTY()
	TObjectPtr<UUIStyle> Style;

	UPROPERTY()
	TObjectPtr<UHotbarComponent> Hotbar;

	UPROPERTY()
	TObjectPtr<UPaintBrushComponent> Brush;

	int32 SlotIndex = INDEX_NONE;
	bool bInteractive = false;
	bool bHovered = false;
	bool bSelected = false;
	bool bFilled = false;
};
