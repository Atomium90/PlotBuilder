#pragma once

#include "CoreMinimal.h"
#include "ContractTypes.generated.h"

/** The three kinds of contract (GDD §6.2). They differ only in what must be destroyed and what must stay: see MissionBinding::Compute. */
UENUM(BlueprintType)
enum class EContractType : uint8
{
	/** The whole property goes. */
	TotalDemolition,
	/** The upper storeys go, from a given storey up. The storeys below stay. */
	PartialDemolition,
	/** The interior is stripped: everything that is not load-bearing, and all the furniture. The shell stays. */
	Renovation
};

/** What a contract can add on top of its type (GDD §6.3). */
UENUM(BlueprintType)
enum class EConditionType : uint8
{
	/** The client wants some pieces of furniture carried out intact to the client drop zone. */
	SalvageFurniture,
	/** A volume (a neighbour's shed, a fence) must not be damaged: the tools, and the pieces of the house that fall into it, count. */
	ProtectedZone,
	/** [+] A named part of the house must still stand at the end. */
	KeepStanding,
	/** [+] A tool is not allowed on this job. */
	ForbiddenTool
};

/** One condition of a contract. What Count and Param mean depends on the type. */
USTRUCT(BlueprintType)
struct FContractCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	EConditionType Type = EConditionType::SalvageFurniture;

	/** SalvageFurniture: how many items the client wants. ProtectedZone: how many zones are active (X of the level's Y). Unused by the other two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract", meta = (ClampMin = "0"))
	int32 Count = 1;

	/** KeepStanding: the part to keep (a storey or a part name). ForbiddenTool: the tool id (as in FDamageInfo::ToolId). Unused by the other two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	FName Param;
};

/**
 *  A contract (GDD §6.1): a small data object. The pool draws it, the board shows it, the mission director builds the
 *  house from it, the evaluator pays it. Everything the generator needs is a request (a footprint in cells, a number
 *  of storeys): the real house is whatever the generator makes of it, so AreaM2 is an estimate made from the request.
 */
USTRUCT(BlueprintType)
struct FContractDefinition
{
	GENERATED_BODY()

	/** Who asks. A placeholder in Block 1 (Block 3 gives clients a personality). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	FName ClientId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	EContractType Type = EContractType::TotalDemolition;

	/** A fragment of the archetype asset's name, as AHouseGenerator::SelectArchetype reads it ("TwoStorey", "Apartment"...). Empty keeps the level's archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	FName ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	int32 Seed = 0;

	/** The footprint asked of the generator, in room cells (200 cm). 0 on an axis keeps the archetype's own bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	FIntPoint FootprintCells = FIntPoint::ZeroValue;

	/** The number of storeys asked of the generator (honoured by an archetype that has a floor range). 0 keeps the archetype's default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract", meta = (ClampMin = "0"))
	int32 Floors = 0;

	/** The first storey a partial demolition removes. 0 lets the director roll it from the seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract", meta = (ClampMin = "0"))
	int32 PartialFromLevel = 0;

	/** Estimated floor area, all storeys together, shown on the board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	float AreaM2 = 0.f;

	/** Estimated duration shown on the board (GDD §6.4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	float EstimatedMinutes = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	TArray<FContractCondition> Conditions;

	/** Money for a perfect job. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	int32 BasePayout = 0;

	/** Gates harder offers (GDD §6.1). A single integer in Block 1, reputation proper is Block 3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contract")
	int32 ReputationRequired = 0;

	/** The first condition of that type, or nullptr. */
	const FContractCondition* FindCondition(EConditionType ConditionType) const
	{
		return Conditions.FindByPredicate([ConditionType](const FContractCondition& Condition) { return Condition.Type == ConditionType; });
	}

	/** Count of the first condition of that type, 0 when the contract has none. */
	int32 CountOf(EConditionType ConditionType) const
	{
		const FContractCondition* Condition = FindCondition(ConditionType);
		return Condition ? Condition->Count : 0;
	}
};

/**
 *  How the client's report turns into money (GDD §6.5). Every number is a starting point to tune; they live on
 *  UDemolitionRules (DA_DemolitionRules), and the evaluator only ever sees this struct, so it runs without any asset.
 */
USTRUCT(BlueprintType)
struct FScoringParams
{
	GENERATED_BODY()

