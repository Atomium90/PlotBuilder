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

void UHotbarComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(FMath::Max(NumColumns * NumRows, 0));
}

void UHotbarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShowDebug || !GEngine)
	{
		return;
	}

	const FString MoneyText = bInfiniteMoney ? TEXT("(infinite)") : FString::FromInt(Money);
	const FString CursorText = CursorSlot.IsEmpty() ? TEXT("(none)") : FString::Printf(TEXT("%s x%d"), *GetNameSafe(CursorSlot.ActorClass), CursorSlot.Count);
	GEngine->AddOnScreenDebugMessage(400, 0.f, FColor::Yellow, FString::Printf(TEXT("Hotbar: money %s | cursor %s"), *MoneyText, *CursorText));

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString ClassName = Slots[i].ActorClass ? Slots[i].ActorClass->GetName() : TEXT("-");
		const TCHAR* Marker = (i == SelectedIndex) ? TEXT(">") : TEXT(" ");
		GEngine->AddOnScreenDebugMessage(401 + i, 0.f, FColor::Yellow, FString::Printf(TEXT(" %s [%d] %s x%d"), Marker, i, *ClassName, Slots[i].Count));
	}
}

bool UHotbarComponent::SlotQualifies(const FInventorySlot& Slot, bool bBuildablesOnly)
{
	if (Slot.IsEmpty())
	{
		return false;
	}

	return !bBuildablesOnly || Slot.ActorClass->IsChildOf(ABuildablePiece::StaticClass());
}

int32 UHotbarComponent::AddItem(TSubclassOf<AActor> ActorClass, const FVector& Scale, int32 StackSize, int32 Count)
{
	if (!ActorClass || Count <= 0 || StackSize <= 0)
	{
		return Count;
	}

	int32 Remaining = Count;

	// An existing stack with room, in index order, before any empty slot.
	for (FInventorySlot& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}

		if (Slot.ActorClass == ActorClass && Slot.Scale.Equals(Scale) && Slot.Count < Slot.StackSize)
		{
			const int32 Added = FMath::Min(Remaining, Slot.StackSize - Slot.Count);
			Slot.Count += Added;
			Remaining -= Added;
		}
	}

	for (FInventorySlot& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}

		if (Slot.IsEmpty())
		{
			Slot.ActorClass = ActorClass;
			Slot.Scale = Scale;
			Slot.StackSize = StackSize;
			const int32 Added = FMath::Min(Remaining, StackSize);
			Slot.Count = Added;
			Remaining -= Added;
		}
	}

	return Remaining;
}

const FShopItemRow* UHotbarComponent::FindCatalogRowFor(TSubclassOf<AActor> ActorClass) const
{
	if (!ShopCatalog || !ActorClass)
	{
		return nullptr;
	}

	TArray<FShopItemRow*> Rows;
	ShopCatalog->GetAllRows<FShopItemRow>(TEXT("FindCatalogRowFor"), Rows);

	for (const FShopItemRow* Row : Rows)
	{
		if (Row && Row->ActorClass == ActorClass)
		{
			return Row;
		}
	}

	return nullptr;
}

void UHotbarComponent::SetSelectedIndex(int32 NewIndex)
{
	if (SelectedIndex == NewIndex)
	{
		return;
	}

	SelectedIndex = NewIndex;
	OnSelectedSlotChanged.Broadcast();
}

bool UHotbarComponent::Store(AActor* ActorToStore)
{
	if (!ActorToStore)
	{
		return false;
	}

	const TSubclassOf<AActor> ActorClass = ActorToStore->GetClass();
	const FVector Scale = ActorToStore->GetActorScale3D();
	const FShopItemRow* Row = FindCatalogRowFor(ActorClass);
	const int32 StackSize = Row ? Row->StackSize : (ActorClass->IsChildOf(ABuildablePiece::StaticClass()) ? 99 : 9);

	if (AddItem(ActorClass, Scale, StackSize) > 0)
	{
		return false;
	}

	ActorToStore->Destroy();
	OnInventoryChanged.Broadcast();
	return true;
}

TSubclassOf<AActor> UHotbarComponent::PopLast()
{
	for (int32 i = Slots.Num() - 1; i >= 0; --i)
	{
		if (!Slots[i].IsEmpty())
		{
			const TSubclassOf<AActor> Result = Slots[i].ActorClass;
			Slots[i].Count -= 1;

			if (Slots[i].Count <= 0)
			{
				Slots[i].Reset();
			}

			OnInventoryChanged.Broadcast();
			return Result;
		}
	}

	return nullptr;
}

bool UHotbarComponent::PurchaseRow(FName RowName)
{
	if (!ShopCatalog)
	{
		return false;
	}

	const FShopItemRow* Row = ShopCatalog->FindRow<FShopItemRow>(RowName, TEXT("PurchaseRow"));

	if (!Row || !Row->ActorClass)
	{
		return false;
	}

	if (!bInfiniteMoney && Money < Row->Cost)
	{
		return false;
	}

	if (AddItem(Row->ActorClass, FVector::OneVector, Row->StackSize) > 0)
	{
		return false;
	}

	if (!bInfiniteMoney)
	{
		Money -= Row->Cost;
		OnMoneyChanged.Broadcast();
	}

	OnInventoryChanged.Broadcast();
	return true;
}

