#include "Environment/FlatZone.h"
#include "Components/BoxComponent.h"

AFlatZone::AFlatZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);

	Box->SetBoxExtent(FVector(1000.f, 1000.f, 50.f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetLineThickness(6.f);
}

UBoxComponent* AFlatZone::GetBox() const
{
	return Box;
}

void AFlatZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Seen in the editor only: yellow for a plain zone, blue for a road.
	Box->ShapeColor = bRoad ? FColor(70, 130, 220) : FColor(240, 190, 40);
	Box->MarkRenderStateDirty();
}
