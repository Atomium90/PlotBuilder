#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Plot.generated.h"

class ABuildablePiece;
class UBoxComponent;
class UStructuralSupportComponent;

/**
 *  Minimal buildable area: a resizable box volume other systems can query with IsLocationInside.
 *  No ownership/cost tracking yet - just enough for Build Mode to keep placement inside a defined
 *  plot. Draws its own bounds every tick as a debug box so the area is visible in-game without any
 *  extra content (mesh/material).
 */
UCLASS()
class APlot : public AActor
{
	GENERATED_BODY()

public:

	APlot();

	UBoxComponent* GetBoundsVolume() const { return BoundsVolume; }

	UFUNCTION(BlueprintPure, Category = "Plot")
	UStructuralSupportComponent* GetStructuralSupport() const { return StructuralSupport; }

	/** Whether WorldLocation falls within this plot's bounds (axis-aligned box test, inclusive with a small tolerance so a vertex/edge exactly on the boundary still counts). */
	UFUNCTION(BlueprintCallable, Category = "Plot")
	bool IsLocationInside(const FVector& WorldLocation) const;

	/** World point a grid-anchored piece's (0,0) cell resolves to - the plot's own pivot, which authoring convention places at the center of a cell (see claude/Kit greybox specs des pieces et plan de remplacement.md). */
	UFUNCTION(BlueprintPure, Category = "Plot")
	FVector GetGridOrigin() const { return GetActorLocation(); }

	/** The plot's ground surface Z - the plot's own pivot, by the same authoring convention (checked on BeginPlay, see the warning logged there if it doesn't hold). Assumes a flat plot. */
	UFUNCTION(BlueprintPure, Category = "Plot")
	float GetGroundZ() const { return GetActorLocation().Z; }

	/**
	 *  Z of the horizontal plane a grid piece sits on: the foundation's pivot is 20cm above the ground
	 *  (it's buried 10cm of its 30cm thickness) and every other piece at a given level shares that Z,
	 *  one FloorHeight per BuildLevel. RoofStep adds a second, independent term (RoofStepHeight per
	 *  step) for a roof piece climbing a slope or stacking a gable infill. VerticalOffset is the
	 *  piece's own authored offset (ABuildablePiece::GetVerticalOffset). The one copy of this formula:
	 *  build mode's aiming ray and PlacePiece both call it.
	 */
	UFUNCTION(BlueprintPure, Category = "Plot")
	float GetPlaneZ(int32 BuildLevel, int32 RoofStep, float VerticalOffset) const;

	/**
	 *  The one way to put a grid piece into this plot, shared by build mode, the house generator and
	 *  (later) the home restore: spawns PieceClass on GridCoord (X, Y, BuildLevel) turned by
	 *  QuarterTurns * 90 degrees, marks it Placed and registers it in the structural graph.
	 *  A piece spawned any other way is invisible to the graph (nothing holds it up, nothing can
	 *  collapse it), so everything that creates pieces goes through here.
	 *
	 *  Refuses (returns nullptr and logs) for a Legacy-anchored class (not a grid piece), a location
	 *  outside the plot, or an occupied slot. Deliberately no physics overlap test: that is the build
	 *  mode ghost's job (it has a player to keep out of walls), a generator works on an empty plot.
	 *  RoofStep only counts for a RoofTile and is ignored for any other class.
	 */
	ABuildablePiece* PlacePiece(TSubclassOf<ABuildablePiece> PieceClass, const FIntVector& GridCoord, uint8 QuarterTurns, int32 RoofStep = 0, const FVector& Scale = FVector::OneVector);

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	bool bShowDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	FColor DebugColor = FColor::Cyan;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoundsVolume;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStructuralSupportComponent> StructuralSupport;
};
