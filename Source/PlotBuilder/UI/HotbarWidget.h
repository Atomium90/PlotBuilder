#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HotbarWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UUIStyle;
class UHotbarComponent;
class UPaintBrushComponent;
class UItemSlotWidget;

/**
 *  The hotbar row of the HUD: the first row of the inventory, with the selected slot framed in orange. When the selection
 *  changes, the name of the item shows in a label for a few seconds, then goes away by itself. In a map that does not allow
 *  the inventory side of the game (a mission) the whole row is hidden. The widget Blueprint only lays it out.
 */
UCLASS(Abstract)
class UHotbarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Hotbar")
	TObjectPtr<UUIStyle> Style;

	UPROPERTY(EditDefaultsOnly, Category = "Hotbar")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hotbar")
	float SlotSize = 56.f;

	/** Seconds the name of the selected item stays up. Zero keeps the label hidden. */
	UPROPERTY(EditDefaultsOnly, Category = "Hotbar")
	float LabelSeconds = 5.f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SlotRow;

	/** Everything that hides with the inventory side of the game. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ContentRoot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LabelRoot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

private:

	bool TryBind();
	void Unbind();
	void BuildSlots();
	void RefreshSlots();
	void RefreshVisibility();

	/** Tells UPlayerMessages where the hotbar begins, so the prompt and the messages are drawn right above it. */
	void PublishBottomLimit();

	void ShowLabel();
	void HideLabel();

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleSelectionChanged();

	UFUNCTION()
	void HandleAllowedChanged();

	UPROPERTY()
	TObjectPtr<UHotbarComponent> Hotbar;

	UPROPERTY()
	TObjectPtr<UPaintBrushComponent> Brush;

	UPROPERTY()
	TArray<TObjectPtr<UItemSlotWidget>> Slots;

	bool bBound = false;
	FTimerHandle LabelTimer;
};
