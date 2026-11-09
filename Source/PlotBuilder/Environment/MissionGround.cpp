#include "Environment/MissionGround.h"
#include "Debug/DebugDisplay.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Environment/DropZone.h"
#include "Environment/FlatZone.h"
#include "Environment/Plot.h"
#include "Environment/ProtectedZone.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PlotBuilder.h"
#include "ProceduralMeshComponent.h"

namespace
{
	constexpr int32 GroundSummaryMessageKey = 7301;

	/** The footprint of a box component, widened by Pad on every side. Only the yaw is kept: the ground is flat. */
	FGroundRegion RegionFromBox(const FString& Name, const UBoxComponent* Box, double Pad)
	{
		FGroundRegion Region;
		Region.Name = Name;

		const FVector Location = Box->GetComponentLocation();
		const FVector Extent = Box->GetScaledBoxExtent();

		Region.Center = FVector2D(Location.X, Location.Y);
		Region.Half = FVector2D(Extent.X + Pad, Extent.Y + Pad);
		Region.Yaw = FMath::DegreesToRadians(static_cast<double>(Box->GetComponentRotation().Yaw));
		return Region;
	}

	/** The footprint of an actor: its box component if it has one (turned like it), otherwise the box around all its parts. */
	FGroundRegion RegionFromActor(const AActor* Actor, double Pad)
	{
		if (const UBoxComponent* Box = Actor->FindComponentByClass<UBoxComponent>())
		{
			return RegionFromBox(Actor->GetName(), Box, Pad);
		}

		FGroundRegion Region;
		Region.Name = Actor->GetName();

		const FBox Bounds = Actor->GetComponentsBoundingBox(true);

		if (Bounds.IsValid)
		{
			const FVector Centre = Bounds.GetCenter();
			const FVector Extent = Bounds.GetExtent();
			Region.Center = FVector2D(Centre.X, Centre.Y);
			Region.Half = FVector2D(Extent.X + Pad, Extent.Y + Pad);
		}

		return Region;
	}
}

double FGroundRegion::SignedDistance(const FVector2D& Point, FVector2D* OutLocal) const
{
	const FVector2D Offset = Point - Center;
	const double CosYaw = FMath::Cos(Yaw);
	const double SinYaw = FMath::Sin(Yaw);

	// The point in the rectangle's own axes: the offset turned by -Yaw.
	const FVector2D Local(Offset.X * CosYaw + Offset.Y * SinYaw, -Offset.X * SinYaw + Offset.Y * CosYaw);

	if (OutLocal)
	{
		*OutLocal = Local;
	}

	const double QX = FMath::Abs(Local.X) - Half.X;
	const double QY = FMath::Abs(Local.Y) - Half.Y;
	const double Outside = FMath::Sqrt(FMath::Square(FMath::Max(QX, 0.0)) + FMath::Square(FMath::Max(QY, 0.0)));
	const double Inside = FMath::Min(FMath::Max(QX, QY), 0.0);
	return Outside + Inside;
}

