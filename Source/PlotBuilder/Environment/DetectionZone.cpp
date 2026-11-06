#include "DetectionZone.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/Interactable.h"

ADetectionZone::ADetectionZone()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	SetRootComponent(ZoneMesh);

	ZoneMesh->SetCollisionProfileName(FName("OverlapAllDynamic"));
	ZoneMesh->SetGenerateOverlapEvents(true);
}

void ADetectionZone::BeginPlay()
{
	Super::BeginPlay();

	ZoneMesh->OnComponentBeginOverlap.AddDynamic(this, &ADetectionZone::OnZoneBeginOverlap);
	ZoneMesh->OnComponentEndOverlap.AddDynamic(this, &ADetectionZone::OnZoneEndOverlap);

	UpdateVisual();
}

bool ADetectionZone::Accepts(AActor* Actor) const
{
	return Actor && Actor->Implements<UInteractable>();
}

void ADetectionZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Accepts(OtherActor))
	{
		OverlappingInteractables.Add(OtherActor);
		UpdateVisual();
	}
}

void ADetectionZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OverlappingInteractables.Remove(OtherActor) > 0)
	{
		UpdateVisual();
	}
}

void ADetectionZone::UpdateVisual()
{
	ZoneMesh->SetMaterial(0, OverlappingInteractables.Num() > 0 ? OccupiedMaterial : EmptyMaterial);
}
