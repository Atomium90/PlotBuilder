#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Buildables/StructuralSocket.h"
#include "Buildables/BuildAnchor.h"
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

	/** Owner's own RoofStep plus the socket's local RoofStep offset - see FStructuralSocket::RoofStep. Always 0 for a non-roof socket, so it never affects matching for one. */
	UPROPERTY()
	int32 RoofStep = 0;
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
 *
 *  Also owns the grid occupancy table (IsSlotFree/GetPieceAt) - separate concern from stability,
 *  but naturally lives here too since it's filled/cleared at the same two points (RegisterPiece/
 *  UnregisterPiece) and keyed by the same EBuildAnchor used for a piece's own placement.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UStructuralSupportComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UStructuralSupportComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structural")
	TObjectPtr<UStructuralRules> Rules;

	/** Colors each registered piece by its stability via Custom Primitive Data (see UpdateStabilityDebugColor on ABuildablePiece) whenever stability is recomputed. */
	UPROPERTY(EditAnywhere, Category = "Structural|Debug")
	bool bShowDebug = false;

	/** World location to grid cell, via Rules->CellSize/FloorHeight - the Legacy conversion path, also used to resolve a Legacy piece's GetStructuralCell(). */
	FIntVector GetPieceCell(const FVector& WorldLocation) const;

	/** World yaw to quarter turns (0-3 meaningfully, any int otherwise) - the Legacy conversion path. */
	static int32 GetQuarterTurns(const FRotator& WorldRotation);

	/**
	 *  Socket -> absolute grid cell: Piece's own cell plus the socket's local offset rotated by
	 *  Piece's orientation. The single source of truth for this conversion - RegisterPiece and
	 *  RecomputeStability both call it, and it's exposed here so a Blueprint tool (e.g. the socket
	 *  debugger) visualizing the same sockets matches it exactly instead of redoing the rotation
	 *  itself. Only meaningful once Piece has gone through RegisterPiece at least once (see
	 *  ABuildablePiece::GetStructuralCell) - returns (0,0,0) for a null Piece.
	 */
	UFUNCTION(BlueprintPure, Category = "Structural")
	FIntVector GetSocketCell(const ABuildablePiece* Piece, const FStructuralSocket& Socket) const;

	/** Socket -> absolute RoofStep: Piece's own RoofStep plus the socket's local offset - the RoofStep counterpart to GetSocketCell above. Never rotated (yaw only ever swaps X/Y). Returns 0 for a null Piece. */
	UFUNCTION(BlueprintPure, Category = "Structural")
	int32 GetSocketRoofStep(const ABuildablePiece* Piece, const FStructuralSocket& Socket) const;

	/**
	 *  Core stability VALUE a candidate would get, in integer grid coordinates - used by the Build
	 *  Mode ghost to tell a comfortably-supported candidate from a barely-standing one
	 *  (UStructuralRules::ComfortThreshold) rather than a plain yes/no; placement itself is no longer
	 *  gated on this at all (see UBuildModeComponent) - a candidate below CollapseThreshold is
	 *  allowed to be placed, and will collapse right after via the normal RegisterPiece ->
	 *  RecomputeStability path. Read-only: never mutates the registry. Compares against neighbors'
	 *  already-cached Stability rather than replaying a full relaxation. RoofStep defaults to 0 -
	 *  meaningless for anything but a Roof_* candidate. 1.f for a candidate with an Anchor socket.
	 */
	UFUNCTION(BlueprintPure, Category = "Structural")
	float ComputeStability(const TArray<FStructuralSocket>& Sockets, const FIntVector& Cell, int32 QuarterTurns, int32 RoofStep = 0) const;

	/** Convenience overload for a Legacy piece: converts WorldLocation/WorldRotation to grid coordinates once, then calls the integer overload above. Not itself a UFUNCTION - UHT doesn't support overloading by parameter type. */
	float ComputeStability(const TArray<FStructuralSocket>& Sockets, const FVector& WorldLocation, const FRotator& WorldRotation, int32 RoofStep = 0) const;

	/** Adds Piece's sockets to the neighbor table (and, for a grid-anchored piece, occupies its slot), links it to whatever matches, then recomputes stability for the whole plot. */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	void RegisterPiece(ABuildablePiece* Piece);

	/**
	 *  Removes Piece's sockets from the neighbor table (and frees its occupancy slot, if any), then
	 *  recomputes stability for the whole plot - this is what lets removing one piece cascade to
	 *  whatever depended on it, since anything that loses its only path to an anchor collapses as
	 *  part of the same recompute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	void UnregisterPiece(ABuildablePiece* Piece);

	/** Occupancy table, separate from structural stability: is SlotType's slot at Cell free? Always true for SlotType == Legacy (nothing is ever tracked there). */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	bool IsSlotFree(EBuildAnchor SlotType, const FIntVector& Cell) const;

	/** The piece occupying SlotType's slot at Cell, or nullptr if free. */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	ABuildablePiece* GetPieceAt(EBuildAnchor SlotType, const FIntVector& Cell) const;

private:

	static FIntVector RotateCellOffset(const FIntVector& Offset, int32 QuarterTurns);

	/** Whether two socket types pair up at all, and if so whether the link counts as vertical (cheap) or horizontal (expensive). */
	static bool AreSocketsLinked(EStructuralSocketType A, EStructuralSocketType B, bool& OutIsVertical);

	void RecomputeStability();

	void OccupySlot(EBuildAnchor SlotType, const FIntVector& Cell, ABuildablePiece* Piece);
	void FreeSlotsFor(ABuildablePiece* Piece);
	TMap<FIntVector, TObjectPtr<ABuildablePiece>>* GetOccupancyMap(EBuildAnchor SlotType);
	const TMap<FIntVector, TObjectPtr<ABuildablePiece>>* GetOccupancyMap(EBuildAnchor SlotType) const;

	UPROPERTY()
	TArray<TObjectPtr<ABuildablePiece>> RegisteredPieces;

	TMap<FIntVector, TArray<FRegisteredSocket>> SocketsByCell;

	TMap<FIntVector, TObjectPtr<ABuildablePiece>> FloorSlotOccupancy;
	TMap<FIntVector, TObjectPtr<ABuildablePiece>> EdgeSlotOccupancy;
	TMap<FIntVector, TObjectPtr<ABuildablePiece>> VertexSlotOccupancy;
};
