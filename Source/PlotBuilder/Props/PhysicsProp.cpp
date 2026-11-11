#include "PhysicsProp.h"
#include "Components/StaticMeshComponent.h"
#include "Buildables/BuildablePiece.h"
#include "Interaction/HotbarComponent.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Debug/DebugDisplay.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Generation/FurnishingRuleRow.h"
#include "PlotBuilder.h"

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

	// A prop that does not simulate (a shed fixed to the ground) has no mass to read and takes no impact damage: leave before GetMass, which warns on every hit.
	if (!MeshComponent->IsSimulatingPhysics())
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

void APhysicsProp::MountOn(ABuildablePiece* Wall)
{
	if (!Wall || bMounted)
	{
		return;
	}

	bMounted = true;
	MountedOn = Wall;

	// Hangs: the body stays where the generator put it (it keeps its collision and its mass, for when it falls).
	MeshComponent->SetSimulatePhysics(false);

	if (UDurabilityComponent* WallDurability = Wall->GetDurabilityComponent())
	{
		WallDurability->OnDurabilityDamaged.AddDynamic(this, &APhysicsProp::HandleMountDamaged);
	}

	Wall->OnDestroyed.AddDynamic(this, &APhysicsProp::HandleMountDestroyed);
	Wall->OnCollapseStarted.AddUObject(this, &APhysicsProp::Release);
}

void APhysicsProp::Release()
{
	if (!bMounted)
	{
		return;
	}

	bMounted = false;

	if (ABuildablePiece* Wall = MountedOn.Get())
	{
		if (UDurabilityComponent* WallDurability = Wall->GetDurabilityComponent())
		{
			WallDurability->OnDurabilityDamaged.RemoveDynamic(this, &APhysicsProp::HandleMountDamaged);
		}

		Wall->OnDestroyed.RemoveDynamic(this, &APhysicsProp::HandleMountDestroyed);
		Wall->OnCollapseStarted.RemoveAll(this);
	}

	MountedOn.Reset();

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
}

void APhysicsProp::Place(AActor* Support)
{
	// MountOn does nothing for a null or a non-piece support; either way the body stops here.
	MountOn(Cast<ABuildablePiece>(Support));
	MeshComponent->SetSimulatePhysics(false);
}

FPropPlacement APhysicsProp::GetPlacement(const TSubclassOf<APhysicsProp> PropClass)
{
	FPropPlacement Result;

	if (!PropClass)
	{
		return Result;
	}

	static const FSoftObjectPath RulesPath(TEXT("/Game/Generation/DT_FurnishingRules"));
	const UDataTable* Table = Cast<UDataTable>(RulesPath.TryLoad());

	if (!Table)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("APhysicsProp::GetPlacement: %s not found, every prop stands on the floor."), *RulesPath.ToString());
		return Result;
	}

	bool bFound = false;

	Table->ForeachRow<FFurnishingRuleRow>(TEXT("APhysicsProp::GetPlacement"), [&Result, &bFound, PropClass](const FName& /*RowName*/, const FFurnishingRuleRow& Row)
	{
		// Several rows may share a class (versions of one object): the first is enough, they set it down the same way.
		if (!bFound && Row.PropClass == PropClass)
		{
			bFound = true;
			Result.bHangsOnWall = Row.Placement == EFurniturePlacement::OnWall;
			Result.FrontYawOffset = Row.FrontYawOffset;
		}
	});

	return Result;
}

void APhysicsProp::HandleMountDamaged(const FDamageInfo& DamageInfo)
{
	Release();
}

void APhysicsProp::HandleMountDestroyed(AActor* DestroyedActor)
{
	Release();
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
