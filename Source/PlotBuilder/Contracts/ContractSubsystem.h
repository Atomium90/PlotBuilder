#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Contracts/ContractTypes.h"
#include "ContractSubsystem.generated.h"

class UContractPool;
struct FClientRow;

/**
 *  The contract board for the whole session (GDD §6.4): the three offers, the contract the player took, and the report of
 *  the last mission. It lives on the game instance, like UProgressSubsystem, so it survives every change of map: the hub
 *  draws the offers, the mission reads the active contract to build its house, and the report waits here for the hub.
 *
 *  Offers are not saved: a new set is drawn each time the game starts, and again each time a mission ends. The drawing
 *  itself is UContractPool's; this class only holds the state and the random stream.
 */
UCLASS()
class UContractSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Offers on the board at a time (GDD §6.4). */
	static constexpr int32 OfferCount = 3;

	/** First on-screen message key of the contract readouts. 7310 to 7329 are reserved for them (one per offer, then the active contract, then the debrief). */
	static constexpr int32 MessageKey = 7310;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The subsystem of the game instance that owns WorldContext, or nullptr (no world, or a world without a game instance). */
	static UContractSubsystem* Get(const UObject* WorldContext);

	/**
	 *  The pool the offers are drawn from. The hub and mission directors hand theirs in when their level starts; a null
	 *  pool is ignored, so the one already set stays. A different pool makes the offers stale.
	 */
	void SetPool(UContractPool* NewPool);

	UContractPool* GetPool() const { return Pool; }

	/** The row of a client id in the pool's client table (name, role, portrait, tint), or nullptr: the screens then show the id itself. */
	const FClientRow* FindClient(FName ClientId) const;

	/**
	 *  The tier the board is drawn for: a kind of contract whose Tier is above it is not offered. 0 by default, which is every
	 *  kind of the current pool. Nothing earns tiers yet (reputation is Block 3): pb.SetTier is the only way to move it, and it
	 *  is not saved. A different tier makes the offers stale.
	 */
	int32 GetPlayerTier() const { return PlayerTier; }
	void SetPlayerTier(int32 NewTier);

	/**
	 *  Throws the board away and draws OfferCount new offers. A Seed of 0 or more restarts the random stream from it (the
	 *  same seed then gives the same offers, for tests); -1 goes on with the stream as it is. Does nothing, with a warning,
	 *  when there is no pool.
	 */
	void RefreshOffers(int32 Seed = -1);

	/** Draws new offers only if the board is empty or a mission has ended since the last draw. The hub calls it when it starts. */
	void EnsureFreshOffers();

	const TArray<FContractDefinition>& GetOffers() const { return Offers; }

	/** Takes offer Index (0 based) as the active contract. False, nothing changed, if there is no such offer. */
	bool AcceptOffer(int32 Index);

	bool HasActiveContract() const { return bHasActive; }

	/** The contract the player took. Only meaningful when HasActiveContract. */
	const FContractDefinition& GetActiveContract() const { return Active; }

	/** Forgets the active contract: the next mission falls back to its travel options and editor values. */
	void ClearActiveContract();

	/**
	 *  A mission has ended with that report: it is kept for the hub (and marked as not shown yet), the active contract is
	 *  closed and the board is renewed on return. Returns the money now due to the player: the payout if there was an active
	 *  contract, 0 if there was none (a mission started from travel options has no client to pay) or if the contract was
	 *  already completed. The caller adds it to the player's money, so a contract can never be paid twice.
	 */
	int32 CompleteActiveContract(const FDebriefResult& Result);

	bool HasLastResult() const { return bHasLastResult; }

	/** The report of the last mission that ended. Only meaningful when HasLastResult. */
	const FDebriefResult& GetLastResult() const { return LastResult; }

	/** The last report has not been shown to the player yet (the hub shows it once, on arrival). */
	bool HasUnseenResult() const { return bResultUnseen; }

	/** Hands out the last report if it has not been shown yet and marks it as shown. False, OutResult untouched, when it was already. GetLastResult still returns it. */
	bool ConsumeUnseenResult(FDebriefResult& OutResult);

	/** One line for a contract: client, type, archetype, requested size, area, time, pay, conditions, seed. */
	static FString Describe(const FContractDefinition& Contract);

	/** The board as text: one line per offer, numbered from 1, then the active contract if there is one. */
	FString DescribeBoard() const;

	/** Prints the board on screen for Seconds (pb.ShowOffers), one key per line. */
	void ShowOnScreen(float Seconds) const;

	/** Fired when a new set of offers is on the board. */
	FSimpleMulticastDelegate OnOffersChanged;

	/** Fired when the active contract is taken, forgotten or completed. */
	FSimpleMulticastDelegate OnActiveContractChanged;

private:

	UPROPERTY()
	TObjectPtr<UContractPool> Pool;

	UPROPERTY()
	TArray<FContractDefinition> Offers;

	UPROPERTY()
	FContractDefinition Active;

	UPROPERTY()
	FDebriefResult LastResult;

	bool bHasActive = false;
	bool bHasLastResult = false;
	bool bResultUnseen = false;

	int32 PlayerTier = 0;

	/** The board needs a new draw: true at the start, and after a mission ends. */
	bool bOffersStale = true;

	FRandomStream OfferStream;
};
