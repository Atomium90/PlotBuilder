#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShopCartTypes.h"
#include "ShopCartComponent.generated.h"

class UHotbarComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCartChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCartPurchased, int32, Units);

/**
 *  The shop cart: rows of the shop catalog and how many of each the player means to buy. It holds only the lines; the
 *  prices, the money and the room in the inventory are UHotbarComponent's, asked through QuoteLines when the cart is
 *  drawn and through PurchaseLines on Confirm, so a purchase is all or nothing.
 *
 *  The cart lives on the player, so it survives the shop being closed and starts empty on every map.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UShopCartComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UShopCartComponent();

	/** Fired after any change to the lines. */
	UPROPERTY(BlueprintAssignable, Category = "Cart")
	FOnCartChanged OnCartChanged;

	/** Fired after a successful Confirm, with the number of items bought. */
	UPROPERTY(BlueprintAssignable, Category = "Cart")
	FOnCartPurchased OnPurchased;

	/** The most of one row the cart accepts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart")
	int32 MaxQuantityPerLine = 999;

	/** How many units a step moves while Shift is held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart")
	int32 ShiftStep = 10;

	/** Adds Direction (+1 or -1) to a row, or Direction x ShiftStep while Shift is held. The row leaves the cart at 0. */
	UFUNCTION(BlueprintCallable, Category = "Cart")
	void StepQuantity(FName RowName, int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Cart")
	void SetQuantity(FName RowName, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Cart")
	void Clear();

	/** Buys the whole cart (all or nothing, see UHotbarComponent::PurchaseLines) and empties it. False, and nothing changes, when it cannot. */
	UFUNCTION(BlueprintCallable, Category = "Cart")
	bool Confirm();

	UFUNCTION(BlueprintPure, Category = "Cart")
	int32 GetQuantity(FName RowName) const;

	UFUNCTION(BlueprintPure, Category = "Cart")
	const TArray<FCartLine>& GetLines() const { return Lines; }

	/** Cost, room and affordability of the cart as it is now. */
	UFUNCTION(BlueprintPure, Category = "Cart")
	FCartQuote GetQuote() const;

private:

	UHotbarComponent* GetHotbar() const;

	TArray<FCartLine> Lines;
};
