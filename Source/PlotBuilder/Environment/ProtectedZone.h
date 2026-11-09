#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Destruction/DemolitionTypes.h"
#include "ProtectedZone.generated.h"

class APlot;
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

	/**
	 *  The thing had not set its hit points when the zone started watching it (its own BeginPlay runs after the zone's, so
	 *  its durability still read 0): its start is its full health, taken at the next sync instead of the 0 that was seen.
	 */
	bool bStartPending = false;
};

/**
 *  A volume that must not be damaged (GDD §6.3, §13.2): a neighbour's shed, a fence, a tree. It watches every
 *  damageable thing inside, through the thing's own UDurabilityComponent, so it never needs to know about the
 *  tools: a hit, a blast or anything else that ends in ApplyDamage is counted. The evaluator reads the totals
 *  at the end of the mission. Registers itself on the level's plot.
 *
 *  A piece of the house that falls into the volume counts too, with its weight (GDD §6.5): a collapse does not deal damage
 *  to other objects, so the zone is told when a falling piece breaks (APlot::NotifyPieceLanded, called by the piece) and
 *  looks where it landed. It has to be that moment: the piece and its fragments are gone a few seconds later, so a look
 *  at the end of the mission would find nothing. Each piece counts once. The fragments themselves are not counted.
 *  RecordExternalDamage stays the generic door for any other source.
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

	/** Damage reported by something other than a tracked thing's own durability, by source name: the weight of a piece of the house that fell in is reported this way. Kept apart from the tools' damage. */
	UFUNCTION(BlueprintCallable, Category = "Protected Zone")
	void RecordExternalDamage(FName Source, float Amount);

	/** Total of RecordExternalDamage: for the pieces of the house, the weight that fell in. */
	float GetExternalDamage() const { return ExternalDamage; }

	/**
	 *  Called by the plot when a piece of the house that was falling has just broken, with the weight it counts for. If the
	 *  piece touches this volume (or, with bCountTouching off, if its centre is inside), it is counted once as the weight
	 *  fallen in, listed by name, boxed on screen for a few seconds. The test is on the piece's world bounds: a zone turned
	 *  off its axes is read through its bounding box.
	 */
	void NotifyPieceLanded(AActor* Piece, int32 Weight);

	/**
	 *  Looks at what stands in the volume right now and starts watching every damageable thing in it (not the house). It reads
	 *  the bounds of the actors, not the physics overlaps, whose timing at the start of a level is not something to rely on.
	 *  Called when the zone starts, and again by the mission director once the house is bound. Logs what it found, and why it
	 *  turned something down: that line is how to see what a zone really sees.
	 */
	void ScanVolume();

	/** Damage dealt to the tracked things, summed by FDamageInfo::ToolId (a charge, the sledgehammer...). Debrief and debug read it. */
	const TMap<FName, float>& GetDamageByTool() const { return DamageByTool; }

	/** Forgets the damage so far and starts counting from what the things have left now (a thing already gone is dropped from the totals). */
	UFUNCTION(BlueprintCallable, Category = "Protected Zone")
	void ResetRecords();

	/** One line per tracked thing and the totals, for the log. */
	FString Describe() const;

	/**
	 *  Whether this zone is part of the mission. A level holds more zones than a mission uses: the mission director switches
	 *  off the ones it does not use, and the contract's ProtectedZone count says how many stay on.
	 */
	bool IsActive() const { return bActive; }

	/**
	 *  Turns the zone on or off. Off: it leaves the plot's list (the report ignores it, the ground no longer keeps room
	 *  for it), stops watching and counting, and with bHideContentWhenInactive hides the protected things it holds and takes
	 *  their collision away: nothing is protected there, and a shed that nothing protects must not look like it. On again: all of
	 *  that comes back. Works before the zone has started, since the director runs first.
	 */
	void SetActive(bool bNewActive);

	/** Hide the protected things of the zone (the neighbour's shed) while it is off. Off this, they stay as scenery, unprotected. */
	UPROPERTY(EditAnywhere, Category = "Protected Zone")
	bool bHideContentWhenInactive = true;

	/** Draws the volume as a debug box every tick: yellow while untouched, red once something inside lost hit points or a piece of the house fell in. */
	UPROPERTY(EditAnywhere, Category = "Protected Zone|Debug")
	bool bShowDebug = true;

	/**
	 *  How a falling piece is judged to be in the zone. On: any part of it inside the volume counts (a wall that lands on the
	 *  edge of the fence did reach it). Off: only a piece whose centre is inside.
	 */
	UPROPERTY(EditAnywhere, Category = "Protected Zone")
	bool bCountTouching = true;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Bound to the durability of every tracked thing. The delegate does not say whose it is, so every record is re-read. */
	UFUNCTION()
	void HandleDamaged(const FDamageInfo& DamageInfo);

	/**
	 *  Starts watching Actor if it can be damaged, is not already watched, and is not part of the house being demolished.
	 *  Returns whether it is watched now (a refusal says why in OutWhyNot, if given).
	 */
	bool Track(AActor* Actor, FString* OutWhyNot = nullptr);

	/** Re-reads every tracked thing's hit points. A thing that is gone keeps what it lost and counts as destroyed. */
	void SyncRecords() const;

	/** Stops watching everything and forgets what was counted: the zone is off. */
	void ClearWatching();

	/** The damageable things standing in the volume that the zone protects (not the house, not a pawn). */
	void CollectProtectedThings(TArray<AActor*>& OutThings) const;

	void HideContent();
	void ShowContent();

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Volume;

	/** Mutable: reading the records refreshes them from the things' durability. */
	mutable TArray<FProtectedRecord> Records;

	TMap<FName, float> DamageByTool;

	float ExternalDamage = 0.f;

	/** What each source reported through RecordExternalDamage. */
	TMap<FName, float> ExternalBySource;

	/** A piece of the house counted as fallen in, by name, for the readout. */
	struct FFallenRecord
	{
		FString Name;
		int32 Weight = 0;
	};

	TArray<FFallenRecord> Fallen;

	/** Pieces already counted: a piece is counted once, whatever tells the zone about it. */
	TSet<TWeakObjectPtr<AActor>> CountedPieces;

	/** The level's plot, found once at BeginPlay (Track runs for every overlap, and a scan of the world each time was wasteful). */
	TWeakObjectPtr<APlot> CachedPlot;

	/** See IsActive. On by default; the director may turn it off before the zone has started. */
	bool bActive = true;

	/** What SetActive(false) hid, so that SetActive(true) shows exactly that again. */
	TArray<TWeakObjectPtr<AActor>> HiddenContent;
};
