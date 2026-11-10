#include "HotbarComponent.h"
#include "Debug/DebugDisplay.h"
#include "ShopItemRow.h"
#include "Buildables/BuildablePiece.h"
#include "Props/PaintBucket.h"
#include "Progress/ProgressSubsystem.h"
#include "UI/PlayerMessages.h"
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

	// The money outlives the pawn (hub, mission and home are different maps): the subsystem holds it, this component
	// starts from it and writes every change back. The editor value of Money only matters when there is no subsystem.
	if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
	{
		Money = Progress->GetMoney();
		OnMoneyChanged.AddDynamic(this, &UHotbarComponent::HandleMoneyChanged);
	}
}

bool UHotbarComponent::TrySpend(const int32 Amount)
{
	if (Amount <= 0 || bInfiniteMoney)
	{
		return true;
	}

	if (Money < Amount)
	{
		return false;
	}

	Money -= Amount;
	OnMoneyChanged.Broadcast();
	return true;
}

void UHotbarComponent::HandleMoneyChanged()
{
	if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
	{
		Progress->SetMoney(Money);
	}
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
	PBDebug::Message(400, 0.f, FColor::Yellow, FString::Printf(TEXT("Hotbar: money %s | cursor %s"), *MoneyText, *CursorText));

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString ClassName = Slots[i].ActorClass ? Slots[i].ActorClass->GetName() : TEXT("-");
		const TCHAR* Marker = (i == SelectedIndex) ? TEXT(">") : TEXT(" ");
		PBDebug::Message(401 + i, 0.f, FColor::Yellow, FString::Printf(TEXT(" %s [%d] %s x%d"), Marker, i, *ClassName, Slots[i].Count));
	}
}

void UHotbarComponent::RestoreSlots(const TArray<FInventorySlot>& NewSlots)
{
	const int32 Total = FMath::Max(NumColumns * NumRows, 0);

	Slots.Reset();
	Slots.SetNum(Total);

	for (int32 Index = 0; Index < FMath::Min(NewSlots.Num(), Total); ++Index)
	{
		Slots[Index] = NewSlots[Index];
	}

	CursorSlot.Reset();

	OnCursorSlotChanged.Broadcast();
	OnInventoryChanged.Broadcast();
}

bool UHotbarComponent::SlotQualifies(const FInventorySlot& Slot, bool bBuildablesOnly)
{
	// Outside build mode the wheel visits every slot, empty ones too: the player sees the selection move even on an empty hotbar.
	if (!bBuildablesOnly)
	{
		return true;
	}

	// Build mode can only build what is there.
	return !Slot.IsEmpty() && Slot.ActorClass->IsChildOf(ABuildablePiece::StaticClass());
}

int32 UHotbarComponent::AddItem(TSubclassOf<AActor> ActorClass, const FVector& Scale, int32 StackSize, int32 Count)
{
	return AddToSlots(Slots, ActorClass, Scale, StackSize, Count);
}

int32 UHotbarComponent::AddToSlots(TArray<FInventorySlot>& Target, TSubclassOf<AActor> ActorClass, const FVector& Scale, int32 StackSize, int32 Count)
{
	if (!ActorClass || Count <= 0 || StackSize <= 0)
	{
		return Count;
	}

	int32 Remaining = Count;

	// An existing stack with room, in index order, before any empty slot.
	for (FInventorySlot& Slot : Target)
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

	for (FInventorySlot& Slot : Target)
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
	return FShopItemRow::FindForClass(ShopCatalog, ActorClass);
}

const FShopItemRow* UHotbarComponent::FindRowForClass(TSubclassOf<AActor> ActorClass) const
{
	return FindCatalogRowFor(ActorClass);
}

const FShopItemRow* UHotbarComponent::FindRow(FName RowName) const
{
	return ShopCatalog ? ShopCatalog->FindRow<FShopItemRow>(RowName, TEXT("UHotbarComponent::FindRow"), false) : nullptr;
}

int32 UHotbarComponent::GetPurchaseLimit(const FShopItemRow& Row) const
{
	return Row.MaxOwned > 0 ? FMath::Max(0, Row.MaxOwned - CountOf(Row.ActorClass)) : TNumericLimits<int32>::Max();
}

void UHotbarComponent::SetAllowed(bool bNewAllowed)
{
	if (bAllowed == bNewAllowed)
	{
		return;
	}

	bAllowed = bNewAllowed;
	OnAllowedChanged.Broadcast();
}

int32 UHotbarComponent::CountOf(TSubclassOf<AActor> ActorClass) const
{
	if (!ActorClass)
	{
		return 0;
	}

	int32 Total = 0;

	for (const FInventorySlot& Slot : Slots)
	{
		Total += (!Slot.IsEmpty() && Slot.ActorClass == ActorClass) ? Slot.Count : 0;
	}

	// The stack on the cursor is still the player's.
	Total += (!CursorSlot.IsEmpty() && CursorSlot.ActorClass == ActorClass) ? CursorSlot.Count : 0;

	return Total;
}

int32 UHotbarComponent::GetFreeSlotCount() const
{
	// Total minus the slots in use, so it is right before BeginPlay has made the grid (nothing is in use then).
	int32 Used = 0;

	for (const FInventorySlot& Slot : Slots)
	{
		Used += Slot.IsEmpty() ? 0 : 1;
	}

	return GetTotalSlotCount() - Used;
}

