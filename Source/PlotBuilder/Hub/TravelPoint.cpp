#include "TravelPoint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSubsystem.h"

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

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(FeedbackMessageKey, 4.f, FColor::Red, FString::Printf(TEXT("%s has no destination."), *GetName()));
		}

		return;
	}

	if (bRequiresParcel)
	{
		const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);

		if (!Progress || !Progress->IsParcelOwned())
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(FeedbackMessageKey, 4.f, FColor::Red, TEXT("You do not own a parcel yet. Buy one at the PC."));
			}

			return;
		}
	}

	const FString TravelOptions = BuildOptions();

	UE_LOG(LogPlotBuilder, Log, TEXT("ATravelPoint %s: opening %s with options '%s'."), *GetName(), *Destination.GetLongPackageName(), *TravelOptions);

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Destination, true, TravelOptions);
}
