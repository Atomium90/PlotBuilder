#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProgressSave.generated.h"

/**
 *  What the player keeps from one session to the next (GDD §18): the money, the tools bought, and whether
 *  the parcel is owned. UProgressSubsystem holds the one live copy and writes it to the "Progress" slot.
 *  The snapshot of the player's house (pieces, props, inventory) joins it with the home persistence.
 */
UCLASS()
class UProgressSave : public USaveGame
{
	GENERATED_BODY()

public:

	/** Money a new game starts with (Sprint 3 plan: the sledgehammer and 500). */
	static constexpr int32 DefaultMoney = 500;

	UPROPERTY(SaveGame)
	int32 Money = DefaultMoney;

	/**
	 *  Ids of the tools the player owns. Nothing reads this yet: the tool belt still spawns every tool. The tool
	 *  purchase at the PC (Nov 11) makes it the source of truth, so the ids below are placeholders until then.
	 */
	UPROPERTY(SaveGame)
	TSet<FName> OwnedTools;

	UPROPERTY(SaveGame)
	bool bParcelOwned = false;

	/** A new game: DefaultMoney, the hands and the sledgehammer, no parcel. */
	void ResetToDefaults();
};
