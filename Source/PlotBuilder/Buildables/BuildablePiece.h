#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Interaction/Interactable.h"
#include "Destruction/Damageable.h"
#include "Destruction/DemolitionTypes.h"
#include "StructuralSocket.h"
#include "BuildAnchor.h"
#include "BuildablePiece.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UStructuralSupportComponent;
class UDurabilityComponent;

UENUM(BlueprintType)
enum class EBuildablePieceState : uint8
{
	Stock,
	Held,
	Placed,
	/** Lost its structural support (or a piece it rested on was demolished): simulating physics, about to be destroyed. Never re-enters Interact/Demolish while in this state. */
	Collapsing
};

/**
 *  How far, in cm, a slab or a stair flight pulls back from each of its four world-axis sides because an
 *  exterior wall stands there (D21, visual only). MinX is the side facing -X, MaxX the one facing +X, and so on.
 */
struct FWallClearance
{
	float MinX = 0.f;
	float MaxX = 0.f;
	float MinY = 0.f;
	float MaxY = 0.f;

	bool IsZero() const { return MinX <= 0.f && MaxX <= 0.f && MinY <= 0.f && MaxY <= 0.f; }
};

/**
 *  Construction piece (wall, floor, roof, door, window...). Never simulates physics while Stock/
 *  Held/Placed, unlike APhysicsProp: it's acquired via Interact into a UHotbarComponent, then
 *  placed kinematically (fixed transform, no Push/Pull/Launch). The one deliberate exception is
 *  Collapsing (see BeginCollapse) - a piece that has lost structural support has left that cycle
 *  entirely and become debris, same category as Chaos gravats, which Push/Pull/Launch already
 *  covers. Cost is authored per-Blueprint for now, independent of the shop catalog's own
 *  FShopItemRow::Cost - the two aren't wired together yet (see UHotbarComponent::ShopCatalog),
 *  since there's no economy system to enforce either value.
 */
UCLASS(abstract)
class ABuildablePiece : public AActor, public IInteractable, public IDamageable
{
	GENERATED_BODY()

public:

