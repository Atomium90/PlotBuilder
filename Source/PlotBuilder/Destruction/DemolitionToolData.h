#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DemolitionTypes.h"
#include "DemolitionToolData.generated.h"

class UCameraShakeBase;
class USoundBase;

/**
 *  Per-tool stats for ADemolitionTool - see GDD §7.4/§18 Appendix A. One asset per tool
 *  (DA_Sledgehammer, DA_Crowbar, DA_Jackhammer...), assigned on the matching BP_* subclass;
 *  the tool class itself never changes, only this data does.
 */
UCLASS(BlueprintType)
class UDemolitionToolData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Which row of DA_DemolitionRules' tool x material matrix this tool reads. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	EDemolitionTool Tool = EDemolitionTool::Sledgehammer;

	/** Base damage per hit/tick, before the tool x material multiplier - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	float BaseDamage = 80.f;

	/** Hits/ticks per second at max cadence - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	float Rate = 1.f;

	/** Max trace distance in cm - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	float Range = 200.f;

	/** AoE radius in cm around the trace impact point; 0 = single target only - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	float HitRadius = 0.f;

	/** Delay between the press and the hit actually landing, counted against the same busy window as Rate (e.g. the sledgehammer's wind-up) - see GDD §7.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	float WindUpDuration = 0.f;

	/** Hold-to-fire instead of one hit per press - unused until the jackhammer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	bool bContinuous = false;

	/** Camera shake played on a landed hit - per-tool, since "strong but imprecise" vs "fast and clean" is a tool trait (GDD §7.4), not a material one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool|Feedback")
	TSubclassOf<UCameraShakeBase> HitCameraShakeClass;

	/**
	 *  Looping sound started on press and stopped (with a short fade) on release - only meaningful
	 *  for a continuous tool (bContinuous). Unset for the sledgehammer/crowbar; the jackhammer's
	 *  drill-loop placeholder is the first user. ADemolitionTool owns the start/
	 *  stop lifecycle generically off bContinuous, same as the rest of this data asset - no
	 *  jackhammer-only C++ needed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool|Feedback")
	TObjectPtr<USoundBase> ContinuousLoopSound;

	/** Volume multiplier applied to PlayHitFeedback's per-tick impact sound while ContinuousLoopSound is playing, so the two don't stack at Rate ticks/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool|Feedback")
	float ContinuousImpactDuckVolume = 0.3f;

	/** Fade-out duration (seconds) when ContinuousLoopSound stops on release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool|Feedback")
	float ContinuousLoopFadeOutDuration = 0.2f;
};
