#include "ToolbeltComponent.h"
#include "PhysicsManipulationComponent.h"
#include "Destruction/ExplosiveCharge.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "PlotBuilder.h"

UToolbeltComponent::UToolbeltComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UToolbeltComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bShowDebug && GEngine)
	{
		static const TArray<FString> SlotNames = { TEXT("Hands"), TEXT("Charge"), TEXT("(empty)"), TEXT("(empty)"), TEXT("(empty)") };
		const FString SlotName = SlotNames.IsValidIndex(EquippedIndex) ? SlotNames[EquippedIndex] : TEXT("?");

		GEngine->AddOnScreenDebugMessage(900, DeltaTime, FColor::Cyan, FString::Printf(TEXT("Toolbelt slot %d: %s"), EquippedIndex, *SlotName));
	}
}

void UToolbeltComponent::EquipSlot(int32 Index)
{
	if (bLocked)
	{
		return;
	}

	EquipSlotInternal(Index);
}

void UToolbeltComponent::CycleTool(int32 Delta)
{
	if (bLocked)
	{
		return;
	}

	EquipSlotInternal((EquippedIndex + Delta + NumSlots) % NumSlots);
}

void UToolbeltComponent::SetLocked(bool bNewLocked)
{
	bLocked = bNewLocked;

	if (bLocked)
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

	EquippedIndex = Index;
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

	UE_LOG(LogPlotBuilder, Verbose, TEXT("UToolbeltComponent::PrimaryUse: slot %d has no tool yet"), EquippedIndex);
}

void UToolbeltComponent::SecondaryUse()
{
	if (IsHandsEquipped())
	{
		if (UPhysicsManipulationComponent* Physics = GetOwner() ? GetOwner()->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr)
		{
			Physics->Push();
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

void UToolbeltComponent::SpawnAndThrowCharge()
{
	if (!ExplosiveChargeClass)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UToolbeltComponent::SpawnAndThrowCharge: ExplosiveChargeClass is not set"));
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
	const FVector ViewForward = ViewRotation.Vector();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();

	ActiveCharge = GetWorld()->SpawnActor<AExplosiveCharge>(ExplosiveChargeClass, ViewLocation + ViewForward * ChargeSpawnDistance, ViewRotation, SpawnParams);

	if (ActiveCharge)
	{
		ActiveCharge->Throw(ViewForward * ChargeThrowSpeed);
		ActiveCharge->OnDestroyed.AddDynamic(this, &UToolbeltComponent::HandleChargeDestroyed);
	}
}

void UToolbeltComponent::HandleChargeDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == ActiveCharge)
	{
		ActiveCharge = nullptr;
	}
}
