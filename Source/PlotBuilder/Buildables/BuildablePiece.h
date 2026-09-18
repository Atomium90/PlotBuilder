#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "BuildablePiece.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EBuildablePieceState : uint8
{
	Stock,
	Held,
	Placed
};

/**
 *  Construction piece (wall, floor, roof, door, window...). Never simulates physics, unlike
 *  APhysicsProp: it's acquired via Interact into a UHotbarComponent, then placed kinematically
 *  (fixed transform, no Push/Pull/Launch). Cost is authored per-Blueprint for now, independent of
 *  the shop catalog's own FShopItemRow::Cost - the two aren't wired together yet (see
 *  UHotbarComponent::ShopCatalog), since there's no economy system to enforce either value.
 */
UCLASS(abstract)
class ABuildablePiece : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	ABuildablePiece();

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildable")
	int32 Cost = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable")
	EBuildablePieceState State = EBuildablePieceState::Stock;

	virtual void Interact_Implementation(AActor* InteractingActor) override;

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MeshComponent;
};
