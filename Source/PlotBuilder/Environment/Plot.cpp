#include "Plot.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

APlot::APlot()
{
	PrimaryActorTick.bCanEverTick = true;

	BoundsVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsVolume"));
	SetRootComponent(BoundsVolume);

	BoundsVolume->SetBoxExtent(FVector(500.f, 500.f, 50.f));
	BoundsVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool APlot::IsLocationInside(const FVector& WorldLocation) const
{
	return BoundsVolume->Bounds.GetBox().IsInside(WorldLocation);
}

void APlot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugBounds)
	{
		DrawDebugBox(GetWorld(), BoundsVolume->GetComponentLocation(), BoundsVolume->GetScaledBoxExtent(), BoundsVolume->GetComponentQuat(), DebugColor, false, -1.f, 0, 3.f);
	}
}
