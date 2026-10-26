#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToolbeltComponent.generated.h"

/**
 *  Owned/equipped demolition tool slot, separate from UHotbarComponent (construction pieces/props)
 *  - see GDD §18.2. Slot 0 (Hands) routes straight to the sibling UPhysicsManipulationComponent
 *  (Grab/Push), same as it already works in Lvl_Home - no separate Grab/Push bindings needed in
 *  the mission input context, LMB/RMB only ever mean one thing there. Slots 1-4 (the four
 *  demolition tools) are still stubs until they exist (Tuesday-Thursday).
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

	/** Switches to Index, clamped to [0, NumSlots). No-op if already equipped. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void EquipSlot(int32 Index);

	/** Moves the equipped slot by Delta, wrapping around - for the mouse wheel. */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void CycleTool(int32 Delta);

	int32 GetEquippedIndex() const { return EquippedIndex; }
	bool IsHandsEquipped() const { return EquippedIndex == HandsSlotIndex; }

	/** Primary use (LMB) on the currently equipped tool. Slot 0 (Hands): Grab. Slots 1-4: no-op until an actual tool exists (Tuesday onward). */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void PrimaryUse();

	/** Secondary use (RMB) on the currently equipped tool. Slot 0 (Hands): Push/Launch. Slots 1-4: no-op until an actual tool exists (Tuesday onward). */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SecondaryUse();

private:

	static constexpr int32 HandsSlotIndex = 0;

	int32 EquippedIndex = HandsSlotIndex;
};
