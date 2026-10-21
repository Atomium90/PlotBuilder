#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Buildables/StructuralSocket.h"
#include "StructuralSupportComponent.generated.h"

class ABuildablePiece;
class UStructuralRules;

/** One socket registered in the plot's neighbor table, with enough to resolve back to its owning piece. */
USTRUCT()
struct FRegisteredSocket
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ABuildablePiece> Owner = nullptr;

	UPROPERTY()
	EStructuralSocketType Type = EStructuralSocketType::WallBottom;
};

/**
 *  Tracks structural support for every ABuildablePiece on this Plot, as a stability value (0 to 1)
 *  propagated outward from anchors rather than a single parent/child link - a piece can have
 *  several valid supports, so losing one doesn't wrongly collapse it if another still holds. Also
 *  the only way to tell "resting on top of" from "merely touching the side of": a plain reachability
 *  test can't, it takes a loss that's much higher on horizontal links than vertical ones.
 *
 *  Neighbors are found by a grid cell lookup from each piece's declared Sockets (data on the BP,
 *  never authored on the mesh) rather than a physics overlap - see claude/Kit greybox specs des
 *  pieces et plan de remplacement.md for the socket table this mirrors. Recomputed only on
 *  placement/removal, never per tick - cheap enough for a few hundred pieces with a plain fixed
 *  point relaxation, no priority queue needed.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UStructuralSupportComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UStructuralSupportComponent();

	UPROPERTY(EditAnywhere, Category = "Structural")
	TObjectPtr<UStructuralRules> Rules;

	/** Colors each registered piece by its stability via Custom Primitive Data (see UpdateStabilityDebugColor on ABuildablePiece) whenever stability is recomputed. */
	UPROPERTY(EditAnywhere, Category = "Structural|Debug")
	bool bShowDebug = false;

	/** Adds Piece's sockets to the neighbor table, links it to whatever matches, then recomputes stability for the whole plot. */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	void RegisterPiece(ABuildablePiece* Piece);

	/**
	 *  Removes Piece's sockets from the neighbor table, then recomputes stability for the whole
	 *  plot - this is what lets removing one piece cascade to whatever depended on it, since
	 *  anything that loses its only path to an anchor collapses as part of the same recompute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	void UnregisterPiece(ABuildablePiece* Piece);

	/**
	 *  Read-only: would a piece with these sockets be stable at this transform? Used by the Build
	 *  Mode ghost before confirming a placement. Never mutates the registry - compares against
	 *  neighbors' already-cached stability rather than replaying a full relaxation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	bool WouldBeStable(const TArray<FStructuralSocket>& Sockets, const FVector& WorldLocation, const FRotator& WorldRotation) const;

private:

	FIntVector GetPieceCell(const FVector& WorldLocation) const;
	static int32 GetQuarterTurns(const FRotator& WorldRotation);
	static FIntVector RotateCellOffset(const FIntVector& Offset, int32 QuarterTurns);

	/** Whether two socket types pair up at all, and if so whether the link counts as vertical (cheap) or horizontal (expensive). */
	static bool AreSocketsLinked(EStructuralSocketType A, EStructuralSocketType B, bool& OutIsVertical);

	void RecomputeStability();

	UPROPERTY()
	TArray<TObjectPtr<ABuildablePiece>> RegisteredPieces;

	TMap<FIntVector, TArray<FRegisteredSocket>> SocketsByCell;
};
