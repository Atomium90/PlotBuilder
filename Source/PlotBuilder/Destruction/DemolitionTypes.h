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
	Light
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
