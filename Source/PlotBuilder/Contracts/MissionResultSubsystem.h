#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Contracts/ContractTypes.h"
#include "Contracts/MissionNotebook.h"
#include "MissionResultSubsystem.generated.h"

class UDemolitionRules;

/**
 *  What happens during a mission that nobody else keeps: the safety events on the player (a collapse that caught them, an
 *  explosion in their face). It lives on the world, so it dies with the mission level; the active contract and the
 *  report are on UContractSubsystem, which outlives it.
 *
 *  At the end of the mission it reads the rest from the level and turns everything into the plain numbers the evaluator
 *  wants (FMissionSnapshot): what is destroyed comes from the target and must-stay sets (an item whose actor is gone, or
 *  a piece that is already falling, is destroyed), what is delivered from the client drop zone, what the zones took from
 *  the zones. Nothing is recorded as it happens except the safety events, so there is nothing to keep in step.
 */
UCLASS()
class UMissionResultSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** The sources RegisterSafetyPenalty reports. */
	static const FName CrushSource;
	static const FName ExplosionSource;

	/** The subsystem of the world WorldContext is in, or nullptr. */
	static UMissionResultSubsystem* Get(const UObject* WorldContext);

	/** A new mission starts (or the house is rebuilt): the counters go back to zero, and so does the forbidden-tool flag (the tool itself stays: SetForbiddenTool says it). */
	void ResetCounters();

	/**
	 *  The tool the mission forbids (a FDamageInfo::ToolId: "Sledgehammer", "Crowbar", "Jackhammer", "Charge"), or none. Set by
	 *  the mission director from the contract's ForbiddenTool condition when a mission starts; pb.ForbidTool sets it by hand.
	 *  Setting it clears "used".
	 */
	void SetForbiddenTool(FName ToolId);

	FName GetForbiddenTool() const { return ForbiddenTool; }

	/**
	 *  A tool dealt damage (called by UDurabilityComponent, where every hit passes, whatever dealt it). If ToolId is the
	 *  forbidden tool, this is its first use of the mission: the player is warned on screen and the report will charge it. Later
	 *  uses do nothing more: the penalty is once for the whole mission.
	 */
	void RecordToolUse(FName ToolId);

	bool WasForbiddenToolUsed() const { return bForbiddenToolUsed; }

	/**
	 *  The rules asset the pieces of the house carry (DA_DemolitionRules), which holds the scoring. The mission director
	 *  gives it when it binds the contract, while a piece still stands: at the end of a mission the house may be gone, and
	 *  nothing else is certain to hold the asset.
	 */
	void SetRules(const UDemolitionRules* NewRules);

	/** A safety event happened to the player. Source is CrushSource or ExplosionSource; anything else is logged and ignored. */
	void RecordSafetyEvent(FName Source);

	int32 GetCrushEvents() const { return CrushEvents; }
	int32 GetExplosionEvents() const { return ExplosionEvents; }

	/** Reads the level as it stands into the plain numbers of the report. Empty (all zero) outside a mission. */
	FMissionSnapshot BuildSnapshot() const;

	/**
	 *  The report for Contract against the level as it stands: builds the snapshot, reads the scoring from the player's
	 *  rules asset (the defaults if it cannot be found), evaluates. OutText gets the snapshot and the report as text, for
	 *  the log and the screen. Changes nothing: it can be run again.
	 */
	FDebriefResult Evaluate(const FContractDefinition& Contract, FString& OutText) const;

	/** The snapshot as one readable block, so a report can be checked by hand. */
	static FString DescribeSnapshot(const FMissionSnapshot& Snapshot);

	/**
	 *  The notebook for the mission's contract as the level stands. The level is not scanned more than twice a second whoever asks
	 *  (the notebook, the progress ring and the leave-the-site card all read this): a call in between returns the last one.
	 *  ForceRefresh reads the level now. Outside a mission it is empty and complete.
	 */
	const FNotebookData& GetLiveNotebook(bool bForceRefresh = false);

private:

	/** Seconds the cached notebook is kept before the level is read again. */
	static constexpr double NotebookRefreshSeconds = 0.5;

	FNotebookData CachedNotebook;
	double NotebookStamp = -1.0;

	/** The rules asset the scoring lives on: the one the director gave, else the one on the player's crush component (optional there, so it may be empty). */
	const UDemolitionRules* FindRules() const;

	/** See SetRules. Weak: the asset belongs to the Blueprints that reference it. */
	TWeakObjectPtr<const UDemolitionRules> CachedRules;

	int32 CrushEvents = 0;
	int32 ExplosionEvents = 0;

	/** See SetForbiddenTool and RecordToolUse. */
	FName ForbiddenTool;
	bool bForbiddenToolUsed = false;
};
