#include "DamageLogSubsystem.h"
#include "Debug/DebugDisplay.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UDamageLogSubsystem::Record(const FDamageInfo& DamageInfo, const AActor* Target, EDemolitionMaterial Material, float HPLeft, float MaxHP, EDurabilityState State)
{
	++TotalHits;
	TotalDamage += DamageInfo.Amount;

	if (State == EDurabilityState::Destroyed)
	{
		++TotalDestroyed;
	}

	// Only this frame's entries can take the hit, and they are always the last ones: walk back while the frame matches.
	for (int32 Index = Entries.Num() - 1; Index >= 0 && Entries[Index].Frame == GFrameCounter; --Index)
	{
		FEntry& Existing = Entries[Index];

		if (Existing.ToolId == DamageInfo.ToolId && Existing.Material == Material)
		{
			++Existing.Count;
			Existing.Damage += DamageInfo.Amount;
			Existing.Destroyed += State == EDurabilityState::Destroyed ? 1 : 0;

			if (bShowLog)
			{
				Redraw();
			}

			return;
		}
	}

	if (Entries.Num() >= MaxEntries)
	{
		Entries.RemoveAt(0);
	}

	FEntry Entry;
	Entry.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Entry.Frame = GFrameCounter;
	Entry.ToolId = DamageInfo.ToolId;
	Entry.TargetName = Target ? Target->GetClass()->GetName() : FString(TEXT("?"));
	Entry.TargetName.RemoveFromEnd(TEXT("_C"));
	Entry.Material = Material;
	Entry.State = State;
	Entry.Damage = DamageInfo.Amount;
	Entry.Multiplier = DamageInfo.Multiplier;
	Entry.HPLeft = HPLeft;
	Entry.MaxHP = MaxHP;
	Entry.Destroyed = State == EDurabilityState::Destroyed ? 1 : 0;
	Entries.Add(Entry);

	if (bShowLog)
	{
		Redraw();
	}
}

void UDamageLogSubsystem::SetShowLog(bool bShow)
{
	bShowLog = bShow;

	if (bShowLog)
	{
		Redraw();
	}
	else
	{
		ClearScreen();
	}
}

void UDamageLogSubsystem::Redraw() const
{
	PBDebug::Message(HeaderMessageKey, DisplaySeconds, FColor::White,
		FString::Printf(TEXT("Damage log, newest first (last %d)"), MaxEntries));

	for (int32 Slot = 0; Slot < MaxEntries; ++Slot)
	{
		const int32 Key = FirstEntryMessageKey + Slot;
		const int32 EntryIndex = Entries.Num() - 1 - Slot;

		if (Entries.IsValidIndex(EntryIndex))
		{
			PBDebug::Message(Key, DisplaySeconds, ColourFor(Entries[EntryIndex]), Describe(Entries[EntryIndex]));
		}
		else if (GEngine)
		{
			GEngine->RemoveOnScreenDebugMessage(Key);
		}
	}

	PBDebug::Message(TotalsMessageKey, DisplaySeconds, FColor::Silver,
		FString::Printf(TEXT("Total: %d hits, %.1f damage, %d destroyed"), TotalHits, TotalDamage, TotalDestroyed));
}

void UDamageLogSubsystem::ClearScreen() const
{
	if (!GEngine)
	{
		return;
	}

	GEngine->RemoveOnScreenDebugMessage(HeaderMessageKey);
	GEngine->RemoveOnScreenDebugMessage(TotalsMessageKey);

	for (int32 Slot = 0; Slot < MaxEntries; ++Slot)
	{
		GEngine->RemoveOnScreenDebugMessage(FirstEntryMessageKey + Slot);
	}
}

FString UDamageLogSubsystem::Describe(const FEntry& Entry)
{
	const FString Tool = Entry.ToolId.IsNone() ? FString(TEXT("?")) : Entry.ToolId.ToString();
	const FString Material = StaticEnum<EDemolitionMaterial>()->GetNameStringByValue(static_cast<int64>(Entry.Material));

	if (Entry.Count > 1)
	{
		return FString::Printf(TEXT("[%6.1fs] %s | %s x%d | -%.1f | %d destroyed"),
			Entry.Time, *Tool, *Material, Entry.Count, Entry.Damage, Entry.Destroyed);
	}

	const FString State = StaticEnum<EDurabilityState>()->GetNameStringByValue(static_cast<int64>(Entry.State));

	return FString::Printf(TEXT("[%6.1fs] %s | %s | %s | -%.1f (x%.2f) | %.0f/%.0f HP | %s"),
		Entry.Time, *Tool, *Entry.TargetName, *Material, Entry.Damage, Entry.Multiplier, Entry.HPLeft, Entry.MaxHP, *State);
}

FColor UDamageLogSubsystem::ColourFor(const FEntry& Entry)
{
	if (Entry.Destroyed > 0)
	{
		return FColor::Red;
	}

	switch (Entry.State)
	{
	case EDurabilityState::Critical:
		return FColor::Orange;
	case EDurabilityState::Damaged:
		return FColor::Yellow;
	default:
		return FColor::White;
	}
}
