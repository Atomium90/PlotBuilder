#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DestructibleComponent.generated.h"

/**
 *  Marks an actor as Chaos-destructible and lets other systems detonate it on command, without
 *  touching how it actually breaks. All the Chaos setup (fracture levels, anchoring, damage
 *  thresholds, debris cleanup) lives on the actor's own Geometry Collection, authored in the
 *  editor's Fracture Mode - a thrown APhysicsProp already fractures it on contact with no code
 *  involved (Enable Damage From Collision). This component only covers triggers that don't go
 *  through a physics collision (demolition tools, detection zones): Detonate spawns a pre-tuned
 *  field actor (a Field System Blueprint, e.g. a duplicate of Epic's FS_MasterField) at a given
 *  location, which does the actual work of applying strain/force fields to the Geometry Collection.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UDestructibleComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDestructibleComponent();

	/** Field System actor to spawn on Detonate - falloff shape, activation and strain magnitude are tuned on this class in the editor, not from code. */
	UPROPERTY(EditAnywhere, Category = "Destructible")
	TSubclassOf<AActor> ChargeFieldClass;

	/** How long the spawned charge actor lives before it's destroyed, in seconds - just long enough to apply its field once. */
	UPROPERTY(EditAnywhere, Category = "Destructible")
	float ChargeLifeSpan = 0.1f;

	/** Spawns ChargeFieldClass at WorldLocation, scaled uniformly by Scale. No-op if ChargeFieldClass isn't set. */
	UFUNCTION(BlueprintCallable, Category = "Destructible")
	void Detonate(FVector WorldLocation, float Scale = 1.f);
};
