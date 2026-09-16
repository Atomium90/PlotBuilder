#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DetectionZone.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  Overlap-based trigger: swaps its own material between two states depending on whether any
 *  actor implementing IInteractable is currently inside. Reuses IInteractable as-is, just detects
 *  it via overlap instead of UInteractionComponent's trace.
 */
UCLASS()
class ADetectionZone : public AActor
{
	GENERATED_BODY()

public:

	ADetectionZone();

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ZoneMesh;

	/** Material applied when no IInteractable actor is inside the zone. */
	UPROPERTY(EditAnywhere, Category = "Detection Zone")
	UMaterialInterface* EmptyMaterial;

	/** Material applied while at least one IInteractable actor is inside the zone. */
	UPROPERTY(EditAnywhere, Category = "Detection Zone")
	UMaterialInterface* OccupiedMaterial;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:

	UPROPERTY()
	TSet<TObjectPtr<AActor>> OverlappingInteractables;

	void UpdateVisual();
};
