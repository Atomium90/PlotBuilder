#include "BuildablePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Environment/StructuralSupportComponent.h"
#include "Destruction/DurabilityComponent.h"

ABuildablePiece::ABuildablePiece()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetCollisionProfileName(FName("BlockAllDynamic"));
	MeshComponent->SetMobility(EComponentMobility::Movable);

	DurabilityComponent = CreateDefaultSubobject<UDurabilityComponent>(TEXT("DurabilityComponent"));
}

void ABuildablePiece::BeginPlay()
{
	Super::BeginPlay();

	DurabilityComponent->OnDurabilityDamaged.AddDynamic(this, &ABuildablePiece::HandleDurabilityDamaged);
	DurabilityComponent->OnDurabilityStateChanged.AddDynamic(this, &ABuildablePiece::HandleDurabilityStateChanged);
	DurabilityComponent->OnDurabilityDestroyed.AddDynamic(this, &ABuildablePiece::HandleDurabilityDestroyed);

	DurabilityComponent->InitializeFromRules(Material, PieceClass);
}

void ABuildablePiece::ApplyDamage_Implementation(const FDamageInfo& DamageInfo)
{
	DurabilityComponent->ApplyDamage(DamageInfo);
}

void ABuildablePiece::HandleDurabilityDamaged(const FDamageInfo& DamageInfo)
{
	UpdateDamageVisual();
}

void ABuildablePiece::HandleDurabilityStateChanged(EDurabilityState OldState, EDurabilityState NewState)
{
	UpdateDamageVisual();
}

void ABuildablePiece::HandleDurabilityDestroyed()
{
	Demolish();
}

void ABuildablePiece::UpdateDamageVisual()
{
	if (MeshComponent)
	{
		MeshComponent->SetCustomPrimitiveDataFloat(1, 1.f - DurabilityComponent->GetHPRatio());
	}
}

void ABuildablePiece::Interact_Implementation(AActor* InteractingActor)
{
	if (State == EBuildablePieceState::Stock)
	{
		if (UHotbarComponent* Hotbar = InteractingActor ? InteractingActor->FindComponentByClass<UHotbarComponent>() : nullptr)
		{
			Hotbar->Store(this);
		}

		return;
	}

	if (State == EBuildablePieceState::Placed)
	{
		Demolish();
	}
}

void ABuildablePiece::Demolish()
{
	if (StructuralComponent)
	{
		StructuralComponent->UnregisterPiece(this);
	}

	Destroy();
}

void ABuildablePiece::BeginCollapse()
{
	if (State == EBuildablePieceState::Collapsing)
	{
		return;
	}

	State = EBuildablePieceState::Collapsing;

	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(true);
	}

	SetLifeSpan(CollapseLifeSpan);

	if (StructuralComponent)
	{
		StructuralComponent->UnregisterPiece(this);
	}
}

void ABuildablePiece::UpdateStabilityDebugColor()
{
	if (MeshComponent)
	{
		MeshComponent->SetCustomPrimitiveDataFloat(0, Stability);
	}
}

FIntVector ABuildablePiece::GetStructuralCell() const
{
	if (BuildAnchor != EBuildAnchor::Legacy)
	{
		return GridCoord;
	}

	return StructuralComponent ? StructuralComponent->GetPieceCell(GetActorLocation()) : FIntVector::ZeroValue;
}

int32 ABuildablePiece::GetStructuralQuarterTurns() const
{
	if (BuildAnchor != EBuildAnchor::Legacy)
	{
		return Orientation;
	}

	return StructuralComponent ? UStructuralSupportComponent::GetQuarterTurns(GetActorRotation()) : 0;
}
