#include "BuildModeComponent.h"
#include "HotbarComponent.h"
#include "Buildables/BuildablePiece.h"
#include "Environment/Plot.h"
#include "Environment/StructuralSupportComponent.h"
#include "Environment/StructuralRules.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionShape.h"
#include "Kismet/GameplayStatics.h"

UBuildModeComponent::UBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBuildModeComponent::BeginPlay()
{
	Super::BeginPlay();

	HotbarComponent = GetOwner() ? GetOwner()->FindComponentByClass<UHotbarComponent>() : nullptr;

	// Single-plot demo for now - no Plot in the level just means placement isn't gated at all.
	CachedPlot = Cast<APlot>(UGameplayStatics::GetActorOfClass(GetWorld(), APlot::StaticClass()));
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
		const ABuildablePiece* CDO = LastPreviewedClass ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
		const EBuildAnchor Anchor = CDO ? CDO->GetBuildAnchor() : EBuildAnchor::Legacy;

		if (Anchor != EBuildAnchor::Legacy)
		{
			GEngine->AddOnScreenDebugMessage(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: %s | level %d | roof step %d | cell (%d,%d,%d) | %s%s"),
				*ClassName, BuildLevel, RoofStepValue, PendingGridCoord.X, PendingGridCoord.Y, PendingGridCoord.Z, *LastBlockReason, bIsRisky ? TEXT(" | risky") : TEXT("")));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: %s | rotation %d deg"), *ClassName, RotationSteps * FMath::RoundToInt(RotationStepDegrees)));
		}
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

	const ABuildablePiece* SelectedCDO = LastPreviewedClass ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
	const EBuildAnchor Anchor = SelectedCDO ? SelectedCDO->GetBuildAnchor() : EBuildAnchor::Legacy;

	// Vertex pieces (posts) don't rotate - they look the same from every side at a grid vertex.
	if (Anchor == EBuildAnchor::Vertex)
	{
		return;
	}

	// Edge pieces (walls, partitions) only flip 180 - which of the two edge candidates to snap to
	// is decided by proximity in TryResolveGridPlacement, not by this step.
	if (Anchor == EBuildAnchor::Edge)
	{
		RotationSteps = (RotationSteps + 1) % 2;
		return;
	}

	// Cell pieces (foundation, floor) rotate by 90 degree steps.
	if (Anchor == EBuildAnchor::Cell)
	{
		RotationSteps = (RotationSteps + FMath::Sign(Direction) + 4) % 4;
		return;
	}

	// Legacy: unchanged from week 1.
	const int32 StepsPerFullTurn = FMath::Max(1, FMath::RoundToInt(360.f / RotationStepDegrees));
	RotationSteps = (RotationSteps + FMath::Sign(Direction) + StepsPerFullTurn) % StepsPerFullTurn;
}

void UBuildModeComponent::ChangeBuildLevel(int32 Direction)
{
	if (!bIsActive || Direction == 0)
	{
		return;
	}

	BuildLevel = FMath::Clamp(BuildLevel + FMath::Sign(Direction), 0, MaxBuildLevel);
}

void UBuildModeComponent::ChangeRoofStep(int32 Direction)
{
	if (!bIsActive || Direction == 0)
	{
		return;
	}

	RoofStepValue = FMath::Clamp(RoofStepValue + FMath::Sign(Direction), 0, MaxRoofStep);
}

