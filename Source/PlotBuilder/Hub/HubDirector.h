#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HubDirector.generated.h"

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

protected:

	virtual void BeginPlay() override;

private:

	static constexpr int32 MaxInputAttempts = 50;
	static constexpr float InputRetryInterval = 0.1f;

	void ApplyHubInput();

	FTimerHandle InputTimer;
	int32 InputAttempts = 0;
};
