#include "DemolitionTool.h"
#include "Contracts/MissionResultSubsystem.h"
#include "Debug/DebugDisplay.h"
#include "DemolitionToolData.h"
#include "DemolitionRules.h"
#include "Damageable.h"
#include "DurabilityComponent.h"
#include "Player/PlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/EngineTypes.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "PlotBuilder.h"

bool ADemolitionTool::bShowHPDebug = true;

ADemolitionTool::ADemolitionTool()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetSimulatePhysics(false);
}

void ADemolitionTool::BeginPlay()
{
	Super::BeginPlay();

	if (const APlayerCharacter* OwnerCharacter = Cast<APlayerCharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* FirstPersonMesh = OwnerCharacter->GetFirstPersonMesh())
		{
			AttachToComponent(FirstPersonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocketName);
			SetActorRelativeTransform(HandOffset);
		}
	}

	SetToolVisible(false);
}

void ADemolitionTool::SetToolVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
}

void ADemolitionTool::PrimaryUse()
{
	if (bBusy)
	{
		return;
	}

	if (!ToolData)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("ADemolitionTool::PrimaryUse: %s has no ToolData set"), *GetName());
		return;
	}

	bBusy = true;

	if (ToolData->bContinuous)
	{
		if (ToolData->ContinuousLoopSound && MeshComponent)
		{
			ContinuousLoopAudio = UGameplayStatics::SpawnSoundAttached(ToolData->ContinuousLoopSound, MeshComponent);
		}

		const float Interval = ToolData->Rate > 0.f ? 1.f / ToolData->Rate : 1.f;
		ResolveHit();
		GetWorldTimerManager().SetTimer(HitTimerHandle, this, &ADemolitionTool::ResolveHit, Interval, true);
		return;
	}

	const float WindUp = FMath::Max(0.f, ToolData->WindUpDuration);
	const float Cycle = ToolData->Rate > 0.f ? 1.f / ToolData->Rate : 1.f;
	const float BusyDuration = FMath::Max(Cycle, WindUp);

	if (WindUp > 0.f)
	{
		GetWorldTimerManager().SetTimer(HitTimerHandle, this, &ADemolitionTool::ResolveHit, WindUp, false);
	}
	else
	{
		ResolveHit();
	}

	if (BusyDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(BusyTimerHandle, this, &ADemolitionTool::ClearBusy, BusyDuration, false);
	}
	else
	{
		ClearBusy();
	}
}

void ADemolitionTool::ReleasePrimaryUse()
{
	if (ContinuousLoopAudio)
	{
		ContinuousLoopAudio->FadeOut(ToolData ? ToolData->ContinuousLoopFadeOutDuration : 0.2f, 0.f);
		ContinuousLoopAudio = nullptr;
	}

	if (!ToolData || !ToolData->bContinuous)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(HitTimerHandle);
	ClearBusy();
}

void ADemolitionTool::ClearBusy()
{
	bBusy = false;
}

void ADemolitionTool::ResolveHit()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!OwnerController || !ToolData)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * ToolData->Range;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);

	if (!bHit)
	{
		return;
	}

	TArray<AActor*> Targets;

	if (ToolData->HitRadius > 0.f)
	{
		TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
		ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
		ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

		UKismetSystemLibrary::SphereOverlapActors(this, Hit.Location, ToolData->HitRadius, ObjectTypes, nullptr, { GetOwner(), this }, Targets);
	}
	else if (Hit.GetActor())
	{
		Targets.Add(Hit.GetActor());
	}

	const FName ToolId(*StaticEnum<EDemolitionTool>()->GetNameStringByValue(static_cast<int64>(ToolData->Tool)));

	bool bHitSomething = false;
	EDemolitionMaterial HitMaterial = EDemolitionMaterial::Wood;

	for (AActor* Target : Targets)
	{
		if (!Target || !Target->Implements<UDamageable>())
		{
			continue;
		}

		const UDurabilityComponent* Durability = Target->FindComponentByClass<UDurabilityComponent>();
		const EDemolitionMaterial Material = Durability ? Durability->GetMaterial() : EDemolitionMaterial::Wood;
		const float FinalDamage = (Rules && Durability) ? Rules->ComputeDamage(ToolData->Tool, Material, ToolData->BaseDamage) : ToolData->BaseDamage;

		FDamageInfo DamageInfo;
		DamageInfo.Amount = FinalDamage;
		DamageInfo.Multiplier = ToolData->BaseDamage > 0.f ? FinalDamage / ToolData->BaseDamage : 1.f;
		DamageInfo.ToolId = ToolId;
		DamageInfo.HitLocation = Hit.Location;
		DamageInfo.Instigator = this;

		IDamageable::Execute_ApplyDamage(Target, DamageInfo);

		bHitSomething = true;
		HitMaterial = Material;

		if (bShowHPDebug && GEngine)
		{
			PBDebug::Message(-1, 2.f, FColor::Orange, FString::Printf(TEXT("%s hit %s for %.0f (HP left: %.0f)"),
				*GetName(), *Target->GetActorNameOrLabel(), FinalDamage, Durability ? Durability->CurrentHP : -1.f));
		}
	}

	if (bHitSomething)
	{
		PlayHitFeedback(Hit.Location, HitMaterial);

		// A hit that landed is 1 / Rate seconds of work: the mission report charges the upkeep of the tool over that time.
		if (!ToolData->Upkeep.IsFree())
		{
			if (UMissionResultSubsystem* Mission = GetWorld()->GetSubsystem<UMissionResultSubsystem>())
			{
				Mission->RecordToolActivity(ToolId, ToolData->Upkeep, ToolData->Rate > 0.f ? 1.f / ToolData->Rate : 1.f);
			}
		}
	}
}

void ADemolitionTool::PlayHitFeedback(const FVector& Location, EDemolitionMaterial Material) const
{
	if (ToolData && ToolData->HitCameraShakeClass)
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->ClientStartCameraShake(ToolData->HitCameraShakeClass);
			}
		}
	}

	if (Rules)
	{
		if (USoundBase* ImpactSound = Rules->GetImpactSound(Material))
		{
			// Ducked while the drill loop plays (jackhammer) so the two don't stack at Rate ticks/s -
			// a no-op (volume 1.0) for a tool with no ContinuousLoopSound.
			const bool bDuck = ContinuousLoopAudio && ContinuousLoopAudio->IsPlaying();
			const float Volume = (bDuck && ToolData) ? ToolData->ContinuousImpactDuckVolume : 1.f;
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Location, Volume);
		}
	}
}
