#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Destruction/DemolitionTypes.h"
#include "FurnishingRuleRow.generated.h"

class APhysicsProp;

/** The five room types a house is made of (GDD §9.4). */
UENUM(BlueprintType)
enum class ERoomType : uint8
{
	Bathroom,
	Kitchen,
	LivingRoom,
	Bedroom,
	Office
};

/** Where in its room a prop goes. AHouseGenerator turns this into the set of positions it accepts. */
UENUM(BlueprintType)
enum class EFurniturePlacement : uint8
{
	/** Back to a wall of the room, front towards the inside. */
	AgainstWall,
	/** Like AgainstWall, and also touching the wall at right angles to it. */
	Corner,
	/** As close to the middle of the room as it fits, any way round. */
	Centred,
	/** Fixtures that need a water supply. No plumbing wall exists yet, so for now this is AgainstWall. */
	NearPlumbing
};

/**
 *  One furniture variant of DT_FurnishingRules (GDD §9.4). The row owns every stat of the prop:
 *  AHouseGenerator sets Material, MaxHP, FragmentCount and Value on the actor before it starts, so
 *  the BP_Prop_* Blueprints only carry the mesh and its collision. A new variant is a new row.
 *
 *  Rows that share a Group are interchangeable versions of one object (a small, normal and large
 *  fridge): a room gets at most one prop per Group, picked among the rows of its type by Weight.
 */
USTRUCT(BlueprintType)
struct FFurnishingRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The object this row is a version of ("Fridge", "Bed"). One prop per group per room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	FName Group;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	ERoomType RoomType = ERoomType::LivingRoom;

	/** 1 = basic, up to 3 = high end. Informational today; Weight is what decides which row of a group is picked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "1", ClampMax = "3"))
	int32 QualityTier = 1;

	/** Blueprint to spawn: an APhysicsProp subclass with the mesh set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	TSubclassOf<APhysicsProp> PropClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	EDemolitionMaterial Material = EDemolitionMaterial::Wood;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "1"))
	float MaxHP = 50.f;

	/** Generic fragments spawned when the prop breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0"))
	int32 FragmentCount = 4;

	/** Worth at full health, in money. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0"))
	int32 Value = 0;

	/** Mass in kg, applied to the prop's physics body when the generator spawns it (replaces the Blueprint's own mass). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "1"))
	float Mass = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	EFurniturePlacement Placement = EFurniturePlacement::AgainstWall;

	/** Relative chance of this row among the rows of its group (a group with one row ignores it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Chance that the picked row is placed at all in a room of its type. 1 = always (if it fits). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	/**
	 *  Degrees added to the yaw that points the prop's local +X at the inside of the room. Kenney meshes
	 *  do not all face the same way after import: a multiple of 90 here turns a prop whose front is not
	 *  its +X, with no code change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Furnishing")
	float FrontYawOffset = 0.f;
};
