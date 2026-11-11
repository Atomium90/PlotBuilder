#include "BuildModeComponent.h"
#include "Debug/DebugDisplay.h"
#include "HotbarComponent.h"
#include "Buildables/BuildablePiece.h"
#include "Environment/Plot.h"
#include "Environment/StructuralSupportComponent.h"
#include "Environment/StructuralRules.h"
#include "Props/PhysicsProp.h"
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
		const ABuildablePiece* CDO = GetPreviewedPiece();
		const EBuildAnchor Anchor = CDO ? CDO->GetBuildAnchor() : EBuildAnchor::Legacy;

		if (GetPreviewedProp())
		{
			PBDebug::Message(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: prop %s | rotation %d deg | on %s | %s"),
				*ClassName, FMath::RoundToInt(RotationSteps * PropRotationStepDegrees), PendingSupport.IsValid() ? *PendingSupport->GetName() : TEXT("(nothing)"), *LastBlockReason));
		}
		else if (Anchor != EBuildAnchor::Legacy)
		{
			PBDebug::Message(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: %s | level %d | roof step %d | cell (%d,%d,%d) | %s%s"),
				*ClassName, BuildLevel, RoofStepValue, PendingGridCoord.X, PendingGridCoord.Y, PendingGridCoord.Z, *LastBlockReason, bIsRisky ? TEXT(" | risky") : TEXT("")));
		}
		else
		{
			PBDebug::Message(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: %s | rotation %d deg"), *ClassName, RotationSteps * FMath::RoundToInt(RotationStepDegrees)));
		}
	}
}

void UBuildModeComponent::ToggleBuildMode()
{
	if (bIsActive)
	{
		ExitBuildMode();
	}
	else if (bAllowed)
	{
		EnterBuildMode();
	}
}

