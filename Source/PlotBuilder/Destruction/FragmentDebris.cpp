#include "FragmentDebris.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AFragmentDebris::AFragmentDebris()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionProfileName(FName("BlockAllDynamic"));
	MeshComponent->SetWorldScale3D(FVector(0.25f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (CubeFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeFinder.Object);
	}

	Tags.Add(FName("Fragment"));
}

void AFragmentDebris::BeginPlay()
{
	Super::BeginPlay();

	if (FadeMaterial)
	{
		MeshComponent->SetMaterial(0, FadeMaterial);
	}

	// Per-instance size jitter so a batch of identical cubes doesn't read as one repeated stamp -
	// cosmetic only.
	MeshComponent->SetWorldScale3D(MeshComponent->GetComponentScale() * FMath::FRandRange(0.7f, 1.3f));
	InitialScale = MeshComponent->GetComponentScale();
}

void AFragmentDebris::Launch(const FVector& Impulse, const FLinearColor& Tint, float Lifetime, float FadeDuration)
{
	LifetimeSeconds = Lifetime;
	FadeDurationSeconds = FadeDuration;

	MeshComponent->SetCustomPrimitiveDataVector3(0, FVector(Tint.R, Tint.G, Tint.B));
	MeshComponent->AddImpulse(Impulse, NAME_None, true);
}

void AFragmentDebris::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;

	if (Age >= LifetimeSeconds)
	{
		Destroy();
		return;
	}

	const float FadeStart = LifetimeSeconds - FadeDurationSeconds;

	if (FadeDurationSeconds > 0.f && Age >= FadeStart)
	{
		// Scale-to-zero fade, not opacity - keeps the fragment material a plain opaque instance,
		// no translucent blend mode / sort-order complexity for a cosmetic shrink.
		const float Alpha = FMath::Clamp(1.f - (Age - FadeStart) / FadeDurationSeconds, 0.f, 1.f);
		MeshComponent->SetWorldScale3D(InitialScale * Alpha);
	}
}
