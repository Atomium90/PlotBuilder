#pragma once

#include "CoreMinimal.h"
#include "Hub/HubInteractable.h"
#include "TravelPoint.generated.h"

class UWorld;

/** What using a travel point does besides opening its level. */
UENUM(BlueprintType)
enum class ETravelRole : uint8
{
	/** Opens Destination and nothing else: the home's access point, a debug door. */
	Plain,
	/** The truck in the hub: refuses to leave until the player has taken a contract at the PC. */
	StartMission,
	/** The truck on the mission site: ends the mission (report, payment, debrief for the hub), then travels. Asks first if the job is not finished. */
	EndMission
};

/**
 *  A door to another level: using it opens Destination. One class serves the truck (hub to mission, with a
 *  random seed), the home's access point (hub to Lvl_Home, only once the parcel is owned) and the way back
 *  (mission to hub); what changes is data on the placed actor, chiefly TravelRole.
 *
 *  The mission level reads the contract the player took from UContractSubsystem (AMissionDirector); the travel options
 *  below are only the fallback for a trip with no contract.
 */
UCLASS()
class ATravelPoint : public AHubInteractable
{
	GENERATED_BODY()

public:

	ATravelPoint();

	/** The level to open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	TSoftObjectPtr<UWorld> Destination;

	/** What using it does besides travelling. See ETravelRole. (Not named Role: AActor already has a replication Role.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	ETravelRole TravelRole = ETravelRole::Plain;

	/** StartMission only: let the truck leave with no contract taken (a debug trip: the mission then falls back to the travel options and the level's values). Off for the game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel", meta = (EditCondition = "TravelRole == ETravelRole::StartMission"))
	bool bAllowWithoutContract = false;

	/** Travel options, without the first "?", separated by "?" (for example "Archetype=twostorey?Type=partial"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	FString Options;

	/** Adds a Seed option drawn at random each time: two trips give two different houses (pb.SetSeed still overrides it in the mission). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	bool bRandomSeed = false;

	/** Refuses to travel until the player owns the parcel (the home's access point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	bool bRequiresParcel = false;

	/** The options a trip made now would carry: Options, then the random seed if asked. */
	FString BuildOptions() const;

protected:

	virtual void Use(AActor* User) override;

private:

	/** On-screen key of the "no destination" debug line (a refused trip goes to the player through UPlayerMessages). */
	static constexpr int32 NoDestinationMessageKey = 7621;

	/** Seconds the player has, after the unfinished-job warning, to press E again and leave anyway. */
	static constexpr double LeaveConfirmSeconds = 5.0;

	/**
	 *  The end-of-mission part of Use: false if the player must not leave yet (the job is not finished and they have not
	 *  confirmed), true once the mission is settled (or if this world has no mission to settle).
	 */
	bool TryEndMission();

	/** The player answered Leave on the card: the trip goes on, this time without asking. */
	void ConfirmLeave();

	/** World time of the last unfinished-job warning: a second press of E within LeaveConfirmSeconds of it confirms. */
	double LeaveWarnedAt = -1000.0;

	bool bLeaveConfirmed = false;
};
