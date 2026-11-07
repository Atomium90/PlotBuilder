#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Buildables/StructuralSocket.h"
#include "Buildables/BuildAnchor.h"
#include "StructuralSupportComponent.generated.h"

class ABuildablePiece;
class UStructuralRules;
class UMaterialInterface;

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
 *  pieces et plan de remplacement.md for the socket table this mirrors. Cheap enough for a few
 *  hundred pieces with a plain fixed point relaxation, no priority queue needed.
 *
 *  RegisterPiece/UnregisterPiece no longer recompute inline - they just mark the graph dirty, and
 *  TickComponent does the actual relaxation at most once per frame. This is what keeps a cascade of
 *  N pieces collapsing in the same frame (BeginCollapse -> UnregisterPiece, one per piece) down to
 *  one recompute instead of N nested ones.
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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structural")
	TObjectPtr<UStructuralRules> Rules;

	/** Colors each registered piece by its stability via Custom Primitive Data (see UpdateStabilityDebugColor on ABuildablePiece) whenever stability is recomputed. Set at edit time only - toggle at runtime through SetShowDebug instead, which also swaps the heatmap material in. */
	UPROPERTY(EditAnywhere, Category = "Structural|Debug")
	bool bShowDebug = false;

	/** Material swapped onto every registered piece's mesh (slot 0) while the heatmap is on - reads Stability from Custom Primitive Data index 0, see claude/Plan_Sprint_1.md's Friday notes. Swapping back to bShow = false reverts to each mesh's own default material (SetMaterial(0, nullptr)), not a cached one, so this never needs to remember what was there before. */
	UPROPERTY(EditAnywhere, Category = "Structural|Debug")
	TObjectPtr<UMaterialInterface> DebugHeatmapMaterial;

	/** Toggles bShowDebug and, on every already-registered piece, swaps its mesh to DebugHeatmapMaterial (or back to its own default) to match - the runtime entry point for a debug command, unlike the EditAnywhere bShowDebug above which only takes effect at the next recompute. */
	UFUNCTION(BlueprintCallable, Category = "Structural|Debug")
	void SetShowDebug(bool bShow);

	/** Draws the pending collapse queue on screen (piece name + seconds to release) every tick - see TickComponent. Independent of bShowDebug, which is the stability heatmap. */
	UPROPERTY(EditAnywhere, Category = "Structural|Debug")
	bool bShowCollapseQueueDebug = false;

	/** Runtime entry point for a debug command, same pattern as SetShowDebug. */
	UFUNCTION(BlueprintCallable, Category = "Structural|Debug")
	void SetShowCollapseQueueDebug(bool bShow) { bShowCollapseQueueDebug = bShow; }

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

	/** Adds Piece's sockets to the neighbor table (and, for a grid-anchored piece, occupies its slot), links it to whatever matches, then marks the plot's stability dirty (recomputed on the next TickComponent, see MarkDirty). */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	void RegisterPiece(ABuildablePiece* Piece);

	/**
	 *  Removes Piece's sockets from the neighbor table (and frees its occupancy slot, if any), then
	 *  marks the plot's stability dirty - this is what lets removing one piece cascade to whatever
	 *  depended on it, since anything that loses its only path to an anchor collapses as part of the
	 *  next recompute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	void UnregisterPiece(ABuildablePiece* Piece);

	/**
	 *  Horizontal unit direction away from the centroid of Piece's still-registered neighbours (any
	 *  linked socket, same adjacency as ComputeHopDistances) - GDD §7.7 Layer A, the collapse
	 *  direction bias. Called from BeginCollapse just before the piece unregisters itself, so its
	 *  neighbours (whether or not they're separately queued to fall later) are still in the table.
	 *  ZeroVector if Piece has no registered neighbour left to be "away from".
	 */
	UFUNCTION(BlueprintPure, Category = "Structural")
	FVector ComputeAwayFromNeighbourCentroid(const ABuildablePiece* Piece) const;

	/**
	 *  GDD §7.6, D29: would removing Candidate (a dry run, nothing is actually unregistered) drop
	 *  some OTHER registered piece's stability below Rules->CollapseThreshold, or off its anchor
	 *  chain entirely? Reuses RelaxStability with a scratch map so the real per-piece Stability
	 *  values are never touched. True means Candidate is a genuine structural bottleneck, not just
	 *  "something rests on it with another path still available".
	 */
	UFUNCTION(BlueprintPure, Category = "Structural")
	bool WouldBeBottleneck(const ABuildablePiece* Candidate) const;

	/**
	 *  Whole-structure dry run of the collapse check: fills OutUnstable with every registered non-anchor
	 *  piece whose stability is under Rules->CollapseThreshold, i.e. exactly the pieces a real
	 *  RecomputeStability would queue to collapse, without touching any piece's real Stability or
	 *  queuing anything (same scratch-map relaxation as WouldBeBottleneck). For the house generator,
	 *  called once after a whole house was emitted in a single frame: at that point every piece's
	 *  cached Stability is still 0 (RegisterPiece only marks the graph dirty, the real recompute runs
	 *  on the next tick), so asking per piece while spawning would read stale zeros from the
	 *  neighbours and flag everything. Relaxing the full registered set gives the answer the real
	 *  recompute will give a tick later.
	 */
	void FindUnstablePieces(TArray<ABuildablePiece*>& OutUnstable) const;

	/**
	 *  Every piece the graph holds right now: the ones placed through APlot::PlacePiece or registered by hand, not a piece
	 *  that already started to fall, and not a child actor (door leaf, window glass), which never registers. The home
	 *  snapshot reads this list instead of walking the world, so it sees exactly what a restore would put back.
	 */
	const TArray<TObjectPtr<ABuildablePiece>>& GetRegisteredPieces() const { return RegisteredPieces; }

	/**
	 *  Immediate D29 check for the "once when the piece enters Critical" case (HandleDurabilityStateChanged's
	 *  transition to Critical) - RecomputeStability's own end-of-batch scan (see RecomputeStability)
	 *  covers "again after each batch of destructions" for every other Critical load-bearing piece.
	 *  No-op if Piece isn't actually a bottleneck, or is already queued.
	 */
	void NotifyEnteredCritical(ABuildablePiece* Piece);

	/**
	 *  Occupancy table, separate from structural stability: is SlotType's slot at Cell free? Always
	 *  true for SlotType == Legacy (nothing is ever tracked there). RoofStep disambiguates a Cell
	 *  slot a roof piece occupies while climbing a slope (gable fill stacking towards the ridge) -
	 *  several such pieces share the same spatial Cell (see ABuildablePiece::GetStructuralCell,
	 *  which never factors RoofStep in - sockets rely on that to find each other at the same cell),
	 *  so occupancy needs its own, separate way to tell them apart. Always 0 for anything else.
	 *
	 *  bRoofLayer only matters for the Cell slot: a roof tile lives in its own layer there (see IsRoofLayerPiece),
	 *  so a ceiling slab and the first row of roof tiles resting on the same walls, both at (cell, level, step 0),
	 *  do not fight over one slot. Pass true when asking about a roof tile, false (the default) for a floor piece.
	 */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	bool IsSlotFree(EBuildAnchor SlotType, const FIntVector& Cell, int32 RoofStep = 0, bool bRoofLayer = false) const;

	/** The piece occupying SlotType's slot at Cell (and RoofStep, and roof layer - see IsSlotFree), or nullptr if free. */
	UFUNCTION(BlueprintCallable, Category = "Structural")
	ABuildablePiece* GetPieceAt(EBuildAnchor SlotType, const FIntVector& Cell, int32 RoofStep = 0, bool bRoofLayer = false) const;

