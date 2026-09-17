#include "BuildModeComponent.h"
#include "HotbarComponent.h"
#include "Buildables/BuildablePiece.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionShape.h"

UBuildModeComponent::UBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBuildModeComponent::BeginPlay()
{
	Super::BeginPlay();

	HotbarComponent = GetOwner() ? GetOwner()->FindComponentByClass<UHotbarComponent>() : nullptr;
}

void UBuildModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsActive)
	{
		return;
	}

	RefreshPreviewMesh();
	UpdatePreviewTransform();

	if (bShowDebug && GEngine)
	{
		const FString ClassName = LastPreviewedClass ? LastPreviewedClass->GetName() : TEXT("(none)");
		GEngine->AddOnScreenDebugMessage(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: %s | rotation %d deg"), *ClassName, RotationSteps * FMath::RoundToInt(RotationStepDegrees)));
	}
}

void UBuildModeComponent::ToggleBuildMode()
{
	if (bIsActive)
	{
		ExitBuildMode();
	}
	else
	{
		EnterBuildMode();
	}
}

void UBuildModeComponent::EnterBuildMode()
{
	if (!HotbarComponent)
	{
		return;
	}

	bIsActive = true;
	RotationSteps = 0;

	// If the current hotbar selection isn't a buildable, snap it to the nearest one that is.
	HotbarComponent->EnsureValidSelection(/*bBuildablesOnly=*/true);

	if (!PreviewActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = GetWorld()->SpawnActor<AStaticMeshActor>(SpawnParams);

		if (PreviewActor)
		{
			if (UStaticMeshComponent* MeshComp = PreviewActor->GetStaticMeshComponent())
			{
				MeshComp->SetMobility(EComponentMobility::Movable);
				MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				MeshComp->SetCastShadow(false);
			}

			LastPreviewedClass = nullptr;
		}
	}

	RefreshPreviewMesh();
}

void UBuildModeComponent::ExitBuildMode()
{
	bIsActive = false;
	RotationSteps = 0;

	if (PreviewActor)
	{
		PreviewActor->SetActorHiddenInGame(true);
	}
}

void UBuildModeComponent::RotatePreview(int32 Direction)
{
	if (!bIsActive || Direction == 0)
	{
		return;
	}

	const int32 StepsPerFullTurn = FMath::Max(1, FMath::RoundToInt(360.f / RotationStepDegrees));
	RotationSteps = (RotationSteps + FMath::Sign(Direction) + StepsPerFullTurn) % StepsPerFullTurn;
}

void UBuildModeComponent::ConfirmPlacement()
{
	if (!bIsActive || !HotbarComponent || !PreviewActor || PreviewActor->IsHidden())
	{
		return;
	}

	FHotbarEntry SelectedEntry;

	if (!HotbarComponent->GetSelectedEntry(SelectedEntry) || !SelectedEntry.ActorClass
		|| !SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()))
	{
		return;
	}

	if (bIsBlocked)
	{
		if (bShowDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(501, 2.f, FColor::Red, TEXT("Can't place here - blocked."));
		}
		return;
	}

	const FVector SpawnLocation = PreviewActor->GetActorLocation();
	const FRotator SpawnRotation = PreviewActor->GetActorRotation();

	const FHotbarEntry ConsumedEntry = HotbarComponent->ConsumeSelected();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(ConsumedEntry.ActorClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		Spawned->SetActorScale3D(ConsumedEntry.Scale);
	}

	// The hotbar selection has already advanced (same-class continuation, or the next available
	// entry) - refresh right away so the ghost doesn't show a stale mesh for one frame.
	RefreshPreviewMesh();
}

