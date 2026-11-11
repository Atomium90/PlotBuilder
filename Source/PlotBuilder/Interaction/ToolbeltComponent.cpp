#include "ToolbeltComponent.h"
#include "Debug/DebugDisplay.h"
#include "PhysicsManipulationComponent.h"
#include "PaintBrushComponent.h"
#include "Destruction/ExplosiveCharge.h"
#include "Destruction/DemolitionTool.h"
#include "Destruction/DemolitionToolData.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSubsystem.h"
#include "UI/PlayerMessages.h"

UToolbeltComponent::UToolbeltComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UToolbeltComponent::BeginPlay()
{
	Super::BeginPlay();

	// NumSlots was 5 before the brush: a Blueprint that stored that value would leave the brush slot out of reach.
	if (NumSlots <= BrushSlotIndex)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UToolbeltComponent: NumSlots is %d, the paint brush needs %d. Using %d."), NumSlots, BrushSlotIndex + 1, BrushSlotIndex + 1);
		NumSlots = BrushSlotIndex + 1;
	}

	SpawnMeleeTools();

	// A purchase at the PC changes what the belt may show: listen to the progress, and let go in EndPlay.
	if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
	{
		ProgressChangedHandle = Progress->OnProgressChanged.AddUObject(this, &UToolbeltComponent::HandleProgressChanged);
	}
}

void UToolbeltComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ProgressChangedHandle.IsValid())
	{
		if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
		{
			Progress->OnProgressChanged.Remove(ProgressChangedHandle);
		}

		ProgressChangedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UToolbeltComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bShowDebug && GEngine)
	{
		FString SlotName = TEXT("(empty)");

		if (EquippedIndex == HandsSlotIndex)
		{
			SlotName = TEXT("Hands");
		}
		else if (EquippedIndex == ChargeSlotIndex)
		{
			SlotName = TEXT("Charge");
		}
		else if (EquippedIndex == BrushSlotIndex)
		{
			SlotName = TEXT("Brush");
		}
		else if (const ADemolitionTool* MeleeTool = GetMeleeToolForSlot(EquippedIndex))
		{
			SlotName = MeleeTool->GetClass()->GetName();
		}

		PBDebug::Message(900, DeltaTime, FColor::Cyan, FString::Printf(TEXT("Toolbelt slot %d: %s"), EquippedIndex, *SlotName));
	}
}

bool UToolbeltComponent::IsSlotAvailable(int32 Index) const
{
	if (Index < 0 || Index >= NumSlots)
	{
		return false;
	}

	if (Index == HandsSlotIndex)
	{
		return true;
	}

	// A tool the player has not bought is not on the belt at all (the belt draws it as a padlock).
	if (!IsSlotOwned(Index))
	{
		return false;
	}

	if (bLocked)
	{
		return false;
	}

	if (Index == BrushSlotIndex)
	{
		return bPaintingAllowed;
	}

	// The rest (the charge and the melee tools) are the demolition tools.
	return bDestructionAllowed;
}

void UToolbeltComponent::EquipSlot(int32 Index)
{
	Index = FMath::Clamp(Index, 0, NumSlots - 1);

	if (!IsSlotAvailable(Index))
	{
		return;
	}

	EquipSlotInternal(Index);
}

void UToolbeltComponent::CycleTool(int32 Delta)
{
	const int32 Step = FMath::Sign(Delta);

	if (Step == 0 || NumSlots <= 0)
	{
		return;
	}

	// Walk the way the wheel turned, skipping what cannot be equipped, at most once around the belt.
	int32 Candidate = EquippedIndex;

	for (int32 Tried = 0; Tried < NumSlots; ++Tried)
	{
		Candidate = (Candidate + Step + NumSlots) % NumSlots;

		if (IsSlotAvailable(Candidate))
		{
			EquipSlotInternal(Candidate);
			return;
		}
	}
}

