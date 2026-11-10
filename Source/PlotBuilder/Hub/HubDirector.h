#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HubDirector.generated.h"

class UContractPool;

/**
 *  The one actor a hub level needs: it puts the player's building side and tool belt to sleep when the level
 *  starts (the hub is only walked through, and its walls are kit pieces with hit points that a charge could
 *  destroy). AMissionDirector does the same for a mission with its own copy of the retry below: the pawn may
 *  not exist yet when the level starts, so it tries again until it does.
 */
UCLASS()
class AHubDirector : public AActor
{
	GENERATED_BODY()

public:

	AHubDirector();

	/** Switch Build Mode, the hotbar and the shop off (APlayerCharacter::SetBuildingAllowed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bDisableBuilding = true;

	/** Lock the tool belt on the hands (UToolbeltComponent::SetLocked), so no tool can be used in the hub. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bLockToolbelt = true;

	/** The pool the contract board draws its offers from (DA_ContractPool). The hub hands it to the contract subsystem and has the board renewed if a mission ended since the last draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	TObjectPtr<UContractPool> ContractPool;

protected:

	virtual void BeginPlay() override;

private:

	static constexpr int32 MaxInputAttempts = 50;
	static constexpr float InputRetryInterval = 0.1f;

	/** The debrief waits for the HUD and the pawn, which are built after the level starts: tried every 0.2 s for 10 s. */
	static constexpr int32 MaxDebriefAttempts = 50;
	static constexpr float DebriefRetryInterval = 0.2f;

	void ApplyHubInput();

	/** Shows the report of the mission the player just came back from, once. Does nothing when there is none. */
	void ShowPendingDebrief();

	FTimerHandle InputTimer;
	int32 InputAttempts = 0;

	FTimerHandle DebriefTimer;
	int32 DebriefAttempts = 0;
};
