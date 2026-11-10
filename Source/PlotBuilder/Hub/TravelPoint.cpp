#include "TravelPoint.h"
#include "Contracts/ContractSubsystem.h"
#include "Contracts/MissionDirector.h"
#include "Contracts/MissionResultSubsystem.h"
#include "Debug/DebugDisplay.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSubsystem.h"
#include "UI/PlayerMessages.h"

ATravelPoint::ATravelPoint()
{
	Prompt = NSLOCTEXT("Hub", "TravelPrompt", "[E] Go");
}

FString ATravelPoint::BuildOptions() const
{
	FString Result = Options;

	if (Result.StartsWith(TEXT("?")))
	{
		Result.RightChopInline(1);
	}

	if (bRandomSeed)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT("?");
		}

		Result += FString::Printf(TEXT("Seed=%d"), FMath::RandRange(1, 999999));
	}

	return Result;
}

bool ATravelPoint::TryEndMission()
{
	UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this);

	if (!Results || !AMissionDirector::Find(this))
	{
		// A mission truck in a level that has no mission: nothing to settle, it is a plain door.
		UE_LOG(LogPlotBuilder, Warning, TEXT("ATravelPoint %s: TravelRole is EndMission but this level has no AMissionDirector, travelling without a report."), *GetName());
		return true;
	}

	// A finished job leaves without a question. An unfinished one is warned about, and a second press of E soon after confirms.
	const FNotebookData& Notebook = Results->GetLiveNotebook(true);

	if (!Notebook.bJobComplete && !Results->IsSettled())
	{
		const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;

		if (Now - LeaveWarnedAt > LeaveConfirmSeconds)
		{
			LeaveWarnedAt = Now;

			const int32 Percent = FMath::FloorToInt(Notebook.Completion * 100.f);
			FText Reason;

			if (Notebook.Completion < 1.f && Notebook.PendingFurniture > 0)
			{
				Reason = FText::Format(NSLOCTEXT("Hub", "LeaveUnfinishedBoth", "{0} % demolished, {1} to deliver"), Percent, Notebook.PendingFurniture);
			}
			else if (Notebook.Completion < 1.f)
			{
				Reason = FText::Format(NSLOCTEXT("Hub", "LeaveUnfinishedHouse", "{0} % demolished"), Percent);
			}
			else
			{
				Reason = FText::Format(NSLOCTEXT("Hub", "LeaveUnfinishedFurniture", "{0} to deliver"), Notebook.PendingFurniture);
			}

			if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
			{
				Messages->Notify(FText::Format(NSLOCTEXT("Hub", "LeaveUnfinished", "Job not finished: {0}. Press E again within {1} seconds to leave."), Reason, FMath::RoundToInt(LeaveConfirmSeconds)), EMessageTone::Warning, static_cast<float>(LeaveConfirmSeconds));
			}

			return false;
		}
	}

	FDebriefResult Report;
	Results->Settle(Report);

	return true;
}

void ATravelPoint::Use(AActor* User)
{
	if (Destination.IsNull())
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("ATravelPoint %s: no destination set."), *GetName());

		// A level-design mistake, not something for the player: debug text only.
		PBDebug::Message(NoDestinationMessageKey, 4.f, FColor::Red, FString::Printf(TEXT("%s has no destination."), *GetName()));

		return;
	}

	if (bRequiresParcel)
	{
		const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);

		if (!Progress || !Progress->IsParcelOwned())
		{
			if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
			{
				Messages->Notify(NSLOCTEXT("Hub", "ParcelRequired", "You do not own a parcel yet. Buy one at the PC."), EMessageTone::Warning);
			}

			return;
		}
	}

	if (TravelRole == ETravelRole::StartMission && !bAllowWithoutContract)
	{
		const UContractSubsystem* Contracts = UContractSubsystem::Get(this);

		if (!Contracts || !Contracts->HasActiveContract())
		{
			if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
			{
				Messages->Notify(NSLOCTEXT("Hub", "ContractRequired", "Pick a contract at the PC first."), EMessageTone::Warning);
			}

			return;
		}
	}

	if (TravelRole == ETravelRole::EndMission && !TryEndMission())
	{
		return;
	}

	const FString TravelOptions = BuildOptions();

	// The level we are about to leave may have something to keep (the home): tell it while its world still exists.
	if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
	{
		Progress->OnBeforeLevelChange.Broadcast();
	}

	UE_LOG(LogPlotBuilder, Log, TEXT("ATravelPoint %s: opening %s with options '%s'."), *GetName(), *Destination.GetLongPackageName(), *TravelOptions);

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Destination, true, TravelOptions);
}