AMissionGround::AMissionGround()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Complex collision as the simple one: the ground is a static triangle mesh. Cooked on the spot (not on another
	// thread), so the player never stands on a ground whose collision does not exist yet.
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->bUseAsyncCooking = false;
	Mesh->SetCollisionProfileName(FName("BlockAll"));
	Mesh->SetGenerateOverlapEvents(false);

	// No navigation mesh in this project, and an empty section (before Generate) makes the navigation octree warn on every registration.
	Mesh->SetCanEverAffectNavigation(false);

	Footprint = CreateDefaultSubobject<UBoxComponent>(TEXT("Footprint"));
	Footprint->SetupAttachment(Mesh);
	Footprint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Footprint->SetGenerateOverlapEvents(false);
	Footprint->SetLineThickness(8.f);
	Footprint->SetBoxExtent(FVector(5000.f, 5000.f, 10.f));
	Footprint->ShapeColor = FColor(220, 195, 140);

	// The five usual families. Meshes are filled in the editor.
	Scatter.SetNum(5);

	FGroundScatter& Trees = Scatter[0];
	Trees.Label = TEXT("Trees");
	Trees.Count = 55;
	Trees.MinDistanceFromPlot = 1400.f;
	Trees.Spacing = 450.f;
	Trees.Clustering = 0.5f;
	Trees.bCollision = true;

	FGroundScatter& Bushes = Scatter[1];
	Bushes.Label = TEXT("Bushes");
	Bushes.Count = 45;
	Bushes.MinDistanceFromPlot = 700.f;
	Bushes.Spacing = 150.f;
	Bushes.Clustering = 0.35f;
	Bushes.ScaleRange = FFloatInterval(0.8f, 1.4f);

	FGroundScatter& Rocks = Scatter[2];
	Rocks.Label = TEXT("Rocks");
	Rocks.Count = 26;
	Rocks.MinDistanceFromPlot = 700.f;
	Rocks.Spacing = 250.f;
	Rocks.ScaleRange = FFloatInterval(0.6f, 1.8f);
	Rocks.ZOffset = -10.f;
	Rocks.bCollision = true;

	FGroundScatter& Clutter = Scatter[3];
	Clutter.Label = TEXT("Site clutter");
	Clutter.Count = 13;
	Clutter.MinDistanceFromPlot = 280.f;
	Clutter.MaxDistanceFromPlot = 900.f;
	Clutter.Spacing = 220.f;
	Clutter.ScaleRange = FFloatInterval(0.95f, 1.1f);

	// Trees and rocks packed along the inside of the lot's edge: the border the player sees before the invisible wall.
	FGroundScatter& Treeline = Scatter[4];
	Treeline.Label = TEXT("Treeline");
	Treeline.Count = 300;
	Treeline.EdgeBand = 900.f;
	Treeline.Spacing = 280.f;
	Treeline.ScaleRange = FFloatInterval(0.9f, 1.5f);
	Treeline.bCollision = true;
}

AMissionGround* AMissionGround::Find(const UObject* WorldContext)
{
	return Cast<AMissionGround>(UGameplayStatics::GetActorOfClass(WorldContext, AMissionGround::StaticClass()));
}

void AMissionGround::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Footprint->SetBoxExtent(FVector(Size * 0.5f, Size * 0.5f, 10.f));
}

void AMissionGround::BeginPlay()
{
	Super::BeginPlay();

	if (bGenerateOnBeginPlay)
	{
		Generate(DefaultSeed);
	}
}

// ---------------------------------------------------------------- field

double AMissionGround::Fbm(double X, double Y, int32 Octaves) const
{
	// Four octaves of Perlin noise remapped to 0..1, the seed moving the whole pattern.
	double Sum = 0.0;
	double Amplitude = 0.5;
	double Frequency = 1.0;

	for (int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		const FVector2D Sample(X * Frequency + NoiseOffset.X + Octave * 17.31, Y * Frequency + NoiseOffset.Y + Octave * 31.77);
		Sum += Amplitude * (0.5 + 0.5 * FMath::PerlinNoise2D(Sample));
		Amplitude *= 0.5;
		Frequency *= 2.0;
	}

	return Sum;
}

double AMissionGround::FlatDistance(const FVector2D& Point) const
{
	double Nearest = TNumericLimits<double>::Max();

	for (const FGroundRegion& Region : Regions)
	{
		Nearest = FMath::Min(Nearest, FMath::Max(0.0, Region.SignedDistance(Point)));
	}

	return Nearest;
}

