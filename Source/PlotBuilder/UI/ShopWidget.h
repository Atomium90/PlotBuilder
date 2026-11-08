#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/ShopItemRow.h"
#include "ShopWidget.generated.h"

class UButton;
class UEditableTextBox;
class UImage;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UUIStyle;
class UHotbarComponent;
class UShopCartComponent;
class UPaintBrushComponent;
class UShopCategoryButton;
class UShopRowWidget;
class UCartLineWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuCloseRequested);

/**
 *  The shop menu: a category rail, a searchable and sortable list of the catalog, and the cart on the right with its
 *  total, the money left, the room in the bag and the Confirm button. The widget Blueprint only lays it out (the named
 *  widgets below are required) and names the three small widget classes it repeats; what the shop shows and does lives here.
 *
 *  It reads the catalog from UHotbarComponent, holds nothing of the purchase itself (UShopCartComponent does) and tells
 *  the HUD to close it through OnCloseRequested, like the menu it replaces.
 */
UCLASS(Abstract)
class UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fired when the player presses Escape or B. The HUD closes the menu. */
	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FOnMenuCloseRequested OnCloseRequested;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** The look of the menu. */
	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TObjectPtr<UUIStyle> Style;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UShopCategoryButton> CategoryButtonClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UShopRowWidget> RowClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UCartLineWidget> CartLineClass;

	/** Seconds the "Purchased" line stays up. */
	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	float PurchaseMessageSeconds = 4.f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> CategoryList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> SearchBox;

	/** The cross at the end of the search field: empties it. Shown only while it holds text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SearchClearButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SortNameButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SortPriceButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SortNameBackground;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SortPriceBackground;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SortNameLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SortPriceLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShownCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BagText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> BagBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> CartLineList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CartEmptyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CartUnitsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ClearButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TotalText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BalanceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AfterText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BagSpaceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConfirmLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DoneText;

private:

	enum class ESort : uint8
	{
		Name,
		Price
	};

	/** Finds the components of the owning pawn and builds the lists. False while the pawn does not exist yet. */
	bool TryBind();

	void Unbind();

	void BuildCategories();
	void BuildRows();

	/** Puts the rows in the panel in the order of the current sort. Slow, so only when the sort changes. */
	void ApplyOrder();

	/** Shows the rows that match the category and the search, hides the others. Cheap. */
	void RefreshList();

	void RefreshCart();
	void RefreshMoney();
	void RefreshBag();
	void RefreshSortButtons();
	void RefreshOwned();

	void Close();

	UFUNCTION()
	void HandleSearchChanged(const FText& NewText);

	UFUNCTION()
	void HandleSearchClear();

	UFUNCTION()
	void HandleSortName();

	UFUNCTION()
	void HandleSortPrice();

	UFUNCTION()
	void HandleClear();

	UFUNCTION()
	void HandleConfirm();

	UFUNCTION()
	void HandleCartChanged();

	UFUNCTION()
	void HandlePurchased(int32 Units);

	UFUNCTION()
	void HandleMoneyChanged();

	UFUNCTION()
	void HandleInventoryChanged();

	void HandleCategoryPicked(int32 FilterIndex);

	void ClearDoneText();

	UPROPERTY()
	TObjectPtr<UHotbarComponent> Hotbar;

	UPROPERTY()
	TObjectPtr<UShopCartComponent> Cart;

	UPROPERTY()
	TObjectPtr<UPaintBrushComponent> Brush;

	UPROPERTY()
	TArray<TObjectPtr<UShopCategoryButton>> CategoryButtons;

	UPROPERTY()
	TArray<TObjectPtr<UShopRowWidget>> Rows;

	/** Which entry of the rail is active: 0 is every category, then the categories in enum order. */
	int32 ActiveFilter = 0;

	ESort Sort = ESort::Name;
	FString Query;
	bool bBound = false;
	FTimerHandle DoneTimer;
};
