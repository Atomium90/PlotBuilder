#pragma once

#include "CoreMinimal.h"
#include "Hub/HubInteractable.h"
#include "Terminal.generated.h"

/**
 *  The PC on the desk of the mezzanine office: where the player will pick a contract, buy tools and buy the
 *  parcel. For now using it only prints a line; the mission board (WBP_MissionBoard, Nov 10) hooks
 *  OnInteracted from the base class, so this class does not change when the screens arrive.
 */
UCLASS()
class ATerminal : public AHubInteractable
{
	GENERATED_BODY()

public:

	ATerminal();

protected:

	virtual void Use(AActor* User) override;

private:

	/** On-screen key of the placeholder line. */
	static constexpr int32 PlaceholderMessageKey = 7630;
};