double AMissionGround::ReliefAt(const FVector2D& Point, double& OutMask) const
{
	const double Distance = FlatDistance(Point);
	OutMask = FMath::SmoothStep(0.0, FMath::Max(1.0, static_cast<double>(BlendDistance)), Distance);

	if (OutMask <= 0.0)
	{
		return 0.0;
	}

	// Long dunes: noise stretched along the wind direction.
	const double Angle = FMath::DegreesToRadians(static_cast<double>(DuneAngle));
	const double Length = FMath::Max(1.0, static_cast<double>(DuneLength));
	const double Along = (Point.X * FMath::Cos(Angle) + Point.Y * FMath::Sin(Angle)) / Length;
	const double Across = (-Point.X * FMath::Sin(Angle) + Point.Y * FMath::Cos(Angle)) / (0.47 * Length);

	double Noise = Fbm(Along, Across, 4);
	Noise += 0.04 * (Fbm(Point.X / 500.0, Point.Y / 500.0, 2) - 0.4);      // a little grain on the dunes

	// Mostly bumps, with shallow hollows. Rises a little more the farther it is from the flat ground.
	const double Shape = FMath::Max((Noise - 0.35) / 0.35, -0.25);
	return Shape * OutMask * (0.65 + 0.6 * FMath::Min(1.0, Distance / 4500.0));
}

float AMissionGround::GetHeightAt(FVector2D WorldXY) const
{
	if (!bBuilt)
	{
		return GetActorLocation().Z;
	}

	double Mask = 0.0;
	return static_cast<float>(BaseZ + ReliefAt(WorldXY, Mask) * RawScale);
}

bool AMissionGround::IsClearOfZones(const FVector2D& Point) const
{
	for (const FGroundRegion& Region : Regions)
	{
		if (Region.bKeepClear && Region.SignedDistance(Point) < ScatterClearance)
		{
			return false;
		}
	}

	return true;
}

// ---------------------------------------------------------------- what is flat

void AMissionGround::GatherRegions()
{
	Regions.Reset();
	bHasPlot = false;
	BaseZ = GetActorLocation().Z;

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	if (const APlot* Plot = Cast<APlot>(UGameplayStatics::GetActorOfClass(World, APlot::StaticClass())))
	{
		BaseZ = Plot->GetGroundZ();

		if (const UBoxComponent* Volume = Plot->GetBoundsVolume())
		{
			PlotRegion = RegionFromBox(TEXT("Plot"), Volume, 0.0);
			bHasPlot = true;

			// The plot and its margin are flat but not kept clear: site clutter lies around the house.
			FGroundRegion Flat = RegionFromBox(TEXT("Plot margin"), Volume, PlotMargin);
			Flat.bKeepClear = false;
			Regions.Add(Flat);
		}
	}

	for (TActorIterator<ADropZone> It(World); It; ++It)
	{
		Regions.Add(RegionFromActor(*It, ZonePadding));
	}

	for (TActorIterator<AProtectedZone> It(World); It; ++It)
	{
		// A zone the mission does not use asks for nothing: the ground there stays as wild as the rest.
		if (It->IsActive())
		{
			Regions.Add(RegionFromActor(*It, ZonePadding));
		}
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		FGroundRegion Start;
		Start.Name = It->GetName();
		Start.Center = FVector2D(It->GetActorLocation().X, It->GetActorLocation().Y);
		Start.Half = FVector2D(StartRadius, StartRadius);
		Regions.Add(Start);
	}

	for (TActorIterator<AFlatZone> It(World); It; ++It)
	{
		if (const UBoxComponent* Box = It->GetBox())
		{
			FGroundRegion Zone = RegionFromBox(It->GetName(), Box, 0.0);
			Zone.bRoad = It->bRoad;
			Zone.bKeepClear = It->bKeepClear;
			Regions.Add(Zone);
		}
	}
}

// ---------------------------------------------------------------- build

void AMissionGround::Clear()
{
	for (UInstancedStaticMeshComponent* Component : ScatterComponents)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}

	ScatterComponents.Reset();

	for (UBoxComponent* Wall : EdgeWalls)
	{
		if (IsValid(Wall))
		{
			Wall->DestroyComponent();
		}
	}

	EdgeWalls.Reset();
	Mesh->ClearAllMeshSections();
	bBuilt = false;
	Summary.Reset();
}

