#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "ShopCartTypes.h"
#include "Interaction/TakeBackConfirm.h"
#include "HotbarComponent.generated.h"

struct FShopItemRow;
class ABuildablePiece;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllowedChanged);

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hotbar")
	int32 NumColumns = 9;

	/** Slot rows, hotbar included (row 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hotbar")
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

	/** Fired when SetAllowed turns the inventory side of the game on or off (a mission turns it off). The HUD hides the hotbar on it. */
	UPROPERTY(BlueprintAssignable, Category = "Hotbar")
	FOnAllowedChanged OnAllowedChanged;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/** Sets Money directly and broadcasts OnMoneyChanged - bypasses bInfiniteMoney/PurchaseRow entirely, for the pb.Money debug command (and later UProgressSave's load path). */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SetMoney(int32 NewAmount) { Money = NewAmount; OnMoneyChanged.Broadcast(); }

	UFUNCTION(BlueprintPure, Category = "Hotbar")
	bool IsInfiniteMoney() const { return bInfiniteMoney; }

	/**
	 *  Pays Amount out of Money and broadcasts OnMoneyChanged (which writes it back to the progress). False, nothing paid, when
	 *  Money is short. With bInfiniteMoney it always succeeds and takes nothing, as PurchaseRow does. Used by the PC's purchases.
	 */
	bool TrySpend(int32 Amount);

	/**
	 *  Whether the inventory side of the game works: picking a prop up (E), buying, placing a prop from the hotbar.
	 *  A mission turns it off (APlayerCharacter::SetBuildingAllowed): the tool belt is the only way to take a house
	 *  apart there. Pieces check it too, before a Placed one can be taken back by hand (ABuildablePiece::Interact).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SetAllowed(bool bNewAllowed);

	UFUNCTION(BlueprintPure, Category = "Hotbar")
	bool IsAllowed() const { return bAllowed; }

	/**
	 *  Destroys ActorToStore and adds its class to the first slot that fits (see AddItem) - an
	 *  existing stack of the same class/scale with room first, an empty slot otherwise. Fails if the
	 *  grid is completely full for this item (no matching stack with room, no empty slot).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool Store(AActor* ActorToStore);

	/**
	 *  Puts one unit of a Placed piece back in the bag, without touching the actor (the piece unregisters itself from the
	 *  graph and destroys itself afterwards, see ABuildablePiece::Interact): the bag is checked first, so a full bag
	 *  leaves the piece standing and the graph untouched. The unit goes in at the scale the piece was placed with, not
	 *  its biased actor scale (a bought piece has scale 1, and a biased one would neither stack with it nor survive being
	 *  placed again without being biased twice), and a mirror variant goes back as the piece Build Mode swaps it in for.
	 *  Fails when the bag is full or the inventory is off (a mission).
	 */
	bool TakeBack(const ABuildablePiece* Piece);

	/**
	 *  The guard in front of TakeBack for a piece that holds others up. The first call arms it, tells the player why nothing happened and
	 *  answers false; a second call on the same piece within TakeBackConfirmSeconds answers true. Call it only for such a piece.
	 */
	bool ConfirmTakeBack(const ABuildablePiece* Piece);

	/** Seconds the player has, after the warning, to take back a piece that holds others up by pressing the interact key again. */
	UPROPERTY(EditAnywhere, Category = "Hotbar", meta = (ClampMin = "0.5"))
	float TakeBackConfirmSeconds = 2.f;

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

	/** How many units of ActorClass the player holds, in the grid and on the cursor. */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	int32 CountOf(TSubclassOf<AActor> ActorClass) const;

	/** Slots of the grid, hotbar and backpack together. Known from the settings, so it is right before BeginPlay has made the grid. */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	int32 GetTotalSlotCount() const { return FMath::Max(NumColumns * NumRows, 0); }

	/** Empty slots of the grid, hotbar and backpack together. */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	int32 GetFreeSlotCount() const;

	/**
	 *  What buying Lines would cost and whether it fits, without changing anything: the money is checked against the
	 *  total, and the items are placed on a copy of the grid with the same rules as a purchase.
	 */
	UFUNCTION(BlueprintPure, Category = "Hotbar")
	FCartQuote QuoteLines(const TArray<FCartLine>& Lines) const;

	/**
	 *  Buys every line or none: nothing is taken and nothing is added unless QuoteLines says the whole cart can be paid and
	 *  stored. The shop cart calls this on Confirm.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool PurchaseLines(const TArray<FCartLine>& Lines);

	/** The catalog row of that name, or nullptr. */
	const FShopItemRow* FindRow(FName RowName) const;

	/**
	 *  How many more of a catalog row the player may still buy: what FShopItemRow::MaxOwned allows minus what is already held,
	 *  or no limit at all (a very large number) when the row has none.
	 */
	int32 GetPurchaseLimit(const FShopItemRow& Row) const;

	/** The catalog row whose ActorClass is ActorClass, or nullptr. The widgets read the name, the price and the category of a stack from it. */
	const FShopItemRow* FindRowForClass(TSubclassOf<AActor> ActorClass) const;

	/**
	 *  Spawns the selected entry, grid-snapped, in front of the view, for whatever is neither a piece nor a prop. Pieces
	 *  (ABuildablePiece) and props (APhysicsProp) are refused here with a line telling the player to enter Build Mode: both
	 *  go through UBuildModeComponent's preview + confirm flow, and a prop is set down there like a wall. The paint
	 *  buckets (APaintBucket) are refused too, silently: only the brush uses them and they are never spawned.
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

	/**
	 *  Puts the cursor-held stack (see GetCursorSlot) back into the grid - an existing matching
	 *  stack with room first, an empty slot otherwise, same priority as AddItem. Call this when the
	 *  inventory UI closes so a stack left on the cursor isn't silently lost. Returns true if the
	 *  cursor ended up empty (fully returned); if the grid has no room at all, the cursor keeps
	 *  whatever didn't fit rather than discarding it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	bool ReturnCursorSlot();

	/**
	 *  Replaces the whole grid with NewSlots (same row-major layout as GetSlots; extra slots are dropped, missing ones
	 *  are empty) and empties the cursor. For the home restore: the inventory the player left comes back as it was.
	 */
	void RestoreSlots(const TArray<FInventorySlot>& NewSlots);

