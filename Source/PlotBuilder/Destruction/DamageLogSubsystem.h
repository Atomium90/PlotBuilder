#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DemolitionTypes.h"
#include "DamageLogSubsystem.generated.h"

/**
 *  The last damaging hits of the world, for tuning and the demo (pb.ShowDamageLog). One feed point:
 *  UDurabilityComponent::ApplyDamage calls Record, so every tool, an impact and a debug command show up
 *  the same way and nothing else has to know the log exists. It records all the time, even while hidden,
 *  so turning the display on shows what just happened.
 *
 *  Hits of the same tool on the same material in the same frame are merged into one line ("Charge | Brick
 *  x12"), otherwise one explosion would flood the screen. The display goes through PBDebug::Message, so
 *  pb.Debug 0 hides it with the other developer displays.
 */
UCLASS()
class UDamageLogSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Adds one damaging hit, already applied: HPLeft and State are the target's values after it. Merges into an entry of the same frame with the same tool and material. */
	void Record(const FDamageInfo& DamageInfo, const AActor* Target, EDemolitionMaterial Material, float HPLeft, float MaxHP, EDurabilityState State);

	/** Shows or hides the on-screen list (pb.ShowDamageLog). Showing draws what is already recorded. */
	void SetShowLog(bool bShow);

	bool IsShowingLog() const { return bShowLog; }

private:

	struct FEntry
	{
		double Time = 0.0;
		uint64 Frame = 0;
		FName ToolId;
		FString TargetName;
		EDemolitionMaterial Material = EDemolitionMaterial::Wood;
		EDurabilityState State = EDurabilityState::Intact;
		float Damage = 0.f;
		/** Tool x material factor of the first hit; shown on single-hit lines only. */
		float Multiplier = 1.f;
		float HPLeft = 0.f;
		float MaxHP = 0.f;
		/** Hits merged into this line. */
		int32 Count = 1;
		/** How many of them ended in Destroyed. */
		int32 Destroyed = 0;
	};

	/** Draws the header, the entries (newest first) and the totals, each on its own message key. */
	void Redraw() const;

	/** Takes the lines down. */
	void ClearScreen() const;

	static FString Describe(const FEntry& Entry);
	static FColor ColourFor(const FEntry& Entry);

	/** Oldest first; the newest is the last element. */
	TArray<FEntry> Entries;

	int32 TotalHits = 0;
	int32 TotalDestroyed = 0;
	float TotalDamage = 0.f;
	bool bShowLog = false;

	/** Entries kept. */
	static constexpr int32 MaxEntries = 10;

	/** Seconds a line stays up after its last redraw. */
	static constexpr float DisplaySeconds = 8.f;

	/** On-screen keys, inside the 7710-7721 block reserved for this log: the header, then one per entry, then the totals. */
	static constexpr int32 HeaderMessageKey = 7710;
	static constexpr int32 FirstEntryMessageKey = 7711;
	static constexpr int32 TotalsMessageKey = 7721;
};
