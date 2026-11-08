#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToolTileWidget.generated.h"

class UImage;
class UTextBlock;
class UUIStyle;
class UToolbeltComponent;
class UInfoTooltipWidget;

/**
 *  One tile of the tool belt: the icon of a tool, framed in orange while it is in hand and dimmed when the map does not
 *  allow it. One more tile, the padlock, stands for all the tools the player does not own yet. The widget Blueprint only
 *  lays it out; it exists in a small form (the HUD column) and a large one with the name under the icon (the bag drawer).
 */
UCLASS(Abstract)
class UToolTileWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Slot is the tool belt slot to show, or INDEX_NONE for the padlock tile. */
	void Setup(UUIStyle* InStyle, UToolbeltComponent* InBelt, int32 InSlot);

	/** Redraws the tile from the belt. */
	void Refresh();

	/** The belt slot the tile shows, or INDEX_NONE for the padlock tile. */
	int32 GetToolSlot() const { return ToolSlot; }

	/** Shows or hides the chip with the name and the hint of the tool, if the tile has one (the HUD column does). */
	void SetLabelVisible(bool bVisible);

protected:

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** The card shown on the padlock tile. Leave empty for no tooltip. */
	UPROPERTY(EditDefaultsOnly, Category = "Tool Tile")
	TSubclassOf<UInfoTooltipWidget> TooltipClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Frame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Icon;

	/** The tool's name; "3 missing" on the padlock tile. Only in the large form. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** "Equipped", "Ready" or "Not usable here". Only in the large form. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** The chip next to the tile with the tool's name and what the two clicks do. Only in the HUD form; hidden until the tool is picked. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LabelRoot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelHintText;

private:

	UPROPERTY()
	TObjectPtr<UUIStyle> Style;

	UPROPERTY()
	TObjectPtr<UToolbeltComponent> Belt;

	int32 ToolSlot = INDEX_NONE;
};