void UBuildModeComponent::SetAllowed(bool bNewAllowed)
{
	bAllowed = bNewAllowed;

	if (!bAllowed && bIsActive)
	{
		ExitBuildMode();
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

	// A prop turns by its own, finer step: furniture is arranged by eye.
	if (GetPreviewedProp())
	{
		const int32 StepsPerTurn = FMath::Max(1, FMath::RoundToInt(360.f / FMath::Max(1.f, PropRotationStepDegrees)));
		RotationSteps = (RotationSteps + FMath::Sign(Direction) + StepsPerTurn) % StepsPerTurn;
		return;
	}

	const ABuildablePiece* SelectedCDO = GetPreviewedPiece();
	const EBuildAnchor Anchor = SelectedCDO ? SelectedCDO->GetBuildAnchor() : EBuildAnchor::Legacy;

	// Vertex pieces (posts) don't rotate - they look the same from every side at a grid vertex.
	// Neither do ceiling lamps, which are round and hang at the centre of a cell.
	if (Anchor == EBuildAnchor::Vertex || Anchor == EBuildAnchor::Ceiling)
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
		|| !(SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()) || SelectedEntry.ActorClass->IsChildOf(APhysicsProp::StaticClass())))
	{
		return;
	}

	if (bIsBlocked)
	{
		if (bShowDebug && GEngine)
		{
			const bool bOutsidePlot = CachedPlot && !CachedPlot->IsLocationInside(PreviewActor->GetActorLocation());
			PBDebug::Message(501, 2.f, FColor::Red, bOutsidePlot
				? TEXT("Can't place here - outside the plot.")
				: TEXT("Can't place here - blocked."));
		}
		return;
	}

	// A prop is set down where the ghost is and stays there, like a wall: no grid, no plot registration (it is not part of the structure).
	if (SelectedEntry.ActorClass->IsChildOf(APhysicsProp::StaticClass()))
	{
		const FVector PropLocation = PreviewActor->GetActorLocation();
		const FRotator PropRotation = PreviewActor->GetActorRotation();
		AActor* Support = PendingSupport.Get();

		const FInventorySlot ConsumedProp = HotbarComponent->ConsumeSelected();

		// The ghost has already checked the overlap, so the prop is spawned exactly there.
		FActorSpawnParameters PropSpawnParams;
		PropSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (APhysicsProp* Prop = GetWorld()->SpawnActor<APhysicsProp>(ConsumedProp.ActorClass, PropLocation, PropRotation, PropSpawnParams))
		{
			Prop->SetActorScale3D(ConsumedProp.Scale);
			Prop->Place(Support);
		}

		RefreshPreviewMesh();
		return;
	}

	const ABuildablePiece* SelectedCDO = SelectedEntry.ActorClass->GetDefaultObject<ABuildablePiece>();
	const EBuildAnchor Anchor = SelectedCDO ? SelectedCDO->GetBuildAnchor() : EBuildAnchor::Legacy;

	const FVector SpawnLocation = PreviewActor->GetActorLocation();
	const FRotator SpawnRotation = PreviewActor->GetActorRotation();
	const FIntVector GridCoordToApply = PendingGridCoord;
	const uint8 OrientationToApply = static_cast<uint8>(((PendingQuarterTurns % 4) + 4) % 4);
	// Same guard as TryResolveGridPlacement's PlaneZ - a leftover RoofStepValue from a previous
	// roof selection must never be stamped onto a non-roof piece.
	const int32 RoofStepToApply = (SelectedCDO && SelectedCDO->PieceClass == EPieceClass::RoofTile) ? RoofStepValue : 0;

	// Same flip toggle TryResolveGridPlacement reads to skip the 180-yaw ManualFlip for this piece -
	// here it decides which of the two mirrored classes is the one actually spawned.
	const bool bSpawnMirrored = SelectedCDO && SelectedCDO->MirrorVariantClass && (RotationSteps % 2) == 1;

	const FInventorySlot ConsumedEntry = HotbarComponent->ConsumeSelected();
	const TSubclassOf<AActor> ClassToSpawn = (bSpawnMirrored && SelectedCDO->MirrorVariantClass)
		? TSubclassOf<AActor>(SelectedCDO->MirrorVariantClass)
		: ConsumedEntry.ActorClass;

	if (Anchor != EBuildAnchor::Legacy)
	{
		// Grid piece: the shared placement function does the spawn, the corner parity bias, the
		// grid stamp and the graph registration - the same path the house generator takes. Build
		// mode only decided *where* (the ghost) and consumed the hotbar item.
		if (CachedPlot)
		{
			CachedPlot->PlacePiece(TSubclassOf<ABuildablePiece>(ClassToSpawn.Get()), GridCoordToApply, OrientationToApply, RoofStepToApply, ConsumedEntry.Scale);
		}
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams))
		{
			Spawned->SetActorScale3D(ConsumedEntry.Scale);

			if (ABuildablePiece* NewPiece = Cast<ABuildablePiece>(Spawned))
			{
				NewPiece->State = EBuildablePieceState::Placed;

				if (CachedPlot && CachedPlot->GetStructuralSupport())
				{
					CachedPlot->GetStructuralSupport()->RegisterPiece(NewPiece);
				}
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
		&& (SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()) || SelectedEntry.ActorClass->IsChildOf(APhysicsProp::StaticClass()));

	if (!bHasBuildableSelected)
	{
		PreviewActor->SetActorHiddenInGame(true);
		LastPreviewedClass = nullptr;
		return;
	}

	PreviewActor->SetActorHiddenInGame(false);

	const bool bProp = SelectedEntry.ActorClass->IsChildOf(APhysicsProp::StaticClass());
	const ABuildablePiece* CDO = bProp ? nullptr : SelectedEntry.ActorClass->GetDefaultObject<ABuildablePiece>();

	// A mirror-variant piece reuses the Edge "flip" toggle (RotationSteps) to pick which of its two
	// mirrored meshes to preview, instead of rotating 180 - see TryResolveGridPlacement's Edge
	// branch. Tracked alongside LastPreviewedClass so toggling it refreshes the mesh below even
	// though the hotbar selection itself hasn't changed.
	const bool bPreviewMirrored = CDO && CDO->MirrorVariantClass && (RotationSteps % 2) == 1;

	if (SelectedEntry.ActorClass == LastPreviewedClass && bPreviewMirrored == bLastPreviewedMirrored)
	{
		// Same class and mirror state as last frame - only the transform needs updating (done every tick separately).
		return;
	}

	const bool bClassChanged = SelectedEntry.ActorClass != LastPreviewedClass;
	LastPreviewedClass = SelectedEntry.ActorClass;
	bLastPreviewedMirrored = bPreviewMirrored;

	if (bClassChanged)
	{
		RotationSteps = 0;
	}

	UStaticMeshComponent* MeshComp = PreviewActor->GetStaticMeshComponent();

	if (!MeshComp)
	{
		return;
	}

	if (bProp)
	{
		const APhysicsProp* PropCDO = SelectedEntry.ActorClass->GetDefaultObject<APhysicsProp>();
		MeshComp->SetStaticMesh(PropCDO && PropCDO->GetMeshComponent() ? PropCDO->GetMeshComponent()->GetStaticMesh() : nullptr);
		PreviewPlacement = APhysicsProp::GetPlacement(TSubclassOf<APhysicsProp>(SelectedEntry.ActorClass.Get()));
	}
	else
	{
		const ABuildablePiece* MeshSourceCDO = (bPreviewMirrored && CDO->MirrorVariantClass)
			? CDO->MirrorVariantClass->GetDefaultObject<ABuildablePiece>()
			: CDO;

		if (!MeshSourceCDO)
		{
			return;
		}

		MeshComp->SetStaticMesh(MeshSourceCDO->GetMeshComponent() ? MeshSourceCDO->GetMeshComponent()->GetStaticMesh() : nullptr);
	}

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

	const ABuildablePiece* SelectedCDO = GetPreviewedPiece();
	const EBuildAnchor Anchor = SelectedCDO ? SelectedCDO->GetBuildAnchor() : EBuildAnchor::Legacy;

	FVector CandidateLocation;
	FRotator CandidateRotation;

	if (GetPreviewedProp())
	{
		AActor* Support = nullptr;

		if (!TryFindPropTransform(ViewLocation, ViewRotation, CandidateLocation, CandidateRotation, Support))
		{
			PreviewActor->SetActorHiddenInGame(true);
			PendingSupport.Reset();
			bIsBlocked = true;
			bIsRisky = false;
			LastBlockReason = PreviewPlacement.bHangsOnWall ? TEXT("aim at a plain wall to hang this on") : TEXT("nothing in reach to stand on");
			return;
		}

		PendingSupport = Support;
		PreviewActor->SetActorLocationAndRotation(CandidateLocation, CandidateRotation);

		// The support (the floor it stands on) touches the prop by design, so it is not an obstacle; anything else overlapping is.
		bIsBlocked = IsCandidateBlocked(CandidateLocation, CandidateRotation, Support);
		bIsRisky = false;
		LastBlockReason = bIsBlocked ? TEXT("blocked") : TEXT("free");
		UpdateGhostMaterial(bIsBlocked, false);
		return;
	}

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

		// Same guard as IsGridCandidateRisky/ConfirmPlacement - a leftover RoofStepValue from a
		// previous roof selection must not make an unrelated piece's occupancy check look at the
		// wrong slot.
		const int32 RoofStepForOccupancy = (SelectedCDO && SelectedCDO->PieceClass == EPieceClass::RoofTile) ? RoofStepValue : 0;
		bIsBlocked = IsGridCandidateBlocked(CandidateLocation, CandidateRotation, Anchor, GridCoord, QuarterTurns, RoofStepForOccupancy);
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

const ABuildablePiece* UBuildModeComponent::GetPreviewedPiece() const
{
	return (LastPreviewedClass && LastPreviewedClass->IsChildOf(ABuildablePiece::StaticClass())) ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
}

const APhysicsProp* UBuildModeComponent::GetPreviewedProp() const
{
	return (LastPreviewedClass && LastPreviewedClass->IsChildOf(APhysicsProp::StaticClass())) ? LastPreviewedClass->GetDefaultObject<APhysicsProp>() : nullptr;
}

bool UBuildModeComponent::TryFindPropTransform(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutLocation, FRotator& OutRotation, AActor*& OutSupport) const
{
	OutSupport = nullptr;

	const APhysicsProp* PropCDO = GetPreviewedProp();
	const UStaticMesh* Mesh = (PropCDO && PropCDO->GetMeshComponent()) ? PropCDO->GetMeshComponent()->GetStaticMesh() : nullptr;

	if (!Mesh || !PreviewActor || !HotbarComponent)
	{
		return false;
	}

	// A normal pointing this far up is a floor (or the top of a table); anything steeper is a wall, a ceiling or a side.
	constexpr float FloorNormalZ = 0.7f;

	const FVector Scale = PreviewActor->GetActorScale3D();
	const FBoxSphereBounds LocalBounds = Mesh->GetBounds();

	OutRotation = FRotator(0.f, RotationSteps * FMath::Max(1.f, PropRotationStepDegrees), 0.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(PreviewActor);

	FHitResult Hit;
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * HotbarComponent->PlaceDistance;

	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams))
	{
		return false;
	}

	// Rest = where the pivot goes (X, Y) and the surface the mesh's bottom rests on (Z).
	FVector Rest = Hit.ImpactPoint;
	AActor* Support = Hit.GetActor();
	bool bSnapX = PropSnapSize > 0.f;
	bool bSnapY = PropSnapSize > 0.f;

	// Puts the pivot (X, Y of Rest) where the box of the mesh, turned as OutRotation is, lies flush against a vertical surface hit at ImpactPoint
	// whose horizontal normal is Out, centred on the aimed point along the surface. The pivot is not the middle of the box (the Kenney
	// meshes have it at a corner), so the box's middle is placed first and the pivot follows from it.
	const auto PlaceFlushAgainst = [&](const FVector& Out)
	{
		const FVector BoxOffset = OutRotation.RotateVector(LocalBounds.Origin * Scale);
		const FVector AxisX = OutRotation.RotateVector(FVector::XAxisVector);
		const FVector AxisY = OutRotation.RotateVector(FVector::YAxisVector);

		// Half the box's width measured along the normal of the wall.
		const float Reach = FMath::Abs(FVector::DotProduct(Out, AxisX)) * LocalBounds.BoxExtent.X * Scale.X
			+ FMath::Abs(FVector::DotProduct(Out, AxisY)) * LocalBounds.BoxExtent.Y * Scale.Y;

		// Walls of the kit run along an axis: there the distance to the wall is exact (only the position along the wall is rounded), so
		// the prop touches it. A skewed wall has no exact axis to keep out of the rounding, so it gets the rounding as a margin.
		const bool bWallAlongY = FMath::Abs(Out.X) > 0.99f;
		const bool bWallAlongX = FMath::Abs(Out.Y) > 0.99f;
		const float Gap = (bWallAlongY || bWallAlongX) ? 1.f : 1.f + PropSnapSize * 0.71f;

		const FVector BoxMiddle = Hit.ImpactPoint + Out * (Reach + Gap);

		Rest.X = BoxMiddle.X - BoxOffset.X;
		Rest.Y = BoxMiddle.Y - BoxOffset.Y;

		bSnapX = bSnapX && !bWallAlongY;
		bSnapY = bSnapY && !bWallAlongX;
	};

	if (PreviewPlacement.bHangsOnWall)
	{
		// A mirror or a cabinet hangs on a plain wall of the kit (what it is mounted on), at the height that was aimed, its front turned
		// away from the wall. Anywhere else it has nothing to hang on.
		const ABuildablePiece* Wall = Cast<ABuildablePiece>(Hit.GetActor());
		const bool bPlainWall = Wall && (Wall->PieceClass == EPieceClass::ExteriorWall || Wall->PieceClass == EPieceClass::InteriorWall)
			&& FMath::Abs(Hit.ImpactNormal.Z) < 0.5f;

		if (!bPlainWall)
		{
			return false;
		}

		const FVector Out = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();

		OutRotation = FRotator(0.f, FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X)) + PreviewPlacement.FrontYawOffset), 0.f);
		PlaceFlushAgainst(Out);

		// The middle of the box is at the aimed height.
		Rest.Z = Hit.ImpactPoint.Z - LocalBounds.BoxExtent.Z * Scale.Z;

		if (PropSnapSize > 0.f)
		{
			Rest.Z = FMath::GridSnap(Rest.Z, PropSnapSize);
		}
	}
	else if (Hit.ImpactNormal.Z < FloorNormalZ)
	{
		// Not a floor: the prop stands on the floor at the foot of what was aimed, not in the air at the height that was aimed. The floor
		// is looked for right against the surface, so it does not depend on the prop's size or on how it is turned.
		const FVector Out = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
		const FVector DropStart = Hit.ImpactPoint + Out * 2.f;

		FHitResult Floor;

		if (!GetWorld()->LineTraceSingleByChannel(Floor, DropStart, DropStart - FVector(0.f, 0.f, PropDropDistance), ECC_GameTraceChannel2, QueryParams)
			|| Floor.ImpactNormal.Z < FloorNormalZ)
		{
			return false;
		}

		Rest.Z = Floor.ImpactPoint.Z;
		Support = Floor.GetActor();

		// A ceiling has no horizontal normal: the prop simply stands under the point that was aimed. A wall: flush against it.
		if (!Out.IsNearlyZero())
		{
			PlaceFlushAgainst(Out);
		}
	}

	if (bSnapX)
	{
		Rest.X = FMath::GridSnap(Rest.X, PropSnapSize);
	}

	if (bSnapY)
	{
		Rest.Y = FMath::GridSnap(Rest.Y, PropSnapSize);
	}

	// Rotation is yaw only, which never changes Z: the mesh's bottom stays a fixed offset below its pivot, and it goes right on the surface.
	const float LocalBottomZ = (LocalBounds.Origin.Z - LocalBounds.BoxExtent.Z) * Scale.Z;
	OutLocation = FVector(Rest.X, Rest.Y, Rest.Z - LocalBottomZ);
	OutSupport = Support;

	return true;
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

	const ABuildablePiece* SelectedCDO = LastPreviewedClass ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
	const float VerticalOffset = SelectedCDO ? SelectedCDO->GetVerticalOffset() : 0.f;

	// RoofStepValue only means something for a Roof_* piece (climbing a slope or stacking a gable
	// infill) - ignored for anything else, so leaving the roof step dialed in from a previous
	// selection can't float a wall or floor at an intermediate height it was never meant to occupy.
	const bool bRoofStepApplies = SelectedCDO && SelectedCDO->PieceClass == EPieceClass::RoofTile;

	// The formula (foundation pivot 20cm above ground, one FloorHeight per level, RoofStepHeight per
	// roof step) lives on the plot, shared with APlot::PlacePiece - see APlot::GetPlaneZ.
	const float PlaneZ = CachedPlot->GetPlaneZ(BuildLevel, bRoofStepApplies ? RoofStepValue : 0, VerticalOffset);

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

	case EBuildAnchor::Ceiling:
		// Centred on a room cell like a floor, but aimed at the ceiling: PlaneZ already includes the
		// piece's VerticalOffset (the room's height), so the player looks up at it. It never rotates.
		CellX = NearestEven(GridX);
		CellY = NearestEven(GridY);
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

				// A roof tile counts as floor here too (a gable stands on the side of the roof): both layers of the Cell table.
				const auto HasFloorOrRoof = [Structural](const FIntVector& Neighbor)
				{
					return Structural->GetPieceAt(EBuildAnchor::Cell, Neighbor) != nullptr || Structural->GetPieceAt(EBuildAnchor::Cell, Neighbor, 0, true) != nullptr;
				};

				const bool bMinusHasFloor = HasFloorOrRoof(MinusNeighbor);
				const bool bPlusHasFloor = HasFloorOrRoof(PlusNeighbor);

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
			// piece) - RotatePreview toggles this for Edge pieces. A mirror-variant piece (e.g. a
			// gable half) reuses that same toggle to pick which of two mirrored classes to
			// preview/spawn instead (see RefreshPreviewMesh/ConfirmPlacement) - an extra 180 here
			// as well would, combined with bFaceAnchoredEdge, just relocate it onto the other
			// flanking cell rather than mirror it.
			const bool bUsesMirrorVariant = SelectedCDO && SelectedCDO->MirrorVariantClass != nullptr;
			const float ManualFlip = bUsesMirrorVariant ? 0.f : (RotationSteps % 2) * 180.f;
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

bool UBuildModeComponent::IsGridCandidateBlocked(const FVector& Location, const FRotator& Rotation, EBuildAnchor Anchor, const FIntVector& GridCoord, int32 QuarterTurns, int32 RoofStepForOccupancy) const
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

	const ABuildablePiece* PreviewedCDO = LastPreviewedClass ? LastPreviewedClass->GetDefaultObject<ABuildablePiece>() : nullptr;
	const bool bRoofLayer = PreviewedCDO && PreviewedCDO->PieceClass == EPieceClass::RoofTile;

	if (Structural && !Structural->IsSlotFree(Anchor, GridCoord, RoofStepForOccupancy, bRoofLayer))
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
	// Same guard as TryResolveGridPlacement/ConfirmPlacement - a leftover RoofStepValue from a
	// previous roof selection must not make an unrelated piece look for neighbours at RoofStep != 0,
	// where it'll find none and read as falsely risky.
	const int32 RoofStepToCheck = SelectedCDO->PieceClass == EPieceClass::RoofTile ? RoofStepValue : 0;
	return Structural->ComputeStability(SelectedCDO->GetSockets(), GridCoord, QuarterTurns, RoofStepToCheck) < ComfortThreshold;
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
