#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "Destruction/Damageable.h"
#include "Destruction/DemolitionTypes.h"
#include "PhysicsProp.generated.h"

class UStaticMeshComponent;
class UDurabilityComponent;
class ABuildablePiece;

/** How a kind of prop is meant to be set down (see APhysicsProp::GetPlacement). */
struct FPropPlacement
{
	/** Hung on a wall, back against it, at the height chosen (a mirror, a cabinet) instead of standing on the floor. */
	bool bHangsOnWall = false;

	/** Degrees added to the yaw that points the mesh's local +X at the front, so a mesh that faces another way turns the right way round. */
	float FrontYawOffset = 0.f;
};

/**
 *  Generic physics-simulated prop (crate, barrel, furniture...). Mass and friction are tuned
 *  per Blueprint variant via the mesh's own physics properties, nothing hardcoded in C++.
 *  Implements IInteractable so UInteractionComponent can focus it. Push/Pull/Launch manipulation
 *  goes through UPhysicsManipulationComponent directly on the mesh's physics body, not through
 *  Interact; Interact is only used to pick the prop up into a UHotbarComponent. Implements
 *  IDamageable (breakable furniture, GDD §10) via the same UDurabilityComponent as
 *  ABuildablePiece, but initialized with a flat per-prop MaxHP instead of the Material x
 *  PieceClass lookup - furniture has no EPieceClass of its own (see the comment on EPieceClass).
 */
UCLASS(abstract)
class APhysicsProp : public AActor, public IInteractable, public IDamageable
{
	GENERATED_BODY()

public:

	APhysicsProp();

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	UDurabilityComponent* GetDurabilityComponent() const { return DurabilityComponent; }

	virtual void Interact_Implementation(AActor* InteractingActor) override;
	virtual void ApplyDamage_Implementation(const FDamageInfo& DamageInfo) override;

	/**
	 *  Hangs this prop on Wall (a mirror, a cabinet): the body stops simulating, so it stays where the generator put it,
	 *  and starts again (Release) as soon as the wall takes any damage, starts to collapse or is destroyed. Only the wall's
	 *  fate is watched, the prop itself still takes tool damage. Does nothing if already mounted or Wall is null.
	 */
	void MountOn(ABuildablePiece* Wall);

	/** Lets go of the wall (see MountOn) and wakes the physics body, so the prop falls, and the hands tool can grab it. Safe to call on a prop that is not mounted. */
	void Release();

	/**
	 *  Sets the prop down where it stands, the way a wall is placed: the body stops simulating, so it stays exactly where the build mode
	 *  put it, and the hands cannot grab it (E takes it back into the bag where building is allowed). Support is what it stands on. If that is
	 *  a piece of the kit the prop is mounted on it (MountOn), so damage or collapse of the piece, or its removal, lets the prop fall. Anything
	 *  else (the ground, level geometry, another prop) holds it for good. Used by the build mode and by the home when it puts its props back.
	 */
	void Place(AActor* Support);

	/**
	 *  Where a prop class goes, read from DT_FurnishingRules, the table the house generator furnishes with (the row whose PropClass is this
	 *  class). One table therefore says it for everyone who sets a prop down: the generator, the build mode and the home. A class without a
	 *  row, or no table, gives the defaults (stands on the floor).
	 */
	static FPropPlacement GetPlacement(TSubclassOf<APhysicsProp> PropClass);

	bool IsMounted() const { return bMounted; }

	/** What this prop is made of - drives the fragment tint and the tool x material matrix, same as ABuildablePiece::Material. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	EDemolitionMaterial Material = EDemolitionMaterial::Wood;

	/** Flat hit points for this prop - set per Blueprint class default (GDD §10: "furniture HP is set per prop", not computed from a piece-class factor). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	float MaxHP = 50.f;

	/** Worth of the prop at full health, in money. Set from the DT_FurnishingRules row when the house generator spawns it; the contract report reads it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
	int32 Value = 0;

	/** How many generic AFragmentDebris shards to spawn when this prop breaks - see UDemolitionRules::SpawnGenericFragments. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction")
	int32 FragmentCount = 4;

	/** Whether a hard enough contact hurts this prop (a chair thrown at a wall, furniture caught in a collapse). Turn off for a prop that must survive anything. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Impact")
	bool bTakesImpactDamage = true;

	/** Speed change at a contact, in cm/s, under which nothing happens. A 1 m drop is about 440 cm/s, so the default keeps drops, rolls and settling harmless. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Impact", meta = (ClampMin = "0"))
	float ImpactSpeedThreshold = 600.f;

	/** Damage per cm/s above the threshold. With the defaults a 1500 cm/s hit deals 72 damage, more than the 50 HP of a default prop, and a 1000 cm/s hit deals 32. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Impact", meta = (ClampMin = "0"))
	float ImpactDamagePerSpeed = 0.08f;

	/** Seconds after BeginPlay during which contacts are ignored: the generator, the home restore and the hotbar drop props that settle against the floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Impact", meta = (ClampMin = "0"))
	float ImpactGraceSeconds = 1.f;

	/** Minimum seconds between two damaging contacts: one bounce reports several. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Destruction|Impact", meta = (ClampMin = "0"))
	float ImpactMinInterval = 0.2f;

protected:

	virtual void BeginPlay() override;

private:

	/** MeshComponent->OnComponentHit handler: turns the speed change of a contact into damage on DurabilityComponent when it is above ImpactSpeedThreshold. */
	UFUNCTION()
	void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** The three ways the wall under MountedOn can stop holding the prop: hit, destroyed (UDurabilityComponent and AActor events), collapsing (ABuildablePiece::OnCollapseStarted). All end in Release. */
	UFUNCTION()
	void HandleMountDamaged(const FDamageInfo& DamageInfo);

	UFUNCTION()
	void HandleMountDestroyed(AActor* DestroyedActor);

	/** The wall this prop hangs on, while bMounted. */
	TWeakObjectPtr<ABuildablePiece> MountedOn;

	bool bMounted = false;

	/** World time of BeginPlay, for the grace period. */
	float SpawnTime = 0.f;

	/** World time of the last contact that dealt damage, for the minimum interval. */
	float LastImpactTime = -1000.f;

	/** DurabilityComponent->OnDurabilityDestroyed handler: spawns generic fragments and removes this prop - furniture's "simple break on destruction", no per-mesh fracture. */
	UFUNCTION()
	void HandleBroken();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UDurabilityComponent* DurabilityComponent;
};
