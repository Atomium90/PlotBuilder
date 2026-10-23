#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "HotbarComponent.generated.h"

/** One inventory slot: class to respawn, the scale its stack was picked up/bought at, how many, and the max for that stack. Empty when ActorClass is null or Count is 0. */
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FVector Scale = FVector::OneVector;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;

	/** Cached capacity for whatever's currently in this slot - 0/meaningless while empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 StackSize = 0;

	bool IsEmpty() const { return ActorClass == nullptr || Count <= 0; }

	void Reset() { ActorClass = nullptr; Scale = FVector::OneVector; Count = 0; StackSize = 0; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedSlotChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCursorSlotChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMoneyChanged);

/**
 *  Minecraft-style grid inventory: NumColumns x NumRows fixed slots, each stacking up to its own
 *  item's StackSize (see FShopItemRow). The first NumColumns slots are the hotbar - the only slots
 *  UBuildModeComponent/TryPlace ever read from or cycle through (see SelectedIndex); the remaining
 *  rows are backpack-only, reachable solely through the inventory UI's click interaction.
 *
 *  Click interaction (no drag-and-drop): left-click on a slot picks its whole stack up onto
 *  CursorSlot if the cursor is empty, or drops/merges/swaps the cursor's stack into that slot
 *  otherwise (see OnSlotLeftClicked). Right-click places one unit at a time from the cursor (see
 *  OnSlotRightClicked). All UI-facing state changes fire a delegate instead of relying on Tick -
 *  OnInventoryChanged, OnSelectedSlotChanged, OnCursorSlotChanged, OnMoneyChanged.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UHotbarComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHotbarComponent();

	/** Slot columns - also the hotbar's own size (see the class comment). */
	UPROPERTY(EditAnywhere, Category = "Hotbar")
	int32 NumColumns = 9;

	/** Slot rows, hotbar included (row 0). */
	UPROPERTY(EditAnywhere, Category = "Hotbar")
	int32 NumRows = 4;

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

	/** When true, PurchaseRow never checks or deducts Money - for testing without an economy to balance. */
	UPROPERTY(EditAnywhere, Category = "Hotbar|Economy")
	bool bInfiniteMoney = true;

	UPROPERTY(EditAnywhere, Category = "Hotbar|Economy")
	int32 Money = 0;

	/** Fired after any change to the grid's contents (buy, pick up, click move/merge/swap, consume). */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar")
	FOnInventoryChanged OnInventoryChanged;

	/** Fired after SelectedIndex changes. */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar")
	FOnSelectedSlotChanged OnSelectedSlotChanged;

	/** Fired after the cursor-held stack changes (see GetCursorSlot). */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar")
	FOnCursorSlotChanged OnCursorSlotChanged;

	/** Fired after Money changes. */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar")
	FOnMoneyChanged OnMoneyChanged;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Read-only access to the full grid, row-major (index = Row * NumColumns + Column). */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	const TArray<FInventorySlot>& GetSlots() const { return Slots; }

	/** The stack currently held by the cursor (click-to-pick-up), or an empty slot if none. */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FInventorySlot GetCursorSlot() const { return CursorSlot; }

	UFUNCTION(BlueprintPure, Category = "Hotbar")
	int32 GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure, Category = "Hotbar")
	bool IsInfiniteMoney() const { return bInfiniteMoney; }

	/**
	 *  Destroys ActorToStore and adds its class to the first slot that fits (see AddItem) - an
	 *  existing stack of the same class/scale with room first, an empty slot otherwise. Fails if the
	 *  grid is completely full for this item (no matching stack with room, no empty slot).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool Store(AActor* ActorToStore);

	/** Removes and returns one unit's class from the selected slot, decrementing its Count (clearing it at 0). Returns nullptr if the selection is empty. */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	TSubclassOf<AActor> PopLast();

	/**
	 *  Looks up RowName in ShopCatalog and adds one unit of its ActorClass to the first slot that
	 *  fits (see AddItem) - no world actor involved, this is "buy from the catalog" rather than
	 *  "pick up". Fails (no charge, no slot change) if Money is short and bInfiniteMoney is false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool PurchaseRow(FName RowName);

	/**
	 *  Spawns the selected entry, grid-snapped, in front of the view. Instant placement, no
	 *  preview: this is the APhysicsProp path. ABuildablePiece entries are refused here - they
	 *  go through UBuildModeComponent's preview + confirm flow instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void TryPlace();

	/** Copies the currently selected slot into OutSlot. Returns false if the selection is out of range or empty. */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool GetSelectedEntry(FInventorySlot& OutSlot) const;

	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	/**
	 *  Moves the selection by Direction slots (+1/-1) within the hotbar only (indices
	 *  [0, NumColumns)), wrapping around. When bBuildablesOnly, slots whose class isn't an
	 *  ABuildablePiece are skipped - used while Build Mode is active so cycling never lands on a
	 *  prop the ghost preview couldn't show anyway.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void CycleSelection(int32 Direction, bool bBuildablesOnly = false);

	/**
	 *  If the current selection doesn't already qualify (see CycleSelection), snaps it to the
	 *  first qualifying hotbar slot found. Leaves the selection untouched otherwise - unlike
	 *  CycleSelection, this never moves an already-qualifying selection. Returns whether a
	 *  qualifying slot ended up selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool EnsureValidSelection(bool bBuildablesOnly);

	/** Removes and returns one unit from the selected slot, decrementing its Count (clearing it at 0 - never shifts another slot into its place, slots are fixed-position now). */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	FInventorySlot ConsumeSelected();

	/**
	 *  Click interaction, Minecraft-style - no drag-and-drop. Left-click: if the cursor is empty,
	 *  picks up SlotIndex's whole stack onto it; otherwise drops the cursor's stack into SlotIndex
	 *  (merges up to StackSize if it's the same item, swaps outright if it's a different one, fills
	 *  it outright if it was empty).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void OnSlotLeftClicked(int32 SlotIndex);

	/**
	 *  Right-click: if the cursor is empty, same as a left-click (picks up the whole stack).
	 *  Otherwise places exactly one unit from the cursor into SlotIndex (only if that slot is empty
	 *  or already holds the same item with room) - for spreading one stack across several slots.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void OnSlotRightClicked(int32 SlotIndex);

private:

	static bool SlotQualifies(const FInventorySlot& Slot, bool bBuildablesOnly);

	/** Adds Count units of ActorClass/Scale (capped at StackSize per slot) starting at the first slot that fits - an existing matching stack with room before any empty slot, both scanned in index order. Returns how many units didn't fit anywhere (0 = fully added). */
	int32 AddItem(TSubclassOf<AActor> ActorClass, const FVector& Scale, int32 StackSize, int32 Count = 1);

	/** ShopCatalog row whose ActorClass matches, or nullptr. Used by Store() - PurchaseRow already has its row directly. */
	const struct FShopItemRow* FindCatalogRowFor(TSubclassOf<AActor> ActorClass) const;

	void SetSelectedIndex(int32 NewIndex);

	UPROPERTY()
	TArray<FInventorySlot> Slots;

	UPROPERTY()
	FInventorySlot CursorSlot;

	UPROPERTY()
	int32 SelectedIndex = 0;
};
