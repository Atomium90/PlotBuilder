#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "HotbarComponent.generated.h"

/**
 *  Minimal hotbar: a fixed number of slots holding actor classes, nothing else. Not a real
 *  inventory (no UI, no stacking, no item data) - just enough to demo acquisition + reuse for
 *  both ABuildablePiece and APhysicsProp. Storing destroys the world actor; popping hands back
 *  the class to spawn, the caller decides how (physics prop vs kinematic placement).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UHotbarComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHotbarComponent();

	UPROPERTY(EditAnywhere, Category = "Hotbar")
	int32 MaxSlots = 5;

	/** Distance in front of the view items are placed at. */
	UPROPERTY(EditAnywhere, Category = "Hotbar")
	float PlaceDistance = 300.f;

	/** Placement position is rounded to this grid size on each axis. */
	UPROPERTY(EditAnywhere, Category = "Hotbar")
	float GridSize = 50.f;

	/** Lists stored slots on screen every frame. Stand-in until there's a real inventory UI. */
	UPROPERTY(EditAnywhere, Category = "Hotbar|Debug")
	bool bShowDebug = true;

	/** Catalog rows are FShopItemRow (see ShopItemRow.h). Assigned in the Blueprint. */
	UPROPERTY(EditAnywhere, Category = "Hotbar")
	TObjectPtr<UDataTable> ShopCatalog;

protected:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Destroys ActorToStore and adds its class to the hotbar. Fails if the hotbar is full. */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool Store(AActor* ActorToStore);

	/** Removes and returns the most recently stored class, or nullptr if the hotbar is empty. */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	TSubclassOf<AActor> PopLast();

	/**
	 *  Looks up RowName in ShopCatalog and adds its ActorClass to the hotbar - no world actor
	 *  involved, this is "buy from the catalog" rather than "pick up". No cost/currency check
	 *  yet, Cost on the row is informational until there's an economy system.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool PurchaseRow(FName RowName);

	/**
	 *  Pops the most recently stored class and spawns it, grid-snapped, in front of the view.
	 *  No live preview, no rotation: instant placement. Works for any stored class - a spawned
	 *  APhysicsProp keeps whatever physics setup its own constructor gives it (simulated), an
	 *  ABuildablePiece stays fixed (it never simulates physics).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void TryPlace();

	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	int32 GetStoredCount() const { return StoredClasses.Num(); }

private:

	UPROPERTY()
	TArray<TSubclassOf<AActor>> StoredClasses;
};