private:

	static bool SlotQualifies(const FInventorySlot& Slot, bool bBuildablesOnly);

	/** Adds Count units of ActorClass/Scale (capped at StackSize per slot) starting at the first slot that fits - an existing matching stack with room before any empty slot, both scanned in index order. Returns how many units didn't fit anywhere (0 = fully added). */
	int32 AddItem(TSubclassOf<AActor> ActorClass, const FVector& Scale, int32 StackSize, int32 Count = 1);

	/** Adds Count units to Target with the AddItem rules and returns how many did not fit. Static so a quote can try it on a copy of the grid. */
	static int32 AddToSlots(TArray<FInventorySlot>& Target, TSubclassOf<AActor> ActorClass, const FVector& Scale, int32 StackSize, int32 Count);

	/** ShopCatalog row whose ActorClass matches, or nullptr. Used by Store() - PurchaseRow already has its row directly. */
	const FShopItemRow* FindCatalogRowFor(TSubclassOf<AActor> ActorClass) const;

	/** The class the shop sells for ActorClass: itself when it has a row, the piece that names it as MirrorVariantClass when it is a mirror, otherwise itself. */
	TSubclassOf<AActor> FindSoldClass(TSubclassOf<AActor> ActorClass) const;

	void SetSelectedIndex(int32 NewIndex);

	/** Bound to OnMoneyChanged: writes Money back to UProgressSubsystem, which is the source of truth across maps. */
	UFUNCTION()
	void HandleMoneyChanged();

	/** See SetAllowed. */
	bool bAllowed = true;

	/** The piece a take-back is armed on, if any (see ConfirmTakeBack). */
	FTakeBackConfirm TakeBackGuard;

	/**
	 *  The progress changed: take the money from it when it differs from the copy held here. A payment is credited to the progress
	 *  directly (UMissionResultSubsystem::Settle), so without this the copy, and everything that reads it (the HUD, the report), would
	 *  keep the amount it had when the map started.
	 */
	void HandleProgressChanged();

	FDelegateHandle ProgressChangedHandle;

	UPROPERTY()
	TArray<FInventorySlot> Slots;

	UPROPERTY()
	FInventorySlot CursorSlot;

	UPROPERTY()
	int32 SelectedIndex = 0;
};
