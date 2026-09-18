#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsManipulationComponent.generated.h"

class UInteractionComponent;
class UPrimitiveComponent;

/**
 *  Push / Pull / Launch: the physical half of the interaction tool. Reads the current focus
 *  from the sibling UInteractionComponent (no separate trace) and acts directly on its physics
 *  body. Grab picks up the current focus (single press, no hold). The force action is a single
 *  contextual button: Launch (strong impulse, releases) if something is held, Push (weaker
 *  impulse, doesn't pick anything up) on the current focus otherwise.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UPhysicsManipulationComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPhysicsManipulationComponent();

	/** Distance in front of the view the held object is pulled towards. */
	UPROPERTY(EditAnywhere, Category = "Physics Manipulation")
	float HoldDistance = 200.f;

	/** Spring strength pulling the held object towards the hold point. */
	UPROPERTY(EditAnywhere, Category = "Physics Manipulation")
	float PullStrength = 2000.f;

	/** Damping applied against the held object's current velocity, to stop it oscillating. */
	UPROPERTY(EditAnywhere, Category = "Physics Manipulation")
	float PullDamping = 200.f;

	/** Outward impulse strength for Push. */
	UPROPERTY(EditAnywhere, Category = "Physics Manipulation")
	float PushImpulseStrength = 600.f;

	/** Forward impulse strength for Launch (releasing a held object). */
	UPROPERTY(EditAnywhere, Category = "Physics Manipulation")
	float LaunchImpulseStrength = 1200.f;

	/** Draw the force vector currently applied to the held/pushed object. */
	UPROPERTY(EditAnywhere, Category = "Physics Manipulation|Debug")
	bool bShowDebug = false;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Grabs the current focus, if it has a simulated physics body and nothing is already held. */
	UFUNCTION(BlueprintCallable, Category = "Physics Manipulation")
	void Grab();

	/** Launches the held object if there is one, otherwise pushes the current focus. */
	UFUNCTION(BlueprintCallable, Category = "Physics Manipulation")
	void Push();

private:

	UPROPERTY()
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> HeldComponent;

	void LaunchHeld();
	void PushFocus();

	FVector GetViewLocationAndForward(FVector& OutForward) const;
	static UPrimitiveComponent* GetPhysicsBody(AActor* Actor);
};
