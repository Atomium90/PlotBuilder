#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Contracts/MissionBinding.h"
#include "Plot.generated.h"

class ABuildablePiece;
class ADropZone;
class AProtectedZone;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UPaintPalette;
class UStaticMesh;
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

	/** The plot's footprint: a flat box on the ground, its X and Y extents are the plot's size. The height is VolumeHeight, not this box's Z. */
	UBoxComponent* GetBoundsVolume() const { return BoundsVolume; }

	UFUNCTION(BlueprintPure, Category = "Plot")
	UStructuralSupportComponent* GetStructuralSupport() const { return StructuralSupport; }

	/** Whether WorldLocation falls within this plot's volume: the footprint, from the ground up to VolumeHeight (axis-aligned box test, inclusive with a small tolerance so a vertex/edge exactly on the boundary still counts). */
	UFUNCTION(BlueprintCallable, Category = "Plot")
	bool IsLocationInside(const FVector& WorldLocation) const;

	/** World point a grid-anchored piece's (0,0) cell resolves to - the plot's own pivot, which authoring convention places at the center of a cell */
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

	/** The target and must-stay sets of the contract being played (GDD §13.3: the plot holds the contract binding). Empty outside a mission. */
	const FMissionSets& GetMissionSets() const { return MissionSets; }

	/** Replaces the mission sets. Called by AMissionDirector once the house is built and the contract bound. */
	void SetMissionSets(FMissionSets&& NewSets) { MissionSets = MoveTemp(NewSets); }

	/** Called by an AProtectedZone when it starts: the plot keeps the list for the evaluator. */
	void RegisterProtectedZone(AProtectedZone* Zone);

	/** Called by an AProtectedZone that is switched off for the mission: it leaves the list, so the report ignores it. */
	void UnregisterProtectedZone(AProtectedZone* Zone);

	/** Called by the client ADropZone when it starts. A second one replaces the first (one client zone per mission map). */
	void RegisterClientDropZone(ADropZone* Zone);

	const TArray<TObjectPtr<AProtectedZone>>& GetProtectedZones() const { return ProtectedZones; }
	ADropZone* GetClientDropZone() const { return ClientDropZone; }

	/**
	 *  A piece of the house that was falling has just broken (it hit something, or its fall ran out): where it lies is where it
	 *  does its damage. The plot looks up what the piece weighs in the mission sets and tells every protected zone, which
	 *  counts it if it is in its volume. Does nothing for something that is not an item of the house (the leaf of a door), and
	 *  outside a mission, where the sets are empty. Called by ABuildablePiece, which does not know the zones.
	 */
	void NotifyPieceLanded(AActor* Piece);

	/**
	 *  Height of the plot's volume above the ground (the pivot), in cm: the house, its roof and everything built on it must fit
	 *  under it. Nothing is built below the pivot. The BoundsVolume box only gives the footprint (X and Y); its Z is reset on
	 *  every construction, the VolumeGuide box shows the whole volume.
	 */
	UPROPERTY(EditAnywhere, Category = "Plot", meta = (ClampMin = "100"))
	float VolumeHeight = 3000.f;

	/** Draws the plot's volume as a debug box every tick. The mission director turns it off: the caution tape marks the plot there. */
	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	bool bShowDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	FColor DebugColor = FColor::Cyan;

	/** Whether the plot's perimeter is marked with poles and caution tape (GDD §13.2). Needs PoleMesh and/or TapeMesh. */
	UPROPERTY(EditAnywhere, Category = "Plot|Tape")
	bool bShowTape = true;

	/** One pole, pivot at its foot. Poles stand on the perimeter, at every corner and at most PoleSpacing apart. Empty = no poles. */
	UPROPERTY(EditAnywhere, Category = "Plot|Tape")
	TObjectPtr<UStaticMesh> PoleMesh;

	/** The tape between two poles: a bar lying along its local X with its pivot at its middle. A gap is filled with as many copies as fit, each stretched a little to close it, so a striped mesh keeps its stripes. The engine cube works too (the look is then the material's). Empty = no tape. */
	UPROPERTY(EditAnywhere, Category = "Plot|Tape")
	TObjectPtr<UStaticMesh> TapeMesh;

	/** Longest gap between two poles along a side, in cm. A side is cut into equal gaps no longer than this. */
	UPROPERTY(EditAnywhere, Category = "Plot|Tape", meta = (ClampMin = "50"))
	float PoleSpacing = 300.f;

	/** Height of the middle of the tape above the ground, in cm. */
	UPROPERTY(EditAnywhere, Category = "Plot|Tape", meta = (ClampMin = "0"))
	float TapeHeight = 100.f;

	/** Size of the tape across its length, in cm: how tall the band is, and how thick. The mesh is scaled to it, so the default fits SM_CautionTape (12 x 0.5 cm) and a 100 cm cube alike. */
	UPROPERTY(EditAnywhere, Category = "Plot|Tape", meta = (ClampMin = "0.1"))
	FVector2D TapeSize = FVector2D(12.f, 0.5f);

	/** The colours the paint brush offers on this plot, and the ones the home restore reads back. Empty = the brush has nothing to apply. Set once on the plot Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Plot|Paint")
	TObjectPtr<UPaintPalette> PaintPalette;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:

	/** Lays the poles and the tape out along the bounds (at the ground, whatever the plot's scale): in the editor on every change, and at spawn. */
	void RebuildTape();

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> TapePoles;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> TapeBars;

	UPROPERTY()
	TArray<TObjectPtr<AProtectedZone>> ProtectedZones;

	UPROPERTY()
	TObjectPtr<ADropZone> ClientDropZone;

	FMissionSets MissionSets;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoundsVolume;

	/** Shows the whole volume (footprint x VolumeHeight) from the ground up, in the editor and in the debug draw. Never collides. */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> VolumeGuide;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStructuralSupportComponent> StructuralSupport;
};