void UToolbeltComponent::SetLocked(bool bNewLocked)
{
	bLocked = bNewLocked;

	if (bLocked)
	{
		EquipSlotInternal(HandsSlotIndex);
	}

	OnBeltChanged.Broadcast();
}

void UToolbeltComponent::SetDestructionAllowed(bool bNewAllowed)
{
	bDestructionAllowed = bNewAllowed;

	// A tool in hand when the switch goes off goes back to the Hands.
	if (!IsSlotAvailable(EquippedIndex))
	{
		EquipSlotInternal(HandsSlotIndex);
	}

	OnBeltChanged.Broadcast();
}

void UToolbeltComponent::SetPaintingAllowed(bool bNewAllowed)
{
	bPaintingAllowed = bNewAllowed;

	// Same as above: the brush in hand goes back to the Hands when painting is switched off.
	if (!IsSlotAvailable(EquippedIndex))
	{
		EquipSlotInternal(HandsSlotIndex);
	}

	OnBeltChanged.Broadcast();
}

FText UToolbeltComponent::GetSlotName(int32 Index) const
{
	switch (Index)
	{
	case HandsSlotIndex:
		return NSLOCTEXT("Toolbelt", "Hands", "Hands");
	case ChargeSlotIndex:
		return NSLOCTEXT("Toolbelt", "Charge", "Explosive charge");
	case FirstMeleeSlotIndex:
		return NSLOCTEXT("Toolbelt", "Sledgehammer", "Sledgehammer");
	case FirstMeleeSlotIndex + 1:
		return NSLOCTEXT("Toolbelt", "Crowbar", "Crowbar");
	case FirstMeleeSlotIndex + 2:
		return NSLOCTEXT("Toolbelt", "Jackhammer", "Jackhammer");
	case BrushSlotIndex:
		return NSLOCTEXT("Toolbelt", "Brush", "Paint brush");
	default:
		return FText::GetEmpty();
	}
}

FText UToolbeltComponent::GetSlotHint(int32 Index) const
{
	switch (Index)
	{
	case HandsSlotIndex:
		return NSLOCTEXT("Toolbelt", "HandsHint", "Left click: grab  |  Right click: throw");
	case ChargeSlotIndex:
	{
		const FText Hint = NSLOCTEXT("Toolbelt", "ChargeHint", "Left click: place  |  Right click: throw, then detonate");
		const int32 Stock = GetChargeStock();
		return Stock == INDEX_NONE ? Hint : FText::Format(NSLOCTEXT("Toolbelt", "ChargeHintStock", "{0}  |  {1} in stock"), Hint, Stock);
	}
	case FirstMeleeSlotIndex:
	case FirstMeleeSlotIndex + 1:
		return NSLOCTEXT("Toolbelt", "MeleeHint", "Left click: swing");
	case FirstMeleeSlotIndex + 2:
		return NSLOCTEXT("Toolbelt", "JackhammerHint", "Hold left click");
	case BrushSlotIndex:
		return NSLOCTEXT("Toolbelt", "BrushHint", "Left click: paint the piece you look at");
	default:
		return FText::GetEmpty();
	}
}

FName UToolbeltComponent::ToolIdForSlot(const int32 Index)
{
	switch (Index)
	{
	case FirstMeleeSlotIndex:
		return FName("Sledgehammer");
	case FirstMeleeSlotIndex + 1:
		return FName("Crowbar");
	case FirstMeleeSlotIndex + 2:
		return FName("Jackhammer");
	default:
		return NAME_None;
	}
}

FToolUpkeep UToolbeltComponent::GetUpkeepForTool(const FName ToolId) const
{
	for (int32 Index = 0; Index < MeleeToolClasses.Num(); ++Index)
	{
		if (!MeleeToolClasses[Index] || ToolIdForSlot(FirstMeleeSlotIndex + Index) != ToolId)
		{
			continue;
		}

		const ADemolitionTool* Tool = MeleeToolClasses[Index]->GetDefaultObject<ADemolitionTool>();
		return (Tool && Tool->ToolData) ? Tool->ToolData->Upkeep : FToolUpkeep();
	}

	return FToolUpkeep();
}

