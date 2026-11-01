#include "BuildablePiece.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Interaction/HotbarComponent.h"
#include "Environment/StructuralSupportComponent.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Destruction/FragmentDebris.h"
#include "Destruction/DebrisSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"

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

	// GDD §7.6, D29: the immediate "once when the piece enters Critical" bottleneck check - entering
	// Critical doesn't yet mean this piece's own stability changed (that's tracked separately), just
	// that it's now eligible to warn and fail on its own if nothing else holds up its dependants.
	if (NewState == EDurabilityState::Critical && IsLoadBearing() && StructuralComponent)
	{
		StructuralComponent->NotifyEnteredCritical(this);
	}
}

void ABuildablePiece::HandleDurabilityDestroyed()
{
	// Direct kill (tool damage reached 0 HP) - stop any Compromised warning first (D29), it never
	// gets to fail on its own if the tool just finished the job directly.
	OnCompromisedCancelled();

	// Fractures in place, no Falling phase, unlike the cascade path below. Unregister first, same as
	// Demolish(), but destroy via Fracture instead.
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
	// Voluntary removal can happen while Compromised (still Placed, not yet Collapsing) - stop the
	// warning before it's moot.
	OnCompromisedCancelled();

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

	const UStaticMesh* OwnMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	const FMeshFractureOverride* Override = OwnMesh ? Rules->MeshFractureOverrides.Find(OwnMesh) : nullptr;

	if (Override && Override->Collection)
	{
		// Match the destroyed mesh's own world rotation, not a fixed zero - the Geometry Collection
		// was authored against this mesh's unrotated orientation, so spawning it at the mesh's actual
		// world rotation is what keeps the fracture cut lined up with how the piece was actually
		// placed (a wall rotated 90° previously fractured along the wrong axis because of this).
		// MeshComponent's own rotation, not the actor's - correct even if the mesh has a relative
		// offset from the actor root, and identical to the actor's rotation when it doesn't.
		const FRotator MeshWorldRotation = MeshComponent ? MeshComponent->GetComponentRotation() : GetActorRotation();

		if (AGeometryCollectionActor* GeometryCollectionActor = World->SpawnActor<AGeometryCollectionActor>(ScatterOrigin, MeshWorldRotation))
		{
			if (UGeometryCollectionComponent* GCComponent = GeometryCollectionActor->GetGeometryCollectionComponent())
			{
				GCComponent->SetRestCollection(Override->Collection);
				GeometryCollectionActor->Tags.Add(FName("Fragment"));

				const FVector Impulse = (ScatterBias.IsNearlyZero() ? FVector::UpVector : ScatterBias) * Rules->FragmentImpulseStrength;
				GCComponent->AddImpulse(Impulse, NAME_None, true);
			}

			// No scale-to-zero fade here, unlike the generic path below - Chaos clusters don't have
			// a clean uniform-scale fade hook, so this is a hard pop at end of life. Accepted as a v1
			// gap, same "simple over literal" call already made elsewhere this sprint.
			GeometryCollectionActor->SetLifeSpan(Rules->FragmentLifetime);

			if (UDebrisSubsystem* DebrisSubsystem = World->GetSubsystem<UDebrisSubsystem>())
			{
				DebrisSubsystem->RegisterDebrisActor(GeometryCollectionActor, Rules->FragmentGlobalCap, FMath::Max(1, Override->FragmentCount));
			}
		}

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

		Fragment->Launch(Impulse, Tint, Rules->FragmentLifetime, Rules->FragmentFadeDuration, Rules->FragmentGlobalCap, Rules->FragmentSleepVelocityThreshold);
	}
}

void ABuildablePiece::UpdateStabilityDebugColor()
{
	if (MeshComponent)
	{
		MeshComponent->SetCustomPrimitiveDataFloat(0, Stability);
	}
}

void ABuildablePiece::OnCompromisedStart()
{
	if (GetWorldTimerManager().IsTimerActive(CompromisedJiggleHandle))
	{
		// Already warning (shouldn't happen - UStructuralSupportComponent only calls this on a fresh
		// queue entry - but never stack a second audio component/jiggle base over the first).
		return;
	}

	USoundBase* WarningSound = DurabilityComponent ? DurabilityComponent->GetCompromisedWarningSound() : nullptr;

	if (WarningSound && MeshComponent)
	{
		CompromisedWarningAudio = UGameplayStatics::SpawnSoundAttached(WarningSound, MeshComponent);

		if (CompromisedWarningAudio)
		{
			CompromisedWarningAudio->bAutoDestroy = false;
			CompromisedWarningAudio->Play();
		}
	}

	if (MeshComponent)
	{
		CompromisedBaseRelativeLocation = MeshComponent->GetRelativeLocation();
		GetWorldTimerManager().SetTimer(CompromisedJiggleHandle, this, &ABuildablePiece::HandleCompromisedJiggle, 0.03f, true);
	}
}

void ABuildablePiece::OnCompromisedCancelled()
{
	if (!GetWorldTimerManager().IsTimerActive(CompromisedJiggleHandle) && !CompromisedWarningAudio)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CompromisedJiggleHandle);

	if (MeshComponent)
	{
		MeshComponent->SetRelativeLocation(CompromisedBaseRelativeLocation);
	}

	if (CompromisedWarningAudio)
	{
		CompromisedWarningAudio->Stop();
		CompromisedWarningAudio->DestroyComponent();
		CompromisedWarningAudio = nullptr;
	}
}

void ABuildablePiece::HandleCompromisedJiggle()
{
	if (!MeshComponent)
	{
		return;
	}

	const float Phase = GetWorld()->GetTimeSeconds() * 2.f * PI * 12.f; // ~12 Hz, cheap and visibly nervous without reading as a seizure
	const FVector Offset(0.f, 0.f, FMath::Sin(Phase) * CompromisedJiggleAmplitude);
	MeshComponent->SetRelativeLocation(CompromisedBaseRelativeLocation + Offset);
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
