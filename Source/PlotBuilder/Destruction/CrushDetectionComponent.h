#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrushDetectionComponent.generated.h"

class UDemolitionRules;
class UPrimitiveComponent;
class UCameraShakeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrushEvent, float, Energy);

/** One hit's worth of crush energy, timestamped so TickComponent can age it out of the rolling window - see UCrushDetectionComponent::HandleHit/TickComponent. */
USTRUCT()
struct FCrushHitRecord
{
	GENERATED_BODY()

	float Time = 0.f;
	float Energy = 0.f;
};

/**
 *  Accumulated crush energy on this actor from Falling pieces (GDD §7.9): "one wall falling on me
 *  is fine, three walls and the roof, no." Owner-agnostic, same split as UDurabilityComponent -
 *  this component only needs the owner's root to be a UPrimitiveComponent (true for ACharacter's
 *  capsule), it never assumes APlayerCharacter concretely. Listens for hits from any
 *  ABuildablePiece currently Collapsing, sums mass x relative speed into a rolling window
 *  (Rules->CrushWindow) instead of a single threshold hit, and on crossing Rules->CrushThreshold
 *  fires a short stun (Rules->CrushStunDuration) plus a camera shake and the shared safety-penalty
 *  log hook also used by AExplosiveCharge's on-player case.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UCrushDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCrushDetectionComponent();

	UPROPERTY(EditDefaultsOnly, Category = "Destruction")
	TObjectPtr<UDemolitionRules> Rules;

	UPROPERTY(BlueprintAssignable, Category = "Destruction")
	FOnCrushEvent OnCrushEvent;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Movement/camera/tool input should early-out while this is true - see APlayerCharacter's input handlers. */
	bool IsStunned() const { return bIsStunned; }

	/** Current summed crush energy within the rolling window. Read by the on-screen gauge (SetShowGauge). */
	float GetCurrentEnergy() const { return CurrentEnergy; }

	/** Shows or hides the gauge: a text bar of the current energy against Rules->CrushThreshold, for tuning and the demo (pb.ShowCrush). */
	void SetShowGauge(bool bShow) { bShowGauge = bShow; }

	/**
	 *  Shared "this counts as a safety penalty" log hook (GDD §6.5 - crush and an explosion on the
	 *  player are the same penalty family). Today this is only a log stub, same style as
	 *  AExplosiveCharge's pre-existing one - the single place a future UContractSubsystem
	 *  (Sprint 3) hooks into instead of two independent stubs. Does not by itself stun or shake the
	 *  camera - only the crush event (see TickComponent) does that.
	 */
	void RegisterSafetyPenalty(FName Source) const;

protected:

	virtual void BeginPlay() override;

private:

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void TriggerCrushEvent();
	void EndStun();

	/** Prints the gauge for this frame when it is on. */
	void DrawGauge() const;

	/** On-screen key of the gauge (7700 and up are reserved for the combat readouts). */
	static constexpr int32 GaugeMessageKey = 7700;

	TArray<FCrushHitRecord> RecentHits;
	float CurrentEnergy = 0.f;
	bool bIsStunned = false;
	bool bShowGauge = false;
	FTimerHandle StunTimerHandle;
};
