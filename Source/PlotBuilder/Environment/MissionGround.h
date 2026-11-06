#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Interval.h"
#include "MissionGround.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;

/**
 *  One family of things the ground scatters around the plot: trees, bushes, rocks, site clutter. Which meshes, how
 *  many and how far from the plot are data; the placement itself is the ground's, seeded.
 */
USTRUCT(BlueprintType)
struct FGroundScatter
{
	GENERATED_BODY()

	/** Name in the log and the on-screen summary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FName Label = NAME_None;

	/** One of these is picked at random for each instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	/** How many to place. Fewer if the room runs out (Spacing, the kept-clear zones). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	int32 Count = 20;

	/** Closest to the plot's edge, in cm. Inside the plot is never used. Ignored when EdgeBand is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	float MinDistanceFromPlot = 1400.f;

	/** Farthest from the plot's edge, in cm. 0 = no limit (the whole lot). Ignored when EdgeBand is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	float MaxDistanceFromPlot = 0.f;

	/**
	 *  The treeline: above 0, the instances go only in a band this wide along the inside of the lot's edge (the two
	 *  distances above are then ignored). A dense family there hides the invisible walls and gives the lot a border.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	float EdgeBand = 0.f;

	/** Smallest distance between two instances of this family, centre to centre, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	float Spacing = 400.f;

	/** Uniform scale picked between the two values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FFloatInterval ScaleRange = FFloatInterval(0.85f, 1.25f);

	/** 0 = spread evenly, up to 1 = a few dense groves with empty ground between. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0", ClampMax = "1"))
	float Clustering = 0.f;

	/** Added to the height of the ground under the instance, in cm (negative sinks it a little). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	float ZOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bRandomYaw = true;

	/** The instances block the player and the props (trees, rocks). The mesh needs simple collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bCollision = false;
};

/**
 *  A rectangle of the ground in world space, turned by Yaw, with its signed distance: negative inside, positive
 *  outside. The ground reads these from the plot, the zones and the AFlatZone actors of the level.
 */
struct FGroundRegion
{
	FString Name;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Half = FVector2D::ZeroVector;
	double Yaw = 0.0;
	bool bRoad = false;
	bool bKeepClear = true;

	/** Distance to the rectangle's edge (negative inside). OutLocal, if given, receives the point in the rectangle's own axes. */
	double SignedDistance(const FVector2D& Point, FVector2D* OutLocal = nullptr) const;
};

/**
 *  The ground of a mission level (GDD §13.2): a square of sand and dirt built at the start of the level from the
 *  seed, flat under everything that must be flat and rolling beyond it, with the trees, rocks and site clutter of
 *  the lot scattered on it. Same seed, same ground.
 *
 *  What stays flat is read from the level, so moving a zone moves the flat ground under it:
 *   - the plot, plus PlotMargin all around (the room to work and for debris),
 *   - every drop zone and protected zone, plus ZonePadding,
 *   - every player start, within StartRadius,
 *   - every AFlatZone (the road, the yard between the road and the plot, anything else).
 *  The relief fades in over BlendDistance beyond all of that. Nothing else is needed: add a protected zone and the
 *  ground flattens under it on the next start.
 *
 *  The ground material gets, per vertex: UV0 = world position in metres (tile textures with it), vertex colour
 *  R = sand weight (0 dirt, 1 sand), G = trampled weight (1 in the flat areas), UV1 = position in the nearest road
 *  zone's own axes in metres, UV2.x = signed distance to the nearest road zone's edge in metres (negative on the
 *  road). Straight edges stay sharp at any grid size because that distance is linear.
 *
 *  The lot is closed by four invisible walls (bBlockEdges), so nothing falls into the void beyond it, and a scatter row
 *  with an EdgeBand puts a treeline along the inside of the edge to hide them.
 *
 *  Nothing is built in the editor (the level shows the flat zones and the lot outline): the mission director calls
 *  Generate at the start of the level, with the same seed as the house. pb.GenerateGround rebuilds it at will.
 */
UCLASS()
class AMissionGround : public AActor
{
	GENERATED_BODY()

public:

	AMissionGround();

