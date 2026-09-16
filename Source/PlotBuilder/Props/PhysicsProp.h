#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "PhysicsProp.generated.h"

class UStaticMeshComponent;

/**
 *  Generic physics-simulated prop (crate, barrel, furniture...). Mass and friction are tuned
 *  per Blueprint variant via the mesh's own physics properties, nothing hardcoded in C++.
 *  Implements IInteractable so UInteractionComponent can focus it. Push/Pull/Launch manipulation
 *  goes through UPhysicsManipulationComponent directly on the mesh's physics body, not through
 *  Interact; Interact is only used to pick the prop up into a UHotbarComponent.
 */
UCLASS()
class APhysicsProp : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	APhysicsProp();

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	virtual void Interact_Implementation(AActor* InteractingActor) override;

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;
};
