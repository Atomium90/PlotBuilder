#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Destruction/DemolitionTypes.h"
#include "ProtectedZone.generated.h"

class UBoxComponent;

/** What the zone knows about one damageable thing standing inside it (a neighbour's shed, a fence...). */
struct FProtectedRecord
{
	TWeakObjectPtr<AActor> Actor;
	FString Name;

	/** Hit points the thing had when the zone started counting (first seen inside, or the last ResetRecords). */
	float StartHP = 0.f;

	/** Hit points it has lost since. A thing that no longer exists has lost them all. */
	float HPLost = 0.f;

	bool bDestroyed = false;
};

/**
 *  A volume that must not be damaged (GDD §6.3, §13.2): a neighbour's shed, a fence, a tree. It watches every
 *  damageable thing inside, through the thing's own UDurabilityComponent, so it never needs to know about the
 *  tools: a hit, a blast or anything else that ends in ApplyDamage is counted. The evaluator reads the totals
 *  at the end of the mission. Registers itself on the level's plot.
 *
 *  What does not count today: pieces of the house falling into the volume. A collapse does not deal damage to
 *  other objects yet, so that case has no source. RecordExternalDamage is the one door for it: whatever later
 *  detects a falling piece (an impact check on the collapse, a volume test at the end) calls it, and the zone
 *  and the evaluator stay as they are.
 */
UCLASS()
class AProtectedZone : public AActor
{
	GENERATED_BODY()

public:

	AProtectedZone();

	/** The things seen inside, with what each lost. Brought up to date before it is returned. */
	const TArray<FProtectedRecord>& GetRecords() const;

	float GetTotalStartHP() const;
	float GetTotalHPLost() const;

	/** Share of the protected things' hit points that is gone, 0 to 1 (0 if nothing is inside). */
	float GetDamageRatio() const;

	/** Damage reported by something other than a tracked thing's own durability (see the class comment), by source name. Nothing calls this today. */
	UFUNCTION(BlueprintCallable, Category = "Protected Zone")
	void RecordExternalDamage(FName Source, float Amount);

	float GetExternalDamage() const { return ExternalDamage; }

	/** Damage dealt to the tracked things, summed by FDamageInfo::ToolId (a charge, the sledgehammer...). Debrief and debug read it. */
	const TMap<FName, float>& GetDamageByTool() const { return DamageByTool; }

	/** Forgets the damage so far and starts counting from what the things have left now (a thing already gone is dropped from the totals). */
	UFUNCTION(BlueprintCallable, Category = "Protected Zone")
	void ResetRecords();

	/** One line per tracked thing and the totals, for the log. */
	FString Describe() const;

	/** Draws the volume as a debug box every tick: yellow while untouched, red once something inside lost hit points. */
	UPROPERTY(EditAnywhere, Category = "Protected Zone|Debug")
	bool bShowDebug = true;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Bound to the durability of every tracked thing. The delegate does not say whose it is, so every record is re-read. */
	UFUNCTION()
	void HandleDamaged(const FDamageInfo& DamageInfo);

	/** Starts watching Actor if it can be damaged, is not already watched, and is not part of the house being demolished. */
	void Track(AActor* Actor);

	/** Re-reads every tracked thing's hit points. A thing that is gone keeps what it lost and counts as destroyed. */
	void SyncRecords() const;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Volume;

	/** Mutable: reading the records refreshes them from the things' durability. */
	mutable TArray<FProtectedRecord> Records;

	TMap<FName, float> DamageByTool;

	float ExternalDamage = 0.f;
};
