#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuildModeComponent.generated.h"

class UHotbarComponent;
class UMaterialInterface;
class AStaticMeshActor;

/**
 *  Dedicated placement flow for ABuildablePiece, separate from the instant placement
 *  UHotbarComponent::TryPlace still does for APhysicsProp. While active, ticks a single reusable
 *  ghost actor (translucent preview) that tracks the view at a fixed distance, grid-snapped, and
 *  can be rotated in fixed steps before being confirmed. The ghost's mesh/material are swapped in
 *  place when the hotbar selection changes - never respawned - so switching previewed items costs
 *  a mesh/material assignment, not an actor spawn.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UBuildModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBuildModeComponent();

	/** Translucent material applied to every material slot of the ghost mesh. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	/** Yaw step size, in degrees, applied per RotatePreview call. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	float RotationStepDegrees = 45.f;

	UPROPERTY(EditAnywhere, Category = "Build Mode|Debug")
	bool bShowDebug = true;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Enters Build Mode if inactive, exits it otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void ToggleBuildMode();

	/** Steps the ghost's yaw by Direction (+1/-1) * RotationStepDegrees. No-op outside Build Mode. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void RotatePreview(int32 Direction);

	/** Spawns the previewed item at the ghost's current transform, consuming it from the hotbar. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void ConfirmPlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	bool IsBuildModeActive() const { return bIsActive; }

private:

	void EnterBuildMode();
	void ExitBuildMode();
	void RefreshPreviewMesh();
	void UpdatePreviewTransform();

	UPROPERTY()
	TObjectPtr<UHotbarComponent> HotbarComponent;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> PreviewActor;

	UPROPERTY()
	TSubclassOf<AActor> LastPreviewedClass;

	bool bIsActive = false;
	int32 RotationSteps = 0;
};
