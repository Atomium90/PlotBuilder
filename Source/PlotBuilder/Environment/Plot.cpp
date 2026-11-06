#include "Plot.h"
#include "PlotBuilder.h"
#include "Buildables/BuildablePiece.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/DropZone.h"
#include "Environment/MissionGround.h"
#include "Environment/ProtectedZone.h"
#include "StructuralRules.h"
#include "StructuralSupportComponent.h"

namespace
{
	/** Half-thickness of the root box, in cm: it is the footprint, a flat plate on the ground. */
	constexpr float FootprintHalfThickness = 1.f;
}

APlot::APlot()
{
	PrimaryActorTick.bCanEverTick = true;

	BoundsVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsVolume"));
	SetRootComponent(BoundsVolume);

	BoundsVolume->SetBoxExtent(FVector(500.f, 500.f, FootprintHalfThickness));
	BoundsVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Only there to show the whole volume in the editor and in the debug view: it never collides.
	VolumeGuide = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeGuide"));
	VolumeGuide->SetupAttachment(BoundsVolume);
	VolumeGuide->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VolumeGuide->SetGenerateOverlapEvents(false);
	VolumeGuide->SetHiddenInGame(true);

	StructuralSupport = CreateDefaultSubobject<UStructuralSupportComponent>(TEXT("StructuralSupport"));

	// The tape is only there to be seen: the player walks through it (the mission rule is "demolish inside", not a wall).
	TapePoles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TapePoles"));
	TapePoles->SetupAttachment(BoundsVolume);
	TapePoles->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TapeBars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TapeBars"));
	TapeBars->SetupAttachment(BoundsVolume);
	TapeBars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APlot::RegisterProtectedZone(AProtectedZone* Zone)
{
	if (Zone)
	{
		ProtectedZones.AddUnique(Zone);
	}
}

void APlot::RegisterClientDropZone(ADropZone* Zone)
{
	if (Zone && ClientDropZone && ClientDropZone != Zone)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("APlot '%s': a second client drop zone '%s' replaces '%s' (one client zone per mission map)."), *GetName(), *Zone->GetName(), *ClientDropZone->GetName());
	}

	ClientDropZone = Zone;
}

void APlot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The root box is the footprint, flat on the ground (the pivot is the ground, GetGroundZ). The volume the plot covers rises
	// from it by VolumeHeight: the guide box shows it. Only the box's X and Y extents are read; its Z is reset here.
	const FVector Footprint = BoundsVolume->GetUnscaledBoxExtent();
	BoundsVolume->SetBoxExtent(FVector(Footprint.X, Footprint.Y, FootprintHalfThickness), false);
	VolumeGuide->SetBoxExtent(FVector(Footprint.X, Footprint.Y, VolumeHeight * 0.5f), false);
	VolumeGuide->SetRelativeLocation(FVector(0.f, 0.f, VolumeHeight * 0.5f));

	RebuildTape();
}

