#include "PhysicsProp.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Engine/World.h"

APhysicsProp::APhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionProfileName(FName("BlockAllDynamic"));

	DurabilityComponent = CreateDefaultSubobject<UDurabilityComponent>(TEXT("DurabilityComponent"));
}

void APhysicsProp::BeginPlay()
{
	Super::BeginPlay();

	DurabilityComponent->OnDurabilityDestroyed.AddDynamic(this, &APhysicsProp::HandleBroken);
	DurabilityComponent->InitializeWithFlatHP(Material, MaxHP);
}

void APhysicsProp::ApplyDamage_Implementation(const FDamageInfo& DamageInfo)
{
	DurabilityComponent->ApplyDamage(DamageInfo);
}

void APhysicsProp::HandleBroken()
{
	if (const UDemolitionRules* Rules = DurabilityComponent->Rules)
	{
		Rules->SpawnGenericFragments(GetWorld(), FragmentCount, Material, GetActorLocation(), FVector::ZeroVector);
	}

	Destroy();
}

void APhysicsProp::Interact_Implementation(AActor* InteractingActor)
{
	if (UHotbarComponent* Hotbar = InteractingActor ? InteractingActor->FindComponentByClass<UHotbarComponent>() : nullptr)
	{
		Hotbar->Store(this);
	}
}
