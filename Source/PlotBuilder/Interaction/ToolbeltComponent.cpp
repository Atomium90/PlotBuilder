#include "ToolbeltComponent.h"
#include "Debug/DebugDisplay.h"
#include "PhysicsManipulationComponent.h"
#include "PaintBrushComponent.h"
#include "Destruction/ExplosiveCharge.h"
#include "Destruction/DemolitionTool.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "PlotBuilder.h"

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
}

void UToolbeltComponent::SetDestructionAllowed(bool bNewAllowed)
{
	bDestructionAllowed = bNewAllowed;

	// A tool in hand when the switch goes off goes back to the Hands.
	if (!IsSlotAvailable(EquippedIndex))
	{
		EquipSlotInternal(HandsSlotIndex);
	}
}

void UToolbeltComponent::SetPaintingAllowed(bool bNewAllowed)
{
	bPaintingAllowed = bNewAllowed;

	// Same as above: the brush in hand goes back to the Hands when painting is switched off.
	if (!IsSlotAvailable(EquippedIndex))
	{
		EquipSlotInternal(HandsSlotIndex);
	}
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

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();

	if (AExplosiveCharge* Charge = SpawnCharge(ViewLocation + ViewForward * ChargeSpawnDistance, ViewRotation, ESpawnActorCollisionHandlingMethod::Undefined))
	{
		Charge->Throw(ViewForward * ChargeThrowSpeed);
	}
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

	// AlwaysSpawn: the spawn point is right on the wall, where the default adjustment would push the charge away. PlaceOn puts it exactly.
	if (AExplosiveCharge* Charge = SpawnCharge(Hit.ImpactPoint, FRotator::ZeroRotator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Charge->PlaceOn(Hit);
	}
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