void UBuildModeComponent::RefreshPreviewMesh()
{
	if (!HotbarComponent || !PreviewActor)
	{
		return;
	}

	FHotbarEntry SelectedEntry;
	const bool bHasBuildableSelected = HotbarComponent->GetSelectedEntry(SelectedEntry) && SelectedEntry.ActorClass
		&& SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass());

	if (!bHasBuildableSelected)
	{
		PreviewActor->SetActorHiddenInGame(true);
		LastPreviewedClass = nullptr;
		return;
	}

	PreviewActor->SetActorHiddenInGame(false);

	if (SelectedEntry.ActorClass == LastPreviewedClass)
	{
		// Same class as last frame - only the transform needs updating (done every tick separately).
		return;
	}

	LastPreviewedClass = SelectedEntry.ActorClass;

	const ABuildablePiece* CDO = SelectedEntry.ActorClass->GetDefaultObject<ABuildablePiece>();
	UStaticMeshComponent* MeshComp = PreviewActor->GetStaticMeshComponent();

	if (!CDO || !MeshComp)
	{
		return;
	}

	MeshComp->SetStaticMesh(CDO->GetMeshComponent() ? CDO->GetMeshComponent()->GetStaticMesh() : nullptr);
	PreviewActor->SetActorScale3D(SelectedEntry.Scale);

	// New mesh slots have no ghost material assigned yet - force UpdateGhostMaterial to (re)apply
	// on this tick regardless of whether the blocked state itself changed.
	bMaterialNeedsRefresh = true;
}

void UBuildModeComponent::UpdatePreviewTransform()
{
	if (!HotbarComponent || !PreviewActor || PreviewActor->IsHidden())
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

	FVector CandidateLocation;
	FRotator CandidateRotation;
	AActor* SnapTarget = nullptr;

	if (!TryFindSnapTransform(ViewLocation, ViewRotation, CandidateLocation, CandidateRotation, SnapTarget))
	{
		ComputeFreeTransform(ViewLocation, ViewRotation, CandidateLocation, CandidateRotation);
	}

	PreviewActor->SetActorLocationAndRotation(CandidateLocation, CandidateRotation);

	bIsBlocked = IsCandidateBlocked(CandidateLocation, CandidateRotation, SnapTarget);
	UpdateGhostMaterial(bIsBlocked);
}

void UBuildModeComponent::ComputeFreeTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = ViewLocation + ViewRotation.Vector() * HotbarComponent->PlaceDistance;
	OutLocation.X = FMath::GridSnap(OutLocation.X, HotbarComponent->GridSize);
	OutLocation.Y = FMath::GridSnap(OutLocation.Y, HotbarComponent->GridSize);
	OutLocation.Z = FMath::GridSnap(OutLocation.Z, HotbarComponent->GridSize);

	OutRotation = FRotator(0.f, RotationSteps * RotationStepDegrees, 0.f);
}