	ABuildablePiece();

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	UDurabilityComponent* GetDurabilityComponent() const { return DurabilityComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildable")
	int32 Cost = 0;

	/** What this piece is made of - see GDD §7.2. Set per-Blueprint class default, not per-instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	EDemolitionMaterial Material = EDemolitionMaterial::Wood;

	/** This piece's role for the HP class factor lookup - see GDD §7.3. Set per-Blueprint class default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	EPieceClass PieceClass = EPieceClass::ExteriorWall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable")
	EBuildablePieceState State = EBuildablePieceState::Stock;

	/**
	 *  Structural attachment points, declared per-Blueprint (never authored on the mesh - see
	 *  claude/Kit greybox specs des pieces et plan de remplacement.md). Read by
	 *  UStructuralSupportComponent to find this piece's neighbors.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Structural")
	TArray<FStructuralSocket> Sockets;

	/** How this piece is positioned in Build Mode - see EBuildAnchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Structural")
	EBuildAnchor BuildAnchor = EBuildAnchor::Legacy;

	/**
	 *  Edge pieces only: true if this piece's mesh is pivoted on one face rather than centered on the
	 *  edge (BP_Wall/BP_WallDoor/BP_WallWindow - the mesh extends towards -X local from the pivot, see
	 *  claude/Kit greybox specs des pieces et plan de remplacement.md). Lets TryResolveGridPlacement
	 *  auto-pick which of the two possible yaws (base or base+180) faces the mesh towards the floor
	 *  side rather than out over empty space. Interior partitions (BP_InteriorWall/
	 *  BP_InteriorWallDoor) stay centered on the edge and leave this false - no inside/outside to pick.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	bool bFaceAnchoredEdge = false;

	/**
	 *  Edge pieces only, for an asymmetric mesh that needs a true left/right mirror rather than a
	 *  180 yaw (e.g. a gable half) - set on the "canonical" Blueprint, pointing at its mirrored
	 *  counterpart. A plain 180 yaw can't mirror an asymmetric mesh, and combined with
	 *  bFaceAnchoredEdge it would just relocate the piece onto the other flanking cell instead -
	 *  see TryResolveGridPlacement's Edge branch. When set, BuildModeComponent reuses the same
	 *  Edge "flip" toggle (RotatePreview) to pick which of the two classes to preview/spawn instead
	 *  of rotating, so the player still only ever sees one hotbar entry. Left null for every piece
	 *  that doesn't need this (the overwhelming majority).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	TSubclassOf<ABuildablePiece> MirrorVariantClass;

	/** Vertical offset from the current build level's placement plane, in cm - 0 for everything except roof/gable pieces (see the greybox spec doc's VerticalOffset = -20 note). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Structural")
	float VerticalOffset = 0.f;

	/**
	 *  Whether this piece passes its own stability on to whatever rests on it (a load-bearing wall
	 *  holding up a floor/roof) or not (a partition - receives stability from what it stands on,
	 *  same as any other piece, but transmits none upward). Same mesh either way - see "Murs
	 *  porteurs et cloisons" in claude/Kit greybox specs des pieces et plan de remplacement.md.
	 *  Structural pieces other than walls (floor, post, roof...) have no reason to ever flip this.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	bool bIsLoadBearing = true;

	/**
	 *  Sub-anchor slab (GDD §8, D28): resets this piece's stability to 1.0, same as a real Anchor
	 *  socket, but only once its own incoming chain is itself valid (see
	 *  UStructuralSupportComponent::RelaxStability and UStructuralRules::SubAnchorMinimumIncomingStability).
	 *  Fixes tall stacks hitting the collapse threshold around the fifth storey even at full health -
	 *  a plain visual/material swap on the floor slab BP, same footprint and height, nothing else
	 *  about the piece changes. False for every other piece, including the real foundation (which
	 *  uses an Anchor socket instead, unconditionally 1.0).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	bool bResetsStability = false;

	/** How long a Collapsing piece simulates physics before being destroyed, in seconds. */
	UPROPERTY(EditAnywhere, Category = "Structural")
	float CollapseLifeSpan = 4.f;

	virtual void Interact_Implementation(AActor* InteractingActor) override;
	virtual void ApplyDamage_Implementation(const FDamageInfo& DamageInfo) override;

	const TArray<FStructuralSocket>& GetSockets() const { return Sockets; }
	float GetStability() const { return Stability; }
	void SetStability(float NewStability) { Stability = NewStability; }
	bool IsStructuralAnchor() const { return bIsStructuralAnchor; }
	void SetIsStructuralAnchor(bool bNewValue) { bIsStructuralAnchor = bNewValue; }
	bool IsLoadBearing() const { return bIsLoadBearing; }
	bool ResetsStability() const { return bResetsStability; }
	void SetStructuralComponent(UStructuralSupportComponent* Component) { StructuralComponent = Component; }

	EBuildAnchor GetBuildAnchor() const { return BuildAnchor; }
	bool IsFaceAnchoredEdge() const { return bFaceAnchoredEdge; }
	float GetVerticalOffset() const { return VerticalOffset; }
	FIntVector GetGridCoord() const { return GridCoord; }
	uint8 GetOrientation() const { return Orientation; }
	int32 GetRoofStep() const { return RoofStep; }

	/**
	 *  The scale APlot::PlacePiece was given, before the corner, height and wall-clearance biases touched the actor's
	 *  own. The home snapshot stores this one: restoring the biased scale would bias it a second time on every visit.
	 */
	const FVector& GetPlacedScale() const { return PlacedScale; }
	void SetPlacedScale(const FVector& NewScale) { PlacedScale = NewScale; }

	/** Called once by UBuildModeComponent::ConfirmPlacement right after spawning a non-Legacy piece - never meaningful for a Legacy one. NewRoofStep defaults to 0 - meaningless for anything but a Roof_* piece. */
	void SetGridPlacement(const FIntVector& NewGridCoord, uint8 NewOrientation, int32 NewRoofStep = 0) { GridCoord = NewGridCoord; Orientation = NewOrientation; RoofStep = NewRoofStep; }

	/**
	 *  This piece's position in the structural grid: GridCoord directly for a grid-anchored piece,
	 *  or the Legacy fallback (world transform converted via the owning UStructuralSupportComponent)
	 *  otherwise. Only meaningful once StructuralComponent is set (see SetStructuralComponent),
	 *  i.e. after this piece has gone through RegisterPiece at least once.
	 */
	FIntVector GetStructuralCell() const;

	/** The rotation half of GetStructuralCell() - see there. */
	int32 GetStructuralQuarterTurns() const;

	/** Voluntary removal (Interact on a Placed piece): detaches from the structural graph, then destroys itself outright - unlike BeginCollapse, this piece chose to leave, so it doesn't become debris. */
	UFUNCTION(BlueprintCallable, Category = "Buildable")
	void Demolish();

	/** Involuntary removal: this piece's stability fell below the collapse threshold (see UStructuralSupportComponent::RecomputeStability), usually because something it depended on was just demolished or collapsed itself. Topples under physics instead of vanishing, then cascades by leaving the structural graph. Safe to call more than once. */
	void BeginCollapse();

	/** Writes Stability into the mesh's Custom Primitive Data (index 0) for the debug color gradient - see UStructuralSupportComponent::bShowDebug. */
	void UpdateStabilityDebugColor();

	/**
	 *  Pulls this piece back from the sides that touch an exterior wall (D21, visual only): the actor's scale and
	 *  location are recomputed from the transform it was placed with and the mesh bounds, so the pivot does not
	 *  matter and a zero clearance restores the full piece. Called by UStructuralSupportComponent whenever a wall
	 *  next to this piece is registered or removed. Does nothing unless the piece is Placed (a falling piece is
	 *  simulating physics) or is warning as Compromised (the jiggle owns its location). Never touches the graph:
	 *  GetGridCoord() and the sockets are unchanged.
	 */
	void ApplyWallClearance(const FWallClearance& Clearance);

	/** Called by UStructuralSupportComponent::StartCompromised (GDD §7.6, D29): starts the warning placeholders (creak loop, dust, a small jiggle) while this piece counts down to failing on its own. Safe to call once per Compromised episode - the component only calls it on a fresh queue entry. */
	void OnCompromisedStart();

	/** Called by UStructuralSupportComponent::CancelCompromised: stops the warning placeholders started by OnCompromisedStart - D29's "dependants destroyed first" un-Compromise case. No-op if not currently Compromised. */
	void OnCompromisedCancelled();

protected:

	virtual void BeginPlay() override;

private:

	/** DurabilityComponent->OnDurabilityDamaged / OnDurabilityStateChanged handler: writes the HP ratio into the mesh's Custom Primitive Data (index 1) for the damage tint - see UpdateStabilityDebugColor for index 0. */
	UFUNCTION()
	void HandleDurabilityDamaged(const FDamageInfo& DamageInfo);

	UFUNCTION()
	void HandleDurabilityStateChanged(EDurabilityState OldState, EDurabilityState NewState);

	/** DurabilityComponent->OnDurabilityDestroyed handler: this piece was killed directly (tool damage reached 0 HP), not by losing structural support - fractures in place immediately, no Falling phase, then destroys itself. */
	UFUNCTION()
	void HandleDurabilityDestroyed();

	/** MeshComponent->OnComponentHit handler, bound only while Collapsing - first significant impact (above MinFractureImpactSpeed) fractures the piece early instead of waiting out CollapseLifeSpan. */
	UFUNCTION()
	void HandleCollapseHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** CollapseLifeSpan timer handler: a Falling piece that never took a significant hit still fractures on timeout, instead of silently vanishing - see BeginCollapse. */
	void HandleCollapseTimeout();

	/** CompromisedJiggleHandle tick: a small, cheap sine offset on MeshComponent's relative location around CompromisedBaseRelativeLocation, read-only visual noise - no physics, stopped by OnCompromisedCancelled/BeginCollapse. */
	void HandleCompromisedJiggle();

	/**
	 *  Shared fracture entry point for both HandleDurabilityDestroyed (direct kill, no fall) and
	 *  HandleCollapseHit/HandleCollapseTimeout (cascade release, after falling) - GDD §7.8's v1:
	 *  generic fragments per EPieceClass, not per mesh (see DA_DemolitionRules). Spawns the fragments,
	 *  does not destroy this actor - callers do that right after. ScatterBias is a direction (not a
	 *  pre-scaled impulse): blended into each fragment's own outward direction before scaling by
	 *  Rules->FragmentImpulseStrength, so a falling piece's collapse direction bias (see BeginCollapse)
	 *  carries through to its fragments too - ZeroVector for a direct kill, which has no fall to bias from.
	 */
	void Fracture(const FVector& ScatterOrigin, const FVector& ScatterBias) const;

	void UpdateDamageVisual();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UDurabilityComponent* DurabilityComponent;

	float Stability = 0.f;
	bool bIsStructuralAnchor = false;

	/** Timer backing the CollapseLifeSpan fracture-on-timeout fallback - see BeginCollapse/HandleCollapseTimeout. */
	FTimerHandle CollapseTimeoutHandle;

	/** Guards against fracturing twice if HandleCollapseHit and HandleCollapseTimeout both fire (e.g. hit arrives the same frame the timer does). */
	bool bHasFractured = false;

	/** Set once in BeginCollapse (StructuralSupportComponent::ComputeAwayFromNeighbourCentroid) - the horizontal direction this piece fell away from, reused both for its own release impulse and as Fracture's ScatterBias once it fractures. ZeroVector for a direct kill (HandleDurabilityDestroyed never sets it). */
	FVector CollapseBiasDirection = FVector::ZeroVector;

	/** Minimum impact speed (cm/s) on MeshComponent's hit event for a Falling piece to fracture early - below this, a grazing contact is ignored and the piece keeps falling. */
	UPROPERTY(EditAnywhere, Category = "Structural")
	float MinFractureImpactSpeed = 200.f;

	/** Looping creak placeholder spawned by OnCompromisedStart, stopped/destroyed by OnCompromisedCancelled or whichever collapse path fires next - see GDD §7.6, D29. */
	UPROPERTY()
	TObjectPtr<class UAudioComponent> CompromisedWarningAudio;

	/** MeshComponent->GetRelativeLocation() cached by OnCompromisedStart, before HandleCompromisedJiggle starts offsetting it - restored by OnCompromisedCancelled so a cancelled warning never leaves the mesh stuck off-pivot. */
	FVector CompromisedBaseRelativeLocation = FVector::ZeroVector;

	/** Timer driving HandleCompromisedJiggle - only runs while Compromised, never a full Tick (this actor doesn't otherwise need one). */
	FTimerHandle CompromisedJiggleHandle;

	/** Amplitude (cm) of the Compromised warning's positional jiggle. */
	UPROPERTY(EditAnywhere, Category = "Structural|Debug")
	float CompromisedJiggleAmplitude = 0.4f;

	/** Location and scale the piece was placed with, kept by the first ApplyWallClearance so the clearance is always computed from them and never stacks. */
	FVector ClearanceBaseLocation = FVector::ZeroVector;
	FVector ClearanceBaseScale = FVector::OneVector;
	bool bClearanceBaseCaptured = false;

	/** Set by UBuildModeComponent::ConfirmPlacement for a grid-anchored piece - (0,0,0) and ignored for Legacy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structural", meta = (AllowPrivateAccess = "true"))
	FIntVector GridCoord = FIntVector::ZeroValue;

	/** Quarter turns (0-3) around Z, set alongside GridCoord - ignored for Legacy, which reads its orientation from the actor's actual rotation instead. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structural", meta = (AllowPrivateAccess = "true"))
	uint8 Orientation = 0;

	/** Fine vertical step, set alongside GridCoord - see FStructuralSocket::RoofStep. Always 0 and meaningless for anything but a Roof_* piece climbing a slope or stacking a gable infill. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structural", meta = (AllowPrivateAccess = "true"))
	int32 RoofStep = 0;

	/** See GetPlacedScale. */
	FVector PlacedScale = FVector::OneVector;

	UPROPERTY()
	TObjectPtr<UStructuralSupportComponent> StructuralComponent;
};
