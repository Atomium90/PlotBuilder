#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ShopItemRow.generated.h"

/**
 *  One catalog entry: what can be acquired, what it costs, what to spawn. Adding a new
 *  buildable piece or physics prop to the shop is a new DataTable row, not new code.
 */
USTRUCT(BlueprintType)
struct FShopItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	FText DisplayName;

	/** ABuildablePiece or APhysicsProp subclass to spawn when this item is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	int32 Cost = 0;
};