void APlot::RebuildTape()
{
	if (!TapePoles || !TapeBars)
	{
		return;
	}

	TapePoles->ClearInstances();
	TapeBars->ClearInstances();
	TapePoles->SetStaticMesh(PoleMesh);
	TapeBars->SetStaticMesh(TapeMesh);

	if (!bShowTape || (!PoleMesh && !TapeMesh))
	{
		return;
	}

	// Corners in the plot's own space, walked once around; the plot's transform (scale included) puts them in the world.
	const FVector Extent = BoundsVolume->GetUnscaledBoxExtent();
	const FVector2D Corners[4] = {
		FVector2D(-Extent.X, -Extent.Y), FVector2D(Extent.X, -Extent.Y),
		FVector2D(Extent.X, Extent.Y), FVector2D(-Extent.X, Extent.Y)
	};

	const FTransform PlotTransform = GetActorTransform();

	// The pole points, in order around the perimeter. A side is cut into equal gaps no longer than PoleSpacing; its first
	// point is its start corner, the end corner is the start of the next side.
	TArray<FVector> PolePoints;

	for (int32 Side = 0; Side < 4; ++Side)
	{
		const FVector Start = PlotTransform.TransformPosition(FVector(Corners[Side], 0.f));
		const FVector End = PlotTransform.TransformPosition(FVector(Corners[(Side + 1) % 4], 0.f));
		const int32 Gaps = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Start, End) / FMath::Max(PoleSpacing, 1.f)));

		for (int32 Gap = 0; Gap < Gaps; ++Gap)
		{
			PolePoints.Add(FMath::Lerp(Start, End, static_cast<float>(Gap) / Gaps));
		}
	}

	// The tape mesh is centred and lies along its X. Its own size is the unit: a gap is filled with as many modules as fit
	// (each a little stretched or squeezed to close the gap exactly), so stripes in the mesh keep their size at any spacing.
	// The engine cube (100 cm each way) is the special case where one module is 100 cm and TapeSize alone sets the band.
	const FVector MeshSize = TapeMesh ? (TapeMesh->GetBounds().BoxExtent * 2.0).ComponentMax(FVector(1.0)) : FVector(100.0);

	for (int32 Index = 0; Index < PolePoints.Num(); ++Index)
	{
		if (PoleMesh)
		{
			TapePoles->AddInstance(FTransform(PolePoints[Index]), true);
		}

		if (TapeMesh)
		{
			const FVector From = PolePoints[Index];
			const FVector To = PolePoints[(Index + 1) % PolePoints.Num()];
			const FVector Along = To - From;
			const float Length = Along.Size();
			const int32 Modules = FMath::Max(1, FMath::RoundToInt(Length / MeshSize.X));
			const FRotator Rotation(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f);
			const FVector Scale((Length / Modules) / MeshSize.X, TapeSize.Y / MeshSize.Y, TapeSize.X / MeshSize.Z);

			for (int32 Module = 0; Module < Modules; ++Module)
			{
				const FVector Middle = From + Along * ((Module + 0.5f) / Modules);
				TapeBars->AddInstance(FTransform(Rotation, Middle + FVector(0.f, 0.f, TapeHeight), Scale), true);
			}
		}
	}
}

bool APlot::IsLocationInside(const FVector& WorldLocation) const
{
	constexpr float Tolerance = 1.f;

	// The footprint is the root box (flat); the volume rises from the ground by VolumeHeight, which scales with the plot.
	const FBox Footprint = BoundsVolume->Bounds.GetBox();
	const double Ground = GetGroundZ();
	const FBox Volume(FVector(Footprint.Min.X, Footprint.Min.Y, Ground), FVector(Footprint.Max.X, Footprint.Max.Y, Ground + VolumeHeight * GetActorScale3D().Z));
	return Volume.ExpandBy(Tolerance).IsInside(WorldLocation);
}

float APlot::GetPlaneZ(int32 BuildLevel, int32 RoofStep, float VerticalOffset) const
{
	const UStructuralRules* Rules = StructuralSupport ? StructuralSupport->Rules : nullptr;
	const float FloorHeight = Rules ? Rules->FloorHeight : 300.f;
	const float RoofStepHeight = Rules ? Rules->RoofStepHeight : 100.f;

	return GetGroundZ() + 20.f + BuildLevel * FloorHeight + RoofStep * RoofStepHeight + VerticalOffset;
}

