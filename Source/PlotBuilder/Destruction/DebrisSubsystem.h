#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DebrisSubsystem.generated.h"

/**
 *  Global cap on live debris actors (GDD §7.8) - a generic AActor registry so it covers both
 *  AFragmentDebris (generic per-class shards) and the per-mesh Geometry Collection actors
 *  ABuildablePiece::Fracture spawns when a mesh override exists (UDemolitionRules::MeshFractureOverrides).
 *  Oldest-first eviction keeps the total registered "fragment weight" under the cap passed in at
 *  registration (sourced from UDemolitionRules::FragmentGlobalCap by the caller) - this subsystem
 *  never reads that data asset itself, it only does the bookkeeping, same split already used
 *  between UStructuralSupportComponent (mechanism) and UDemolitionRules/UStructuralRules (tuning).
 */
UCLASS()
class UDebrisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/**
	 *  Adds Actor to the registry, evicting the oldest entries first (Destroy()) until the total
	 *  weight is back at or under Cap. Also binds Actor->OnDestroyed so this entry is removed
	 *  automatically whenever Actor is destroyed, by any means (an explicit Destroy() call, its own
	 *  SetLifeSpan timeout, or this subsystem's own eviction) - OnDestroyed is a generic AActor
	 *  delegate, so this works the same for AFragmentDebris and an engine AGeometryCollectionActor
	 *  alike, with no per-class hook needed on either.
	 */
	void RegisterDebrisActor(AActor* Actor, int32 Cap, int32 FragmentWeight = 1);

private:

	struct FDebrisEntry
	{
		TWeakObjectPtr<AActor> Actor;
		int32 Weight = 1;
	};

	/** Bound to every registered actor's OnDestroyed - removes its entry and updates TotalWeight. Safe even if the entry was already removed (e.g. by EvictOldestUntilUnderCap's stale-pointer branch). */
	UFUNCTION()
	void HandleDebrisActorDestroyed(AActor* DestroyedActor);

	/** Destroys the oldest live entries (front of LiveDebris) until TotalWeight is at or under Cap. Relies on AActor::Destroy() firing OnDestroyed synchronously, which is what actually removes the entry via HandleDebrisActorDestroyed - this only self-removes a stale (already-gone) entry directly. */
	void EvictOldestUntilUnderCap(int32 Cap);

	TArray<FDebrisEntry> LiveDebris;
	int32 TotalWeight = 0;
};
