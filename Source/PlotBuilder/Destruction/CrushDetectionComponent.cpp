#include "CrushDetectionComponent.h"
#include "DemolitionRules.h"
#include "Buildables/BuildablePiece.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PlotBuilder.h"

UCrushDetectionComponent::UCrushDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCrushDetectionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr))
	{
		RootPrimitive->SetNotifyRigidBodyCollision(true);
		RootPrimitive->OnComponentHit.AddDynamic(this, &UCrushDetectionComponent::HandleHit);
	}
}

void UCrushDetectionComponent::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const ABuildablePiece* Piece = Cast<ABuildablePiece>(OtherActor);

	if (!Piece || Piece->State != EBuildablePieceState::Collapsing || !OtherComp)
	{
		return;
	}

	const float Energy = OtherComp->GetMass() * OtherComp->GetComponentVelocity().Size();

	RecentHits.Add(FCrushHitRecord{ static_cast<float>(GetWorld()->GetTimeSeconds()), Energy });
}

void UCrushDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RecentHits.IsEmpty())
	{
		CurrentEnergy = 0.f;
		return;
	}

	const float Window = Rules ? Rules->CrushWindow : 2.f;
	const float Now = GetWorld()->GetTimeSeconds();

	RecentHits.RemoveAll([Now, Window](const FCrushHitRecord& Record)
	{
		return Now - Record.Time > Window;
	});

	CurrentEnergy = 0.f;

	for (const FCrushHitRecord& Record : RecentHits)
	{
		CurrentEnergy += Record.Energy;
	}

	const float Threshold = Rules ? Rules->CrushThreshold : 500.f;

	if (!bIsStunned && CurrentEnergy >= Threshold)
	{
		TriggerCrushEvent();
	}
}

void UCrushDetectionComponent::TriggerCrushEvent()
{
	RecentHits.Reset();

	const float StunDuration = Rules ? Rules->CrushStunDuration : 2.f;

	bIsStunned = true;
	GetWorld()->GetTimerManager().SetTimer(StunTimerHandle, this, &UCrushDetectionComponent::EndStun, StunDuration, false);

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			if (Rules && Rules->CrushCameraShakeClass)
			{
				PC->ClientStartCameraShake(Rules->CrushCameraShakeClass);
			}
		}
	}

	RegisterSafetyPenalty(FName("Crush"));
	OnCrushEvent.Broadcast(CurrentEnergy);
}

void UCrushDetectionComponent::EndStun()
{
	bIsStunned = false;
}

void UCrushDetectionComponent::RegisterSafetyPenalty(FName Source) const
{
	UE_LOG(LogPlotBuilder, Log, TEXT("UCrushDetectionComponent::RegisterSafetyPenalty: safety penalty stub (%s)"), *Source.ToString());
}
