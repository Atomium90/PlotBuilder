#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Contracts/ContractTypes.h"
#include "Generation/FurnishingRuleRow.h"
#include "ContractPool.generated.h"

class UHouseArchetype;

/**
 *  One kind of job the board can offer: what to build, how big, which types and conditions can come with it. Adding a
 *  kind of contract is a new row in the pool, not new code (GDD §6.4). Everything left at zero comes from the archetype.
 */
USTRUCT(BlueprintType)
struct FContractTemplate
{
	GENERATED_BODY()

	/** A name for the logs and the editor list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FName Id;

	/** The archetype the contracts of this kind build. The ranges below that are left at zero are read from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	TObjectPtr<UHouseArchetype> Archetype;

	/** How likely this kind is next to the others. 0 never appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** ReputationRequired of the contracts drawn from this kind (a single integer in Block 1: the board can filter on it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template", meta = (ClampMin = "0"))
	int32 Tier = 0;

	/** Footprint range in room cells (X, Y). A zero on an axis takes the archetype's MinRoomCells / MaxRoomCells. A plan can only stretch so far, so keep the range inside what its plans reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	FIntPoint MinFootprintCells = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size")
	FIntPoint MaxFootprintCells = FIntPoint::ZeroValue;

	/** Storey range. Zero takes the archetype's floor range, or its fixed count. An archetype with no floor range always gets its own count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size", meta = (ClampMin = "0"))
	int32 MinFloors = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Size", meta = (ClampMin = "0"))
	int32 MaxFloors = 0;

	/** Relative weights of the three types. A partial demolition needs two storeys: below that its weight counts for nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type", meta = (ClampMin = "0"))
	float TotalWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type", meta = (ClampMin = "0"))
	float PartialWeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type", meta = (ClampMin = "0"))
	float RenovationWeight = 0.f;

	/** Pieces of furniture the client wants carried out: a count drawn in this range, 0 means no salvage condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (ClampMin = "0"))
	int32 MinSalvage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (ClampMin = "0"))
	int32 MaxSalvage = 0;

	/** Protected zones that are active: a count drawn in this range, 0 means none. The level's candidate zones are the ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (ClampMin = "0"))
	int32 MinZones = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (ClampMin = "0"))
	int32 MaxZones = 0;

	/**
	 *  Chance, 0 to 1, that a job of this kind asks to keep a room standing. Never on a renovation, which keeps the whole shell
	 *  already. The room is one of KeepStandingRooms, drawn here; which of the house's rooms of that type is kept is decided when
	 *  the house is built.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (ClampMin = "0", ClampMax = "1"))
	float KeepStandingChance = 0.f;

	/** The room types the job may ask to keep. Bathroom, Kitchen, LivingRoom and Bedroom are in every plan of the project; the Office is not, leave it out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	TArray<ERoomType> KeepStandingRooms;

	/** Chance, 0 to 1, that a job of this kind forbids one of ForbiddenTools. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions", meta = (ClampMin = "0", ClampMax = "1"))
	float ForbiddenToolChance = 0.f;

	/** The tools the job may forbid, by tool id as in the damage log: Sledgehammer, Crowbar, Jackhammer or Charge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	TArray<FName> ForbiddenTools;

	/** Multiplier on the base payout of the GDD formula (12 per m2 times the type factor). 1 leaves it alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pay", meta = (ClampMin = "0"))
	float PayoutMultiplier = 1.f;
};

/**
 *  The weighted pool the contract board draws its offers from (DA_ContractPool). Data only: the draw is the two functions
 *  below, which take their random stream as a parameter, so the same stream gives the same offers.
 */
UCLASS(BlueprintType)
class UContractPool : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<FContractTemplate> Templates;

	/** Who asks: a client id is drawn from this list (a placeholder in Block 1). Empty uses "Client". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<FName> Clients;

	/** Floor area of one room cell: a cell is 200 cm, so 4 m2. */
	static constexpr float SquareMetresPerCell = 4.f;

	/**
	 *  One contract of that kind: footprint, storeys, type, conditions and house seed drawn from the stream, then the area,
	 *  the estimated minutes and the base payout worked out from them (GDD §6.4, §11.2). Always makes the same number of
	 *  draws, whatever the template says, so a change in the data never shifts what comes after it.
	 */
	FContractDefinition DrawContract(const FContractTemplate& Template, FRandomStream& Stream) const;

	/**
	 *  Count offers by weight, never the same kind twice in a set. If the pool has fewer kinds than Count, a kind comes
	 *  back only once every one has been drawn. Empty when no template has a weight.
	 */
	TArray<FContractDefinition> DrawOffers(int32 Count, FRandomStream& Stream) const;
};
