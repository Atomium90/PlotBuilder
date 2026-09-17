#include "HotbarComponent.h"
#include "ShopItemRow.h"
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

	GEngine->AddOnScreenDebugMessage(400, 0.f, FColor::Yellow, FString::Printf(TEXT("Hotbar: %d/%d"), StoredEntries.Num(), MaxSlots));

	for (int32 i = 0; i < StoredEntries.Num(); ++i)
	{
		const FString ClassName = StoredEntries[i].ActorClass ? StoredEntries[i].ActorClass->GetName() : TEXT("None");
		GEngine->AddOnScreenDebugMessage(401 + i, 0.f, FColor::Yellow, FString::Printf(TEXT("  [%d] %s"), i, *ClassName));
	}
}

bool UHotbarComponent::Store(AActor* ActorToStore)
{
	if (!ActorToStore || StoredEntries.Num() >= MaxSlots)
	{
		return false;
	}

	FHotbarEntry Entry;
	Entry.ActorClass = ActorToStore->GetClass();
	Entry.Scale = ActorToStore->GetActorScale3D();
	StoredEntries.Add(Entry);
	ActorToStore->Destroy();
	return true;
}

TSubclassOf<AActor> UHotbarComponent::PopLast()
{
	if (StoredEntries.Num() == 0)
	{
		return nullptr;
	}

	const TSubclassOf<AActor> Result = StoredEntries.Last().ActorClass;
	StoredEntries.RemoveAt(StoredEntries.Num() - 1);
	return Result;
}

bool UHotbarComponent::PurchaseRow(FName RowName)
{
	if (!ShopCatalog || StoredEntries.Num() >= MaxSlots)
	{
		return false;
	}

	const FShopItemRow* Row = ShopCatalog->FindRow<FShopItemRow>(RowName, TEXT("PurchaseRow"));

	if (!Row || !Row->ActorClass)
	{
		return false;
	}

	FHotbarEntry Entry;
	Entry.ActorClass = Row->ActorClass;
	StoredEntries.Add(Entry);
	return true;
}

void UHotbarComponent::PurchaseByIndex(int32 SlotIndex)
{
	if (!ShopCatalog)
	{
		return;
	}

	const TArray<FName> RowNames = ShopCatalog->GetRowNames();

	if (RowNames.IsValidIndex(SlotIndex))
	{
		PurchaseRow(RowNames[SlotIndex]);
	}
}

void UHotbarComponent::TryPlace()
{
	if (StoredEntries.Num() == 0)
	{
		return;
	}

	const FHotbarEntry Entry = StoredEntries.Last();
	StoredEntries.RemoveAt(StoredEntries.Num() - 1);

	if (!Entry.ActorClass)
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

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(Entry.ActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
	{
		Spawned->SetActorScale3D(Entry.Scale);
	}
}
