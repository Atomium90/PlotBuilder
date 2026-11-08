#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/ShopItemRow.h"
#include "CartLineWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UUIStyle;
class UShopCartComponent;
class UPaintBrushComponent;

/** One line of the cart panel: what is in it, how many, what it costs, and a button that takes it out. */
UCLASS(Abstract)
class UCartLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Setup(UUIStyle* InStyle, FName InRowName, const FShopItemRow& Row, int32 Quantity, UShopCartComponent* InCart, UPaintBrushComponent* InBrush);

protected:

	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TotalText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RemoveButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconBackground;

private:

	UFUNCTION()
	void HandleRemove();

	UPROPERTY()
	TObjectPtr<UShopCartComponent> Cart;

	FName RowName;
};
