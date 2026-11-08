#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ShopItemRow.generated.h"

/** Which shop tab/filter an item belongs to - purely a UI grouping, no effect on placement or structural behaviour. */
UENUM(BlueprintType)
enum class EShopItemCategory : uint8
{
	/** Load-bearing skeleton pieces: foundation, floor, plain wall, interior wall, corner post, stairs. */
	Structure,
	/** Pieces that put an opening in a wall: door/window wall variants. */
	Opening,
	/** All roof/gable pieces. */
	Roof,
	/** Furniture/decor - APhysicsProp items. */
	Prop,
	/** Paint buckets (APaintBucket): one per colour, used by the brush, never placed. Added last so saved rows keep their value. */
	Paint,
};

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

	/** Max count a single inventory slot of this item can hold. No single default - set per row (99 for a buildable piece, 9 for a prop, as a starting point). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	int32 StackSize = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	EShopItemCategory Category = EShopItemCategory::Structure;

	/**
	 *  The row of Catalog whose ActorClass is exactly ActorClass, or nullptr (no catalog, no class, no such row).
	 *  The one lookup by class: the hotbar uses it to store a prop, the mission director to weigh a piece.
	 *  The catalog is the source of truth for costs; the Cost field on the piece Blueprints is not read.
	 */
	static const FShopItemRow* FindForClass(const UDataTable* Catalog, TSubclassOf<AActor> ActorClass)
	{
		if (!Catalog || !ActorClass)
		{
			return nullptr;
		}

		TArray<FShopItemRow*> Rows;
		Catalog->GetAllRows<FShopItemRow>(TEXT("FShopItemRow::FindForClass"), Rows);

		for (const FShopItemRow* Row : Rows)
		{
			if (Row && Row->ActorClass == ActorClass)
			{
				return Row;
			}
		}

		return nullptr;
	}
};