	/** Side of the square lot, in cm, centred on this actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "2000"))
	float Size = 10000.f;

	/** Distance between two vertices, in cm. Smaller = smoother relief and a heavier mesh (10 000 / 100 = about 20 000 triangles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground", meta = (ClampMin = "50", ClampMax = "400"))
	float CellSize = 100.f;

	/** Single material of the ground (see the class comment for what it receives). Empty = the engine's default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	TObjectPtr<UMaterialInterface> GroundMaterial;

	/** Height of the highest dune above the flat ground, in cm. 0 = a flat lot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Relief", meta = (ClampMin = "0"))
	float ReliefHeight = 200.f;

	/** Length of a dune along the wind, in cm (across it they are about half as long). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Relief", meta = (ClampMin = "500"))
	float DuneLength = 3000.f;

	/** Direction the dunes run, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Relief")
	float DuneAngle = 24.f;

	/** Flat ground around the plot, in cm, beyond its edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Flat", meta = (ClampMin = "0"))
	float PlotMargin = 1000.f;

	/** Flat ground around each drop zone and protected zone, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Flat", meta = (ClampMin = "0"))
	float ZonePadding = 400.f;

	/** Flat ground around each player start, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Flat", meta = (ClampMin = "0"))
	float StartRadius = 600.f;

	/** Distance over which the flat ground turns into relief, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Flat", meta = (ClampMin = "1"))
	float BlendDistance = 2000.f;

	/** Nothing is scattered closer than this to a kept-clear zone or to the plot itself, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Scatter", meta = (ClampMin = "0"))
	float ScatterClearance = 300.f;

	/** The scattered families: trees, bushes, rocks, site clutter, the treeline. Add a row and a mesh list for another one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Scatter")
	TArray<FGroundScatter> Scatter;

	/** Four invisible walls along the edges of the lot: the player, the props and the debris cannot leave it (or fall into the void beyond). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Border")
	bool bBlockEdges = true;

	/** Height of the border walls above the ground, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground|Border", meta = (ClampMin = "500"))
	float WallHeight = 3000.f;

	/** Build the ground when the level starts, with DefaultSeed. Off in a mission level: the director does it with the mission's seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	bool bGenerateOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	int32 DefaultSeed = 1;

	/** Builds (or rebuilds) the ground and its scatter from the seed. */
	UFUNCTION(BlueprintCallable, Category = "Ground")
	void Generate(int32 Seed);

	/** Removes the mesh and everything scattered. */
	UFUNCTION(BlueprintCallable, Category = "Ground")
	void Clear();

	/** Height of the ground at a world position (X, Y), in world cm. The flat level before the ground is built. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ground")
	float GetHeightAt(FVector2D WorldXY) const;

	/** Draws the flat zones, the kept-clear ones (orange) and the roads (blue) over the ground, for as long as it is on. */
	void SetShowDebug(bool bShow);

	/** What the last Generate built, for the log and the debug command. */
	const FString& GetSummary() const { return Summary; }

	/** The ground of this world, or nullptr. */
	static AMissionGround* Find(const UObject* WorldContext);

protected:

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

private:

	void GatherRegions();
	void ScatterAll(int32 Seed);

	/** Builds the four border walls (bBlockEdges). Runtime only, like the ground itself. */
	void BuildEdgeWalls();

	double FlatDistance(const FVector2D& Point) const;

	/** Relief before it is scaled to ReliefHeight, already faded by the distance to the flat ground. OutMask is 0 on flat ground, 1 in full relief. */
	double ReliefAt(const FVector2D& Point, double& OutMask) const;
	double Fbm(double X, double Y, int32 Octaves) const;
	bool IsClearOfZones(const FVector2D& Point) const;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> Mesh;

	/** The lot's outline in the editor (hidden in game). */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Footprint;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ScatterComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> EdgeWalls;

	TArray<FGroundRegion> Regions;
	FGroundRegion PlotRegion;
	bool bHasPlot = false;
	bool bBuilt = false;
	bool bShowDebug = false;

	FVector2D NoiseOffset = FVector2D::ZeroVector;
	double RawScale = 0.0;
	double BaseZ = 0.0;
	FString Summary;
};