FCartQuote UHotbarComponent::QuoteLines(const TArray<FCartLine>& Lines) const
{
	FCartQuote Quote;
	Quote.FreeSlots = GetFreeSlotCount();

	// The items go on a copy of the grid with the same rules as a real purchase, so what the quote promises is what happens.
	TArray<FInventorySlot> Simulated = Slots;
	int32 EstimatedExtraSlots = 0;

	for (const FCartLine& Line : Lines)
	{
		const FShopItemRow* Row = Line.Quantity > 0 ? FindRow(Line.RowName) : nullptr;

		if (!Row || !Row->ActorClass)
		{
			continue;
		}

		Quote.Units += Line.Quantity;
		Quote.TotalCost += Row->Cost * Line.Quantity;

		const int32 NotPlaced = AddToSlots(Simulated, Row->ActorClass, FVector::OneVector, Row->StackSize, Line.Quantity);

		if (NotPlaced > 0)
		{
			Quote.bFits = false;
			EstimatedExtraSlots += FMath::DivideAndRoundUp(NotPlaced, FMath::Max(1, Row->StackSize));
		}
	}

	int32 FreeAfter = 0;

	for (const FInventorySlot& Slot : Simulated)
	{
		FreeAfter += Slot.IsEmpty() ? 1 : 0;
	}

	Quote.SlotsNeeded = (Quote.FreeSlots - FreeAfter) + EstimatedExtraSlots;
	Quote.bAffordable = bInfiniteMoney || Money >= Quote.TotalCost;

	return Quote;
}

bool UHotbarComponent::PurchaseLines(const TArray<FCartLine>& Lines)
{
	if (!ShopCatalog || !bAllowed)
	{
		return false;
	}

	const FCartQuote Quote = QuoteLines(Lines);

	if (!Quote.CanConfirm())
	{
		return false;
	}

	for (const FCartLine& Line : Lines)
	{
		const FShopItemRow* Row = Line.Quantity > 0 ? FindRow(Line.RowName) : nullptr;

		if (Row && Row->ActorClass)
		{
			AddItem(Row->ActorClass, FVector::OneVector, Row->StackSize, Line.Quantity);
		}
	}

	if (!bInfiniteMoney)
	{
		Money -= Quote.TotalCost;
		OnMoneyChanged.Broadcast();
	}

	OnInventoryChanged.Broadcast();
	return true;
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
	if (!ActorToStore || !bAllowed)
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

TSubclassOf<AActor> UHotbarComponent::FindSoldClass(TSubclassOf<AActor> ActorClass) const
{
	if (!ShopCatalog || !ActorClass || FindCatalogRowFor(ActorClass))
	{
		return ActorClass;
	}

	TArray<FShopItemRow*> Rows;
	ShopCatalog->GetAllRows<FShopItemRow>(TEXT("UHotbarComponent::FindSoldClass"), Rows);

	for (const FShopItemRow* Row : Rows)
	{
		const ABuildablePiece* Sold = (Row && Row->ActorClass) ? Cast<ABuildablePiece>(Row->ActorClass->GetDefaultObject()) : nullptr;

		if (Sold && Sold->MirrorVariantClass.Get() == ActorClass.Get())
		{
			return Row->ActorClass;
		}
	}

	return ActorClass;
}

bool UHotbarComponent::TakeBack(const ABuildablePiece* Piece)
{
	if (!Piece || !bAllowed)
	{
		return false;
	}

	const TSubclassOf<AActor> ActorClass = FindSoldClass(Piece->GetClass());

	// A grid piece carries its placement scale; a Legacy one was never given one (its actor scale is the scale it was bought at).
	const FVector Scale = Piece->GetBuildAnchor() == EBuildAnchor::Legacy ? Piece->GetActorScale3D() : Piece->GetPlacedScale();

	const FShopItemRow* Row = FindCatalogRowFor(ActorClass);
	const int32 StackSize = Row ? Row->StackSize : 99;

	// One unit: it either goes in whole or not at all, so a refusal leaves the grid exactly as it was.
	if (AddItem(ActorClass, Scale, StackSize) > 0)
	{
		return false;
	}

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
	if (!ShopCatalog || !bAllowed)
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

	if (!bAllowed || !GetSelectedEntry(SelectedEntry) || !SelectedEntry.ActorClass)
	{
		return;
	}

	if (SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()))
	{
		// Say why the click did nothing. This one used to be debug text, so the player never saw it.
		if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
		{
			Messages->Notify(NSLOCTEXT("Hotbar", "EnterBuildMode", "Enter Build Mode to place buildables."), EMessageTone::Info);
		}
		return;
	}

	// A paint bucket is an item for the brush, never an object to spawn. No message: painting is done with the brush,
	// and a click with the brush in hand must not complain about the bucket it is painting with.
	if (SelectedEntry.ActorClass->IsChildOf(APaintBucket::StaticClass()))
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

bool UHotbarComponent::ReturnCursorSlot()
{
	if (CursorSlot.IsEmpty())
	{
		return true;
	}

	const int32 Leftover = AddItem(CursorSlot.ActorClass, CursorSlot.Scale, CursorSlot.StackSize, CursorSlot.Count);
	CursorSlot.Count = Leftover;

	if (Leftover <= 0)
	{
		CursorSlot.Reset();
	}

	OnInventoryChanged.Broadcast();
	OnCursorSlotChanged.Broadcast();
	return Leftover <= 0;
}
