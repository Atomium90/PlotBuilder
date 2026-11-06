#pragma once

#include "CoreMinimal.h"
#include "Environment/DetectionZone.h"
#include "DropZone.generated.h"

class APhysicsProp;

/** One prop inside a drop zone, as the report reads it: its worth at full health and how much of its health is left. */
struct FDropItem
{
	TWeakObjectPtr<APhysicsProp> Prop;
	int32 Value = 0;
	float HPRatio = 1.f;
};

/**
 *  The client drop zone (GDD §10.2): furniture carried in here counts as handed to the client. It is a
 *  detection zone that only counts props, and it keeps the live list of what is inside: nothing is
 *  locked, a prop thrown back out is not delivered any more, a prop that breaks inside leaves with its
 *  actor. The mission report reads the list once, when the job ends. Registers itself on the level's
 *  plot, which is where the evaluator finds it.
 */
UCLASS()
class ADropZone : public ADetectionZone
{
	GENERATED_BODY()

public:

	/** The props inside right now, with their value and health, in no particular order. */
	TArray<FDropItem> GetItems() const;

	/** Sum of the values of the props inside, at full health (no damage curve yet). */
	int32 GetTotalValue() const;

protected:

	virtual void BeginPlay() override;

	/** Only furniture counts: a collapsing piece or a fragment drifting in is not a delivery. */
	virtual bool Accepts(AActor* Actor) const override;
};
