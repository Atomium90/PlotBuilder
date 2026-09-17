// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/PhysicsManipulationComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/BuildModeComponent.h"
#include "PlotBuilder.h"

DEFINE_LOG_CATEGORY(LogPlayerCharacter);

APlayerCharacter::APlayerCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	// Generic interaction trace + focus tracking
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));

	// Push / Pull / Launch on the current focus
	PhysicsManipulationComponent = CreateDefaultSubobject<UPhysicsManipulationComponent>(TEXT("PhysicsManipulationComponent"));

	// Minimal hotbar: pickup + reuse for ABuildablePiece and APhysicsProp
	HotbarComponent = CreateDefaultSubobject<UHotbarComponent>(TEXT("HotbarComponent"));

	// Preview + confirm placement flow for ABuildablePiece
	BuildModeComponent = CreateDefaultSubobject<UBuildModeComponent>(TEXT("BuildModeComponent"));
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::LookInput);

		// Grab: single press, picks up the current focus
		EnhancedInputComponent->BindAction(GrabAction, ETriggerEvent::Started, PhysicsManipulationComponent, &UPhysicsManipulationComponent::Grab);

		// Push: Launch if something is held, Push on the current focus otherwise
		EnhancedInputComponent->BindAction(PushAction, ETriggerEvent::Started, PhysicsManipulationComponent, &UPhysicsManipulationComponent::Push);

		// Interact: generic Interact() on the current focus (also how storable objects get picked up)
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, InteractionComponent, &UInteractionComponent::TryInteract);

		// Place: confirms the Build Mode preview if active, otherwise instantly places a Prop
		EnhancedInputComponent->BindAction(PlaceAction, ETriggerEvent::Started, this, &APlayerCharacter::PlaceInput);

		// Toggle Build Mode: dedicated preview + rotate + confirm flow for ABuildablePiece
		EnhancedInputComponent->BindAction(ToggleBuildModeAction, ETriggerEvent::Started, BuildModeComponent, &UBuildModeComponent::ToggleBuildMode);

		// Rotate the Build Mode preview by one step (no-op outside Build Mode)
		EnhancedInputComponent->BindAction(RotatePreviewAction, ETriggerEvent::Started, this, &APlayerCharacter::RotatePreviewInput);

		// Cycle which hotbar entry is selected/previewed
		EnhancedInputComponent->BindAction(CycleSelectionAction, ETriggerEvent::Started, this, &APlayerCharacter::CycleSelectionInput);

		// Purchase: each action in the array buys the ShopCatalog row at its own index
		for (int32 i = 0; i < PurchaseActions.Num(); ++i)
		{
			if (PurchaseActions[i])
			{
				EnhancedInputComponent->BindAction(PurchaseActions[i], ETriggerEvent::Started, HotbarComponent, &UHotbarComponent::PurchaseByIndex, i);
			}
		}
	}
	else
	{
		UE_LOG(LogPlayerCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void APlayerCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void APlayerCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void APlayerCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void APlayerCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void APlayerCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void APlayerCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void APlayerCharacter::PlaceInput()
{
	if (BuildModeComponent && BuildModeComponent->IsBuildModeActive())
	{
		BuildModeComponent->ConfirmPlacement();
	}
	else if (HotbarComponent)
	{
		HotbarComponent->TryPlace();
	}
}

void APlayerCharacter::RotatePreviewInput(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();
	const int32 Direction = (Raw > 0.f) - (Raw < 0.f);

	if (Direction != 0 && BuildModeComponent)
	{
		BuildModeComponent->RotatePreview(Direction);
	}
}

void APlayerCharacter::CycleSelectionInput(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();
	const int32 Direction = (Raw > 0.f) - (Raw < 0.f);

	if (Direction != 0 && HotbarComponent)
	{
		HotbarComponent->CycleSelection(Direction, BuildModeComponent && BuildModeComponent->IsBuildModeActive());
	}
}
