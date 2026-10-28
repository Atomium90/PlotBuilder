#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemolitionTypes.h"
#include "ExplosiveCharge.generated.h"

class UStaticMeshComponent;
class UDemolitionRules;
class USoundBase;

/**
 *  First demolition tool (GDD §7.4) - thrown/dropped physics actor, manual detonation. Mirrors
 *  APhysicsProp's mesh/collision setup (simulates physics, BlockAllDynamic) since it's tossed and
 *  left to settle, not attached to anything. UToolbeltComponent owns spawning/throwing it (slot 1)
 *  and calls RequestDetonation on the second RMB press.
 */
UCLASS(abstract)
class AExplosiveCharge : public AActor
{
	GENERATED_BODY()

public:

	AExplosiveCharge();

	/** Flat damage dealt to everything in Radius - see GDD §7.4 ("400 flat"). */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	float Damage = 400.f;

	/** Blast radius in cm - see GDD §7.4 ("radius 4 m"). */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	float Radius = 400.f;

	/** Radial impulse strength applied to nearby simulating bodies, no falloff - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	float ImpulseStrength = 1500.f;

	/** Delay between RequestDetonation and the actual blast, for the beep/warning - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	float DetonationDelay = 0.4f;

	/** DA_DemolitionRules asset - set once per Blueprint class default, same pass as UDurabilityComponent::Rules. */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	TObjectPtr<UDemolitionRules> Rules;

	/** Explosion sound, played from the mesh's "Blast" socket on detonation. Typically a SoundCue (e.g. SC_Charge_Explosion) that picks a random variant and adds pitch/volume jitter - the random pick deliberately isn't in code, so this stays a plain USoundBase and doesn't care whether it's one SoundWave or a Cue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	TObjectPtr<USoundBase> ExplosionSound;

	/** Called right after SpawnActor to give the charge its initial toss velocity. */
	UFUNCTION(BlueprintCallable, Category = "Charge")
	void Throw(const FVector& Velocity);

	/** Secondary action while this charge is the equipped tool's active charge. No-op if already counting down to detonation. */
	UFUNCTION(BlueprintCallable, Category = "Charge")
	void RequestDetonation();

protected:

	/** Scales RadiusPreviewComponent so it visually matches Radius regardless of which placeholder mesh the Blueprint assigns it. */
	virtual void BeginPlay() override;

	/** Hook for the beep sound/visual warning - BP_ExplosiveCharge implements it, no new C++ needed per charge variant. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Charge")
	void OnDetonationRequested();

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Translucent sphere scaled to Radius, visible for the charge's whole armed lifetime - doubles as the gameplay blast-radius indicator and the visual debug tool (GDD §7.4). BP assigns the mesh/material. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> RadiusPreviewComponent;

	void Detonate();

	bool bIsDetonating = false;
	FTimerHandle DetonationTimerHandle;
};
