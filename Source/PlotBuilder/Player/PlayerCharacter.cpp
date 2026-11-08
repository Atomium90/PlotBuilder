// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCharacter.h"
#include "Debug/DebugDisplay.h"
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
#include "Interaction/ShopItemRow.h"
#include "Interaction/ToolbeltComponent.h"
#include "Destruction/Damageable.h"
#include "Destruction/DemolitionTypes.h"
#include "Destruction/CrushDetectionComponent.h"
#include "Destruction/DebrisSubsystem.h"
#include "Environment/Plot.h"
#include "Environment/StructuralSupportComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
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

	// Equipped demolition tool slot, separate from the hotbar
	ToolbeltComponent = CreateDefaultSubobject<UToolbeltComponent>(TEXT("ToolbeltComponent"));

	// Accumulated crush energy from Falling pieces, stun gating for movement/camera/tool input
	CrushDetectionComponent = CreateDefaultSubobject<UCrushDetectionComponent>(TEXT("CrushDetectionComponent"));
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

		// Legacy Grab/Push: single press, only act while the toolbelt is locked (Lvl_Home) - see LegacyGrabInput/LegacyPushInput
		EnhancedInputComponent->BindAction(GrabAction, ETriggerEvent::Started, this, &APlayerCharacter::LegacyGrabInput);
		EnhancedInputComponent->BindAction(PushAction, ETriggerEvent::Started, this, &APlayerCharacter::LegacyPushInput);

		// Interact: generic Interact() on the current focus (also how storable objects get picked up)
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, InteractionComponent, &UInteractionComponent::TryInteract);

		// Place: confirms the Build Mode preview if active, otherwise instantly places a Prop
		EnhancedInputComponent->BindAction(PlaceAction, ETriggerEvent::Started, this, &APlayerCharacter::PlaceInput);

		// Toggle Build Mode: dedicated preview + rotate + confirm flow for ABuildablePiece
		EnhancedInputComponent->BindAction(ToggleBuildModeAction, ETriggerEvent::Started, BuildModeComponent, &UBuildModeComponent::ToggleBuildMode);

		// Rotate the Build Mode preview by one step (no-op outside Build Mode)
		EnhancedInputComponent->BindAction(RotatePreviewAction, ETriggerEvent::Started, this, &APlayerCharacter::RotatePreviewInput);

		// Change the Build Mode grid level by one step (no-op outside Build Mode, and for a Legacy piece)
		EnhancedInputComponent->BindAction(ChangeBuildLevelAction, ETriggerEvent::Started, this, &APlayerCharacter::ChangeBuildLevelInput);

		// Change the Build Mode roof step by one step (no-op outside Build Mode)
		EnhancedInputComponent->BindAction(ChangeRoofStepAction, ETriggerEvent::Started, this, &APlayerCharacter::ChangeRoofStepInput);

		// Cycle which hotbar entry is selected/previewed
		EnhancedInputComponent->BindAction(CycleSelectionAction, ETriggerEvent::Started, this, &APlayerCharacter::CycleSelectionInput);

		// Toggle the inventory/shop widgets - implemented in the Blueprint, see ToggleInventory/ToggleShop
		EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &APlayerCharacter::ToggleInventory);
		EnhancedInputComponent->BindAction(ToggleShopAction, ETriggerEvent::Started, this, &APlayerCharacter::ToggleShop);

		// Cycle the equipped tool (mission context only)
		EnhancedInputComponent->BindAction(CycleToolAction, ETriggerEvent::Started, this, &APlayerCharacter::CycleToolInput);

		// Equipped tool primary/secondary use (mission context only) - gated on CrushDetectionComponent->IsStunned() by the wrapper handlers instead of binding ToolbeltComponent directly
		EnhancedInputComponent->BindAction(ToolPrimaryUseAction, ETriggerEvent::Started, this, &APlayerCharacter::ToolPrimaryUseInput);
		EnhancedInputComponent->BindAction(ToolPrimaryUseAction, ETriggerEvent::Completed, this, &APlayerCharacter::ToolReleasePrimaryUseInput);
		EnhancedInputComponent->BindAction(ToolPrimaryUseAction, ETriggerEvent::Canceled, this, &APlayerCharacter::ToolReleasePrimaryUseInput);
		EnhancedInputComponent->BindAction(ToolSecondaryUseAction, ETriggerEvent::Started, this, &APlayerCharacter::ToolSecondaryUseInput);
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
	if (GetController() && (!CrushDetectionComponent || !CrushDetectionComponent->IsStunned()))
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void APlayerCharacter::DoMove(float Right, float Forward)
{
	if (GetController() && (!CrushDetectionComponent || !CrushDetectionComponent->IsStunned()))
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void APlayerCharacter::DoJumpStart()
{
	if (CrushDetectionComponent && CrushDetectionComponent->IsStunned())
	{
		return;
	}

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

void APlayerCharacter::ChangeBuildLevelInput(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();
	const int32 Direction = (Raw > 0.f) - (Raw < 0.f);

	if (Direction != 0 && BuildModeComponent)
	{
		BuildModeComponent->ChangeBuildLevel(Direction);
	}
}

void APlayerCharacter::ChangeRoofStepInput(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();
	const int32 Direction = (Raw > 0.f) - (Raw < 0.f);

	if (Direction != 0 && BuildModeComponent)
	{
		BuildModeComponent->ChangeRoofStep(Direction);
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

void APlayerCharacter::CycleToolInput(const FInputActionValue& Value)
{
	if (CrushDetectionComponent && CrushDetectionComponent->IsStunned())
	{
		return;
	}

	const float Raw = Value.Get<float>();
	const int32 Direction = (Raw > 0.f) - (Raw < 0.f);

	if (Direction != 0 && ToolbeltComponent)
	{
		ToolbeltComponent->CycleTool(Direction);
	}
}

void APlayerCharacter::LegacyGrabInput()
{
	if (ToolbeltComponent && ToolbeltComponent->IsLocked() && PhysicsManipulationComponent)
	{
		PhysicsManipulationComponent->Grab();
	}
}

void APlayerCharacter::LegacyPushInput()
{
	if (ToolbeltComponent && ToolbeltComponent->IsLocked() && PhysicsManipulationComponent)
	{
		PhysicsManipulationComponent->Push();
	}
}

void APlayerCharacter::ToolPrimaryUseInput()
{
	if (ToolbeltComponent && (!CrushDetectionComponent || !CrushDetectionComponent->IsStunned()))
	{
		ToolbeltComponent->PrimaryUse();
	}
}

void APlayerCharacter::ToolReleasePrimaryUseInput()
{
	// Never gated by the stun, unlike the press above - a continuous tool (e.g. the jackhammer)
	// that started firing before the stun began must still be stoppable, not left looping.
	if (ToolbeltComponent)
	{
		ToolbeltComponent->ReleasePrimaryUse();
	}
}

void APlayerCharacter::ToolSecondaryUseInput()
{
	if (ToolbeltComponent && (!CrushDetectionComponent || !CrushDetectionComponent->IsStunned()))
	{
		ToolbeltComponent->SecondaryUse();
	}
}

void APlayerCharacter::ListShop() const
{
	if (!HotbarComponent || !HotbarComponent->ShopCatalog)
	{
		return;
	}

	const TArray<FName> RowNames = HotbarComponent->ShopCatalog->GetRowNames();

	for (int32 i = 0; i < RowNames.Num(); ++i)
	{
		const FShopItemRow* Row = HotbarComponent->ShopCatalog->FindRow<FShopItemRow>(RowNames[i], TEXT("ListShop"));

		if (!Row)
		{
			continue;
		}

		const FString Line = FString::Printf(TEXT("[%d] %s - %d"), i, *Row->DisplayName.ToString(), Row->Cost);

		if (GEngine)
		{
			PBDebug::Message(600 + i, 10.f, FColor::Cyan, Line);
		}

		UE_LOG(LogPlayerCharacter, Log, TEXT("%s"), *Line);
	}
}

void APlayerCharacter::Buy(int32 ItemIndex)
{
	if (!HotbarComponent || !HotbarComponent->ShopCatalog)
	{
		return;
	}

	const TArray<FName> RowNames = HotbarComponent->ShopCatalog->GetRowNames();

	if (!RowNames.IsValidIndex(ItemIndex))
	{
		return;
	}

	const bool bBought = HotbarComponent->PurchaseRow(RowNames[ItemIndex]);

	if (GEngine)
	{
		PBDebug::Message(650, 2.f, bBought ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("Buy %d: %s"), ItemIndex, bBought ? TEXT("ok") : TEXT("failed")));
	}
}

void APlayerCharacter::DebugDamageFocus(float Amount)
{
	AActor* Focus = InteractionComponent ? InteractionComponent->GetCurrentFocus() : nullptr;

	if (!Focus || !Focus->Implements<UDamageable>())
	{
		return;
	}

	FDamageInfo DamageInfo;
	DamageInfo.Amount = Amount;
	DamageInfo.ToolId = FName("Debug");
	DamageInfo.HitLocation = Focus->GetActorLocation();
	DamageInfo.Instigator = this;

	IDamageable::Execute_ApplyDamage(Focus, DamageInfo);

	if (GEngine)
	{
		PBDebug::Message(700, 2.f, FColor::Orange, FString::Printf(TEXT("DebugDamageFocus %s: -%.0f"), *Focus->GetActorNameOrLabel(), Amount));
	}
}

void APlayerCharacter::SetBuildingAllowed(bool bAllowed)
{
	if (BuildModeComponent)
	{
		BuildModeComponent->SetAllowed(bAllowed);
	}

	if (HotbarComponent)
	{
		HotbarComponent->SetAllowed(bAllowed);
	}
}

void APlayerCharacter::SetDestructionAllowed(bool bAllowed)
{
	if (ToolbeltComponent)
	{
		ToolbeltComponent->SetDestructionAllowed(bAllowed);
	}
}

void APlayerCharacter::DebugShowStability(bool bShow)
{
	APlot* Plot = Cast<APlot>(UGameplayStatics::GetActorOfClass(GetWorld(), APlot::StaticClass()));
	UStructuralSupportComponent* StructuralSupport = Plot ? Plot->GetStructuralSupport() : nullptr;

	if (!StructuralSupport)
	{
		return;
	}

	StructuralSupport->SetShowDebug(bShow);

	if (GEngine)
	{
		PBDebug::Message(701, 2.f, FColor::Cyan, FString::Printf(TEXT("DebugShowStability: %s"), bShow ? TEXT("on") : TEXT("off")));
	}
}

void APlayerCharacter::DebugShowCollapseQueue(bool bShow)
{
	APlot* Plot = Cast<APlot>(UGameplayStatics::GetActorOfClass(GetWorld(), APlot::StaticClass()));
	UStructuralSupportComponent* StructuralSupport = Plot ? Plot->GetStructuralSupport() : nullptr;

	if (!StructuralSupport)
	{
		return;
	}

	StructuralSupport->SetShowCollapseQueueDebug(bShow);

	if (GEngine)
	{
		PBDebug::Message(702, 2.f, FColor::Cyan, FString::Printf(TEXT("DebugShowCollapseQueue: %s"), bShow ? TEXT("on") : TEXT("off")));
	}
}

void APlayerCharacter::DebugShowFragmentCount(bool bShow)
{
	if (UDebrisSubsystem* DebrisSubsystem = GetWorld()->GetSubsystem<UDebrisSubsystem>())
	{
		DebrisSubsystem->SetShowDebugCount(bShow);
	}

	if (GEngine)
	{
		PBDebug::Message(703, 2.f, FColor::Cyan, FString::Printf(TEXT("DebugShowFragmentCount: %s"), bShow ? TEXT("on") : TEXT("off")));
	}
}