void AMissionGround::Generate(int32 Seed)
{
	Clear();

	if (!GetWorld())
	{
		return;
	}

	FRandomStream NoiseStream(Seed);
	NoiseOffset = FVector2D(NoiseStream.FRandRange(20.f, 900.f), NoiseStream.FRandRange(20.f, 900.f));

	GatherRegions();

	const int32 Cells = FMath::Clamp(FMath::RoundToInt(Size / FMath::Max(CellSize, 25.f)), 8, 400);
	const int32 Verts = Cells + 1;
	const double Step = static_cast<double>(Size) / Cells;
	const FVector Centre = GetActorLocation();
	const FVector2D Origin(Centre.X - Size * 0.5, Centre.Y - Size * 0.5);

	// Pass 1: the relief everywhere, unscaled. The highest point is then brought to exactly ReliefHeight.
	TArray<double> Relief;
	TArray<float> Mask;
	Relief.SetNumUninitialized(Verts * Verts);
	Mask.SetNumUninitialized(Verts * Verts);

	double Peak = 0.0;

	for (int32 I = 0; I < Verts; ++I)
	{
		for (int32 J = 0; J < Verts; ++J)
		{
			double VertexMask = 0.0;
			const double Value = ReliefAt(FVector2D(Origin.X + I * Step, Origin.Y + J * Step), VertexMask);
			Relief[I * Verts + J] = Value;
			Mask[I * Verts + J] = static_cast<float>(VertexMask);
			Peak = FMath::Max(Peak, Value);
		}
	}

	RawScale = Peak > 1e-3 ? static_cast<double>(ReliefHeight) / Peak : 0.0;
	bBuilt = true;       // from here GetHeightAt answers with the ground that is about to exist

	const auto HeightAtVertex = [&](int32 I, int32 J)
	{
		return BaseZ + Relief[FMath::Clamp(I, 0, Cells) * Verts + FMath::Clamp(J, 0, Cells)] * RawScale;
	};

	// Pass 2: the vertices.
	const FTransform ActorTransform = GetActorTransform();

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FVector2D> UV1;
	TArray<FVector2D> UV2;
	TArray<FVector2D> UV3;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;

	const int32 VertexCount = Verts * Verts;
	Vertices.Reserve(VertexCount);
	Normals.Reserve(VertexCount);
	UV0.Reserve(VertexCount);
	UV1.Reserve(VertexCount);
	UV2.Reserve(VertexCount);
	Colors.Reserve(VertexCount);
	Tangents.Reserve(VertexCount);
	Triangles.Reserve(Cells * Cells * 6);

	double LowestZ = TNumericLimits<double>::Max();
	double HighestZ = TNumericLimits<double>::Lowest();

	for (int32 I = 0; I < Verts; ++I)
	{
		for (int32 J = 0; J < Verts; ++J)
		{
			const FVector2D Point(Origin.X + I * Step, Origin.Y + J * Step);
			const double Z = HeightAtVertex(I, J);
			LowestZ = FMath::Min(LowestZ, Z);
			HighestZ = FMath::Max(HighestZ, Z);

			Vertices.Add(ActorTransform.InverseTransformPosition(FVector(Point.X, Point.Y, Z)));

			// Slope from the neighbours, for the normal and for the tangent along X.
			const double SlopeX = (HeightAtVertex(I + 1, J) - HeightAtVertex(I - 1, J)) / (2.0 * Step);
			const double SlopeY = (HeightAtVertex(I, J + 1) - HeightAtVertex(I, J - 1)) / (2.0 * Step);
			Normals.Add(FVector(-SlopeX, -SlopeY, 1.0).GetSafeNormal());
			Tangents.Add(FProcMeshTangent(FVector(1.0, 0.0, SlopeX).GetSafeNormal(), false));

			// UV0: world position in metres, so a texture tiles the same way whatever the grid.
			UV0.Add(FVector2D(Point.X / 100.0, Point.Y / 100.0));

			// The nearest road: position in its own axes (UV1) and signed distance to its edge (UV2.x), in metres.
			double RoadDistance = 50.0;
			FVector2D RoadLocal = FVector2D::ZeroVector;

			for (const FGroundRegion& Region : Regions)
			{
				if (!Region.bRoad)
				{
					continue;
				}

				FVector2D Local;
				const double Distance = Region.SignedDistance(Point, &Local) / 100.0;

				if (Distance < RoadDistance)
				{
					RoadDistance = Distance;
					RoadLocal = Local / 100.0;
				}
			}

			UV1.Add(RoadLocal);
			UV2.Add(FVector2D(FMath::Clamp(RoadDistance, -50.0, 50.0), 0.0));

			// R: sand against dirt, from a slow noise that favours sand on the dunes. G: how trampled, 1 on the flat ground.
			const double HeightRatio = ReliefHeight > 0.f ? Relief[I * Verts + J] * RawScale / ReliefHeight : 0.0;
			const double SandNoise = Fbm(Point.X / 2500.0 + 10.0, Point.Y / 2500.0 - 5.0, 3) + 0.04 * HeightRatio;
			const double Sand = FMath::SmoothStep(0.34, 0.56, SandNoise);
			const double Trampled = 1.0 - Mask[I * Verts + J];

			Colors.Add(FColor(static_cast<uint8>(Sand * 255.0), static_cast<uint8>(Trampled * 255.0), 0, 255));
		}
	}

	// Two triangles per cell. The winding decides which side is the front: it must face up, or the ground is culled when seen
	// from above and its collision normals point down (the player then stays in the falling state, blocked but never landing).
	// I runs along X and J along Y, so (V00, V01, V10) is the order that faces +Z.
	for (int32 I = 0; I < Cells; ++I)
	{
		for (int32 J = 0; J < Cells; ++J)
		{
			const int32 V00 = I * Verts + J;
			const int32 V10 = (I + 1) * Verts + J;
			const int32 V11 = (I + 1) * Verts + J + 1;
			const int32 V01 = I * Verts + J + 1;

			Triangles.Add(V00);
			Triangles.Add(V01);
			Triangles.Add(V10);

			Triangles.Add(V10);
			Triangles.Add(V01);
			Triangles.Add(V11);
		}
	}

	Mesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, UV1, UV2, UV3, Colors, Tangents, true);
	Mesh->SetMaterial(0, GroundMaterial);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	int32 RoadCount = 0;

	for (const FGroundRegion& Region : Regions)
	{
		RoadCount += Region.bRoad ? 1 : 0;
	}

	Summary = FString::Printf(TEXT("Ground: seed %d, %d x %d cells (%d triangles), height %.0f to %.0f cm above the plot, %d flat region(s) of which %d road(s)%s"),
		Seed, Cells, Cells, Cells * Cells * 2, LowestZ - BaseZ, HighestZ - BaseZ, Regions.Num(), RoadCount,
		bHasPlot ? TEXT("") : TEXT(". No plot found: nothing is kept flat for it"));

	BuildEdgeWalls();
	Summary += bBlockEdges ? FString::Printf(TEXT("\nBorder: %d invisible walls, %.0f cm high"), EdgeWalls.Num(), WallHeight) : FString(TEXT("\nBorder: walls off, the lot's edge is open"));

	ScatterAll(Seed);       // appends its own line to the summary

	if (!GroundMaterial)
	{
		Summary += TEXT("\nNo GroundMaterial set: the engine's default is used.");
	}

	UE_LOG(LogPlotBuilder, Log, TEXT("%s"), *Summary);

	if (GEngine)
	{
		PBDebug::Message(GroundSummaryMessageKey, 15.f, FColor::Green, Summary);
	}
}

