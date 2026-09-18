#include "PhysicsManipulationComponent.h"
#include "InteractionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"

UPhysicsManipulationComponent::UPhysicsManipulationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPhysicsManipulationComponent::BeginPlay()
{
	Super::BeginPlay();

	InteractionComponent = GetOwner()->FindComponentByClass<UInteractionComponent>();
}

FVector UPhysicsManipulationComponent::GetViewLocationAndForward(FVector& OutForward) const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!OwnerController)
	{
		OutForward = FVector::ForwardVector;
		return FVector::ZeroVector;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	OutForward = ViewRotation.Vector();
	return ViewLocation;
}

UPrimitiveComponent* UPhysicsManipulationComponent::GetPhysicsBody(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	UPrimitiveComponent* Primitive = Actor->FindComponentByClass<UPrimitiveComponent>();
	return (Primitive && Primitive->IsSimulatingPhysics()) ? Primitive : nullptr;
}

void UPhysicsManipulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(HeldComponent))
	{
		HeldComponent = nullptr;
		return;
	}

	FVector ViewForward;
	const FVector ViewLocation = GetViewLocationAndForward(ViewForward);
	const FVector HoldPoint = ViewLocation + ViewForward * HoldDistance;

	// Spring targets the center of mass, not GetComponentLocation() - an off-center mesh pivot
	// would otherwise spin continuously, since AddForce applies at the center of mass while the
	// pivot-to-COM offset rotates with the object, feeding rotational energy back into the spring.
	const FVector CenterOfMass = HeldComponent->GetCenterOfMass();
	const FVector ToTarget = HoldPoint - CenterOfMass;
	const FVector Velocity = HeldComponent->GetPhysicsLinearVelocity();
	const FVector Force = ToTarget * PullStrength - Velocity * PullDamping;

	HeldComponent->AddForce(Force, NAME_None, false);

	if (bShowDebug)
	{
		DrawDebugDirectionalArrow(GetWorld(), CenterOfMass, CenterOfMass + Force.GetClampedToMaxSize(200.f), 20.f, FColor::Cyan, false, 0.f, 0, 2.f);
	}
}

void UPhysicsManipulationComponent::Grab()
{
	if (HeldComponent || !InteractionComponent)
	{
		return;
	}

	HeldComponent = GetPhysicsBody(InteractionComponent->GetCurrentFocus());
}

void UPhysicsManipulationComponent::Push()
{
	if (IsValid(HeldComponent))
	{
		LaunchHeld();
	}
	else
	{
		HeldComponent = nullptr;
		PushFocus();
	}
}

void UPhysicsManipulationComponent::LaunchHeld()
{
	FVector ViewForward;
	GetViewLocationAndForward(ViewForward);

	const FVector Impulse = ViewForward * LaunchImpulseStrength;
	HeldComponent->AddImpulse(Impulse, NAME_None, false);

	if (bShowDebug)
	{
		const FVector CenterOfMass = HeldComponent->GetCenterOfMass();
		DrawDebugDirectionalArrow(GetWorld(), CenterOfMass, CenterOfMass + Impulse.GetClampedToMaxSize(200.f), 20.f, FColor::Orange, false, 1.f, 0, 2.f);
	}

	HeldComponent = nullptr;
}

void UPhysicsManipulationComponent::PushFocus()
{
	if (!InteractionComponent)
	{
		return;
	}

	UPrimitiveComponent* Target = GetPhysicsBody(InteractionComponent->GetCurrentFocus());

	if (!Target)
	{
		return;
	}

	FVector ViewForward;
	GetViewLocationAndForward(ViewForward);

	const FVector Impulse = ViewForward * PushImpulseStrength;
	Target->AddImpulse(Impulse, NAME_None, false);

	if (bShowDebug)
	{
		const FVector CenterOfMass = Target->GetCenterOfMass();
		DrawDebugDirectionalArrow(GetWorld(), CenterOfMass, CenterOfMass + Impulse.GetClampedToMaxSize(200.f), 20.f, FColor::Red, false, 1.f, 0, 2.f);
	}
}