void UBuildModeComponent::ConfirmPlacement()
{
	if (!bIsActive || !HotbarComponent || !PreviewActor || PreviewActor->IsHidden())
	{
		return;
	}

	FInventorySlot SelectedEntry;

	if (!HotbarComponent->GetSelectedEntry(SelectedEntry) || !SelectedEntry.ActorClass
		|| !SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()))
	{
		return;
	}

	if (bIsBlocked)
	{
		if (bShowDebug && GEngine)
		{
			const bool bOutsidePlot = CachedPlot && !CachedPlot->IsLocationInside(PreviewActor->GetActorLocation());
			GEngine->AddOnScreenDebugMessage(501, 2.f, FColor::Red, bOutsidePlot
				? TEXT("Can't place here - outside the plot.")
				: TEXT("Can't place here - blocked."));
		}
		return;
	}

	const ABuildablePiece* SelectedCDO = SelectedEntry.ActorClass->GetDefaultObject<ABuildablePiece>();
	const EBuildAnchor Anchor = SelectedCDO ? SelectedCDO->GetBuildAnchor() : EBuildAnchor::Legacy;

	const FVector SpawnLocation = PreviewActor->GetActorLocation();
	const FRotator SpawnRotation = PreviewActor->GetActorRotation();
	const FIntVector GridCoordToApply = PendingGridCoord;
	const uint8 OrientationToApply = static_cast<uint8>(((PendingQuarterTurns % 4) + 4) % 4);
	const int32 RoofStepToApply = RoofStepValue;

	const FInventorySlot ConsumedEntry = HotbarComponent->ConsumeSelected();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = (Anchor != EBuildAnchor::Legacy)
		? ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		: ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(ConsumedEntry.ActorClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		Spawned->SetActorScale3D(ConsumedEntry.Scale);

		if (ABuildablePiece* NewPiece = Cast<ABuildablePiece>(Spawned))
		{
			NewPiece->State = EBuildablePieceState::Placed;

			if (Anchor != EBuildAnchor::Legacy)
			{
				NewPiece->SetGridPlacement(GridCoordToApply, OrientationToApply, RoofStepToApply);
			}

			if (CachedPlot && CachedPlot->GetStructuralSupport())
			{
				CachedPlot->GetStructuralSupport()->RegisterPiece(NewPiece);
			}
		}
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

	FInventorySlot SelectedEntry;
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
	RotationSteps = 0;

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

	const ABuildablePiece* SelectedCDO = LastPreviewedClass ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
	const EBuildAnchor Anchor = SelectedCDO ? SelectedCDO->GetBuildAnchor() : EBuildAnchor::Legacy;

	FVector CandidateLocation;
	FRotator CandidateRotation;

	if (Anchor != EBuildAnchor::Legacy)
	{
		FIntVector GridCoord;
		int32 QuarterTurns = 0;

		if (!TryResolveGridPlacement(ViewLocation, ViewRotation, Anchor, CandidateLocation, CandidateRotation, GridCoord, QuarterTurns))
		{
			PreviewActor->SetActorHiddenInGame(true);
			LastBlockReason = TEXT("not looking at the build plane");
			return;
		}

		PendingGridCoord = GridCoord;
		PendingQuarterTurns = QuarterTurns;
		PreviewActor->SetActorLocationAndRotation(CandidateLocation, CandidateRotation);

		bIsBlocked = IsGridCandidateBlocked(CandidateLocation, CandidateRotation, Anchor, GridCoord, QuarterTurns);
		bIsRisky = !bIsBlocked && IsGridCandidateRisky(SelectedCDO, GridCoord, QuarterTurns);
		UpdateGhostMaterial(bIsBlocked, bIsRisky);
		return;
	}

	AActor* SnapTarget = nullptr;

	if (!TryFindSnapTransform(ViewLocation, ViewRotation, CandidateLocation, CandidateRotation, SnapTarget))
	{
		ComputeFreeTransform(ViewLocation, ViewRotation, CandidateLocation, CandidateRotation);
	}

	PreviewActor->SetActorLocationAndRotation(CandidateLocation, CandidateRotation);

	bIsBlocked = IsCandidateBlocked(CandidateLocation, CandidateRotation, SnapTarget);
	bIsRisky = !bIsBlocked && IsCandidateRisky(SelectedCDO, CandidateLocation, CandidateRotation);
	UpdateGhostMaterial(bIsBlocked, bIsRisky);
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
	//
	// HitLocalPoint is relative to the hit piece's own PIVOT, but HitLocalExtent (from the mesh's
	// bounds) is centered on the mesh's bounds ORIGIN - which only coincides with the pivot for a
	// mesh whose pivot sits at its bounds center. The greybox kit's foundation/slab/walls pivot at
	// their base instead, so HitLocalPoint is re-centered on HitMeshBounds.Origin before dividing;
	// skipping this would misread which face was actually hit for any of them.
	const FVector HitLocalPoint = HitPiece->GetActorTransform().InverseTransformPosition(Hit.Location);
	const FBoxSphereBounds HitMeshBounds = HitPiece->GetMeshComponent()->GetStaticMesh()->GetBounds();
	const FVector HitLocalExtent = HitMeshBounds.BoxExtent;
	const FVector HitLocalPointCentered = HitLocalPoint - HitMeshBounds.Origin;

	const FVector Frac(
		HitLocalExtent.X > KINDA_SMALL_NUMBER ? HitLocalPointCentered.X / HitLocalExtent.X : 0.f,
		HitLocalExtent.Y > KINDA_SMALL_NUMBER ? HitLocalPointCentered.Y / HitLocalExtent.Y : 0.f,
		HitLocalExtent.Z > KINDA_SMALL_NUMBER ? HitLocalPointCentered.Z / HitLocalExtent.Z : 0.f);
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

	if (CachedPlot && !CachedPlot->IsLocationInside(BoxCenter))
	{
		return true;
	}

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

bool UBuildModeComponent::IsCandidateRisky(const ABuildablePiece* SelectedCDO, const FVector& Location, const FRotator& Rotation) const
{
	UStructuralSupportComponent* Structural = CachedPlot ? CachedPlot->GetStructuralSupport() : nullptr;

	if (!Structural || !SelectedCDO || SelectedCDO->GetSockets().IsEmpty())
	{
		return false;
	}

	const UStructuralRules* Rules = Structural->Rules;
	const float ComfortThreshold = Rules ? Rules->ComfortThreshold : 0.5f;
	return Structural->ComputeStability(SelectedCDO->GetSockets(), Location, Rotation) < ComfortThreshold;
}

bool UBuildModeComponent::TryResolveGridPlacement(const FVector& ViewLocation, const FRotator& ViewRotation, EBuildAnchor Anchor, FVector& OutLocation, FRotator& OutRotation, FIntVector& OutGridCoord, int32& OutQuarterTurns) const
{
	if (!CachedPlot || !CachedPlot->GetStructuralSupport())
	{
		return false;
	}

	const UStructuralRules* Rules = CachedPlot->GetStructuralSupport()->Rules;
	const float CellSize = Rules ? Rules->CellSize : 100.f;
	const float FloorHeight = Rules ? Rules->FloorHeight : 300.f;
	const float RoofStepHeight = Rules ? Rules->RoofStepHeight : 100.f;

	const ABuildablePiece* SelectedCDO = LastPreviewedClass ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
	const float VerticalOffset = SelectedCDO ? SelectedCDO->GetVerticalOffset() : 0.f;

	// FoundationTopZ(BuildLevel): the fondation's own pivot sits 20cm above the plot's ground
	// surface (it's buried 10cm of its 30cm thickness) - every other piece at a given level shares
	// that same Z, one FloorHeight step apart per level (see the greybox spec doc). RoofStepValue
	// adds a second, independent vertical term on top (RoofStepHeight per step) for a roof piece
	// climbing a slope or stacking a gable infill - it never changes what BuildLevel itself means,
	// and it's always 0 unless the player has actually used ChangeRoofStep.
	const float PlaneZ = CachedPlot->GetGroundZ() + 20.f + BuildLevel * FloorHeight + RoofStepValue * RoofStepHeight + VerticalOffset;

	const FVector RayDir = ViewRotation.Vector();

	if (FMath::IsNearlyZero(RayDir.Z))
	{
		return false;
	}

	const float T = (PlaneZ - ViewLocation.Z) / RayDir.Z;

	if (T <= 0.f || T > MaxGridPlaceDistance)
	{
		return false;
	}

	const FVector HitPoint = ViewLocation + RayDir * T;
	const FVector GridOrigin = CachedPlot->GetGridOrigin();

	// Hit point in the plot's grid frame, in units of CellSize - not yet snapped to an integer.
	const float GridX = (HitPoint.X - GridOrigin.X) / CellSize;
	const float GridY = (HitPoint.Y - GridOrigin.Y) / CellSize;

	const auto NearestEven = [](float Value) { return 2 * FMath::RoundToInt(Value / 2.f); };
	const auto NearestOdd = [](float Value) { return 2 * FMath::RoundToInt((Value - 1.f) / 2.f) + 1; };

	int32 CellX = 0;
	int32 CellY = 0;
	float Yaw = 0.f;

	switch (Anchor)
	{
	case EBuildAnchor::Cell:
		CellX = NearestEven(GridX);
		CellY = NearestEven(GridY);
		Yaw = RotationSteps * 90.f;
		break;

	case EBuildAnchor::Vertex:
		CellX = NearestOdd(GridX);
		CellY = NearestOdd(GridY);
		Yaw = 0.f;
		break;

	case EBuildAnchor::Edge:
		{
			// Candidate A: (odd, even) - the piece's local Y runs along the edge, base Yaw 0.
			const int32 AX = NearestOdd(GridX);
			const int32 AY = NearestEven(GridY);
			// Candidate B: (even, odd) - local X runs along the edge instead, base Yaw 90.
			const int32 BX = NearestEven(GridX);
			const int32 BY = NearestOdd(GridY);

			const float DistASq = FMath::Square(GridX - AX) + FMath::Square(GridY - AY);
			const float DistBSq = FMath::Square(GridX - BX) + FMath::Square(GridY - BY);
			const bool bVerticalEdge = DistASq <= DistBSq;

			CellX = bVerticalEdge ? AX : BX;
			CellY = bVerticalEdge ? AY : BY;
			const float BaseYaw = bVerticalEdge ? 0.f : 90.f;

			// Face-anchored pieces (walls): the mesh extends towards -X local from the pivot, so
			// which of the two possible yaws (BaseYaw or BaseYaw+180) to use decides which of the
			// two flanking cells the mesh actually sits over - pick whichever one has a floor at
			// this level, read from the occupancy table (never a physics trace, a cell can be
			// occupied before anything is spawned for it to hit). Centered pieces (partitions) have
			// no inside/outside to pick, so they keep the old behaviour: manual flip only.
			float PreferredFlip = 0.f;

			if (SelectedCDO && SelectedCDO->IsFaceAnchoredEdge() && CachedPlot && CachedPlot->GetStructuralSupport())
			{
				const UStructuralSupportComponent* Structural = CachedPlot->GetStructuralSupport();

				// "Minus" is the side BaseYaw's -X local already points to (no flip); "plus" is the
				// other side.
				const FIntVector MinusNeighbor = bVerticalEdge ? FIntVector(CellX - 1, CellY, BuildLevel) : FIntVector(CellX, CellY - 1, BuildLevel);
				const FIntVector PlusNeighbor = bVerticalEdge ? FIntVector(CellX + 1, CellY, BuildLevel) : FIntVector(CellX, CellY + 1, BuildLevel);

				const bool bMinusHasFloor = Structural->GetPieceAt(EBuildAnchor::Cell, MinusNeighbor) != nullptr;
				const bool bPlusHasFloor = Structural->GetPieceAt(EBuildAnchor::Cell, PlusNeighbor) != nullptr;

				if (bMinusHasFloor != bPlusHasFloor)
				{
					PreferredFlip = bMinusHasFloor ? 0.f : 180.f;
				}
				else
				{
					// Ambiguous (both or neither have a floor yet) - fall back to which side of the
					// edge line the view ray actually landed on.
					const float SignedOffset = bVerticalEdge ? (GridX - CellX) : (GridY - CellY);
					PreferredFlip = SignedOffset < 0.f ? 0.f : 180.f;
				}
			}

			// Manual override on top of the auto-picked side (or the sole driver, for a centered
			// piece) - RotatePreview toggles this for Edge pieces.
			const float ManualFlip = (RotationSteps % 2) * 180.f;
			Yaw = FMath::Fmod(BaseYaw + PreferredFlip + ManualFlip, 360.f);
		}
		break;

	default:
		return false;
	}

	OutGridCoord = FIntVector(CellX, CellY, BuildLevel);
	OutQuarterTurns = FMath::RoundToInt(Yaw / 90.f);
	OutLocation = FVector(GridOrigin.X + CellX * CellSize, GridOrigin.Y + CellY * CellSize, PlaneZ);
	OutRotation = FRotator(0.f, Yaw, 0.f);

	return true;
}

bool UBuildModeComponent::IsGridCandidateBlocked(const FVector& Location, const FRotator& Rotation, EBuildAnchor Anchor, const FIntVector& GridCoord, int32 QuarterTurns) const
{
	if (!CachedPlot)
	{
		LastBlockReason = TEXT("no plot in the level");
		return true;
	}

	if (!CachedPlot->IsLocationInside(Location))
	{
		LastBlockReason = TEXT("outside the plot");
		return true;
	}

	UStructuralSupportComponent* Structural = CachedPlot->GetStructuralSupport();

	if (Structural && !Structural->IsSlotFree(Anchor, GridCoord))
	{
		LastBlockReason = TEXT("slot occupied");
		return true;
	}

	UStaticMeshComponent* MeshComp = PreviewActor ? PreviewActor->GetStaticMeshComponent() : nullptr;
	UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;

	if (Mesh)
	{
		const FBoxSphereBounds LocalBounds = Mesh->GetBounds();
		const FVector Scale = PreviewActor->GetActorScale3D();
		FVector BoxCenter = Location + Rotation.RotateVector(LocalBounds.Origin * Scale);
		FVector HalfExtent(LocalBounds.BoxExtent.X * Scale.X, LocalBounds.BoxExtent.Y * Scale.Y, LocalBounds.BoxExtent.Z * Scale.Z);

		// Clip the test box at ground level - a foundation's buried portion is intentional, never
		// an obstruction. Rotation here is yaw-only, so the vertical extent doesn't depend on it.
		// GroundClipMargin nudges the clipped bottom slightly above GroundZ rather than exactly onto
		// it - an exact flush touch against the ground mesh would otherwise register as an overlap
		// (same float/narrow-phase issue IsCandidateBlocked's OverlapShrink works around for the
		// Legacy path's flush-adjacent pieces).
		constexpr float GroundClipMargin = 0.5f;
		const float GroundZ = CachedPlot->GetGroundZ();
		const float Top = BoxCenter.Z + HalfExtent.Z;
		const float Bottom = BoxCenter.Z - HalfExtent.Z;

		if (Bottom < GroundZ)
		{
			const float NewBottom = FMath::Min(GroundZ + GroundClipMargin, Top);
			HalfExtent.Z = FMath::Max((Top - NewBottom) * 0.5f, 0.f);
			BoxCenter.Z = (Top + NewBottom) * 0.5f;
		}

		if (HalfExtent.Z > 0.f)
		{
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(PreviewActor);
			QueryParams.AddIgnoredActor(GetOwner());

			FCollisionObjectQueryParams ObjectParams;
			ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
			ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
			ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
			ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

			TArray<FOverlapResult> Overlaps;
			GetWorld()->OverlapMultiByObjectType(Overlaps, BoxCenter, Rotation.Quaternion(), ObjectParams, FCollisionShape::MakeBox(HalfExtent), QueryParams);

			for (const FOverlapResult& Overlap : Overlaps)
			{
				// Other Buildables are governed by the occupancy table above and by stability below,
				// not raw overlap - two grid pieces are expected to touch at a shared edge/vertex.
				if (AActor* OtherActor = Overlap.GetActor())
				{
					if (!OtherActor->IsA<ABuildablePiece>())
					{
						LastBlockReason = TEXT("obstacle");
						return true;
					}
				}
			}
		}
	}

	LastBlockReason = TEXT("free");
	return false;
}

bool UBuildModeComponent::IsGridCandidateRisky(const ABuildablePiece* SelectedCDO, const FIntVector& GridCoord, int32 QuarterTurns) const
{
	UStructuralSupportComponent* Structural = CachedPlot ? CachedPlot->GetStructuralSupport() : nullptr;

	if (!Structural || !SelectedCDO || SelectedCDO->GetSockets().IsEmpty())
	{
		return false;
	}

	const UStructuralRules* Rules = Structural->Rules;
	const float ComfortThreshold = Rules ? Rules->ComfortThreshold : 0.5f;
	return Structural->ComputeStability(SelectedCDO->GetSockets(), GridCoord, QuarterTurns, RoofStepValue) < ComfortThreshold;
}

void UBuildModeComponent::UpdateGhostMaterial(bool bBlocked, bool bRisky)
{
	UStaticMeshComponent* MeshComp = PreviewActor ? PreviewActor->GetStaticMeshComponent() : nullptr;

	if (!MeshComp)
	{
		return;
	}

	if (!bMaterialNeedsRefresh && bBlocked == bLastAppliedBlocked && bRisky == bLastAppliedRisky)
	{
		return;
	}

	// A hard block always wins over a stability warning - bRisky is meaningless once bBlocked is true.
	UMaterialInterface* MaterialToApply = bBlocked ? BlockedGhostMaterial : bRisky ? RiskyGhostMaterial : GhostMaterial;

	for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
	{
		MeshComp->SetMaterial(i, MaterialToApply);
	}

	bLastAppliedBlocked = bBlocked;
	bLastAppliedRisky = bRisky;
	bMaterialNeedsRefresh = false;
}
