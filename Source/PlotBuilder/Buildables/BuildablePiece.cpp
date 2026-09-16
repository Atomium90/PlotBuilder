#include "BuildablePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/HotbarComponent.h"

ABuildablePiece::ABuildablePiece()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetCollisionProfileName(FName("BlockAllDynamic"));
	MeshComponent->SetMobility(EComponentMobility::Movable);
}

void ABuildablePiece::Interact_Implementation(AActor* InteractingActor)
{
	if (State != EBuildablePieceState::Stock)
	{
		return;
	}

	if (UHotbarComponent* Hotbar = InteractingActor ? InteractingActor->FindComponentByClass<UHotbarComponent>() : nullptr)
	{
		Hotbar->Store(this);
	}
}
