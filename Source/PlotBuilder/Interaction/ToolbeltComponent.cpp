#include "ToolbeltComponent.h"
#include "PhysicsManipulationComponent.h"
#include "PlotBuilder.h"

UToolbeltComponent::UToolbeltComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UToolbeltComponent::EquipSlot(int32 Index)
{
	Index = FMath::Clamp(Index, 0, NumSlots - 1);

	if (Index == EquippedIndex)
	{
		return;
	}

	EquippedIndex = Index;
}

void UToolbeltComponent::CycleTool(int32 Delta)
{
	EquipSlot((EquippedIndex + Delta + NumSlots) % NumSlots);
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

	UE_LOG(LogPlotBuilder, Verbose, TEXT("UToolbeltComponent::SecondaryUse: slot %d has no tool yet"), EquippedIndex);
}
