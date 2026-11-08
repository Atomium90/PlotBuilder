#pragma once

#include "CoreMinimal.h"
#include "ShopCartTypes.generated.h"

/** One line of the shop cart: a row of the shop catalog and how many of it. */
USTRUCT(BlueprintType)
struct FCartLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	FName RowName;

	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	int32 Quantity = 0;
};

/** What buying a whole cart would do, computed without changing anything. */
USTRUCT(BlueprintType)
struct FCartQuote
{
	GENERATED_BODY()

	/** Items in the cart, all lines together. */
	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	int32 Units = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	int32 TotalCost = 0;

	/** Slots the purchase would fill. Past FreeSlots it is an estimate, since the cart does not fit anyway. */
	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	int32 SlotsNeeded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	int32 FreeSlots = 0;

	/** True when the balance covers the total, or when money is infinite. */
	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	bool bAffordable = true;

	/** True when every item finds a place in the inventory. */
	UPROPERTY(BlueprintReadOnly, Category = "Cart")
	bool bFits = true;

	bool IsEmpty() const { return Units <= 0; }

	bool CanConfirm() const { return Units > 0 && bAffordable && bFits; }
};
