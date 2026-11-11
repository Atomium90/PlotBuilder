#pragma once

#include "CoreMinimal.h"
#include "DemolitionTypes.generated.h"

/** The six materials from GDD §7.2. Shared by ABuildablePiece and (later) breakable APhysicsProp furniture. */
UENUM(BlueprintType)
enum class EDemolitionMaterial : uint8
{
	Wood,
	Plaster,
	Brick,
	Concrete,
	Glass,
	Metal
};

/** Piece role, used as the HP class factor lookup key in DA_DemolitionRules - see GDD §7.3. Only meaningful for ABuildablePiece, not furniture (furniture HP is set per prop). */
UENUM(BlueprintType)
enum class EPieceClass : uint8
{
	Foundation,
	FloorSlab,
	ExteriorWall,
	InteriorWall,
	CornerPost,
	Stairs,
	RoofTile,
	DoorLeaf,
	/** Glass pane child actor of a window wall (GDD §7.3): always Glass, factor 1.0. Appended last so serialized values in existing BPs and DA_DemolitionRules stay valid. */
	WindowPane,
	/** Ceiling lamp (a buildable hanging from a slab): fragile, no structural role. Appended last for the same reason as WindowPane. */
	Light,
	/** Low balcony wall (concrete or glass, about 1 m): its own HP factor, since it is not a full-height wall. Takes the corner parity bias like the other wall classes. Appended last for the same reason as WindowPane. */
	Parapet
};

/** Readability states from GDD §7.6, driven by remaining-HP ratio against DA_DemolitionRules' thresholds. */
UENUM(BlueprintType)
enum class EDurabilityState : uint8
{
	Intact,
	Damaged,
	Critical,
	Destroyed
};

/** The four demolition tools from GDD §7.4, used only to index DA_DemolitionRules' tool x material matrix - FDamageInfo::ToolId (FName) stays the free-form logging/debug id. */
UENUM(BlueprintType)
enum class EDemolitionTool : uint8
{
	Charge,
	Sledgehammer,
	Crowbar,
	Jackhammer
};

/** One damage instance, passed to IDamageable::ApplyDamage - see GDD Appendix A. */
USTRUCT(BlueprintType)
struct FDamageInfo
{
	GENERATED_BODY()

	/** Flat damage amount, already resolved (tool base damage x tool/material multiplier) by whoever deals it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Amount = 0.f;

	/** The tool x material factor already inside Amount, 1 when there is none (an impact, a debug hit). Read only by the damage log; the damage itself never uses it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Multiplier = 1.f;

	/** Which tool dealt this damage (Crowbar, Sledgehammer, Jackhammer, Charge, Debug...), for logging/evaluation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ToolId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Impulse = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<AActor> Instigator = nullptr;
};

/**
 *  What a tool costs to keep, in money (GDD §11). It is data on UDemolitionToolData: a tool that costs nothing leaves both
 *  at 0. The mission report charges it (UContractEvaluator): a flat fee for each mission where the tool worked, plus a rate
 *  for each second it was working. Two parts because a flat fee alone ignores how hard the tool was used, and a rate alone
 *  turns a long drilling session into most of the payout.
 */
USTRUCT(BlueprintType)
struct FToolUpkeep
{
	GENERATED_BODY()

	/** Charged once for a mission in which the tool did any work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upkeep", meta = (ClampMin = "0"))
	int32 PerJob = 0;

	/** Charged for each second of active use (a continuous tool counts the time it is hitting something). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upkeep", meta = (ClampMin = "0"))
	float PerSecond = 0.f;

	/** The tool costs nothing to keep. */
	bool IsFree() const
	{
		return PerJob <= 0 && PerSecond <= 0.f;
	}

	/** The fee for a mission in which the tool worked ActiveSeconds in all, in whole money. Nothing if it did not work at all. */
	int32 Cost(float ActiveSeconds) const
	{
		if (IsFree() || ActiveSeconds <= 0.f)
		{
			return 0;
		}

		return FMath::Max(0, PerJob) + FMath::RoundToInt(FMath::Max(0.f, PerSecond) * ActiveSeconds);
	}
};