	/** Collateral is weighed heavier than its raw share. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	float CollateralCoefficient = 1.5f;

	/** Same weight as collateral for the protected zones (the GDD formula had no zone term). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	float ZoneCoefficient = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	float FurnitureCoefficient = 1.f;

	/** The penalty never goes above this share of what was earned: the player keeps at least 1 - PenaltyCap of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0", ClampMax = "1"))
	float PenaltyCap = 0.8f;

	/** The part of the penalty that comes from safety events never goes above this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0", ClampMax = "1"))
	float SafetyCap = 0.25f;

	/** Penalty per crush event on the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	float CrushPenalty = 0.1f;

	/** Penalty per explosion that catches the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	float ExplosionPenalty = 0.1f;

	/** How hard the house falling into a protected zone counts: the share of the whole house's weight that landed there, times this, is added to the zone's damage. 3 means a third of the house is a destroyed zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	float ZoneFallFactor = 3.f;

	/** Penalty for using the tool a contract forbade, once for the whole mission whatever the number of uses (a share of what was earned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0", ClampMax = "1"))
	float ForbiddenToolPenalty = 0.15f;

	/**
	 *  Share of a salvaged item's value that is left, by its HP ratio: points (HP ratio, multiplier) sorted by HP ratio,
	 *  linear between them and flat beyond the ends. The default is the GDD §10.3 curve: the value drops by half fast, then
	 *  slowly (1.0 at full HP, 0.5 at 0.8, 0.05 at 0). Empty, or one point, means a straight line from nothing at 0 HP to the
	 *  full value at full HP.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring")
	TArray<FVector2D> FurnitureValuePoints = {FVector2D(0.0, 0.05), FVector2D(0.8, 0.5), FVector2D(1.0, 1.0)};
};

/** A piece of furniture the client asked for, as it stands at the end of the mission. */
USTRUCT(BlueprintType)
struct FSalvageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	FString Name;

	/** Its value at full health (the prop's Value). */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int32 Value = 0;

	/** Hit points left, 0 to 1. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	float HPRatio = 1.f;

	/** Inside the client drop zone at the end. Anything else is lost: destroyed, or not handed over. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	bool bDelivered = false;

	/** Broken, or gone. Only a word in the report: a lost item costs the same whether it was destroyed or left behind. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	bool bDestroyed = false;

	/** Not handed over and still standing on the plot (in or around the house) at the end, as opposed to carried out of the plot but not to the client zone. Again only a word in the report. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	bool bOnSite = false;

	/** In the player's hands at this very moment. Only the notebook reads it: the report ignores it. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	bool bCarried = false;
};

/** What happened to one protected zone. */
USTRUCT(BlueprintType)
struct FZoneResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	FString Name;

	/** What the player reads ("Neighbour's shed"): the zone's DisplayName. Empty falls back to "Protected zone". */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	FString Label;

	/** Share of the hit points of the things inside that the tools and the charges took, 0 to 1. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	float ToolDamageRatio = 0.f;

	/** Weight of the pieces of the house that landed in the zone, each counted once. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int64 FallenWeight = 0;
};

/**
 *  What the evaluator needs to know about a finished mission, as plain numbers: no actor, no level. Whoever ends the
 *  mission fills it from the world; the tests fill it by hand.
 */
USTRUCT(BlueprintType)
struct FMissionSnapshot
{
	GENERATED_BODY()

	/** Weight of what had to be destroyed (the target set), and of that, what is gone. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int64 TargetWeight = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int64 TargetDestroyedWeight = 0;

	/** Weight of what had to stay (the must-stay set), and of that, what is gone. Does not hold the salvaged furniture. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int64 MustStayWeight = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int64 MustStayDestroyedWeight = 0;

	/** Weight of the whole house, salvaged furniture included: what the weight that fell into a zone is measured against. 0 means target + must-stay. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int64 HouseWeight = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	TArray<FSalvageResult> Salvage;

	/** Active protected zones only. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	TArray<FZoneResult> Zones;

	/** Times a collapse caught the player. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int32 CrushEvents = 0;

	/** Times an explosion caught the player. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	int32 ExplosionEvents = 0;

	/** The tool the contract forbade (none if it forbade nothing), and whether it was used at least once: that is all that counts. */
	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	FName ForbiddenToolName;

	UPROPERTY(BlueprintReadWrite, Category = "Mission Result")
	bool bForbiddenToolUsed = false;
};

/** One line of the report: a term of the formula, what it measured, and what it did to the money. */
USTRUCT(BlueprintType)
struct FDebriefLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	FString Label;

	/** What was measured, in words ("destroyed 800 of 1000"). */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	FString Detail;

	/** The term itself, 0 to 1 (a share of the target set, of the furniture value...). */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float Value = 0.f;

	/** What the term does to the payout: plus for what was earned, minus for a penalty. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 Money = 0;
};

/** The client's report (GDD §6.5): every term of the formula, the money, and the lines to show. */
USTRUCT(BlueprintType)
struct FDebriefResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float Completion = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float Collateral = 0.f;

	/** Damage to the protected zones, 0 to 1 (summed over the zones). */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float ZoneDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float FurnitureLoss = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float Safety = 0.f;

	/** The share taken for using a forbidden tool: the penalty if it was used, 0 otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float ForbiddenTool = 0.f;

	/** The safety events behind Safety, so the debrief can name them (a collapse that caught the player, an explosion in their face). */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 CrushEvents = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 ExplosionEvents = 0;

	/** The share of the earned money taken away, after the cap. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	float Penalty = 0.f;

	/** The terms alone would have taken more than the cap allows. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	bool bPenaltyCapped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 BasePayout = 0;

	/** Base payout times completion. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 Earned = 0;

	/** What is paid: Earned minus the penalty, always. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 Payout = 0;

	/** Block 3 (reputation). Always 0 for now. */
	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	int32 ReputationDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debrief")
	TArray<FDebriefLine> Lines;

	/** The report as text, one line per term, for the log and the on-screen readout. */
	FString ToString() const;
};
