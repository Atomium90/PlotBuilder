#include "FragmentDebris.h"
#include "DebrisSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AFragmentDebris::AFragmentDebris()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetSimulatePhysics(true);
	// "Debris" (Config/DefaultEngine.ini): ignores Camera/Pawn/other debris, still blocks world
	// geometry - lets a fragment pile sit on the ground without ever blocking the player's movement
	// or build-mode's obstruction checks (a different object type than what that query asks for).
	MeshComponent->SetCollisionProfileName(FName("Debris"));
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

void AFragmentDebris::Launch(const FVector& Impulse, const FLinearColor& Tint, float Lifetime, float FadeDuration, int32 DebrisCap, float InSleepVelocityThreshold)
{
	LifetimeSeconds = Lifetime;
	FadeDurationSeconds = FadeDuration;
	SleepVelocityThreshold = InSleepVelocityThreshold;

	MeshComponent->SetCustomPrimitiveDataVector3(0, FVector(Tint.R, Tint.G, Tint.B));

	if (UWorld* World = GetWorld())
	{
		// Chaos's physics thread can lag a tick or more behind the game thread, especially when many
		// pieces fracture the same frame (a batch of foundations, a cascade) - the new rigid body isn't
		// always live in the solver yet by the next game tick, so the impulse can still be silently
		// dropped. A fixed short delay gives the solver real margin instead of gambling on one frame.
		TWeakObjectPtr<UStaticMeshComponent> WeakMesh(MeshComponent);
		FTimerHandle ImpulseDelayHandle;

		World->GetTimerManager().SetTimer(ImpulseDelayHandle, FTimerDelegate::CreateLambda([WeakMesh, Impulse]()
		{
			if (UStaticMeshComponent* Mesh = WeakMesh.Get())
			{
				Mesh->AddImpulse(Impulse, NAME_None, true);
			}
		}), 0.1f, false);

		if (UDebrisSubsystem* DebrisSubsystem = World->GetSubsystem<UDebrisSubsystem>())
		{
			DebrisSubsystem->RegisterDebrisActor(this, DebrisCap);
		}
	}
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

	// Checked once per tick until asleep, then skipped - a settled fragment stops costing anything
	// here. Linear speed only (not angular): close enough to "settled" for this heuristic without a
	// second tunable in a different unit.
	if (!bAsleep && SleepVelocityThreshold > 0.f && MeshComponent->GetPhysicsLinearVelocity().Size() < SleepVelocityThreshold)
	{
		MeshComponent->PutRigidBodyToSleep();
		bAsleep = true;
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
