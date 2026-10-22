#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "StructuralSocket.h"
#include "BuildAnchor.h"
#include "BuildablePiece.generated.h"

class UStaticMeshComponent;
class UStructuralSupportComponent;

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
class ABuildablePiece : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	ABuildablePiece();

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildable")
	int32 Cost = 0;

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

	/** Vertical offset from the current build level's placement plane, in cm - 0 for everything except future roof pieces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Structural")
	float VerticalOffset = 0.f;

	/** Vertical spacing this piece advances the build level by, in cm - the floor-to-floor height for normal pieces, something finer for future roof pieces. Not consumed by anything yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Structural")
	float VerticalStep = 300.f;

	/**
	 *  Whether this piece passes its own stability on to whatever rests on it (a load-bearing wall
	 *  holding up a floor/roof) or not (a partition - receives stability from what it stands on,
	 *  same as any other piece, but transmits none upward). Same mesh either way - see "Murs
	 *  porteurs et cloisons" in claude/Kit greybox specs des pieces et plan de remplacement.md.
	 *  Structural pieces other than walls (floor, post, roof...) have no reason to ever flip this.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	bool bIsLoadBearing = true;

	/** How long a Collapsing piece simulates physics before being destroyed, in seconds. */
	UPROPERTY(EditAnywhere, Category = "Structural")
	float CollapseLifeSpan = 4.f;

	virtual void Interact_Implementation(AActor* InteractingActor) override;

	const TArray<FStructuralSocket>& GetSockets() const { return Sockets; }
	float GetStability() const { return Stability; }
	void SetStability(float NewStability) { Stability = NewStability; }
	bool IsStructuralAnchor() const { return bIsStructuralAnchor; }
	void SetIsStructuralAnchor(bool bNewValue) { bIsStructuralAnchor = bNewValue; }
	bool IsLoadBearing() const { return bIsLoadBearing; }
	void SetStructuralComponent(UStructuralSupportComponent* Component) { StructuralComponent = Component; }

	EBuildAnchor GetBuildAnchor() const { return BuildAnchor; }
	bool IsFaceAnchoredEdge() const { return bFaceAnchoredEdge; }
	float GetVerticalOffset() const { return VerticalOffset; }
	float GetVerticalStep() const { return VerticalStep; }
	FIntVector GetGridCoord() const { return GridCoord; }
	uint8 GetOrientation() const { return Orientation; }

	/** Called once by UBuildModeComponent::ConfirmPlacement right after spawning a non-Legacy piece - never meaningful for a Legacy one. */
	void SetGridPlacement(const FIntVector& NewGridCoord, uint8 NewOrientation) { GridCoord = NewGridCoord; Orientation = NewOrientation; }

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

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;

	float Stability = 0.f;
	bool bIsStructuralAnchor = false;

	/** Set by UBuildModeComponent::ConfirmPlacement for a grid-anchored piece - (0,0,0) and ignored for Legacy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structural", meta = (AllowPrivateAccess = "true"))
	FIntVector GridCoord = FIntVector::ZeroValue;

	/** Quarter turns (0-3) around Z, set alongside GridCoord - ignored for Legacy, which reads its orientation from the actor's actual rotation instead. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structural", meta = (AllowPrivateAccess = "true"))
	uint8 Orientation = 0;

	UPROPERTY()
	TObjectPtr<UStructuralSupportComponent> StructuralComponent;
};
