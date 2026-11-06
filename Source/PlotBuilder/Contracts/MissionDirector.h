#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Contracts/ContractTypes.h"
#include "MissionDirector.generated.h"

class AHouseGenerator;
class AMissionGround;
class APlot;
class UDataTable;

/**
 *  The one actor a mission level needs besides its plot and generator: at the start of the level it builds
 *  the house, then binds the contract to it (GDD §9.2 steps 1 to 7) and puts the building side of the game to
 *  sleep (a mission is played with the tool belt only).
 *
 *  What the mission is comes from the level's travel options, so the hub can send the player there with a plain
 *  OpenLevel: "Lvl_Mission?Seed=7?Archetype=twostorey?Type=partial?From=1". Anything missing falls back to the
 *  defaults below. pb.SetSeed overrides the seed for the rest of the session, for demos and tests. The contract
 *  subsystem will replace the option reading: it will set the same values from the active contract.
 *
 *  The result (the target and must-stay sets) is stored on the level's plot (APlot::GetMissionSets), which is
 *  where the evaluator reads it.
 */
UCLASS()
class AMissionDirector : public AActor
{
	GENERATED_BODY()

public:

	AMissionDirector();

	/** The generator that builds the house. Empty = the first one in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	TObjectPtr<AHouseGenerator> Generator;

	/** The ground of the level, rebuilt from the mission's seed just before the house. Empty = the first one in the level, or none (the level then keeps its own ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	TObjectPtr<AMissionGround> Ground;

	/** The shop catalog (DT_Shop, the same asset as the player's hotbar): the cost of a class there is its weight in the report. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	TObjectPtr<UDataTable> ShopCatalog;

	/** Build the house and bind the contract when the level starts. Off for a level that drives it by hand (Blueprint, console). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	bool bStartOnBeginPlay = true;

	/** Switch Build Mode, the hotbar and the shop off for the player when the level starts (a mission is played with the tool belt only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	bool bDisableBuilding = true;

	/** Seed when neither pb.SetSeed nor a Seed option gives one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Defaults")
	int32 DefaultSeed = 1;

	/** Contract type when no Type option gives one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Defaults")
	EContractType DefaultType = EContractType::TotalDemolition;

	/** First storey a partial demolition removes, when no From option gives one. 0 = rolled from the seed, between 1 and the storey count minus 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Defaults", meta = (ClampMin = "0"))
	int32 DefaultPartialFromLevel = 0;

	/** Seed set by pb.SetSeed, used by every mission that starts after it in this session. -1 = none. */
	static int32 DebugSeed;

	/**
	 *  Reads the mission from the travel options and the defaults, builds the house (GenerateValid: a house
	 *  always exists unless every attempt failed) and binds the contract to it. False if no house could be built.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool StartMission();

	/**
	 *  Computes the target and must-stay sets of the current house for Type (FromLevel is the first storey a
	 *  partial demolition removes, 0 = rolled from the seed), stores them on the plot and prints the summary.
	 *  Callable at any time on a built house, so a type can be compared without rebuilding.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void Rebind(EContractType Type, int32 FromLevel);

	/** Draws a box on every piece and prop for as long as it is on: green for what must be destroyed, blue for what must stay. */
	void SetShowTargets(bool bShow);

	/** The summary printed by the last Rebind, for the debug commands. */
	const FString& GetSummary() const { return Summary; }

	/** The director of this world, or nullptr (a world without a mission). */
	static AMissionDirector* Find(const UObject* WorldContext);

	/** Reads a type from text ("total", "partial", "renovation", or just the first letter, any case). False, OutType untouched, for anything else. */
	static bool ParseType(const FString& Text, EContractType& OutType);

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	/** Puts the player's building side to sleep. The pawn may not exist yet when the level starts: retried until it does. */
	void ApplyMissionInput();

	AHouseGenerator* ResolveGenerator();
	AMissionGround* ResolveGround();
	APlot* ResolvePlot() const;

	FTimerHandle InputTimer;
	int32 InputAttempts = 0;
	bool bShowTargets = false;
	FString Summary;
};
