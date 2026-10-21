#include "DestructibleComponent.h"
#include "Engine/World.h"

UDestructibleComponent::UDestructibleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDestructibleComponent::Detonate(FVector WorldLocation, float Scale)
{
	if (!ChargeFieldClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Charge = GetWorld()->SpawnActor<AActor>(ChargeFieldClass, WorldLocation, FRotator::ZeroRotator, SpawnParams);

	if (Charge)
	{
		Charge->SetActorScale3D(FVector(Scale));
		Charge->SetLifeSpan(ChargeLifeSpan);
	}
}
