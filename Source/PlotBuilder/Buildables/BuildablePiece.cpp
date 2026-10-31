#include "BuildablePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Environment/StructuralSupportComponent.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Destruction/FragmentDebris.h"

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
	// Direct kill (tool damage reached 0 HP) - fractures in place, no Falling phase, unlike the
	// cascade path below. Unregister first, same as Demolish(), but destroy via Fracture instead.
	if (StructuralComponent)
	{
		StructuralComponent->UnregisterPiece(this);
	}

	Fracture(GetActorLocation(), FVector::ZeroVector);
	Destroy();
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

	// Computed before UnregisterPiece below removes this piece's own sockets from the table - its
	// neighbours' registration is what matters here, and unregistering this piece never touches them.
	CollapseBiasDirection = StructuralComponent ? StructuralComponent->ComputeAwayFromNeighbourCentroid(this) : FVector::ZeroVector;

	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->SetNotifyRigidBodyCollision(true);
		MeshComponent->OnComponentHit.AddDynamic(this, &ABuildablePiece::HandleCollapseHit);

		if (!CollapseBiasDirection.IsNearlyZero())
		{
			const float ImpulseStrength = DurabilityComponent ? DurabilityComponent->GetCollapseImpulseStrength() : 400.f;
			MeshComponent->AddImpulse(CollapseBiasDirection * ImpulseStrength, NAME_None, /*bVelChange=*/true);
		}
	}

	// CollapseLifeSpan is now an explicit, hookable deadline (fracture-on-timeout) instead of a
	// plain SetLifeSpan - a piece that falls "softly" and never clears MinFractureImpactSpeed must
	// still end up replaced by fragments, not silently vanish via engine lifespan.
	GetWorldTimerManager().SetTimer(CollapseTimeoutHandle, this, &ABuildablePiece::HandleCollapseTimeout, CollapseLifeSpan, false);

	if (StructuralComponent)
	{
		StructuralComponent->UnregisterPiece(this);
	}
}

void ABuildablePiece::HandleCollapseHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasFractured || !MeshComponent || MeshComponent->GetComponentVelocity().Size() < MinFractureImpactSpeed)
	{
		return;
	}

	bHasFractured = true;
	GetWorldTimerManager().ClearTimer(CollapseTimeoutHandle);
	Fracture(GetActorLocation(), CollapseBiasDirection);
	Destroy();
}

void ABuildablePiece::HandleCollapseTimeout()
{
	if (bHasFractured)
	{
		return;
	}

	bHasFractured = true;
	Fracture(GetActorLocation(), CollapseBiasDirection);
	Destroy();
}

void ABuildablePiece::Fracture(const FVector& ScatterOrigin, const FVector& ScatterBias) const
{
	const UDemolitionRules* Rules = DurabilityComponent ? DurabilityComponent->Rules : nullptr;
	UWorld* World = GetWorld();

	if (!Rules || !World)
	{
		return;
	}

	const int32 Count = Rules->ComputeFragmentCount(PieceClass);
	const FLinearColor Tint = Rules->GetFragmentTint(Material);
	UClass* SpawnClass = Rules->FragmentClass ? Rules->FragmentClass.Get() : AFragmentDebris::StaticClass();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector RandomOffset = FMath::VRand() * FMath::FRandRange(0.f, Rules->FragmentScatterRadius);
		const FRotator RandomRotation(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));

		AFragmentDebris* Fragment = World->SpawnActor<AFragmentDebris>(SpawnClass, ScatterOrigin + RandomOffset, RandomRotation);

		if (!Fragment)
		{
			continue;
		}

		const FVector OutwardDirection = RandomOffset.IsNearlyZero() ? FMath::VRand() : RandomOffset.GetSafeNormal();
		const FVector Impulse = (OutwardDirection + ScatterBias) * Rules->FragmentImpulseStrength;

		Fragment->Launch(Impulse, Tint, Rules->FragmentLifetime, Rules->FragmentFadeDuration);
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
