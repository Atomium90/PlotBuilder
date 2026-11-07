#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProgressSubsystem.generated.h"

class UProgressSave;

DECLARE_MULTICAST_DELEGATE(FOnProgressChanged);

/**
 *  The player's progress for the whole session: money, owned tools, parcel. It lives on the game instance, so it
 *  survives every change of map (hub, mission, home), and it is written to the "Progress" save slot at every
 *  change, so it survives closing the game too. Changes are rare (a payment, a purchase), which is why there is
 *  no deferred save.
 *
 *  The player's hotbar keeps a per-map copy of the money (UHotbarComponent::Money, read when the pawn starts and
 *  written back through OnMoneyChanged), so the existing money UI and pb.Money keep working unchanged.
 */
UCLASS()
class UProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Save slot written by this subsystem. */
	static const FString SlotName;

	/** On-screen message key of ShowOnScreen (7600 and up are reserved for the progress and hub readouts). */
	static constexpr int32 MessageKey = 7600;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The subsystem of the game instance that owns WorldContext, or nullptr (no world, or a world without a game instance). */
	static UProgressSubsystem* Get(const UObject* WorldContext);

	int32 GetMoney() const;

	/** Sets the money (never below 0) and saves. Does nothing when the amount does not change. */
	void SetMoney(int32 NewAmount);

	void AddMoney(int32 Delta);

	bool OwnsTool(FName ToolId) const;

	/** Adds the tool to the owned ones and saves. Does nothing if it is already owned. */
	void GrantTool(FName ToolId);

	bool IsParcelOwned() const;

	void SetParcelOwned(bool bOwned);

	/** Back to a new game (UProgressSave::ResetToDefaults), saved. */
	void Reset();

	/** One line with everything the save holds, for the log and the on-screen readout. */
	FString Describe() const;

	/** Prints Describe() on screen for Seconds (pb.ShowProgress). */
	void ShowOnScreen(float Seconds) const;

	/** Fired after every change, once the slot is written. */
	FOnProgressChanged OnProgressChanged;

private:

	/** Writes the slot, then tells the listeners. */
	void Persist();

	static constexpr int32 UserIndex = 0;

	UPROPERTY()
	TObjectPtr<UProgressSave> Progress;
};
