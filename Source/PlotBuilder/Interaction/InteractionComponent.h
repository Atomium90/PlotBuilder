#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

/**
 *  Traces from the owner's view point, tracks the current IInteractable focus and relays
 *  interaction requests to it. Owner-agnostic: works on any Pawn with a Controller.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UInteractionComponent();

	/** How far, in cm, the interaction trace reaches. */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float TraceDistance = 300.f;

	/** Draw the trace line every tick (green when focused, red otherwise). */
	UPROPERTY(EditAnywhere, Category = "Interaction|Debug")
	bool bShowDebugTrace = false;

	/** Show the focused actor's name on screen. */
	UPROPERTY(EditAnywhere, Category = "Interaction|Debug")
	bool bShowDebugFocusName = false;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Calls Interact on the current focus, if any. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/** Actor currently focused by the trace, or nullptr. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetCurrentFocus() const { return CurrentFocus; }

private:

	void UpdateFocus();

	UPROPERTY()
	TObjectPtr<AActor> CurrentFocus;
};