void AMissionGround::BuildEdgeWalls()
{
	if (!bBlockEdges)
	{
		return;
	}

	// Thick enough that nothing tunnels through at speed, and sunk below the ground so a fall through the border is caught too.
	constexpr float Thickness = 400.f;
	constexpr float Depth = 1500.f;

	const float Half = Size * 0.5f;
	const float HalfHeight = (WallHeight + Depth) * 0.5f;
	const float CentreZ = static_cast<float>(BaseZ - GetActorLocation().Z) + (WallHeight - Depth) * 0.5f;
	const float Reach = Half + Thickness;

	// In the actor's own axes: +X, -X, +Y, -Y. Each wall overlaps its neighbours at the corners.
	const FVector Offsets[4] = {
		FVector(Half + Thickness * 0.5f, 0.f, CentreZ), FVector(-Half - Thickness * 0.5f, 0.f, CentreZ),
		FVector(0.f, Half + Thickness * 0.5f, CentreZ), FVector(0.f, -Half - Thickness * 0.5f, CentreZ)
	};

	const FVector Extents[4] = {
		FVector(Thickness * 0.5f, Reach, HalfHeight), FVector(Thickness * 0.5f, Reach, HalfHeight),
		FVector(Reach, Thickness * 0.5f, HalfHeight), FVector(Reach, Thickness * 0.5f, HalfHeight)
	};

	for (int32 Side = 0; Side < 4; ++Side)
	{
		UBoxComponent* Wall = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		Wall->SetBoxExtent(Extents[Side]);
		Wall->SetCollisionProfileName(FName("BlockAll"));
		Wall->SetGenerateOverlapEvents(false);
		Wall->SetCanEverAffectNavigation(false);
		Wall->SetHiddenInGame(true);
		Wall->SetupAttachment(GetRootComponent());
		Wall->RegisterComponent();
		Wall->SetRelativeLocation(Offsets[Side]);
		EdgeWalls.Add(Wall);
	}
}

