#include "Plot.h"
#include "PlotBuilder.h"
#include "Buildables/BuildablePiece.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "StructuralRules.h"
#include "StructuralSupportComponent.h"

APlot::APlot()
{
	PrimaryActorTick.bCanEverTick = true;

	BoundsVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsVolume"));
	SetRootComponent(BoundsVolume);

	BoundsVolume->SetBoxExtent(FVector(500.f, 500.f, 50.f));
	BoundsVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StructuralSupport = CreateDefaultSubobject<UStructuralSupportComponent>(TEXT("StructuralSupport"));
}

bool APlot::IsLocationInside(const FVector& WorldLocation) const
{
	constexpr float Tolerance = 1.f;
	return BoundsVolume->Bounds.GetBox().ExpandBy(Tolerance).IsInside(WorldLocation);
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

	if (!StructuralSupport->IsSlotFree(Anchor, GridCoord, AppliedRoofStep))
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
	const bool bIsWallClass = Piece->PieceClass == EPieceClass::ExteriorWall || Piece->PieceClass == EPieceClass::InteriorWall;

	if (bIsWallClass && Rules)
	{
		const float Sign = (Orientation % 2 == 0) ? 1.f : -1.f;
		FVector BiasedScale = Piece->GetActorScale3D();
		BiasedScale.Y *= (1.f + Sign * Rules->CornerParityBias);
		Piece->SetActorScale3D(BiasedScale);
	}

	Piece->SetGridPlacement(GridCoord, Orientation, AppliedRoofStep);
	StructuralSupport->RegisterPiece(Piece);

	return Piece;
}

void APlot::BeginPlay()
{
	Super::BeginPlay();

	// Sanity check for the GetGroundZ()/GetGridOrigin() authoring convention (plot pivot = ground
	// surface, at the center of a grid cell) - a warning here is the first thing to check if grid
	// placement ends up floating or buried.
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
		DrawDebugBox(GetWorld(), BoundsVolume->GetComponentLocation(), BoundsVolume->GetScaledBoxExtent(), BoundsVolume->GetComponentQuat(), DebugColor, false, -1.f, 0, 3.f);
	}
}
