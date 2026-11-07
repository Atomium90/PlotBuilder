#include "TravelPoint.h"
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

	const FString TravelOptions = BuildOptions();

	// The level we are about to leave may have something to keep (the home): tell it while its world still exists.
	if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
	{
		Progress->OnBeforeLevelChange.Broadcast();
	}

	UE_LOG(LogPlotBuilder, Log, TEXT("ATravelPoint %s: opening %s with options '%s'."), *GetName(), *Destination.GetLongPackageName(), *TravelOptions);

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Destination, true, TravelOptions);
}
