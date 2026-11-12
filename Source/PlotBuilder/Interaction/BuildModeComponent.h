#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Buildables/BuildAnchor.h"
#include "Props/PhysicsProp.h"
#include "BuildModeComponent.generated.h"

class UHotbarComponent;
class UMaterialInterface;
class AStaticMeshActor;
class APlot;
class ABuildablePiece;
class APhysicsProp;

/**
 *  Placement flow for ABuildablePiece, separate from the instant placement UHotbarComponent::TryPlace
 *  still does for APhysicsProp. Two placement paths, chosen per piece via ABuildablePiece::BuildAnchor:
 *
 *  - Legacy (the original behaviour, untouched): the ghost snaps flush against whatever placed piece
 *    or surface the view trace hits (TryFindSnapTransform), or falls back to a fixed distance in
 *    front of the view (ComputeFreeTransform). Geometric, not grid-aware - exact for box-shaped
 *    pieces snapping onto a bounds-centered pivot, approximate otherwise.
 *  - Cell / Edge / Vertex (the greybox kit): the ghost snaps to an integer grid cell instead
 *    (TryResolveGridPlacement), via a ray/plane intersection at the current BuildLevel rather than a
 *    trace against existing geometry - no bounds math, no flush-against-neighbour snap.
 *
 *  Either way, one obstruction test per tick decides whether the current transform is blocked (red)
 *  or free (green) to confirm - IsCandidateBlocked for Legacy, IsGridCandidateBlocked for the grid
 *  path (plot bounds, occupancy table, a ground-clipped overlap, and structural stability).
 *
 *  A third path is for props (APhysicsProp, the furniture of the hotbar): they are placed like a wall, not
 *  dropped. The ghost goes where the view lands on a surface, whatever the BuildLevel (TryFindPropTransform),
 *  turns by a finer step, uses the Legacy obstruction test, and the confirmed prop is set down frozen on what it
 *  stands on (APhysicsProp::Place), taken back with E like a piece.
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

	/** Translucent material applied to the ghost mesh when it would overlap something, fall outside the plot, or land on an occupied slot - a hard block, the game refusing the placement outright. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	TObjectPtr<UMaterialInterface> BlockedGhostMaterial;

	/** Translucent material applied to the ghost mesh when placement is allowed but the resulting stability would be below UStructuralRules::ComfortThreshold - a warning, not a block: placing here is a choice, including one that collapses the piece right after (below CollapseThreshold). */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	TObjectPtr<UMaterialInterface> RiskyGhostMaterial;

	/** Yaw step size, in degrees, applied per RotatePreview call - Legacy pieces only, grid pieces use a fixed step per EBuildAnchor (90 deg for Cell, 180 deg for Edge, none for Vertex). */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	float RotationStepDegrees = 45.f;

	/** Yaw step, in degrees, of a prop's preview (a piece uses RotationStepDegrees or its anchor's step): finer, since furniture is arranged by eye. */
	UPROPERTY(EditAnywhere, Category = "Build Mode|Props", meta = (ClampMin = "1", ClampMax = "90"))
	float PropRotationStepDegrees = 15.f;

	/** A prop's position is rounded to this size, in cm, on the floor (0 = exactly where the view lands). Far finer than the grid of the pieces. */
	UPROPERTY(EditAnywhere, Category = "Build Mode|Props", meta = (ClampMin = "0"))
	float PropSnapSize = 10.f;

	/** How far down, in cm, a prop looks for a floor when the view lands on a wall or something steep: it stands on the floor beside it. */
	UPROPERTY(EditAnywhere, Category = "Build Mode|Props", meta = (ClampMin = "0"))
	float PropDropDistance = 300.f;

	/** Highest BuildLevel ChangeBuildLevel will reach - level 0 is the ground floor. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	int32 MaxBuildLevel = 3;

	/** Highest RoofStep ChangeRoofStep will reach - see FStructuralSocket::RoofStep. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	int32 MaxRoofStep = 10;

	/** How far out, in cm, the grid path's view-ray/placement-plane intersection is allowed to reach. Independent of UHotbarComponent::PlaceDistance, which only matters for the Legacy fallback. */
	UPROPERTY(EditAnywhere, Category = "Build Mode")
	float MaxGridPlaceDistance = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Build Mode|Debug")
	bool bShowDebug = false;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Enters Build Mode if inactive, exits it otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void ToggleBuildMode();

	/** Steps the ghost's yaw/orientation by Direction (+1/-1). Behaviour depends on the previewed piece's BuildAnchor - see the class comment. No-op outside Build Mode. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void RotatePreview(int32 Direction);

	/** Changes BuildLevel by Direction (+1/-1), clamped to [0, MaxBuildLevel]. No-op outside Build Mode. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void ChangeBuildLevel(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	int32 GetBuildLevel() const { return BuildLevel; }

	/** Changes RoofStep by Direction (+1/-1), clamped to [0, MaxRoofStep] - see FStructuralSocket::RoofStep. No-op outside Build Mode. Never reset automatically (not tied to BuildLevel or to switching the selected piece), so climbing a slope across several piece picks keeps the same RoofStep until changed on purpose. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void ChangeRoofStep(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	int32 GetRoofStepValue() const { return RoofStepValue; }

	/** Spawns the previewed item at the ghost's current transform, consuming it from the hotbar. */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void ConfirmPlacement();

	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	bool IsBuildModeActive() const { return bIsActive; }

	/**
	 *  Whether Build Mode can be entered at all. A mission turns it off (APlayerCharacter::SetBuildingAllowed):
	 *  turning it off while active leaves Build Mode first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Build Mode")
	void SetAllowed(bool bNewAllowed);

	UFUNCTION(BlueprintPure, Category = "Build Mode")
	bool IsAllowed() const { return bAllowed; }

private:

	bool bAllowed = true;

	void EnterBuildMode();
	void ExitBuildMode();
	void RefreshPreviewMesh();
	void UpdatePreviewTransform();

	/**
	 *  Legacy path. Traces from the view for a placed ABuildablePiece. On hit, derives a snap
	 *  transform from the hit piece's world-space mesh bounds (dominant axis/sign of the impact
	 *  normal decides which face - top, side...) and the selected piece's own bounds, so the two sit
	 *  flush against each other. Geometric, not socket-based: zero content authoring, exact for
	 *  box-shaped pieces whose pivot sits at their mesh's bounds center, approximate otherwise (see
	 *  the bounds-Origin correction in the .cpp - still approximate for a non-box mesh, just no
	 *  longer wrong for an off-center pivot like the greybox foundation/slab/walls).
	 */
	bool TryFindSnapTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation, AActor*& OutSnapTarget) const;

	/** The class being previewed as a piece, or nullptr when it is a prop (or nothing). The one safe way to read the piece's defaults. */
	const ABuildablePiece* GetPreviewedPiece() const;

	/** The class being previewed as a prop, or nullptr when it is a piece (or nothing). */
	const APhysicsProp* GetPreviewedProp() const;

	/**
	 *  Prop path. A prop is not placed on the grid or on a plane at the BuildLevel: it goes where the view lands. Traces the view on the
	 *  interaction channel; a floor-like hit (normal pointing up) is where the mesh's bottom rests, with the position rounded to PropSnapSize;
	 *  anything steeper (a wall) puts the mesh's box flush against it, centred on the aimed point, on the floor at the wall's foot. False when nothing is in reach or
	 *  there is no floor to stand on. OutSupport is the actor it stands on (a piece, the ground...), which the obstruction test ignores and
	 *  the prop is mounted on once placed.
	 */
	bool TryFindPropTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation, AActor*& OutSupport) const;

	/** Legacy path fallback when TryFindSnapTransform finds nothing to snap onto: view + fixed distance, grid-snapped. */
	void ComputeFreeTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation) const;

	/**
	 *  Legacy path obstruction test: one box overlap (from the selected piece's own mesh bounds) at
	 *  the candidate transform, against world statics/dynamics/pawns - covers other Buildables,
	 *  Props, the environment and the player capsule with a single cheap query. Ignores the ghost
	 *  itself and SnapTarget (the piece being snapped onto, if any) so a flush fit isn't flagged as
	 *  blocked. Also blocked if a Plot exists in the level and the candidate falls outside it. Never
	 *  blocks on stability alone - see IsCandidateRisky for that.
	 */
	bool IsCandidateBlocked(const FVector& Location, const FRotator& Rotation, AActor* SnapTarget) const;

	/** Whether the candidate's resulting stability would fall under UStructuralRules::ComfortThreshold - a warning (yellow ghost), never a reason to block placement by itself. False for a piece with no sockets, or with no Plot/structural component in the level. */
	bool IsCandidateRisky(const ABuildablePiece* SelectedCDO, const FVector& Location, const FRotator& Rotation) const;

	/**
	 *  Grid path. Intersects the view ray with the horizontal placement plane at the current
	 *  BuildLevel (+ the piece's own VerticalOffset), then snaps the hit point to the nearest valid
	 *  grid position for Anchor: both coordinates even for Cell, both odd for Vertex, and for Edge
	 *  the nearer of the two odd/even candidates (which also picks the base yaw, 0 or 90). No trace
	 *  against existing geometry, no bounds math.
	 */
	bool TryResolveGridPlacement(const FVector& ViewLocation, const FRotator& ViewRotation, EBuildAnchor Anchor, FVector& OutLocation, FRotator& OutRotation, FIntVector& OutGridCoord, int32& OutQuarterTurns) const;

	/**
	 *  Grid path obstruction test, in this order: outside the plot, the (Anchor, GridCoord) slot
	 *  already occupied, a ground-clipped physics overlap that's blind to other ABuildablePiece
	 *  (those are governed by the occupancy table, not raw overlap - two pieces are expected to touch
	 *  at a shared edge/vertex). Never blocks on stability alone - see IsGridCandidateRisky for that.
	 *  LastBlockReason is set for the debug display.
	 */
	bool IsGridCandidateBlocked(const FVector& Location, const FRotator& Rotation, EBuildAnchor Anchor, const FIntVector& GridCoord, int32 QuarterTurns, int32 RoofStepForOccupancy) const;

	/** Whether the candidate's resulting stability would fall under UStructuralRules::ComfortThreshold - a warning (yellow ghost), never a reason to block placement by itself. False for a piece with no sockets, or with no Plot/structural component in the level. */
	bool IsGridCandidateRisky(const ABuildablePiece* SelectedCDO, const FIntVector& GridCoord, int32 QuarterTurns) const;

	/** Applies GhostMaterial, RiskyGhostMaterial or BlockedGhostMaterial to every slot (in that priority: a hard block wins over a stability warning), only when something changed. */
	void UpdateGhostMaterial(bool bBlocked, bool bRisky);

	/** How far a box with the given local half-extents and rotation reaches along WorldAxis (a unit vector) - the standard oriented-box-onto-axis projection, correct for any rotation. */
	static float ProjectExtentOntoAxis(const FVector& LocalHalfExtent, const FQuat& Rotation, const FVector& WorldAxis);

	UPROPERTY()
	TObjectPtr<UHotbarComponent> HotbarComponent;

	/** The level's plot, if any. Resolved once in BeginPlay - single-plot demo for now, see APlot. */
	UPROPERTY()
	TObjectPtr<APlot> CachedPlot;

	UPROPERTY()
	TObjectPtr<AStaticMeshActor> PreviewActor;

	UPROPERTY()
	TSubclassOf<AActor> LastPreviewedClass;

	/** Mirror state RefreshPreviewMesh's ghost mesh currently reflects, for a MirrorVariantClass piece - see RefreshPreviewMesh. Meaningless otherwise. */
	bool bLastPreviewedMirrored = false;

	bool bIsActive = false;
	int32 RotationSteps = 0;
	bool bIsBlocked = false;
	bool bIsRisky = false;
	int32 BuildLevel = 0;
	int32 RoofStepValue = 0;

	/** What the prop ghost stands on this tick (see TryFindPropTransform) - consumed by ConfirmPlacement. Meaningless for a piece. */
	TWeakObjectPtr<AActor> PendingSupport;

	/** How the previewed prop class is set down (hung on a wall or standing), read once when the class changes. Meaningless for a piece. */
	FPropPlacement PreviewPlacement;

	/** Grid coordinate/orientation the ghost resolved to this tick, for a non-Legacy piece - consumed by ConfirmPlacement. Meaningless for a Legacy piece. */
	FIntVector PendingGridCoord = FIntVector::ZeroValue;
	int32 PendingQuarterTurns = 0;

	/** Why the grid path's ghost is currently blocked (or "free") - for the debug display only. */
	mutable FString LastBlockReason;

	/** Forces the next UpdateGhostMaterial call to reapply even if the blocked state hasn't changed - set whenever the mesh itself changes, since its slots start with no ghost material assigned. */
	bool bMaterialNeedsRefresh = true;

	/** Blocked/risky state the ghost's material currently reflects - lets UpdateGhostMaterial skip reassigning when nothing changed. */
	bool bLastAppliedBlocked = false;
	bool bLastAppliedRisky = false;
};