void AMissionGround::ScatterAll(int32 Seed)
{
	struct FPlaced
	{
		FVector2D Point;
		double Radius;
	};

	TArray<FPlaced> Placed;
	const FVector Centre = GetActorLocation();
	const float HalfLot = Size * 0.5f - 150.f;
	FString Report;

	for (int32 SetIndex = 0; SetIndex < Scatter.Num(); ++SetIndex)
	{
		const FGroundScatter& Family = Scatter[SetIndex];

		if (Family.Count <= 0 || Family.Meshes.IsEmpty())
		{
			continue;
		}

		FRandomStream Stream(static_cast<int32>(HashCombine(static_cast<uint32>(Seed), static_cast<uint32>(SetIndex + 1) * 2654435761u)));

		// 0 = off. Otherwise the share of the noise below which nothing grows: the higher, the fewer and denser the groves.
		const double Threshold = Family.Clustering > 0.f ? FMath::Lerp(0.25, 0.65, static_cast<double>(Family.Clustering)) : 0.0;

		TMap<const UStaticMesh*, UInstancedStaticMeshComponent*> Components;
		int32 Count = 0;
		const int32 MaxTries = Family.Count * 80;

		for (int32 Try = 0; Try < MaxTries && Count < Family.Count; ++Try)
		{
			// The same draws on every try, accepted or not, so a change of one setting moves little else.
			const FVector2D Point(Centre.X + Stream.FRandRange(-HalfLot, HalfLot), Centre.Y + Stream.FRandRange(-HalfLot, HalfLot));
			const int32 MeshIndex = Stream.RandRange(0, Family.Meshes.Num() - 1);
			const float Scale = Stream.FRandRange(Family.ScaleRange.Min, Family.ScaleRange.Max);
			const float Yaw = Family.bRandomYaw ? Stream.FRandRange(0.f, 360.f) : 0.f;

			UStaticMesh* Picked = Family.Meshes[MeshIndex];

			if (!Picked)
			{
				continue;
			}

			// The treeline lives in a band along the inside of the lot's edge, whatever the distance to the plot.
			const bool bTreeline = Family.EdgeBand > 0.f;

			if (bTreeline)
			{
				const double ToEdge = FMath::Min(Size * 0.5 - FMath::Abs(Point.X - Centre.X), Size * 0.5 - FMath::Abs(Point.Y - Centre.Y));

				if (ToEdge > Family.EdgeBand)
				{
					continue;
				}
			}

			if (bHasPlot)
			{
				const double FromPlot = PlotRegion.SignedDistance(Point);

				if (FromPlot < ScatterClearance
					|| (!bTreeline && Family.MinDistanceFromPlot > 0.f && FromPlot < Family.MinDistanceFromPlot)
					|| (!bTreeline && Family.MaxDistanceFromPlot > 0.f && FromPlot > Family.MaxDistanceFromPlot))
				{
					continue;
				}
			}

			if (!IsClearOfZones(Point))
			{
				continue;
			}

			if (Threshold > 0.0 && Fbm(Point.X / 3500.0 + 3.0, Point.Y / 3500.0 + 9.0, 3) < Threshold)
			{
				continue;
			}

			const double Radius = Family.Spacing * 0.5;
			bool bTooClose = false;

			for (const FPlaced& Other : Placed)
			{
				if (FVector2D::Distance(Point, Other.Point) < Radius + Other.Radius)
				{
					bTooClose = true;
					break;
				}
			}

			if (bTooClose)
			{
				continue;
			}

			UInstancedStaticMeshComponent*& Component = Components.FindOrAdd(Picked);

			if (!Component)
			{
				Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
				Component->SetStaticMesh(Picked);
				Component->SetCollisionProfileName(Family.bCollision ? FName("BlockAll") : FName("NoCollision"));
				Component->SetupAttachment(GetRootComponent());
				Component->RegisterComponent();
				ScatterComponents.Add(Component);
			}

			Placed.Add(FPlaced{ Point, Radius });
			Component->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Point.X, Point.Y, GetHeightAt(Point) + Family.ZOffset), FVector(Scale)), true);
			++Count;
		}

		Report += FString::Printf(TEXT("%s%s %d/%d"), Report.IsEmpty() ? TEXT("") : TEXT(", "), *Family.Label.ToString(), Count, Family.Count);
	}

	if (!Report.IsEmpty())
	{
		UE_LOG(LogPlotBuilder, Log, TEXT("Ground scatter: %s"), *Report);
		Summary += TEXT("\nScatter: ") + Report;
	}
}