bool UToolbeltComponent::IsSlotOwned(int32 Index) const
{
	if (Index < 0 || Index >= NumSlots)
	{
		return false;
	}

	if (!bOwnershipEnforced)
	{
		return true;
	}

	const FName ToolId = ToolIdForSlot(Index);

	if (ToolId.IsNone())
	{
		return true;
	}

	const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	return Progress && Progress->OwnsTool(ToolId);
}

void UToolbeltComponent::SetOwnershipEnforced(const bool bNewEnforced)
{
	if (bOwnershipEnforced == bNewEnforced)
	{
		return;
	}

	bOwnershipEnforced = bNewEnforced;

	// Same as the other switches: a tool in hand that is no longer owned goes back to the hands.
	if (!IsSlotAvailable(EquippedIndex))
	{
		EquipSlotInternal(HandsSlotIndex);
	}

	OnBeltChanged.Broadcast();
}

int32 UToolbeltComponent::GetChargeStock() const
{
	if (!bOwnershipEnforced)
	{
		return INDEX_NONE;
	}

	const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	return Progress ? Progress->GetCharges() : 0;
}

bool UToolbeltComponent::TakeCharge()
{
	if (!bOwnershipEnforced)
	{
		return true;
	}

	UProgressSubsystem* Progress = UProgressSubsystem::Get(this);

	if (Progress && Progress->TryConsumeCharge())
	{
		return true;
	}

	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(NSLOCTEXT("Toolbelt", "NoCharge", "No charge left. Buy more at the PC."), EMessageTone::Warning);
	}

	return false;
}

void UToolbeltComponent::GiveBackCharge()
{
	if (bOwnershipEnforced)
	{
		if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
		{
			Progress->AddCharges(1);
		}
	}
}

void UToolbeltComponent::HandleProgressChanged()
{
	if (!IsSlotAvailable(EquippedIndex))
	{
		EquipSlotInternal(HandsSlotIndex);
	}

	OnBeltChanged.Broadcast();
}

int32 UToolbeltComponent::CountMissingTools() const
{
	int32 Missing = 0;

	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		Missing += IsSlotOwned(Index) ? 0 : 1;
	}

	return Missing;
}

void UToolbeltComponent::EquipSlotInternal(int32 Index)
{
	Index = FMath::Clamp(Index, 0, NumSlots - 1);

	if (Index == EquippedIndex)
	{
		return;
	}

	if (ADemolitionTool* PreviouslyEquipped = GetMeleeToolForSlot(EquippedIndex))
	{
		PreviouslyEquipped->SetToolVisible(false);
	}

	EquippedIndex = Index;

	if (ADemolitionTool* NewlyEquipped = GetMeleeToolForSlot(EquippedIndex))
	{
		NewlyEquipped->SetToolVisible(true);
	}

	OnBeltChanged.Broadcast();
}

void UToolbeltComponent::PrimaryUse()
{
	if (IsHandsEquipped())
	{
		if (UPhysicsManipulationComponent* Physics = GetOwner() ? GetOwner()->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr)
		{
			Physics->Grab();
		}

		return;
	}

	if (EquippedIndex == ChargeSlotIndex)
	{
		PlaceCharge();
		return;
	}

	if (EquippedIndex == BrushSlotIndex)
	{
		if (UPaintBrushComponent* Brush = GetOwner() ? GetOwner()->FindComponentByClass<UPaintBrushComponent>() : nullptr)
		{
			Brush->Paint();
		}

		return;
	}

	if (ADemolitionTool* MeleeTool = GetMeleeToolForSlot(EquippedIndex))
	{
		MeleeTool->PrimaryUse();
		return;
	}

	UE_LOG(LogPlotBuilder, Verbose, TEXT("UToolbeltComponent::PrimaryUse: slot %d has no tool yet"), EquippedIndex);
}

