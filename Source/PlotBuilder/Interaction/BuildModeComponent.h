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

	/** Translucent material applied to the ghost mesh when the current position/rotation is free to place. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	/** Translucent material applied to the ghost mesh when it would overlap something. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	TObjectPtr<UMaterialInterface> BlockedGhostMaterial;

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

	/**
	 *  Traces from the view for a placed ABuildablePiece. On hit, derives a snap transform from
	 *  the hit piece's world-space mesh bounds (dominant axis/sign of the impact normal decides
	 *  which face - top, side...) and the selected piece's own bounds, so the two sit flush
	 *  against each other. Geometric, not socket-based: zero content authoring, exact for
	 *  box-shaped pieces, approximate for irregular ones.
	 */
	bool TryFindSnapTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation, AActor*& OutSnapTarget) const;

	/** Fallback when TryFindSnapTransform finds nothing to snap onto: view + fixed distance, grid-snapped. */
	void ComputeFreeTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 *  One box overlap test (from the selected piece's own mesh bounds) at the candidate
	 *  transform, against world statics/dynamics/pawns - covers other Buildables, Props, the
	 *  environment and the player capsule with a single cheap query. Ignores the ghost itself and
	 *  SnapTarget (the piece being snapped onto, if any) so a flush fit isn't flagged as blocked.
	 */
	bool IsCandidateBlocked(const FVector& Location, const FRotator& Rotation, AActor* SnapTarget) const;

	/** Applies GhostMaterial or BlockedGhostMaterial to every slot, only when something changed. */
	void UpdateGhostMaterial(bool bBlocked);

	/** How far a box with the given local half-extents and rotation reaches along WorldAxis (a unit vector) - the standard oriented-box-onto-axis projection, correct for any rotation. */
	static float ProjectExtentOntoAxis(const FVector& LocalHalfExtent, const FQuat& Rotation, const FVector& WorldAxis);

	UPROPERTY()
	TObjectPtr<UHotbarComponent> HotbarComponent;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> PreviewActor;

	UPROPERTY()
	TSubclassOf<AActor> LastPreviewedClass;

	bool bIsActive = false;
	int32 RotationSteps = 0;
	bool bIsBlocked = false;

	/** Forces the next UpdateGhostMaterial call to reapply even if the blocked state hasn't changed - set whenever the mesh itself changes, since its slots start with no ghost material assigned. */
	bool bMaterialNeedsRefresh = true;

	/** Blocked state the ghost's material currently reflects - lets UpdateGhostMaterial skip reassigning when nothing changed. */
	bool bLastAppliedBlocked = false;
};
