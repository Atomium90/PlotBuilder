#pragma once

#include "CoreMinimal.h"
#include "Hub/HubInteractable.h"
#include "Terminal.generated.h"

/**
 *  The PC on the desk of the mezzanine office: where the player picks a contract, and later buys tools and the
 *  parcel. Using it asks the HUD to open the terminal window (UPCTerminalWidget); the tabs live there, so this class
 *  does not change when the Tools and Parcel tabs are filled in. OnInteracted from the base class stays free for a Blueprint.
 */
UCLASS()
class ATerminal : public AHubInteractable
{
	GENERATED_BODY()

public:

	ATerminal();

protected:

	virtual void Use(AActor* User) override;
};
