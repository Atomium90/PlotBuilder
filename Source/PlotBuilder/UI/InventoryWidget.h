#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UUIStyle;
class UHotbarComponent;
class UPaintBrushComponent;
class UItemSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryCloseRequested);

/**
 *  The bag drawer: the hotbar row and the backpack grid of the inventory, the balance, and (laid out by the Blueprint) the
 *  tool belt. A click on a slot picks a stack up or drops it, a right click drops one; that is UHotbarComponent's job, the
 *  slot widgets only forward the click. The widget Blueprint only lays it out (the named widgets below are required).
 *
 *  It tells the HUD to close it through OnCloseRequested, like the menu it replaces.
 */
UCLASS(Abstract)
class UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fired when the player presses Escape or I. The HUD closes the menu. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryCloseRequested OnCloseRequested;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UUIStyle> Style;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UItemSlotWidget> SlotClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	float SlotSize = 48.f;

	/** Row 0 of the grid: the hotbar. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> HotbarGrid;

	/** The other rows of the grid. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BalanceText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BackpackUsedText;

private:

	bool TryBind();
	void Unbind();
	void BuildSlots();
	void RefreshSlots();
	void RefreshInfo();

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleMoneyChanged();

	UPROPERTY()
	TObjectPtr<UHotbarComponent> Hotbar;

	UPROPERTY()
	TObjectPtr<UPaintBrushComponent> Brush;

	UPROPERTY()
	TArray<TObjectPtr<UItemSlotWidget>> Slots;

	bool bBound = false;
};