bool UBuildModeComponent::TryFindSnapTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation, AActor*& OutSnapTarget) const
{
	OutSnapTarget = nullptr;

	if (!LastPreviewedClass)
	{
		return false;
	}

	const ABuildablePiece* SelectedCDO = LastPreviewedClass->GetDefaultObject<ABuildablePiece>();
	const UStaticMesh* SelectedMesh = SelectedCDO && SelectedCDO->GetMeshComponent() ? SelectedCDO->GetMeshComponent()->GetStaticMesh() : nullptr;

	if (!SelectedMesh)
	{
		return false;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(PreviewActor);

	FHitResult Hit;
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * HotbarComponent->PlaceDistance;

	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams))
	{
		return false;
	}

	const FVector PreviewScale = PreviewActor ? PreviewActor->GetActorScale3D() : FVector::OneVector;
	ABuildablePiece* HitPiece = Cast<ABuildablePiece>(Hit.GetActor());

	if (!HitPiece || !HitPiece->GetMeshComponent() || !HitPiece->GetMeshComponent()->GetStaticMesh())
	{
		// Not a placed piece (ground, level geometry, a Prop...) - rest the preview flush on
		// whatever was actually hit instead of floating at a fixed distance from the view.
		const FBoxSphereBounds LocalBounds = SelectedMesh->GetBounds();

		OutRotation = FRotator(0.f, RotationSteps * RotationStepDegrees, 0.f);
		OutLocation.X = FMath::GridSnap(Hit.Location.X, HotbarComponent->GridSize);
		OutLocation.Y = FMath::GridSnap(Hit.Location.Y, HotbarComponent->GridSize);

		// Rotation here is yaw-only, which never changes Z, so the mesh's local bottom stays a
		// fixed offset below its pivot regardless of RotationSteps.
		const float LocalBottomZ = (LocalBounds.Origin.Z - LocalBounds.BoxExtent.Z) * PreviewScale.Z;
		OutLocation.Z = Hit.Location.Z - LocalBottomZ;

		return true;
	}

	// Which local face of the hit piece to snap to is decided from where the hit lands within
	// its OWN local bounding box, not just the literal impact normal: the physically-struck face
	// always has a fraction of ~1 along its axis, so a plain "largest fraction" would never pick a
	// thin top/side edge unless the ray grazed it exactly. Biasing towards a secondary axis once
	// its fraction crosses EdgeMargin lets aiming near the top/side portion of the big front face
	// resolve to that edge instead, without requiring pixel-perfect aim on a razor-thin face.
	const FVector HitLocalPoint = HitPiece->GetActorTransform().InverseTransformPosition(Hit.Location);
	const FVector HitLocalExtent = HitPiece->GetMeshComponent()->GetStaticMesh()->GetBounds().BoxExtent;

	const FVector Frac(
		HitLocalExtent.X > KINDA_SMALL_NUMBER ? HitLocalPoint.X / HitLocalExtent.X : 0.f,
		HitLocalExtent.Y > KINDA_SMALL_NUMBER ? HitLocalPoint.Y / HitLocalExtent.Y : 0.f,
		HitLocalExtent.Z > KINDA_SMALL_NUMBER ? HitLocalPoint.Z / HitLocalExtent.Z : 0.f);
	const FVector AbsFrac(FMath::Abs(Frac.X), FMath::Abs(Frac.Y), FMath::Abs(Frac.Z));

	int32 AxisIndex = 2;
	if (AbsFrac.X >= AbsFrac.Y && AbsFrac.X >= AbsFrac.Z)
	{
		AxisIndex = 0;
	}
	else if (AbsFrac.Y >= AbsFrac.Z)
	{
		AxisIndex = 1;
	}

	constexpr float EdgeMargin = 0.65f;
	float BestSecondaryFrac = EdgeMargin;

	for (int32 i = 0; i < 3; ++i)
	{
		if (i != AxisIndex && AbsFrac[i] > BestSecondaryFrac)
		{
			BestSecondaryFrac = AbsFrac[i];
			AxisIndex = i;
		}
	}

	const float Sign = FMath::Sign(Frac[AxisIndex]);

	OutRotation = HitPiece->GetActorRotation() + FRotator(0.f, RotationSteps * RotationStepDegrees, 0.f);

	// Snap axis/sign are expressed in the hit piece's LOCAL space (found above) - convert to a
	// world direction to offset along, since positions are world-space.
	FVector LocalAxisUnit = FVector::ZeroVector;
	LocalAxisUnit[AxisIndex] = 1.f;
	const FVector WorldAxisDir = HitPiece->GetActorRotation().RotateVector(LocalAxisUnit);

	const FVector SelectedLocalExtent = SelectedMesh->GetBounds().BoxExtent * PreviewScale;
	const float HitHalfExtentAlongAxis = HitLocalExtent[AxisIndex];
	const float SelectedHalfExtentAlongAxis = ProjectExtentOntoAxis(SelectedLocalExtent, OutRotation.Quaternion(), WorldAxisDir);

	const FVector HitWorldCenter = HitPiece->GetMeshComponent()->Bounds.Origin;
	const FVector BoxCenter = HitWorldCenter + WorldAxisDir * Sign * (HitHalfExtentAlongAxis + SelectedHalfExtentAlongAxis);

	// The ghost's pivot may not sit at its mesh's bounding box center (e.g. a base-pivoted wall) -
	// correct for that offset so BoxCenter above is where the box actually ends up.
	const FVector LocalOrigin = SelectedMesh->GetBounds().Origin * PreviewScale;
	OutLocation = BoxCenter - OutRotation.RotateVector(LocalOrigin);
	OutSnapTarget = HitPiece;

	return true;
}

