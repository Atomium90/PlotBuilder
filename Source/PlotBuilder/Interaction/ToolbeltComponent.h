#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToolbeltComponent.generated.h"

class AExplosiveCharge;
class ADemolitionTool;

/**
 *  Owned/equipped demolition tool slot, separate from UHotbarComponent (construction pieces/props)
 *  - see GDD §18.2. Slot 0 (Hands) routes straight to the sibling UPhysicsManipulationComponent:
 *  PrimaryUse Grabs, SecondaryUse Throws (no-op if nothing is held - GDD §14's grab/carry/throw,
 *  deliberately narrower than Lvl_Home's legacy Grab/Push, which also shoves an unheld focus and
 *  stays reachable there through APlayerCharacter's locked-toolbelt gate, not through this class).
 *  Slot 1 (Charge) spawns/
 *  throws an AExplosiveCharge on secondary use, then detonates it on the next press - see
 *  SpawnAndThrowCharge. Slots 2-4 (Sledgehammer/Crowbar/Jackhammer) are ADemolitionTool
 *  instances, spawned once in BeginPlay from MeleeToolClasses and kept for the owner's lifetime
 *  (unlike the consumable charge) - PrimaryUse for those slots just forwards to the matching
 *  instance's own ADemolitionTool::PrimaryUse.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UToolbeltComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UToolbeltComponent();

	/** Slot 0 = Hands, slots 1-4 = the four demolition tools - see GDD §14. */
	UPROPERTY(EditDefaultsOnly, Category = "Toolbelt")
	int32 NumSlots = 5;

	/** Class spawned by slot 1's secondary use - see SpawnAndThrowCharge. */
	UPROPERTY(EditDefaultsOnly, Category = "Toolbelt")
	TSubclassOf<AExplosiveCharge> ExplosiveChargeClass;

	/** Initial toss speed given to a freshly spawned charge. */
	UPROPERTY(EditAnywhere, Category = "Toolbelt")
	float ChargeThrowSpeed = 800.f;

	/** Forward distance from the view point a charge is spawned at, so it doesn't spawn inside the player. */
	UPROPERTY(EditAnywhere, Category = "Toolbelt")
	float ChargeSpawnDistance = 150.f;

	/** One ADemolitionTool class per melee slot (index 0 -> slot 2 Sledgehammer, 1 -> slot 3 Crowbar, 2 -> slot 4 Jackhammer). A slot left unset (e.g. Jackhammer before Thursday) stays a no-op. */
	UPROPERTY(EditDefaultsOnly, Category = "Toolbelt")
	TArray<TSubclassOf<ADemolitionTool>> MeleeToolClasses;

	/** On-screen readout of the equipped slot - there's no real HUD indicator yet (Thursday's task), this is the stand-in. */
	UPROPERTY(EditAnywhere, Category = "Toolbelt|Debug")
	bool bShowDebug = true;

	/** Switches to Index, clamped to [0, NumSlots). No-op if already equipped, or if locked - see SetLocked. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void EquipSlot(int32 Index);

	/** Moves the equipped slot by Delta, wrapping around - for the mouse wheel. No-op if locked - see SetLocked. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void CycleTool(int32 Delta);

	/** While locked, EquipSlot/CycleTool are no-ops and the equipped slot is forced back to Hands. For a build-only context (Lvl_Home): also the signal APlayerCharacter uses to re-enable its legacy, full-featured Grab/Push bindings (push-on-unheld-focus included) instead of routing LMB/RMB through this toolbelt - see APlayerCharacter::LegacyGrabInput/LegacyPushInput. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SetLocked(bool bNewLocked);

	bool IsLocked() const { return bLocked; }

	int32 GetEquippedIndex() const { return EquippedIndex; }
	bool IsHandsEquipped() const { return EquippedIndex == HandsSlotIndex; }

	/** Primary use (LMB) on the currently equipped tool. Slot 0 (Hands): Grab. Slots 1-4: no-op until an actual tool exists (Tuesday onward). */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void PrimaryUse();

	/** Secondary use (RMB) on the currently equipped tool. Slot 0 (Hands): Throw (no-op if nothing held). Slots 1-4: no-op until an actual tool exists (Tuesday onward). */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SecondaryUse();

	/** Primary use released (LMB up) - stops a continuous melee tool (e.g. the jackhammer). No-op for Hands/Charge/discrete tools. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void ReleasePrimaryUse();

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	static constexpr int32 HandsSlotIndex = 0;
	static constexpr int32 ChargeSlotIndex = 1;
	static constexpr int32 FirstMeleeSlotIndex = 2;

	void EquipSlotInternal(int32 Index);
	void SpawnAndThrowCharge();
	void SpawnMeleeTools();

	/** SpawnedMeleeTools[Index - FirstMeleeSlotIndex] if Index is a melee slot with a spawned tool, else nullptr. */
	ADemolitionTool* GetMeleeToolForSlot(int32 Index) const;

	UFUNCTION()
	void HandleChargeDestroyed(AActor* DestroyedActor);

	int32 EquippedIndex = HandsSlotIndex;
	bool bLocked = false;

	UPROPERTY()
	TObjectPtr<AExplosiveCharge> ActiveCharge;

	UPROPERTY()
	TArray<TObjectPtr<ADemolitionTool>> SpawnedMeleeTools;
};
