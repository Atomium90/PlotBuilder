#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/ShopItemRow.h"
#include "ShopRowWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UUIStyle;
class UShopCartComponent;
class UHotbarComponent;
class UPaintBrushComponent;

/**
 *  One line of the shop list: the item, how many the player already has, its price, and the - and + that move its
 *  quantity in the cart. It buys nothing itself: it talks to UShopCartComponent. The widget Blueprint only lays it out.
 */
UCLASS(Abstract)
class UShopRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Setup(UUIStyle* InStyle, FName InRowName, const FShopItemRow& InRow, UShopCartComponent* InCart, UHotbarComponent* InHotbar, UPaintBrushComponent* InBrush);

	FName GetRowName() const { return RowName; }
	const FShopItemRow& GetRow() const { return Row; }

	/** Redraws the quantity in the cart and the highlight of a row that has some. */
	void RefreshCart();

	/** Redraws how many the player already holds. */
	void RefreshOwned();

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MinusButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PlusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OwnedText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconBackground;

	/** Fills the row; tinted while the pointer is over it, and while the cart holds some of it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Background;

private:

	UFUNCTION()
	void HandleMinus();

	UFUNCTION()
	void HandlePlus();

	void UpdateBackground();

	UPROPERTY()
	TObjectPtr<UUIStyle> Style;

	UPROPERTY()
	TObjectPtr<UShopCartComponent> Cart;

	UPROPERTY()
	TObjectPtr<UHotbarComponent> Hotbar;

	FName RowName;
	FShopItemRow Row;
	bool bHovered = false;
	bool bInCart = false;
};
