#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToolsTabWidget.generated.h"

class UDataTable;
class UPanelWidget;
class UToolCardWidget;

/**
 *  The Tools tab of the PC: one card per row of DT_ToolShop, in the table's order. Buying pays from the player's money (the hotbar's,
 *  so the balance on screen follows), then writes the tool to the progress (UProgressSubsystem::GrantTool), or adds the charges to
 *  the stock for the consumable. The tab does the buying; each card only says what the state of its purchase is.
 *
 *  The terminal calls Refresh when the tab opens and listens to OnPurchased to redraw its balance. The widget Blueprint only lays it
 *  out (the named widget below is required) and names the table and the card class.
 */
UCLASS(Abstract)
class UToolsTabWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Builds the cards the first time, then redraws every one from the money, the owned tools and the stock as they are now. */
	void Refresh();

	/** Fired after a purchase went through. */
	FSimpleMulticastDelegate OnPurchased;

protected:

	/** DT_ToolShop, rows of FToolOfferRow. */
	UPROPERTY(EditDefaultsOnly, Category = "Tools")
	TObjectPtr<UDataTable> Offers;

	UPROPERTY(EditDefaultsOnly, Category = "Tools")
	TSubclassOf<UToolCardWidget> CardClass;

	/** Where the cards go, side by side. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> CardHost;

private:

	void HandleBuy(FName ToolId, int32 Quantity);

	UPROPERTY()
	TArray<TObjectPtr<UToolCardWidget>> Cards;
};
