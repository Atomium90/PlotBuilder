#include "HotbarComponent.h"
#include "ShopItemRow.h"
#include "Buildables/BuildablePiece.h"
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
		const TCHAR* Marker = (i == SelectedIndex) ? TEXT(">") : TEXT(" ");
		GEngine->AddOnScreenDebugMessage(401 + i, 0.f, FColor::Yellow, FString::Printf(TEXT(" %s [%d] %s"), Marker, i, *ClassName));
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
	SelectedIndex = StoredEntries.Num() - 1;
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
	SelectedIndex = StoredEntries.Num() - 1;
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
	FHotbarEntry SelectedEntry;

	if (!GetSelectedEntry(SelectedEntry) || !SelectedEntry.ActorClass)
	{
		return;
	}

	if (SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()))
	{
		if (bShowDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(410, 2.f, FColor::Orange, TEXT("Enter Build Mode to place buildables."));
		}
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

	const FHotbarEntry Entry = ConsumeSelected();

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(Entry.ActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
	{
		Spawned->SetActorScale3D(Entry.Scale);
	}
}

bool UHotbarComponent::GetSelectedEntry(FHotbarEntry& OutEntry) const
{
	if (!StoredEntries.IsValidIndex(SelectedIndex))
	{
		return false;
	}

	OutEntry = StoredEntries[SelectedIndex];
	return true;
}

bool UHotbarComponent::EntryQualifies(const FHotbarEntry& Entry, bool bBuildablesOnly)
{
	if (!Entry.ActorClass)
	{
		return false;
	}

	return !bBuildablesOnly || Entry.ActorClass->IsChildOf(ABuildablePiece::StaticClass());
}

bool UHotbarComponent::EnsureValidSelection(bool bBuildablesOnly)
{
	if (StoredEntries.IsValidIndex(SelectedIndex) && EntryQualifies(StoredEntries[SelectedIndex], bBuildablesOnly))
	{
		return true;
	}

	for (int32 i = 0; i < StoredEntries.Num(); ++i)
	{
		if (EntryQualifies(StoredEntries[i], bBuildablesOnly))
		{
			SelectedIndex = i;
			return true;
		}
	}

	SelectedIndex = INDEX_NONE;
	return false;
}

void UHotbarComponent::CycleSelection(int32 Direction, bool bBuildablesOnly)
{
	const int32 Num = StoredEntries.Num();

	if (Direction == 0 || Num == 0)
	{
		return;
	}

	const int32 Step = FMath::Sign(Direction);
	int32 Index = StoredEntries.IsValidIndex(SelectedIndex) ? SelectedIndex : (Step > 0 ? Num - 1 : 0);

	for (int32 Attempts = 0; Attempts < Num; ++Attempts)
	{
		Index = (Index + Step + Num) % Num;

		if (EntryQualifies(StoredEntries[Index], bBuildablesOnly))
		{
			SelectedIndex = Index;
			return;
		}
	}

	// No qualifying entry anywhere in the hotbar - leave SelectedIndex untouched.
}

FHotbarEntry UHotbarComponent::ConsumeSelected()
{
	if (!StoredEntries.IsValidIndex(SelectedIndex))
	{
		return FHotbarEntry();
	}

	const FHotbarEntry Consumed = StoredEntries[SelectedIndex];
	const int32 RemovedIndex = SelectedIndex;
	StoredEntries.RemoveAt(RemovedIndex);

	// The entry that shifted into RemovedIndex is likely the next of a bulk purchase/pickup of
	// the same class (e.g. buying 20 walls in a row) - keep it selected so Build Mode can chain
	// placements without a full rescan.
	if (StoredEntries.IsValidIndex(RemovedIndex) && StoredEntries[RemovedIndex].ActorClass == Consumed.ActorClass)
	{
		SelectedIndex = RemovedIndex;
		return Consumed;
	}

	for (int32 i = 0; i < StoredEntries.Num(); ++i)
	{
		if (StoredEntries[i].ActorClass == Consumed.ActorClass)
		{
			SelectedIndex = i;
			return Consumed;
		}
	}

	SelectedIndex = StoredEntries.IsValidIndex(RemovedIndex) ? RemovedIndex : StoredEntries.Num() - 1;
	return Consumed;
}
