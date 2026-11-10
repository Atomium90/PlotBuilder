#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToolbeltComponent.generated.h"

class AExplosiveCharge;
class ADemolitionTool;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBeltChanged);

/**
 *  Owned/equipped demolition tool slot, separate from UHotbarComponent (construction pieces/props)
 *  - see GDD §18.2. Slot 0 (Hands) routes straight to the sibling UPhysicsManipulationComponent:
 *  PrimaryUse Grabs, SecondaryUse Throws (no-op if nothing is held - GDD §14's grab/carry/throw,
 *  deliberately narrower than Lvl_Home's legacy Grab/Push, which also shoves an unheld focus and
 *  stays reachable there through APlayerCharacter's locked-toolbelt gate, not through this class).
 *  Slot 1 (Charge) spawns an AExplosiveCharge: secondary use (RMB) throws it, primary use (LMB)
 *  sticks it to the surface in reach; the next secondary use detonates it, however it got there -
 *  see SpawnAndThrowCharge and PlaceCharge. Slots 2-4 (Sledgehammer/Crowbar/Jackhammer) are ADemolitionTool
 *  instances, spawned once in BeginPlay from MeleeToolClasses and kept for the owner's lifetime
 *  (unlike the consumable charge) - PrimaryUse for those slots just forwards to the matching
 *  instance's own ADemolitionTool::PrimaryUse. Slot 5 (Brush) forwards PrimaryUse to the sibling
 *  UPaintBrushComponent, the way slot 0 forwards to UPhysicsManipulationComponent.
 *
 *  Which slots can be held is decided by three switches a map sets on its own, see SetLocked. All the tools
 *  are unlocked by default: there is no purchase of tools yet, so no notion of owning one.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UToolbeltComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UToolbeltComponent();

	/** Slot 0 = Hands, slots 1-4 = the four demolition tools (GDD §14), slot 5 = the paint brush. The brush is last so that adding it renumbers nothing. */
	UPROPERTY(EditDefaultsOnly, Category = "Toolbelt")
	int32 NumSlots = 6;

	/** Class spawned by slot 1's secondary use - see SpawnAndThrowCharge. */
	UPROPERTY(EditDefaultsOnly, Category = "Toolbelt")
	TSubclassOf<AExplosiveCharge> ExplosiveChargeClass;

	/** Initial toss speed given to a freshly spawned charge. */
	UPROPERTY(EditAnywhere, Category = "Toolbelt")
	float ChargeThrowSpeed = 800.f;

	/** Forward distance from the view point a charge is spawned at, so it doesn't spawn inside the player. */
	UPROPERTY(EditAnywhere, Category = "Toolbelt")
	float ChargeSpawnDistance = 150.f;

	/** How far in cm slot 1's primary use reaches to stick a charge on a surface. Nothing in reach, nothing happens. */
	UPROPERTY(EditAnywhere, Category = "Toolbelt")
	float ChargePlaceReach = 250.f;

	/** One ADemolitionTool class per melee slot (index 0 -> slot 2 Sledgehammer, 1 -> slot 3 Crowbar, 2 -> slot 4 Jackhammer). A slot left unset stays a no-op. */
	UPROPERTY(EditDefaultsOnly, Category = "Toolbelt")
	TArray<TSubclassOf<ADemolitionTool>> MeleeToolClasses;

	/** On-screen readout of the equipped slot, for debugging (the belt widgets show it to the player). */
	UPROPERTY(EditAnywhere, Category = "Toolbelt|Debug")
	bool bShowDebug = true;

	/** Switches to Index, clamped to [0, NumSlots). No-op if already equipped, or if that slot is not available - see IsSlotAvailable. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void EquipSlot(int32 Index);

	/** Moves the equipped slot by the sign of Delta to the next available slot, wrapping around - for the mouse wheel. Slots that are not available are skipped - see IsSlotAvailable. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void CycleTool(int32 Delta);

	/**
	 *  The belt has three independent switches, so a map can mix them (a renovation allows building and demolition together,
	 *  the home allows building only, the hub allows neither):
	 *   - SetLocked: the whole belt. Only the Hands remain, and the legacy Grab/Push bindings take over (see below).
	 *   - SetDestructionAllowed: the demolition tools (the charge and the three melee tools).
	 *   - SetPaintingAllowed: the paint brush. Painting is part of building, so APlayerCharacter::SetBuildingAllowed sets it.
	 *  The Hands are always available.
	 *
	 *  While locked, EquipSlot/CycleTool keep the Hands and the equipped slot is forced back to Hands. It is also the signal
	 *  APlayerCharacter uses to re-enable its legacy, full-featured Grab/Push bindings (push-on-unheld-focus included) instead
	 *  of routing LMB/RMB through this toolbelt - see APlayerCharacter::LegacyGrabInput/LegacyPushInput.
	 */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SetLocked(bool bNewLocked);

	bool IsLocked() const { return bLocked; }

	/**
	 *  Allows or forbids the demolition tools (slots 1 to 4) without locking the rest of the belt. Forbidden, they cannot be
	 *  equipped, the cycle skips them, and a tool already in hand goes back to the Hands. On by default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SetDestructionAllowed(bool bNewAllowed);

	bool IsDestructionAllowed() const { return bDestructionAllowed; }

	/**
	 *  Allows or forbids the paint brush (slot 5), like SetDestructionAllowed does for the tools. Normally set through
	 *  APlayerCharacter::SetBuildingAllowed, since painting belongs to building. On by default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SetPaintingAllowed(bool bNewAllowed);

	bool IsPaintingAllowed() const { return bPaintingAllowed; }

	/**
	 *  Whether the tools have to be bought. When on, a melee tool is usable only if UProgressSubsystem says the player owns it, and every
	 *  charge thrown or placed takes one from the stock. Off by default, so a test map (Lvl_DemoTest) keeps every tool and unlimited
	 *  charges; the hub, mission and home directors turn it on. Independent of the lock, of destruction and of painting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SetOwnershipEnforced(bool bNewEnforced);

	bool IsOwnershipEnforced() const { return bOwnershipEnforced; }

	/** The charges in stock, or INDEX_NONE when charges are unlimited (ownership not enforced). */
	UFUNCTION(BlueprintPure, Category = "Toolbelt")
	int32 GetChargeStock() const;

	bool IsBrushEquipped() const { return EquippedIndex == BrushSlotIndex; }

	/** Whether Index can be equipped right now: in range, and either the Hands or a slot that neither the lock nor its own switch forbids. */
	UFUNCTION(BlueprintPure, Category = "Toolbelt")
	bool IsSlotAvailable(int32 Index) const;

	int32 GetEquippedIndex() const { return EquippedIndex; }
	bool IsHandsEquipped() const { return EquippedIndex == HandsSlotIndex; }

	/** Fired when the equipped slot changes, or when a switch changes what can be equipped. The belt widgets redraw on it. */
	UPROPERTY(BlueprintAssignable, Category = "Toolbelt")
	FOnBeltChanged OnBeltChanged;

	/** The name of a slot as the player reads it ("Sledgehammer"). */
	UFUNCTION(BlueprintPure, Category = "Toolbelt")
	FText GetSlotName(int32 Index) const;

	/** One line on what the two clicks do with a slot. */
	UFUNCTION(BlueprintPure, Category = "Toolbelt")
	FText GetSlotHint(int32 Index) const;

	/**
	 *  Whether the player owns Index. Always true for now: the tools cannot be bought yet, every one is owned from the start.
	 *  When the purchase at the terminal exists, this is the one function to point at the saved list of owned tools.
	 */
	UFUNCTION(BlueprintPure, Category = "Toolbelt")
	bool IsSlotOwned(int32 Index) const;

	/** How many slots the player does not own yet. The belt widgets show them as one padlock slot. */
	UFUNCTION(BlueprintPure, Category = "Toolbelt")
	int32 CountMissingTools() const;

	/** Primary use (LMB) on the currently equipped tool. Slot 0 (Hands): Grab. Slot 1 (Charge): stick a charge on the surface in reach. Melee slots: swing. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void PrimaryUse();

	/** Secondary use (RMB) on the currently equipped tool. Slot 0 (Hands): Throw (no-op if nothing held). Slot 1 (charge): throw one, or detonate the one that is out. The melee tools and the brush have no secondary use. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SecondaryUse();

	/** Primary use released (LMB up) - stops a continuous melee tool (e.g. the jackhammer). No-op for Hands/Charge/discrete tools. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void ReleasePrimaryUse();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	static constexpr int32 HandsSlotIndex = 0;
	static constexpr int32 ChargeSlotIndex = 1;
	static constexpr int32 FirstMeleeSlotIndex = 2;
	static constexpr int32 BrushSlotIndex = 5;

	void EquipSlotInternal(int32 Index);
	/** Spawns the charge and tracks it as ActiveCharge. Null if the class is unset or there is no controller. */
	AExplosiveCharge* SpawnCharge(const FVector& Location, const FRotator& Rotation, ESpawnActorCollisionHandlingMethod CollisionHandling);

	void SpawnAndThrowCharge();

	/** Traces ChargePlaceReach along the view and sticks a new charge on what it hits. No-op while a charge is already out (detonate it first) or if nothing is in reach. */
	void PlaceCharge();

	void SpawnMeleeTools();

	/** SpawnedMeleeTools[Index - FirstMeleeSlotIndex] if Index is a melee slot with a spawned tool, else nullptr. */
	ADemolitionTool* GetMeleeToolForSlot(int32 Index) const;

	UFUNCTION()
	void HandleChargeDestroyed(AActor* DestroyedActor);

	int32 EquippedIndex = HandsSlotIndex;
	/** The id of the tool a slot holds in the save (the row names of DT_ToolShop), or none for a slot that is never sold: the hands, the charges (a stock, not a tool) and the brush. */
	static FName ToolIdForSlot(int32 Index);

	/** Takes one charge from the stock when ownership is enforced, and tells the player when there is none. False means do not spawn. */
	bool TakeCharge();

	/** Gives back a charge TakeCharge took, when the spawn that followed failed. */
	void GiveBackCharge();

	/** The progress changed (a purchase): redraw the belt, and fall back to the hands if the equipped tool is no longer usable. */
	void HandleProgressChanged();

	FDelegateHandle ProgressChangedHandle;

	bool bLocked = false;
	bool bDestructionAllowed = true;
	bool bPaintingAllowed = true;
	bool bOwnershipEnforced = false;

	UPROPERTY()
	TObjectPtr<AExplosiveCharge> ActiveCharge;

	UPROPERTY()
	TArray<TObjectPtr<ADemolitionTool>> SpawnedMeleeTools;
};