float UBuildModeComponent::ProjectExtentOntoAxis(const FVector& LocalHalfExtent, const FQuat& Rotation, const FVector& WorldAxis)
{
	return FMath::Abs(FVector::DotProduct(Rotation.RotateVector(FVector(LocalHalfExtent.X, 0.f, 0.f)), WorldAxis))
		+ FMath::Abs(FVector::DotProduct(Rotation.RotateVector(FVector(0.f, LocalHalfExtent.Y, 0.f)), WorldAxis))
		+ FMath::Abs(FVector::DotProduct(Rotation.RotateVector(FVector(0.f, 0.f, LocalHalfExtent.Z)), WorldAxis));
}

bool UBuildModeComponent::IsCandidateBlocked(const FVector& Location, const FRotator& Rotation, AActor* SnapTarget) const
{
	UStaticMeshComponent* MeshComp = PreviewActor ? PreviewActor->GetStaticMeshComponent() : nullptr;
	UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;

	if (!Mesh)
	{
		return false;
	}

	const FBoxSphereBounds LocalBounds = Mesh->GetBounds();
	const FVector Scale = PreviewActor->GetActorScale3D();
	const FVector BoxCenter = Location + Rotation.RotateVector(LocalBounds.Origin * Scale);

	// Flush-adjacent pieces touch at zero distance - float error in the snap math (or just the
	// physics engine treating an exact touch as a hit) would otherwise flag every neighbour as
	// blocking, not just the one this frame's SnapTarget ignores. Shrinking the test box by a
	// small tolerance absorbs that without weakening genuine overlap detection.
	constexpr float OverlapShrink = 0.5f;
	const FVector HalfExtent(
		FMath::Max(LocalBounds.BoxExtent.X * Scale.X - OverlapShrink, 1.f),
		FMath::Max(LocalBounds.BoxExtent.Y * Scale.Y - OverlapShrink, 1.f),
		FMath::Max(LocalBounds.BoxExtent.Z * Scale.Z - OverlapShrink, 1.f));

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(PreviewActor);
	QueryParams.AddIgnoredActor(GetOwner());

	if (SnapTarget)
	{
		// Flush against the snap target is expected, not an obstruction.
		QueryParams.AddIgnoredActor(SnapTarget);
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	// A single box overlap test - covers other Buildables, Props, the environment and the player
	// capsule in one cheap query, no per-object-type special-casing needed.
	return GetWorld()->OverlapAnyTestByObjectType(BoxCenter, Rotation.Quaternion(), ObjectParams, FCollisionShape::MakeBox(HalfExtent), QueryParams);
}

void UBuildModeComponent::UpdateGhostMaterial(bool bBlocked)
{
	UStaticMeshComponent* MeshComp = PreviewActor ? PreviewActor->GetStaticMeshComponent() : nullptr;

	if (!MeshComp)
	{
		return;
	}

	if (!bMaterialNeedsRefresh && bBlocked == bLastAppliedBlocked)
	{
		return;
	}

	UMaterialInterface* MaterialToApply = bBlocked ? BlockedGhostMaterial : GhostMaterial;

	for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
	{
		MeshComp->SetMaterial(i, MaterialToApply);
	}

	bLastAppliedBlocked = bBlocked;
	bMaterialNeedsRefresh = false;
}
