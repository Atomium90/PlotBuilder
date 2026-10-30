#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FragmentDebris.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  Generic greybox fragment spawned by ABuildablePiece::Fracture - fracture-on-destroy v1 (GDD §7.8):
 *  one procedural shard shape per destroyed piece, not an authored per-mesh Geometry Collection
 *  (that's UFractureLibrary, Sprint 2). Not interactable, unlike APhysicsProp - purely a short-lived
 *  physics prop that fades and destroys itself. Tagged "Fragment" at spawn so a future
 *  UDebrisSubsystem can enumerate/cap live fragments (GetAllActorsWithTag) without this class
 *  needing to change.
 */
UCLASS()
class AFragmentDebris : public AActor
{
	GENERATED_BODY()

public:

	AFragmentDebris();

	/** Called right after SpawnActor by ABuildablePiece::Fracture - tints the mesh, applies the launch impulse, and starts the lifetime/fade clock. */
	void Launch(const FVector& Impulse, const FLinearColor& Tint, float Lifetime, float FadeDuration);

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;

	/**
	 *  Tint material swapped onto the mesh at BeginPlay, if set - a BP child of this class assigns
	 *  it (see DA_DemolitionRules::FragmentClass, same C++ base + BP wrapper split as
	 *  ADemolitionTool/BP_Sledgehammer). Reads an RGB tint from Custom Primitive Data indices 0-2 -
	 *  see Launch. Plain opaque material, no translucency needed: fade-out is done by scaling the
	 *  mesh to zero (see Tick), not by animating opacity. Left unset, the fragment still times out
	 *  and destroys, just without a tint - a missing placeholder material is not an error.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Fragment")
	TObjectPtr<UMaterialInterface> FadeMaterial;

	/** MeshComponent's scale right after spawn (post size-jitter) - Tick lerps towards zero from this during the fade window, rather than from whatever scale happens to be set. */
	FVector InitialScale = FVector(1.f);

	float LifetimeSeconds = 6.f;
	float FadeDurationSeconds = 1.f;
	float Age = 0.f;
};