void UHotbarComponent::TryPlace()
{
	FInventorySlot SelectedEntry;

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

	const FInventorySlot Entry = ConsumeSelected();

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(Entry.ActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
	{
		Spawned->SetActorScale3D(Entry.Scale);
	}
}

bool UHotbarComponent::GetSelectedEntry(FInventorySlot& OutSlot) const
{
	if (!Slots.IsValidIndex(SelectedIndex) || Slots[SelectedIndex].IsEmpty())
	{
		return false;
	}

	OutSlot = Slots[SelectedIndex];
	return true;
}

void UHotbarComponent::CycleSelection(int32 Direction, bool bBuildablesOnly)
{
	const int32 HotbarSize = FMath::Min(NumColumns, Slots.Num());

	if (Direction == 0 || HotbarSize == 0)
	{
		return;
	}

	const int32 Step = FMath::Sign(Direction);
	int32 Index = (SelectedIndex >= 0 && SelectedIndex < HotbarSize) ? SelectedIndex : (Step > 0 ? HotbarSize - 1 : 0);

	for (int32 Attempts = 0; Attempts < HotbarSize; ++Attempts)
	{
		Index = (Index + Step + HotbarSize) % HotbarSize;

		if (SlotQualifies(Slots[Index], bBuildablesOnly))
		{
			SetSelectedIndex(Index);
			return;
		}
	}

	// No qualifying slot anywhere in the hotbar - leave SelectedIndex untouched.
}

bool UHotbarComponent::EnsureValidSelection(bool bBuildablesOnly)
{
	const int32 HotbarSize = FMath::Min(NumColumns, Slots.Num());

	if (SelectedIndex >= 0 && SelectedIndex < HotbarSize && SlotQualifies(Slots[SelectedIndex], bBuildablesOnly))
	{
		return true;
	}

	for (int32 i = 0; i < HotbarSize; ++i)
	{
		if (SlotQualifies(Slots[i], bBuildablesOnly))
		{
			SetSelectedIndex(i);
			return true;
		}
	}

	return false;
}

FInventorySlot UHotbarComponent::ConsumeSelected()
{
	if (!Slots.IsValidIndex(SelectedIndex) || Slots[SelectedIndex].IsEmpty())
	{
		return FInventorySlot();
	}

	FInventorySlot& Slot = Slots[SelectedIndex];
	FInventorySlot Consumed = Slot;
	Consumed.Count = 1;

	Slot.Count -= 1;

	if (Slot.Count <= 0)
	{
		Slot.Reset();
	}

	OnInventoryChanged.Broadcast();
	return Consumed;
}

void UHotbarComponent::OnSlotLeftClicked(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FInventorySlot& Slot = Slots[SlotIndex];

	if (CursorSlot.IsEmpty())
	{
		if (Slot.IsEmpty())
		{
			return;
		}

		CursorSlot = Slot;
		Slot.Reset();
		OnInventoryChanged.Broadcast();
		OnCursorSlotChanged.Broadcast();
		return;
	}

	if (Slot.IsEmpty())
	{
		Slot = CursorSlot;
		CursorSlot.Reset();
		OnInventoryChanged.Broadcast();
		OnCursorSlotChanged.Broadcast();
		return;
	}

	if (Slot.ActorClass == CursorSlot.ActorClass && Slot.Scale.Equals(CursorSlot.Scale))
	{
		const int32 Added = FMath::Min(CursorSlot.Count, Slot.StackSize - Slot.Count);
		Slot.Count += Added;
		CursorSlot.Count -= Added;

		if (CursorSlot.Count <= 0)
		{
			CursorSlot.Reset();
		}

		OnInventoryChanged.Broadcast();
		OnCursorSlotChanged.Broadcast();
		return;
	}

	// Different items - swap outright.
	const FInventorySlot Temp = Slot;
	Slot = CursorSlot;
	CursorSlot = Temp;
	OnInventoryChanged.Broadcast();
	OnCursorSlotChanged.Broadcast();
}

void UHotbarComponent::OnSlotRightClicked(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	if (CursorSlot.IsEmpty())
	{
		OnSlotLeftClicked(SlotIndex);
		return;
	}

	FInventorySlot& Slot = Slots[SlotIndex];

	if (Slot.IsEmpty())
	{
		Slot.ActorClass = CursorSlot.ActorClass;
		Slot.Scale = CursorSlot.Scale;
		Slot.StackSize = CursorSlot.StackSize;
		Slot.Count = 1;
	}
	else if (Slot.ActorClass == CursorSlot.ActorClass && Slot.Scale.Equals(CursorSlot.Scale) && Slot.Count < Slot.StackSize)
	{
		Slot.Count += 1;
	}
	else
	{
		return;
	}

	CursorSlot.Count -= 1;

	if (CursorSlot.Count <= 0)
	{
		CursorSlot.Reset();
	}

	OnInventoryChanged.Broadcast();
	OnCursorSlotChanged.Broadcast();
}
