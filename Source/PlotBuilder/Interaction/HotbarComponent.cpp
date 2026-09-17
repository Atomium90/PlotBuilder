#include "HotbarComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UHotbarComponent::UHotbarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHotbarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShowDebug || !GEngine)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(400, 0.f, FColor::Yellow, FString::Printf(TEXT("Hotbar: %d/%d"), StoredClasses.Num(), MaxSlots));

	for (int32 i = 0; i < StoredClasses.Num(); ++i)
	{
		const FString ClassName = StoredClasses[i] ? StoredClasses[i]->GetName() : TEXT("None");
		GEngine->AddOnScreenDebugMessage(401 + i, 0.f, FColor::Yellow, FString::Printf(TEXT("  [%d] %s"), i, *ClassName));
	}
}

bool UHotbarComponent::Store(AActor* ActorToStore)
{
	if (!ActorToStore || StoredClasses.Num() >= MaxSlots)
	{
		return false;
	}

	StoredClasses.Add(ActorToStore->GetClass());
	ActorToStore->Destroy();
	return true;
}

TSubclassOf<AActor> UHotbarComponent::PopLast()
{
	if (StoredClasses.Num() == 0)
	{
		return nullptr;
	}

	const TSubclassOf<AActor> Result = StoredClasses.Last();
	StoredClasses.RemoveAt(StoredClasses.Num() - 1);
	return Result;
}

void UHotbarComponent::TryPlace()
{
	TSubclassOf<AActor> ClassToPlace = PopLast();

	if (!ClassToPlace)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!OwnerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector SpawnLocation = ViewLocation + ViewRotation.Vector() * PlaceDistance;
	SpawnLocation.X = FMath::GridSnap(SpawnLocation.X, GridSize);
	SpawnLocation.Y = FMath::GridSnap(SpawnLocation.Y, GridSize);
	SpawnLocation.Z = FMath::GridSnap(SpawnLocation.Z, GridSize);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	GetWorld()->SpawnActor<AActor>(ClassToPlace, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
}
