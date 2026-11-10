#include "HubDirector.h"
#include "Contracts/ContractSubsystem.h"
#include "Engine/World.h"
#include "Interaction/ToolbeltComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "PlotBuilder.h"
#include "TimerManager.h"
#include "UI/GameHUDWidget.h"

AHubDirector::AHubDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHubDirector::BeginPlay()
{
	Super::BeginPlay();

	// The board the mission board will show: the pool in, and a new draw if a mission ended since the last one (or none was ever made).
	if (UContractSubsystem* Contracts = UContractSubsystem::Get(this))
	{
		Contracts->SetPool(ContractPool);
		Contracts->EnsureFreshOffers();
	}

	ApplyHubInput();
	ShowPendingDebrief();
}

void AHubDirector::ShowPendingDebrief()
{
	const UContractSubsystem* Contracts = UContractSubsystem::Get(this);

	if (!Contracts || !Contracts->HasUnseenResult())
	{
		GetWorldTimerManager().ClearTimer(DebriefTimer);
		return;
	}

	// The report is shown with the account as it is now, so the pawn (which carries the money) must be there as well as the HUD.
	UGameHUDWidget* Hud = UGameplayStatics::GetPlayerPawn(this, 0) ? UGameHUDWidget::Find(this) : nullptr;

	if (Hud && Hud->ShowDebrief())
	{
		GetWorldTimerManager().ClearTimer(DebriefTimer);
		return;
	}

	if (++DebriefAttempts > MaxDebriefAttempts)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHubDirector: no HUD with a debrief screen after %d tries, the report stays unseen (pb.ShowDebrief shows it)."), MaxDebriefAttempts);
		GetWorldTimerManager().ClearTimer(DebriefTimer);
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(DebriefTimer))
	{
		GetWorldTimerManager().SetTimer(DebriefTimer, this, &AHubDirector::ShowPendingDebrief, DebriefRetryInterval, true);
	}
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

		if (bEnforceToolOwnership)
		{
			Character->SetToolOwnershipEnforced(true);
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