void UToolbeltComponent::SecondaryUse()
{
	if (IsHandsEquipped())
	{
		if (UPhysicsManipulationComponent* Physics = GetOwner() ? GetOwner()->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr)
		{
			Physics->Throw();
		}

		return;
	}

	if (EquippedIndex == ChargeSlotIndex)
	{
		if (IsValid(ActiveCharge))
		{
			ActiveCharge->RequestDetonation();
		}
		else
		{
			SpawnAndThrowCharge();
		}

		return;
	}

	UE_LOG(LogPlotBuilder, Verbose, TEXT("UToolbeltComponent::SecondaryUse: slot %d has no tool yet"), EquippedIndex);
}

void UToolbeltComponent::ReleasePrimaryUse()
{
	if (ADemolitionTool* MeleeTool = GetMeleeToolForSlot(EquippedIndex))
	{
		MeleeTool->ReleasePrimaryUse();
	}
}

AExplosiveCharge* UToolbeltComponent::SpawnCharge(const FVector& Location, const FRotator& Rotation, ESpawnActorCollisionHandlingMethod CollisionHandling)
{
	if (!ExplosiveChargeClass)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UToolbeltComponent::SpawnCharge: ExplosiveChargeClass is not set"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = CollisionHandling;

	ActiveCharge = GetWorld()->SpawnActor<AExplosiveCharge>(ExplosiveChargeClass, Location, Rotation, SpawnParams);

	if (ActiveCharge)
	{
		ActiveCharge->OnDestroyed.AddDynamic(this, &UToolbeltComponent::HandleChargeDestroyed);
	}

	return ActiveCharge;
}

void UToolbeltComponent::SpawnAndThrowCharge()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!OwnerController)
	{
		return;
	}

	if (!TakeCharge())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();

	if (AExplosiveCharge* Charge = SpawnCharge(ViewLocation + ViewForward * ChargeSpawnDistance, ViewRotation, ESpawnActorCollisionHandlingMethod::Undefined))
	{
		Charge->Throw(ViewForward * ChargeThrowSpeed);
		return;
	}

	GiveBackCharge();
}

void UToolbeltComponent::PlaceCharge()
{
	// One charge at a time, whether thrown or placed: the next RMB detonates it.
	if (IsValid(ActiveCharge))
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

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	// Same channel the melee tools aim with, so the charge goes where a hammer swing would land.
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * ChargePlaceReach, ECC_Visibility, QueryParams);

	if (!bHit)
	{
		if (bShowDebug && GEngine)
		{
			PBDebug::Message(901, 1.5f, FColor::Yellow, TEXT("Charge: no surface in reach"));
		}

		return;
	}

	// A charge is taken only once there is a surface to put it on, so a miss costs nothing.
	if (!TakeCharge())
	{
		return;
	}

	// AlwaysSpawn: the spawn point is right on the wall, where the default adjustment would push the charge away. PlaceOn puts it exactly.
	if (AExplosiveCharge* Charge = SpawnCharge(Hit.ImpactPoint, FRotator::ZeroRotator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Charge->PlaceOn(Hit);
		return;
	}

	GiveBackCharge();
}

void UToolbeltComponent::HandleChargeDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == ActiveCharge)
	{
		ActiveCharge = nullptr;
	}
}

void UToolbeltComponent::SpawnMeleeTools()
{
	SpawnedMeleeTools.SetNum(MeleeToolClasses.Num());

	for (int32 Index = 0; Index < MeleeToolClasses.Num(); ++Index)
	{
		if (!MeleeToolClasses[Index])
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();

		SpawnedMeleeTools[Index] = GetWorld()->SpawnActor<ADemolitionTool>(MeleeToolClasses[Index], GetOwner()->GetActorTransform(), SpawnParams);
	}
}

ADemolitionTool* UToolbeltComponent::GetMeleeToolForSlot(int32 Index) const
{
	const int32 MeleeIndex = Index - FirstMeleeSlotIndex;
	return SpawnedMeleeTools.IsValidIndex(MeleeIndex) ? SpawnedMeleeTools[MeleeIndex] : nullptr;
}
