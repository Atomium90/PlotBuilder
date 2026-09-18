#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "HotbarComponent.generated.h"

/** One stored slot: class to respawn, plus the scale it had when picked up (SpawnActor only takes location/rotation). */
USTRUCT(BlueprintType)
struct FHotbarEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AActor> ActorClass;

	UPROPERTY()
	FVector Scale = FVector::OneVector;
};

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
	bool bShowDebug = false;

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

	/** Same as PurchaseRow, but resolves the row by its position in ShopCatalog (row order = slot index). */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void PurchaseByIndex(int32 SlotIndex);

	/**
	 *  Spawns the selected entry, grid-snapped, in front of the view. Instant placement, no
	 *  preview: this is the APhysicsProp path. ABuildablePiece entries are refused here - they
	 *  go through UBuildModeComponent's preview + confirm flow instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void TryPlace();

	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	int32 GetStoredCount() const { return StoredEntries.Num(); }

	/** Copies the currently selected entry into OutEntry. Returns false if there is no selection. */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool GetSelectedEntry(FHotbarEntry& OutEntry) const;

	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	/**
	 *  Moves the selection by Direction slots (+1/-1), wrapping around. When bBuildablesOnly,
	 *  entries whose class isn't an ABuildablePiece are skipped - used while Build Mode is active
	 *  so cycling never lands on a prop the ghost preview couldn't show anyway.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void CycleSelection(int32 Direction, bool bBuildablesOnly = false);

	/**
	 *  If the current selection doesn't already qualify (see CycleSelection), snaps it to the
	 *  first qualifying entry found. Leaves the selection untouched otherwise - unlike
	 *  CycleSelection, this never moves an already-qualifying selection. Returns whether a
	 *  qualifying entry ended up selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool EnsureValidSelection(bool bBuildablesOnly);

	/**
	 *  Removes and returns the selected entry. When another entry of the same class shifts into
	 *  its slot (the common case after buying/picking up several of the same piece in a row), it
	 *  stays selected so Build Mode can chain placements without a full rescan; otherwise the
	 *  first remaining entry of the same class is selected, if any.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	FHotbarEntry ConsumeSelected();

private:

	static bool EntryQualifies(const FHotbarEntry& Entry, bool bBuildablesOnly);

	UPROPERTY()
	TArray<FHotbarEntry> StoredEntries;

	UPROPERTY()
	int32 SelectedIndex = INDEX_NONE;
};
