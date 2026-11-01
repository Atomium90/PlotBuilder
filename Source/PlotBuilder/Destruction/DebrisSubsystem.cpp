#include "DebrisSubsystem.h"
#include "GameFramework/Actor.h"

void UDebrisSubsystem::RegisterDebrisActor(AActor* Actor, int32 Cap, int32 FragmentWeight)
{
	if (!Actor)
	{
		return;
	}

	LiveDebris.Add({Actor, FragmentWeight});
	TotalWeight += FragmentWeight;
	Actor->OnDestroyed.AddDynamic(this, &UDebrisSubsystem::HandleDebrisActorDestroyed);

	EvictOldestUntilUnderCap(Cap);
}

void UDebrisSubsystem::HandleDebrisActorDestroyed(AActor* DestroyedActor)
{
	const int32 Index = LiveDebris.IndexOfByPredicate([DestroyedActor](const FDebrisEntry& Entry)
	{
		return Entry.Actor.Get() == DestroyedActor;
	});

	if (Index != INDEX_NONE)
	{
		TotalWeight -= LiveDebris[Index].Weight;
		LiveDebris.RemoveAt(Index);
	}
}

void UDebrisSubsystem::EvictOldestUntilUnderCap(int32 Cap)
{
	while (TotalWeight > Cap && LiveDebris.Num() > 0)
	{
		const FDebrisEntry Entry = LiveDebris[0];

		if (AActor* Actor = Entry.Actor.Get())
		{
			// Destroy() fires OnDestroyed synchronously here, which is what actually updates
			// LiveDebris/TotalWeight via HandleDebrisActorDestroyed - nothing more to do once this returns.
			Actor->Destroy();
		}
		else
		{
			// Stale weak pointer (actor already gone some other way) - nothing will fire
			// OnDestroyed for it, so drop the bookkeeping ourselves.
			TotalWeight -= Entry.Weight;
			LiveDebris.RemoveAt(0);
		}
	}
}
