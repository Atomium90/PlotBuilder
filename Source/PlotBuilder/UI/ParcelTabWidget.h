#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParcelTabWidget.generated.h"

class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;
class UUIStyle;
class UWidget;

/**
 *  The Parcel tab of the PC: the price of the plot, how far the player's money goes towards it, and the button that buys it. The button
 *  has three states, from ToolShop::EvaluateParcel: "30 000 to go" (disabled, with the bar showing the share saved), "Buy the parcel",
 *  and "Owned". Buying pays from the player's money (the hotbar's, so the balance on screen follows) and sets the parcel flag in the
 *  progress, which is what opens the home's access point in the hub. The map and the facts about the plot are drawn by the Blueprint.
 *
 *  The terminal calls Refresh when the tab opens and listens to OnPurchased to redraw its balance. The widget Blueprint only lays it
 *  out (the named widgets below are required).
 */
UCLASS(Abstract)
class UParcelTabWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Redraws the tab from the money and the parcel flag as they are now. */
	void Refresh();

	/** Fired after the purchase went through. */
	FSimpleMulticastDelegate OnPurchased;

protected:

	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Parcel")
	TObjectPtr<UUIStyle> Style;

	/** What the parcel costs (the GDD default: 40 000, about fifteen average contracts). */
	UPROPERTY(EditDefaultsOnly, Category = "Parcel", meta = (ClampMin = "0"))
	int32 Price = 40000;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	/** The share of the price the player has saved. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> SavingBar;

	/** "27 550 to go", or "You can afford it", next to the price. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SavingText;

	/** The price and the bar: shown while the parcel is for sale, hidden once it is owned. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ForSaleBlock;

	/** "The parcel is yours": shown once it is owned. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OwnedBlock;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BuyButton;

	/** "27 550 to go", "Buy the parcel" or "Owned". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BuyLabelText;

	/** The coloured shape behind the button's words: orange when it can be pressed, green once owned, quiet otherwise. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BuyFrame;

private:

	UFUNCTION()
	void HandleBuy();
};