// ---------------------------------------------------------------- debug

void AMissionGround::SetShowDebug(bool bShow)
{
	bShowDebug = bShow;
	SetActorTickEnabled(bShow);
}

void AMissionGround::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bShowDebug || !PBDebug::IsEnabled() || !GetWorld())
	{
		return;
	}

	for (const FGroundRegion& Region : Regions)
	{
		const FColor Color = Region.bRoad ? FColor(70, 130, 220) : Region.bKeepClear ? FColor(240, 140, 30) : FColor::White;
		const FQuat Rotation(FVector::UpVector, Region.Yaw);

		DrawDebugBox(GetWorld(), FVector(Region.Center.X, Region.Center.Y, BaseZ + 30.0), FVector(Region.Half.X, Region.Half.Y, 20.0), Rotation, Color, false, -1.f, 0, 8.f);
	}

	if (bHasPlot)
	{
		DrawDebugBox(GetWorld(), FVector(PlotRegion.Center.X, PlotRegion.Center.Y, BaseZ + 30.0), FVector(PlotRegion.Half.X, PlotRegion.Half.Y, 20.0), FQuat(FVector::UpVector, PlotRegion.Yaw), FColor::Yellow, false, -1.f, 0, 8.f);
	}

	// The border walls, in red.
	for (const UBoxComponent* Wall : EdgeWalls)
	{
		if (IsValid(Wall))
		{
			DrawDebugBox(GetWorld(), Wall->GetComponentLocation(), Wall->GetScaledBoxExtent(), Wall->GetComponentQuat(), FColor::Red, false, -1.f, 0, 6.f);
		}
	}
}
