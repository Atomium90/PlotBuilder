#include "HubDirector.h"
#include "Engine/World.h"
#include "Interaction/ToolbeltComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "PlotBuilder.h"
#include "TimerManager.h"

AHubDirector::AHubDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHubDirector::BeginPlay()
{
	Super::BeginPlay();

	ApplyHubInput();
}

void AHubDirector::ApplyHubInput()
{
	if (APlayerCharacter* Character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (bDisableBuilding)
		{
			Character->SetBuildingAllowed(false);
		}

		if (bLockToolbelt)
		{
			if (UToolbeltComponent* Toolbelt = Character->GetToolbeltComponent())
			{
				Toolbelt->SetLocked(true);
			}
		}

		GetWorldTimerManager().ClearTimer(InputTimer);
		return;
	}

	if (++InputAttempts > MaxInputAttempts)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHubDirector: no player character after %d tries, building and tools stay on."), MaxInputAttempts);
		GetWorldTimerManager().ClearTimer(InputTimer);
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(InputTimer))
	{
		GetWorldTimerManager().SetTimer(InputTimer, this, &AHubDirector::ApplyHubInput, InputRetryInterval, true);
	}
}
