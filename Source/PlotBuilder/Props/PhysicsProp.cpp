#include "PhysicsProp.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Debug/DebugDisplay.h"
#include "Engine/World.h"

APhysicsProp::APhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionProfileName(FName("BlockAllDynamic"));

	DurabilityComponent = CreateDefaultSubobject<UDurabilityComponent>(TEXT("DurabilityComponent"));
}

void APhysicsProp::BeginPlay()
{
	Super::BeginPlay();

	DurabilityComponent->OnDurabilityDestroyed.AddDynamic(this, &APhysicsProp::HandleBroken);
	DurabilityComponent->InitializeWithFlatHP(Material, MaxHP);

	SpawnTime = GetWorld()->GetTimeSeconds();

	if (bTakesImpactDamage)
	{
		// A simulating body only reports hits when asked to; done here, not in the constructor, so a Blueprint
		// that turns bTakesImpactDamage off does not pay for the events.
		MeshComponent->SetNotifyRigidBodyCollision(true);
		MeshComponent->OnComponentHit.AddDynamic(this, &APhysicsProp::HandleImpact);
	}
}

void APhysicsProp::HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Now = GetWorld()->GetTimeSeconds();

	if (Now - SpawnTime < ImpactGraceSeconds || Now - LastImpactTime < ImpactMinInterval)
	{
		return;
	}

	const float Mass = MeshComponent->GetMass();

	if (Mass <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Impulse / mass is the speed change the contact gave this body: the same number whatever hit it, a wall
	// stopping a thrown chair or a falling slab landing on it.
	const float ImpactSpeed = NormalImpulse.Size() / Mass;

	if (ImpactSpeed <= ImpactSpeedThreshold)
	{
		return;
	}

	LastImpactTime = Now;

	FDamageInfo DamageInfo;
	DamageInfo.Amount = (ImpactSpeed - ImpactSpeedThreshold) * ImpactDamagePerSpeed;
	DamageInfo.ToolId = FName("Impact");
	DamageInfo.HitLocation = Hit.ImpactPoint;

	PBDebug::Message(-1, 3.f, FColor::Orange, FString::Printf(TEXT("Impact on %s: %.0f cm/s, %.1f damage"), *GetName(), ImpactSpeed, DamageInfo.Amount));

	DurabilityComponent->ApplyDamage(DamageInfo);
}

void APhysicsProp::ApplyDamage_Implementation(const FDamageInfo& DamageInfo)
{
	DurabilityComponent->ApplyDamage(DamageInfo);
}

void APhysicsProp::HandleBroken()
{
	if (const UDemolitionRules* Rules = DurabilityComponent->Rules)
	{
		Rules->SpawnGenericFragments(GetWorld(), FragmentCount, Material, GetActorLocation(), FVector::ZeroVector);
	}

	Destroy();
}

void APhysicsProp::Interact_Implementation(AActor* InteractingActor)
{
	if (UHotbarComponent* Hotbar = InteractingActor ? InteractingActor->FindComponentByClass<UHotbarComponent>() : nullptr)
	{
		Hotbar->Store(this);
	}
}
