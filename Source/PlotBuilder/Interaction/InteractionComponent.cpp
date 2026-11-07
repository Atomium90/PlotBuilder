#include "InteractionComponent.h"
#include "Debug/DebugDisplay.h"
#include "Interactable.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Destruction/DurabilityComponent.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFocus();
}

void UInteractionComponent::UpdateFocus()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!OwnerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams);

	AActor* HitActor = (bHit && Hit.GetActor() && Hit.GetActor()->Implements<UInteractable>()) ? Hit.GetActor() : nullptr;

	if (HitActor != CurrentFocus)
	{
		if (CurrentFocus)
		{
			IInteractable::Execute_OnFocusEnd(CurrentFocus, GetOwner());
		}

		CurrentFocus = HitActor;

		if (CurrentFocus)
		{
			IInteractable::Execute_OnFocusBegin(CurrentFocus, GetOwner());
		}
	}

	if (bShowDebugTrace && PBDebug::IsEnabled())
	{
		DrawDebugLine(GetWorld(), ViewLocation, bHit ? Hit.Location : TraceEnd, CurrentFocus ? FColor::Green : FColor::Red, false, 0.f, 0, 1.f);
	}

	if (bShowDebugFocusName && CurrentFocus && GEngine)
	{
		PBDebug::Message(1, 0.f, FColor::Green, FString::Printf(TEXT("Focus: %s"), *CurrentFocus->GetActorNameOrLabel()));
	}

	if (bShowDebugDurability && CurrentFocus && GEngine)
	{
		if (const UDurabilityComponent* Durability = CurrentFocus->FindComponentByClass<UDurabilityComponent>())
		{
			PBDebug::Message(2, 0.f, FColor::Yellow, FString::Printf(TEXT("HP: %.0f / %.0f  Material: %s  State: %s"),
				Durability->CurrentHP, Durability->MaxHP,
				*UEnum::GetValueAsString(Durability->GetMaterial()),
				*UEnum::GetValueAsString(Durability->GetState())));
		}
	}
}

void UInteractionComponent::TryInteract()
{
	if (CurrentFocus)
	{
		IInteractable::Execute_Interact(CurrentFocus, GetOwner());
	}
}
