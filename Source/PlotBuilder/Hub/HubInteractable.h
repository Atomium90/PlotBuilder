#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "HubInteractable.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHubInteracted, AActor*, User);

/**
 *  Base of the things the player uses in the hub with E (the PC, the truck door, the home access point): an
 *  invisible box on the interaction trace channel that UInteractionComponent can focus, a prompt shown on
 *  screen while it is focused, and a Use hook the subclass fills in. Nothing here knows the player or the
 *  levels: the interaction component only needs IInteractable, so no change was needed on the player side.
 *
 *  Place the box around the mesh it stands for (the truck, the shutter door, the monitor) and size it in the
 *  editor. It must stick out of the mesh by about 20 cm on every side: the mesh blocks the interaction trace
 *  too, so a box that is smaller than it, or hidden inside it, is never hit and nothing gets focused. It does
 *  not block walking: it only answers the interaction trace.
 */
UCLASS(Abstract)
class AHubInteractable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	AHubInteractable();

	/** Shown to the player (UPlayerMessages) while they look at this. Subclasses set a default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	FText Prompt;

	/** Fired when the player uses it, before the subclass acts. A Blueprint can hook it. */
	UPROPERTY(BlueprintAssignable, Category = "Hub")
	FOnHubInteracted OnInteracted;

	virtual void OnFocusBegin_Implementation(AActor* User) override;
	virtual void OnFocusEnd_Implementation(AActor* User) override;
	virtual void Interact_Implementation(AActor* User) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** What using it does. Empty by default. */
	virtual void Use(AActor* User) {}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hub")
	TObjectPtr<UBoxComponent> Trigger;
};
