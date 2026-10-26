#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToolbeltComponent.generated.h"

/**
 *  Owned/equipped demolition tool slot, separate from UHotbarComponent (construction pieces/props)
 *  - see GDD §18.2. Today's skeleton only tracks the equipped slot and routes primary/secondary
 *  use; slot 0 is reserved for Hands, slots 1-4 for the four demolition tools once they exist
 *  (Tuesday-Thursday).
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

	/** Primary use (LMB) on the currently equipped tool. No-op until an actual tool exists (Tuesday onward). */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void PrimaryUse();

	/** Secondary use (RMB) on the currently equipped tool. No-op until an actual tool exists (Tuesday onward). */
	UFUNCTION(BlueprintCallable, Category = "Toolbelt")
	void SecondaryUse();

private:

	int32 EquippedIndex = 0;
};
