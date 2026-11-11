#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Destruction/DemolitionTypes.h"
#include "Progress/ToolShop.h"
#include "ToolCardWidget.generated.h"

class UBorder;
class UButton;
class UContractChipWidget;
class UImage;
class UPanelWidget;
class UTextBlock;
class UUIStyle;
class UWidget;

DECLARE_DELEGATE_TwoParams(FOnToolBuyRequested, FName /*ToolId*/, int32 /*Quantity*/);

/**
 *  One thing of the PC's Tools tab, as a card: the icon, the name, a line on what it is for, its price, and the state of the purchase.
 *  A tool is Owned (a badge), can be bought (the button says "Buy 300") or is too expensive (a disabled button says "180 short").
 *  A consumable (the charge) has a stepper for the quantity ("Buy 3 · 450"), shows the stock, and is never Owned. The card only
 *  decides what to say, through ToolShop::Evaluate; the tab does the buying when the button asks for it (OnBuyRequested).
 *
 *  The widget Blueprint only lays it out (the named widgets below are required, the optional ones are skipped when absent).
 */
UCLASS(Abstract)
class UToolCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 *  Fills the card with the state of the game now. Stock is the charges in stock, which only a consumable shows. InUpkeep is what the tool
	 *  costs to keep, read by the tab from the tool's data asset (free for a tool that costs nothing, and for a consumable).
	 */
	void SetOffer(FName InToolId, const FToolOfferRow& InRow, bool bInOwned, int32 InMoney, bool bInInfiniteMoney, int32 InStock, const FToolUpkeep& InUpkeep);

	FName GetToolId() const { return ToolId; }

	/** Runs when the button is pressed on a card that can be bought. */
	FOnToolBuyRequested OnBuyRequested;

protected:

	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Card")
	TObjectPtr<UUIStyle> Style;

	/** The chip Blueprint repeated for each material the tool is good against (the contract chip, a plain pill). */
	UPROPERTY(EditDefaultsOnly, Category = "Card")
	TSubclassOf<UContractChipWidget> ChipClass;

	/** The rounded background and outline: green once owned, a quiet outline otherwise. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Frame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BlurbText;

	/** The price ("300", or "150 each" for a consumable, "Included" for what costs nothing). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BuyButton;

	/** The words on the button: "Buy 300", "Buy 3 · 450", "180 short", or "On your belt" for a tool the player has. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BuyLabelText;

	/** The coloured shape behind the button's words: orange when it can be pressed, quiet when it cannot. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BuyFrame;

	/** Where the "good against" chips go (a wrap box). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ChipHost;

	/** What the clicks do. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClicksText;

	/** The coin, the price and "each": hidden for a tool the player already has. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PriceRow;

	/** The "OWNED" badge, shown only for a tool the player has. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OwnedBadge;

	/** "Upkeep: 50 per job + 2 per second", hidden when the tool costs nothing to keep. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UpkeepText;

	/** "In stock: 4", for a consumable only. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StockText;

	/** The quantity stepper (minus, number, plus), for a consumable only. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StepperRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MinusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

private:

	void Redraw();

	/** Makes the chips of the tool, once per tool. */
	void BuildChips();

	UFUNCTION()
	void HandleBuy();

	UFUNCTION()
	void HandleMinus();

	UFUNCTION()
	void HandlePlus();

	FName ToolId;
	FToolOfferRow Row;
	FToolUpkeep Upkeep;
	bool bOwned = false;
	int32 Money = 0;
	bool bInfiniteMoney = false;
	int32 Stock = 0;
	int32 Quantity = 1;
	bool bChipsBuilt = false;
};
