#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FragmentDebris.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  Generic greybox fragment spawned by ABuildablePiece::Fracture - fracture-on-destroy v1 (GDD §7.8):
 *  one procedural shard shape per destroyed piece for any mesh without an authored per-mesh
 *  Geometry Collection override (UDemolitionRules::MeshFractureOverrides - Sprint 2 day 2). Not
 *  interactable, unlike APhysicsProp - purely a short-lived physics prop that fades and destroys
 *  itself. Tagged "Fragment" at spawn, and self-registers with UDebrisSubsystem (global fragment
 *  cap, oldest-first eviction) in Launch - the subsystem unregisters it automatically via the
 *  generic AActor::OnDestroyed delegate, so this class needs no EndPlay override for that.
 */
UCLASS()
class AFragmentDebris : public AActor
{
	GENERATED_BODY()

public:

	AFragmentDebris();

	/**
	 *  Called right after SpawnActor by ABuildablePiece::Fracture - tints the mesh, applies the
	 *  launch impulse, starts the lifetime/fade clock, and registers with UDebrisSubsystem (Cap
	 *  sourced from UDemolitionRules::FragmentGlobalCap by the caller - this class never reads that
	 *  data asset itself). SleepVelocityThreshold (UDemolitionRules::FragmentSleepVelocityThreshold)
	 *  is checked every Tick once the fragment's linear speed drops under it, putting the rigid body
	 *  to sleep once and skipping the check from then on - a cheap win since this actor already ticks
	 *  for the fade, no extra tick added.
	 */
	void Launch(const FVector& Impulse, const FLinearColor& Tint, float Lifetime, float FadeDuration, int32 DebrisCap, float InSleepVelocityThreshold);

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

	/** Checked every Tick while awake - see Launch. 0 keeps the check always-false (never sleeps early), the safe default before Launch runs. */
	float SleepVelocityThreshold = 0.f;

	/** Set once MeshComponent has been put to sleep, so Tick stops checking its velocity every frame. */
	bool bAsleep = false;
};
