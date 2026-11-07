#pragma once

#include "CoreMinimal.h"
#include "Hub/HubInteractable.h"
#include "TravelPoint.generated.h"

class UWorld;

/**
 *  A door to another level: using it opens Destination. One class serves the truck (hub to mission, with a
 *  random seed), the home's access point (hub to Lvl_Home, only once the parcel is owned) and the way back
 *  (mission to hub); what changes is data on the placed actor.
 *
 *  The mission level reads its options from the travel URL (AMissionDirector), so the truck only has to put
 *  them in Options. The contract subsystem will replace this on Nov 10 by carrying the chosen contract and its
 *  seed itself; the return trigger is replaced on Nov 11 by the end-of-mission interaction.
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

	/** On-screen key of the feedback line (a refused trip, a missing destination). */
	static constexpr int32 FeedbackMessageKey = 7620;
};