ABuildablePiece* APlot::PlacePiece(TSubclassOf<ABuildablePiece> PieceClass, const FIntVector& GridCoord, uint8 QuarterTurns, int32 RoofStep, const FVector& Scale)
{
	const ABuildablePiece* ClassDefaults = PieceClass ? PieceClass->GetDefaultObject<ABuildablePiece>() : nullptr;
	UWorld* World = GetWorld();

	if (!ClassDefaults || !World || !StructuralSupport)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("APlot::PlacePiece: no piece class, world or structural support on '%s'"), *GetName());
		return nullptr;
	}

	const EBuildAnchor Anchor = ClassDefaults->GetBuildAnchor();

	if (Anchor == EBuildAnchor::Legacy)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("APlot::PlacePiece: %s has a Legacy build anchor, it isn't a grid piece"), *PieceClass->GetName());
		return nullptr;
	}

	// Same guard build mode applies: a RoofStep only means something for a roof tile, so a stale value
	// can never float a wall or a floor at an intermediate height.
	const int32 AppliedRoofStep = ClassDefaults->PieceClass == EPieceClass::RoofTile ? RoofStep : 0;
	const uint8 Orientation = QuarterTurns % 4;

	const UStructuralRules* Rules = StructuralSupport->Rules;
	const float CellSize = Rules ? Rules->CellSize : 100.f;
	const FVector Origin = GetGridOrigin();
	const FVector Location(
		Origin.X + GridCoord.X * CellSize,
		Origin.Y + GridCoord.Y * CellSize,
		GetPlaneZ(GridCoord.Z, AppliedRoofStep, ClassDefaults->GetVerticalOffset()));

	if (!IsLocationInside(Location))
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("APlot::PlacePiece: %s at cell (%d, %d, %d) is outside the plot"), *PieceClass->GetName(), GridCoord.X, GridCoord.Y, GridCoord.Z);
		return nullptr;
	}

	// A roof tile has its own layer in the Cell table, so it can rest on walls that also carry a ceiling slab.
	if (!StructuralSupport->IsSlotFree(Anchor, GridCoord, AppliedRoofStep, ClassDefaults->PieceClass == EPieceClass::RoofTile))
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("APlot::PlacePiece: %s at cell (%d, %d, %d) is already occupied"), *PieceClass->GetName(), GridCoord.X, GridCoord.Y, GridCoord.Z);
		return nullptr;
	}

	// Grid pieces never get nudged out of overlaps: they are supposed to touch their neighbours.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABuildablePiece* Piece = World->SpawnActor<ABuildablePiece>(PieceClass, Location, FRotator(0.f, Orientation * 90.f, 0.f), SpawnParams);

	if (!Piece)
	{
		return nullptr;
	}

	Piece->SetActorScale3D(Scale);
	Piece->State = EBuildablePieceState::Placed;

	// Corner parity bias (D21, visual only): nudges this wall-class piece's length (local Y - every
	// wall mesh's 200cm run, local X is its 20cm/10cm thickness) scale by a tiny,
	// orientation-parity-dependent fraction so two walls meeting at a corner don't share an
	// exactly coplanar end face/top. Never touches grid/socket math, which only ever reads
	// GridCoord/Orientation, not actor scale. CornerPost is deliberately excluded: its
	// cross-section is a symmetric 20x20cm square with no length axis to nudge.
	const bool bIsWallClass = Piece->PieceClass == EPieceClass::ExteriorWall || Piece->PieceClass == EPieceClass::InteriorWall || Piece->PieceClass == EPieceClass::Parapet;

	// Top face bias (D21, visual only): a full-height wall (300cm) ends exactly at the top plane of the slab
	// above it, so over the strip where they overlap the two top faces are coplanar and flicker. Taking a
	// tiny fraction off the wall's height (local Z, pivot at the foot) keeps the top just under the slab's.
	// Walls and posts only: a parapet is 1m, it never reaches a slab.
	const bool bHasFullHeight = Piece->PieceClass == EPieceClass::ExteriorWall || Piece->PieceClass == EPieceClass::InteriorWall || Piece->PieceClass == EPieceClass::CornerPost;

	if ((bIsWallClass || bHasFullHeight) && Rules)
	{
		FVector BiasedScale = Piece->GetActorScale3D();

		if (bIsWallClass)
		{
			// A partition always shortens, whatever its orientation: its ends sit on the outer plane of the exterior
			// wall it meets, so a lengthened one (even orientation) would stick out 0.01cm through that wall's face.
			// Where two partitions run end to end the hairline gap this leaves is far under a pixel.
			const bool bLengthen = Orientation % 2 == 0 && Piece->PieceClass != EPieceClass::InteriorWall;
			BiasedScale.Y *= (1.f + (bLengthen ? 1.f : -1.f) * Rules->CornerParityBias);
		}

		if (bHasFullHeight)
		{
			BiasedScale.Z *= (1.f - Rules->TopFaceBias);
		}

		Piece->SetActorScale3D(BiasedScale);
	}

	Piece->SetGridPlacement(GridCoord, Orientation, AppliedRoofStep);
	StructuralSupport->RegisterPiece(Piece);

	return Piece;
}

