#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemolitionTypes.h"
#include "DemolitionTool.generated.h"

class UDemolitionToolData;
class UDemolitionRules;
class UStaticMeshComponent;

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
	 *  Continuous tools (ToolData->bContinuous) start ticking damage at ToolData->Rate until
	 *  ReleasePrimaryUse() is called - see UToolbeltComponent::PrimaryUse/ReleasePrimaryUse.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tool")
	void PrimaryUse();

	/**
	 *  Stops a continuous tool started by PrimaryUse() (LMB released). No-op for discrete tools,
	 *  so it is safe to call on every slot regardless of which tool is equipped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tool")
	void ReleasePrimaryUse();

	/** Shows/hides the held mesh - UToolbeltComponent calls this on equip/unequip so only the active tool is visible. */
	UFUNCTION(BlueprintCallable, Category = "Tool")
	void SetToolVisible(bool bVisible);

	/** Gates the "HP left" on-screen print every tool hit already makes - on by default (today's always-on behaviour), toggled by the pb.ShowHP console command. Shared by every tool instance, not per-instance state. */
	static bool bShowHPDebug;

protected:

	/** Mesh held in the player's hand - BP assigns the Static Mesh (e.g. SM_Sledgehammer from _Inbox/Tools). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tool|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Socket on the player's first-person mesh this tool attaches to - must match a socket actually authored on that skeleton. */
	UPROPERTY(EditDefaultsOnly, Category = "Tool|Visual")
	FName HandSocketName = FName("GripPoint");

	/** Fine-tuning offset applied after snapping to HandSocketName, since each mesh's pivot/scale differs - adjust by eye in the BP viewport. */
	UPROPERTY(EditDefaultsOnly, Category = "Tool|Visual")
	FTransform HandOffset = FTransform::Identity;

	/** Attaches MeshComponent to the owner's first-person mesh at HandSocketName, then applies HandOffset. Hidden by default - UToolbeltComponent shows it once equipped. */
	virtual void BeginPlay() override;

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
