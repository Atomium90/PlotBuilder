#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "StructuralSocket.h"
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
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	TArray<FStructuralSocket> Sockets;

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

	UPROPERTY()
	TObjectPtr<UStructuralSupportComponent> StructuralComponent;
};