private:

	static FIntVector RotateCellOffset(const FIntVector& Offset, int32 QuarterTurns);

	/**
	 *  Whether two socket types pair up at all, and if so which loss tier the link uses: vertical
	 *  (cheap, resting on top of something), roof-lateral (a roof tile's edge touching another roof
	 *  tile's matching edge - cheaper than a generic horizontal contact, since there's no framing/
	 *  truss piece underneath a roof skin to anchor every tile individually), or generic horizontal
	 *  (OutIsVertical and OutIsRoofLateral both false - a wall or floor touching sideways). A fixture
	 *  hanging under a slab (CeilingMount on FloorBottom) is reported vertical, and also hanging when
	 *  OutIsHanging is passed: that link costs no stability at all. Callers that only need to know whether
	 *  two pieces are neighbours leave OutIsHanging out.
	 */
	static bool AreSocketsLinked(EStructuralSocketType A, EStructuralSocketType B, bool& OutIsVertical, bool& OutIsRoofLateral, bool* OutIsHanging = nullptr);

	void RecomputeStability();

	/**
	 *  Visual fix near exterior walls (D21, nothing to do with stability): works out which sides of a floor slab or a
	 *  stair flight touch an exterior wall, from the Edge occupancy table, and tells the piece to pull back from them
	 *  (UStructuralRules::SlabWallRetreat / StairWallRetreat). A slab at storey N overlaps the walls of storey N - 1
	 *  (a wall is 300cm, it runs up through the slab's thickness); a flight overlaps the walls of its own storey.
	 *  A side counts only if every edge segment along it holds an exterior wall. A removed wall frees its slot first,
	 *  so the same call gives the piece its full size back. Piece's footprint comes from its mesh bounds, turned by
	 *  its orientation, so no piece class has a hard-coded size.
	 */
	void RefreshWallClearance(ABuildablePiece* Piece);

	/** RefreshWallClearance for every slab and stair flight close enough to Wall (an exterior wall piece) to touch it. Called when a wall is registered or removed. */
	void RefreshWallClearanceAround(const ABuildablePiece* Wall);

	/**
	 *  Shared fixed-point relaxation core behind RecomputeStability and WouldBeBottleneck's dry run.
	 *  DryRunResult == nullptr: the real pass - reads/writes each piece's own Stability field
	 *  directly (GetStability/SetStability), exactly as before this was factored out. DryRunResult
	 *  != nullptr: a scratch pass - reads/writes that map instead, and ExcludedPiece is treated as
	 *  if it had never been registered (skipped whenever it would otherwise be read as a neighbour's
	 *  support source), so nothing here ever mutates a real piece's Stability. ExcludedPiece is only
	 *  ever non-null together with a DryRunResult.
	 */
	void RelaxStability(const ABuildablePiece* ExcludedPiece, TMap<const ABuildablePiece*, float>* DryRunResult) const;

	/** Re-checks every registered load-bearing piece currently Critical against WouldBeBottleneck, adding newly-found bottlenecks to CompromisedQueue and cancelling entries that no longer qualify - GDD §7.6, D29's "again after each batch of destructions". Called once per RecomputeStability. */
	void RefreshCompromisedQueue();

	/** Adds Piece to CompromisedQueue with a fresh random fail time and fires its warning FX, unless it's already queued. */
	void StartCompromised(ABuildablePiece* Piece);

	/** Removes Piece from CompromisedQueue (if present) and cancels its warning FX - D29's "dependants destroyed first" un-Compromise case. */
	void CancelCompromised(ABuildablePiece* Piece);

	/** BFS hop distance (graph adjacency via SocketsByCell, any link direction) from RecentlyVacatedCells to every registered piece - used to stagger the collapse queue. Pieces unreachable from a vacated cell are absent from the result. */
	TMap<ABuildablePiece*, int32> ComputeHopDistances() const;

	/** Appends newly-unstable pieces (ToCollapse) to CollapseQueue with a release time derived from hop distance, compressing this batch's hop spread to fit Rules->MaxCascadeSpread if needed (BaseDelay is added after compression, untouched). */
	void EnqueueCollapse(const TArray<ABuildablePiece*>& ToCollapse, const TMap<ABuildablePiece*, int32>& HopDistances);

	/** Flags a pending RecomputeStability for the next TickComponent, instead of recomputing inline - see the class comment. */
	void MarkDirty() { bRecomputeDirty = true; }

	void OccupySlot(EBuildAnchor SlotType, const FIntVector& Cell, ABuildablePiece* Piece);
	void FreeSlotsFor(ABuildablePiece* Piece);
	TMap<FIntVector, TObjectPtr<ABuildablePiece>>* GetOccupancyMap(EBuildAnchor SlotType);
	const TMap<FIntVector, TObjectPtr<ABuildablePiece>>* GetOccupancyMap(EBuildAnchor SlotType) const;

	/**
	 *  Folds RoofStep into Cell for the occupancy table only (never for GetStructuralCell/socket matching - see IsSlotFree) - same
	 *  large-offset trick as elsewhere, safely outside any real BuildLevel's range. The roof layer adds 500, which no multiple
	 *  of 1000 (a RoofStep) can reach, so a roof tile and a floor piece never share a key.
	 */
	static FIntVector MakeOccupancyKey(const FIntVector& Cell, int32 RoofStep, bool bRoofLayer = false) { return Cell + FIntVector(0, 0, RoofStep * 1000 + (bRoofLayer ? 500 : 0)); }

	/** A roof tile that sits in a Cell slot: the only pieces that live in the Cell table's roof layer. Edge roof pieces (gables) have their own table and no layer. */
	static bool IsRoofLayerPiece(const ABuildablePiece* Piece);

	bool bRecomputeDirty = false;

	UPROPERTY()
	TArray<TObjectPtr<ABuildablePiece>> RegisteredPieces;

	TMap<FIntVector, TArray<FRegisteredSocket>> SocketsByCell;

	TMap<FIntVector, TObjectPtr<ABuildablePiece>> FloorSlotOccupancy;
	TMap<FIntVector, TObjectPtr<ABuildablePiece>> EdgeSlotOccupancy;
	TMap<FIntVector, TObjectPtr<ABuildablePiece>> VertexSlotOccupancy;
	TMap<FIntVector, TObjectPtr<ABuildablePiece>> CeilingSlotOccupancy;

	/**
	 *  Cells vacated by UnregisterPiece since the last RecomputeStability - seeds the hop-distance BFS
	 *  for the collapse queue (§7.7). Cells, not piece pointers: the piece that triggered the
	 *  unregister may already be mid-Destroy() by the time RecomputeStability runs (e.g. Demolish()'s
	 *  synchronous Unregister-then-Destroy), so nothing here ever dereferences it. Cleared once
	 *  consumed at the top of RecomputeStability.
	 */
	TSet<FIntVector> RecentlyVacatedCells;

	/** One pending, staggered BeginCollapse() call - see EnqueueCollapse/TickComponent. */
	struct FQueuedCollapse
	{
		TWeakObjectPtr<ABuildablePiece> Piece;
		float ReleaseTime = 0.f;
	};

	/** Pieces found unstable by RecomputeStability, waiting for their staggered release time instead of collapsing immediately - see EnqueueCollapse. Drained every tick in TickComponent, independent of bRecomputeDirty. */
	TArray<FQueuedCollapse> CollapseQueue;

	/** One pending self-failure for a Compromised piece (GDD §7.6, D29) - see StartCompromised/CancelCompromised/RefreshCompromisedQueue. */
	struct FCompromisedEntry
	{
		TWeakObjectPtr<ABuildablePiece> Piece;
		float FailTime = 0.f;
	};

	/** Critical, load-bearing pieces currently found to be a genuine structural bottleneck (WouldBeBottleneck), each counting down to its own BeginCollapse() - drained every tick in TickComponent, same as CollapseQueue. */
	TArray<FCompromisedEntry> CompromisedQueue;
};