void APlot::BeginPlay()
{
	Super::BeginPlay();

	// The plot's structural Rules reference has been lost twice without any symptom (every read falls back to
	// the code's own defaults, so the game runs without the corner parity bias or the tuned losses). Say so loudly.
	if (StructuralSupport && !StructuralSupport->Rules)
	{
		const FString Message = FString::Printf(TEXT("APlot '%s': the structural support has no Rules (DA_StructuralRules), the graph runs on code defaults. Reassign it on the plot's StructuralSupport component."), *GetName());
		UE_LOG(LogPlotBuilder, Warning, TEXT("%s"), *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(7200, 20.f, FColor::Red, Message);
		}
	}

	// Sanity check for the GetGroundZ()/GetGridOrigin() authoring convention (plot pivot = ground
	// surface, at the center of a grid cell) - a warning here is the first thing to check if grid
	// placement ends up floating or buried.
	// A mission level builds its ground after the plot starts (the director does it, flush with the pivot by construction):
	// there is nothing to hit yet, so the check would only warn for nothing.
	if (!AMissionGround::Find(this))
	{
		FHitResult Hit;
		const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 50.f);
		const FVector End = GetActorLocation() - FVector(0.f, 0.f, 500.f);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, QueryParams))
		{
			if (FMath::Abs(Hit.Location.Z - GetActorLocation().Z) > 1.f)
			{
				UE_LOG(LogTemp, Warning, TEXT("APlot '%s': ground hit at Z=%.1f is more than 1cm from the plot's own pivot Z=%.1f - GetGroundZ() assumes they match."), *GetName(), Hit.Location.Z, GetActorLocation().Z);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("APlot '%s': no WorldStatic ground found below the plot's pivot - GetGroundZ() assumes the pivot sits at ground level."), *GetName());
		}
	}

	// Authoring convention: the plot's footprint should be a whole number of room cells (200cm) on
	// each horizontal axis, so grid pieces tile it exactly with no leftover sliver at the edge.
	constexpr float RoomCellSize = 200.f;
	const auto IsWholeMultiple = [](float Value, float Unit)
	{
		return FMath::IsNearlyEqual(Value, Unit * FMath::RoundToFloat(Value / Unit), 1.f);
	};

	const FVector FootprintSize = BoundsVolume->GetUnscaledBoxExtent() * 2.f;

	if (!IsWholeMultiple(FootprintSize.X, RoomCellSize) || !IsWholeMultiple(FootprintSize.Y, RoomCellSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("APlot '%s': BoundsVolume footprint (%.0f x %.0f) isn't a whole multiple of the %.0fcm room cell - grid pieces may not line up with its edges."), *GetName(), FootprintSize.X, FootprintSize.Y, RoomCellSize);
	}
}

void APlot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugBounds)
	{
		DrawDebugBox(GetWorld(), VolumeGuide->GetComponentLocation(), VolumeGuide->GetScaledBoxExtent(), VolumeGuide->GetComponentQuat(), DebugColor, false, -1.f, 0, 3.f);
	}
}
