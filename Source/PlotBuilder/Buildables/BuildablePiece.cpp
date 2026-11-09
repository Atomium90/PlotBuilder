#include "BuildablePiece.h"
#include "PaintPalette.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Interaction/HotbarComponent.h"
#include "UI/PlayerMessages.h"
#include "Environment/StructuralSupportComponent.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
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

void ABuildablePiece::ApplyPaint(int32 NewIndex, const UPaintPalette* Palette)
{
	if (!MeshComponent || !Palette)
	{
		return;
	}

	PaintIndex = Palette->Wrap(NewIndex);

	// The colour is written even for the neutral entry (it is simply not shown): a stale colour never lingers next to a blend of 0.
	const FLinearColor Color = Palette->GetColor(PaintIndex);
	MeshComponent->SetCustomPrimitiveDataVector3(UPaintPalette::ColorIndex, FVector(Color.R, Color.G, Color.B));
	MeshComponent->SetCustomPrimitiveDataFloat(UPaintPalette::BlendIndex, Palette->IsNeutral(PaintIndex) ? 0.f : 1.f);
}

void ABuildablePiece::Interact_Implementation(AActor* InteractingActor)
{
	// A piece that is the child of another one (the glass of a window wall) is not an item of its own: it never leaves
	// the Stock state, so it would go into the bag alone. Aiming at it means the piece that carries it. The door leaf
	// is not concerned, its Blueprint replaces Interact to open and close.
	if (AActor* Parent = GetParentActor())
	{
		if (Parent->Implements<UInteractable>())
		{
			IInteractable::Execute_Interact(Parent, InteractingActor);
		}

		return;
	}

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
		// In a mission the hotbar is off and a house only comes down through the tools: no free demolition by hand.
		UHotbarComponent* Hotbar = InteractingActor ? InteractingActor->FindComponentByClass<UHotbarComponent>() : nullptr;

		if (Hotbar && !Hotbar->IsAllowed())
		{
			return;
		}

		// The piece goes back in the bag (to correct a mistake without losing it). The bag is checked before the piece
		// leaves the graph, so a full bag changes nothing.
		if (Hotbar)
		{
			if (!Hotbar->TakeBack(this))
			{
				if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
				{
					Messages->Notify(NSLOCTEXT("Buildable", "BagFull", "The bag is full, the piece stays where it is."), EMessageTone::Warning);
				}

				return;
			}
		}

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
	// One of the mesh's authored cuts, picked at random (null if the mesh has none, or none with a Geometry Collection).
	const FMeshFractureVariants* Variants = OwnMesh ? Rules->MeshFractureOverrides.Find(OwnMesh) : nullptr;
	const FMeshFractureOverride* Override = Variants ? Variants->PickVariant() : nullptr;

	if (Override)
	{
		// Match the destroyed mesh's own world rotation, not a fixed zero - the Geometry Collection
		// was authored against this mesh's unrotated orientation, so spawning it at the mesh's actual
		// world rotation is what keeps the fracture cut lined up with how the piece was actually
		// placed (a wall rotated 90° previously fractured along the wrong axis because of this).
		// MeshComponent's own rotation, not the actor's - correct even if the mesh has a relative
		// offset from the actor root, and identical to the actor's rotation when it doesn't.
		const FRotator MeshWorldRotation = MeshComponent ? MeshComponent->GetComponentRotation() : GetActorRotation();

		// Scale too: a piece rescaled in its Blueprint (the window panes share one 100 x 100 mesh and one
		// Geometry Collection, stretched per window wall) must break at its visible size, not at the size
		// the collection was authored at. Scale is applied from the actor origin, same as the mesh.
		const FVector MeshWorldScale = MeshComponent ? MeshComponent->GetComponentScale() : GetActorScale3D();
		const FTransform GeometryCollectionTransform(MeshWorldRotation, ScatterOrigin, MeshWorldScale);

		if (AGeometryCollectionActor* GeometryCollectionActor = World->SpawnActor<AGeometryCollectionActor>(AGeometryCollectionActor::StaticClass(), GeometryCollectionTransform))
		{
			if (UGeometryCollectionComponent* GCComponent = GeometryCollectionActor->GetGeometryCollectionComponent())
			{
				GCComponent->SetRestCollection(Override->Collection);
				GeometryCollectionActor->Tags.Add(FName("Fragment"));

				// Push first, then release: the chunks inherit the velocity of the cluster they break out of.
				// Collapse direction if the piece fell, else the direction authored on the override (turned by
				// the piece's yaw only, zero by default = no push, so a wall doesn't hop before breaking).
				const FVector InPlaceDirection = FRotator(0.f, MeshWorldRotation.Yaw, 0.f).RotateVector(Override->InPlaceImpulseDirection.GetSafeNormal());
				const FVector PushDirection = ScatterBias.IsNearlyZero() ? InPlaceDirection : ScatterBias;

				if (!PushDirection.IsNearlyZero())
				{
					GCComponent->AddImpulse(PushDirection * Rules->FragmentImpulseStrength, NAME_None, true);
				}

				// Break every chunk loose now. Left alone the piece stays one rigid body and only breaks if an
				// impact exceeds the collection's DamageThreshold (500 000 at the top level, never reached by
				// a light plate like glass, and lowering it to 1 wasn't enough in testing) - this skips that.
				GCComponent->CrumbleActiveClusters();
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

	Rules->SpawnGenericFragments(World, Rules->ComputeFragmentCount(PieceClass), Material, ScatterOrigin, ScatterBias);
}

void ABuildablePiece::UpdateStabilityDebugColor()
{
	if (MeshComponent)
	{
		MeshComponent->SetCustomPrimitiveDataFloat(0, Stability);
	}
}

void ABuildablePiece::ApplyWallClearance(const FWallClearance& Clearance)
{
	const UStaticMesh* StaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;

	// A falling piece simulates physics, and a Compromised one has its location driven by the jiggle: leave both alone.
	if (!StaticMesh || State != EBuildablePieceState::Placed || GetWorldTimerManager().IsTimerActive(CompromisedJiggleHandle))
	{
		return;
	}

	if (!bClearanceBaseCaptured)
	{
		if (Clearance.IsZero())
		{
			return;
		}

		ClearanceBaseLocation = GetActorLocation();
		ClearanceBaseScale = GetActorScale3D();
		bClearanceBaseCaptured = true;
	}

	const FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();
	const FVector LocalMin = MeshBounds.Origin - MeshBounds.BoxExtent;
	const FVector LocalMax = MeshBounds.Origin + MeshBounds.BoxExtent;

	// A quarter turn only ever swaps and flips the two horizontal axes, so each local axis points along one world axis.
	const FRotator Turn(0.f, Orientation * 90.f, 0.f);
	const FVector AxisDirection[2] = {Turn.RotateVector(FVector::XAxisVector), Turn.RotateVector(FVector::YAxisVector)};

	// The retreat on the side a direction faces.
	const auto RetreatToward = [&Clearance](const FVector& Direction)
	{
		if (FMath::Abs(Direction.X) > 0.5f)
		{
			return Direction.X > 0.f ? Clearance.MaxX : Clearance.MinX;
		}

		return Direction.Y > 0.f ? Clearance.MaxY : Clearance.MinY;
	};

	FVector NewScale = ClearanceBaseScale;
	FVector NewLocation = ClearanceBaseLocation;

	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const FVector Direction = AxisDirection[Axis];
		const float RetreatAtMax = RetreatToward(Direction);
		const float RetreatAtMin = RetreatToward(-Direction);
		const float Min = Axis == 0 ? LocalMin.X : LocalMin.Y;
		const float Max = Axis == 0 ? LocalMax.X : LocalMax.Y;
		const float BaseAxisScale = Axis == 0 ? ClearanceBaseScale.X : ClearanceBaseScale.Y;
		const float Length = (Max - Min) * BaseAxisScale;

		// Never shrink a piece to nothing: a piece that short has no business being retreated.
		if (RetreatAtMin + RetreatAtMax <= 0.f || Length <= RetreatAtMin + RetreatAtMax + 1.f)
		{
			continue;
		}

		const float NewAxisScale = BaseAxisScale * (Length - RetreatAtMin - RetreatAtMax) / Length;
		(Axis == 0 ? NewScale.X : NewScale.Y) = NewAxisScale;

		// The face on the min side has to end up RetreatAtMin further along the axis than where it started:
		// the pivot moves by what the new scale takes off (or adds to) the pivot-to-face distance, plus the retreat.
		NewLocation += Direction * ((BaseAxisScale - NewAxisScale) * Min + RetreatAtMin);
	}

	SetActorScale3D(NewScale);
	SetActorLocation(NewLocation);
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
