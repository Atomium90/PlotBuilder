#include "HubInteractable.h"
#include "Components/BoxComponent.h"
#include "UI/PlayerMessages.h"

AHubInteractable::AHubInteractable()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);

	Trigger->SetBoxExtent(FVector(100.0, 100.0, 100.0));
	Trigger->ShapeColor = FColor::Orange;
	Trigger->SetGenerateOverlapEvents(false);

	// Only the interaction trace (ECC_GameTraceChannel2, the one UInteractionComponent uses) sees it: the player walks through.
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
}

void AHubInteractable::OnFocusBegin_Implementation(AActor* User)
{
	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->SetPrompt(this, Prompt);
	}
}

void AHubInteractable::OnFocusEnd_Implementation(AActor* User)
{
	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->ClearPrompt(this);
	}
}

void AHubInteractable::Interact_Implementation(AActor* User)
{
	OnInteracted.Broadcast(User);
	Use(User);
}

void AHubInteractable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The level is leaving while the prompt is up (the use that sent the player away): take it down.
	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->ClearPrompt(this);
	}

	Super::EndPlay(EndPlayReason);
}
