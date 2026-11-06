#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlatZone.generated.h"

class UBoxComponent;

/**
 *  A box that the mission ground keeps flat (AMissionGround reads every one in the level). Move it, rotate it
 *  around Z and stretch it in the editor like any actor: that is how the yard between the road and the plot, or the
 *  road itself, is laid out. Only the footprint counts, the height of the box does not.
 *
 *  Two options say what else the ground does with it. A road also gets the road look (the ground material blends in
 *  the asphalt there). A zone kept clear gets no tree, rock or crate scattered on it.
 *
 *  The plot, the drop zones, the protected zones and the player starts are flattened by the ground on their own:
 *  this actor is for everything else (a road, a yard, a parking spot).
 */
UCLASS()
class AFlatZone : public AActor
{
	GENERATED_BODY()

public:

	AFlatZone();

	/** The zone is a road: the ground material shows the asphalt on it, laid out along the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flat Zone")
	bool bRoad = false;

	/** Nothing is scattered on the zone (trees, rocks, site clutter). Off for a zone where clutter may lie. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flat Zone")
	bool bKeepClear = true;

	/** The footprint, for the ground. */
	UBoxComponent* GetBox() const;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Box;
};
