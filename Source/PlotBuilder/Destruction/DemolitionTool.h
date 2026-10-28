#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemolitionTypes.h"
#include "DemolitionTool.generated.h"

class UDemolitionToolData;
class UDemolitionRules;

/**
 *  Shared base for the melee demolition tools (GDD §18): one C++ class, BP_Sledgehammer /
 *  BP_Crowbar / BP_Jackhammer differ only by their UDemolitionToolData asset - never a new
 *  C++ subclass for a stat variant. Persistent (spawned once by UToolbeltComponent, unlike
 *  the consumable AExplosiveCharge): PrimaryUse traces from the owner's view, resolves
 *  IDamageable targets (the single traced actor, or an AoE at the impact point if
 *  ToolData->HitRadius > 0 - mirrors AExplosiveCharge::Detonate's overlap loop), and gates
 *  re-use behind one busy window covering wind-up + the rest of the 1/Rate cycle.
 */
UCLASS(abstract)
class ADemolitionTool : public AActor
{
	GENERATED_BODY()

public:

	ADemolitionTool();

	/** Stats asset, set once per Blueprint class default (DA_Sledgehammer, DA_Crowbar...). */
	UPROPERTY(EditDefaultsOnly, Category = "Tool")
	TObjectPtr<UDemolitionToolData> ToolData;

	/** DA_DemolitionRules asset - same one assigned on UDurabilityComponent/AExplosiveCharge. */
	UPROPERTY(EditDefaultsOnly, Category = "Tool")
	TObjectPtr<UDemolitionRules> Rules;

	/**
	 *  Primary action (LMB). No-op while busy (wind-up + cooldown) or if ToolData is unset.
	 *  Continuous tools (ToolData->bContinuous) are not handled yet - Thursday's jackhammer task.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tool")
	void PrimaryUse();

private:

	/** Re-traces at wind-up's end (so late reaiming during the wind-up still counts) and applies damage to whatever is found. */
	void ResolveHit();

	void ClearBusy();

	/** Camera shake (per-tool, from ToolData) + impact sound (per-material, from Rules) on a landed hit. */
	void PlayHitFeedback(const FVector& Location, EDemolitionMaterial Material) const;

	FTimerHandle HitTimerHandle;
	FTimerHandle BusyTimerHandle;
	bool bBusy = false;
};
