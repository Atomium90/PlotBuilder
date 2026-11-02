#include "ExplosiveCharge.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Destruction/Damageable.h"
#include "Destruction/DurabilityComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Destruction/CrushDetectionComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "PlotBuilder.h"

AExplosiveCharge::AExplosiveCharge()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionProfileName(FName("BlockAllDynamic"));

	RadiusPreviewComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RadiusPreviewComponent"));
	RadiusPreviewComponent->SetupAttachment(MeshComponent);
	RadiusPreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadiusPreviewComponent->SetSimulatePhysics(false);
}

void AExplosiveCharge::Throw(const FVector& Velocity)
{
	MeshComponent->SetPhysicsLinearVelocity(Velocity);
}

void AExplosiveCharge::BeginPlay()
{
	Super::BeginPlay();

	if (const UStaticMesh* PreviewMesh = RadiusPreviewComponent->GetStaticMesh())
	{
		const float MeshRadius = PreviewMesh->GetBounds().SphereRadius;

		if (MeshRadius > 0.f)
		{
			RadiusPreviewComponent->SetWorldScale3D(FVector(Radius / MeshRadius));
		}
	}
}

void AExplosiveCharge::RequestDetonation()
{
	if (bIsDetonating)
	{
		return;
	}

	bIsDetonating = true;

	OnDetonationRequested();

	GetWorldTimerManager().SetTimer(DetonationTimerHandle, this, &AExplosiveCharge::Detonate, DetonationDelay, false);
}

void AExplosiveCharge::Detonate()
{
	const FVector Origin = GetActorLocation();

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	TArray<AActor*> Overlaps;
	UKismetSystemLibrary::SphereOverlapActors(this, Origin, Radius, ObjectTypes, nullptr, { this }, Overlaps);

	for (AActor* Actor : Overlaps)
	{
		if (Actor->Implements<UDamageable>())
		{
			float FinalDamage = Damage;

			if (Rules)
			{
				if (const UDurabilityComponent* Durability = Actor->FindComponentByClass<UDurabilityComponent>())
				{
					FinalDamage = Rules->ComputeDamage(EDemolitionTool::Charge, Durability->GetMaterial(), Damage);
				}
			}

			FDamageInfo DamageInfo;
			DamageInfo.Amount = FinalDamage;
			DamageInfo.ToolId = FName("Charge");
			DamageInfo.HitLocation = Origin;
			DamageInfo.Instigator = this;

			IDamageable::Execute_ApplyDamage(Actor, DamageInfo);
		}

		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			if (Primitive->IsSimulatingPhysics())
			{
				Primitive->AddRadialImpulse(Origin, Radius, ImpulseStrength, ERadialImpulseFalloff::RIF_Constant, false);
			}
		}
	}

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (FVector::Dist(PlayerPawn->GetActorLocation(), Origin) <= Radius)
		{
			if (UCrushDetectionComponent* CrushDetection = PlayerPawn->FindComponentByClass<UCrushDetectionComponent>())
			{
				CrushDetection->RegisterSafetyPenalty(FName("Explosion"));
			}
			else
			{
				UE_LOG(LogPlotBuilder, Log, TEXT("AExplosiveCharge::Detonate: player caught in blast radius - safety penalty stub"));
			}
		}
	}

	if (ExplosionSound)
	{
		const FVector BlastLocation = MeshComponent->DoesSocketExist(FName("Blast")) ? MeshComponent->GetSocketLocation(FName("Blast")) : Origin;
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, BlastLocation);
	}

	Destroy();
}
