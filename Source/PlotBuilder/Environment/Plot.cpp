#include "Plot.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
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
