#include "CrushDetectionComponent.h"
#include "DemolitionRules.h"
#include "Buildables/BuildablePiece.h"
#include "Components/PrimitiveComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/Engine.h"
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
		DrawGauge();
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

	DrawGauge();
}

void UCrushDetectionComponent::DrawGauge() const
{
	if (!bShowGauge || !GEngine)
	{
		return;
	}

	const float Threshold = Rules ? Rules->CrushThreshold : 500.f;
	const float Ratio = Threshold > 0.f ? FMath::Clamp(CurrentEnergy / Threshold, 0.f, 1.f) : 0.f;
	const int32 BarLength = 20;
	const int32 Filled = FMath::RoundToInt(Ratio * BarLength);
	const FString Bar = FString::ChrN(Filled, TEXT('#')) + FString::ChrN(BarLength - Filled, TEXT('-'));
	const FColor Colour = bIsStunned ? FColor::Red : (Ratio >= 0.75f ? FColor::Orange : (Ratio >= 0.4f ? FColor::Yellow : FColor::Green));

	GEngine->AddOnScreenDebugMessage(GaugeMessageKey, 0.f, Colour,
		FString::Printf(TEXT("Crush [%s] %.0f / %.0f%s"), *Bar, CurrentEnergy, Threshold, bIsStunned ? TEXT("  STUNNED") : TEXT("")));
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
