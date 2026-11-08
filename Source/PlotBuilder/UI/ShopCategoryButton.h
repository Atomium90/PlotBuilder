#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interaction/ShopItemRow.h"
#include "ShopCategoryButton.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UUIStyle;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShopCategoryPicked, int32 /*FilterIndex*/);

/**
 *  One entry of the shop's category rail: an icon, a name and how many items it holds. The widget Blueprint only lays it
 *  out (the named widgets below are required); what it shows and how it reacts is decided here.
 */
UCLASS(Abstract)
class UShopCategoryButton : public UUserWidget
{
	GENERATED_BODY()

public:

	/** FilterIndex tells the shop which entry was picked back. All = true makes it the entry that shows every category. */
	void Setup(UUIStyle* InStyle, int32 InFilterIndex, bool bInAll, EShopItemCategory InCategory, int32 ItemCount);

	void SetActive(bool bActive);

	FOnShopCategoryPicked OnPicked;

protected:

	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconBackground;

	/** The thin bar on the left edge, lit while the entry is the active one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ActiveBar;

	/** Fills the entry, tinted while it is the active one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Background;

private:

	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	TObjectPtr<UUIStyle> Style;

	int32 FilterIndex = 0;
};
