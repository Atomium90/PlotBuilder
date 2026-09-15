#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Generic contract for anything a UInteractionComponent can focus and interact with.
 *  Push/Pull/Launch are a separate, physics-specific concern handled by UPhysicsManipulationComponent.
 */
class IInteractable
{
	GENERATED_BODY()

public:

	/** Called once when this actor becomes the current focus of an InteractionComponent. */
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnFocusBegin(AActor* Instigator);
	virtual void OnFocusBegin_Implementation(AActor* Instigator);

	/** Called once when this actor stops being the current focus. */
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnFocusEnd(AActor* Instigator);
	virtual void OnFocusEnd_Implementation(AActor* Instigator);

	/** Generic interaction entry point (use/open/activate...). */
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Instigator);
	virtual void Interact_Implementation(AActor* Instigator);
};
