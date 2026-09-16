// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "PlayerCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInteractionComponent;
class UPhysicsManipulationComponent;
class UHotbarComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Generic interaction trace + focus tracking */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UInteractionComponent* InteractionComponent;

	/** Push / Pull / Launch on the current focus */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPhysicsManipulationComponent* PhysicsManipulationComponent;

	/** Minimal hotbar: pickup + reuse for ABuildablePiece and APhysicsProp */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UHotbarComponent* HotbarComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Grab Input Action: hold to Pull, release to Launch */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* GrabAction;

	/** Push Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* PushAction;

	/** Interact Input Action: generic interact, also picks up storable objects into the hotbar */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* InteractAction;

	/** Place Input Action: places the last hotbar item */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* PlaceAction;

public:
	APlayerCharacter();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;


public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Returns InteractionComponent subobject **/
	UInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** Returns PhysicsManipulationComponent subobject **/
	UPhysicsManipulationComponent* GetPhysicsManipulationComponent() const { return PhysicsManipulationComponent; }

	/** Returns HotbarComponent subobject **/
	UHotbarComponent* GetHotbarComponent() const { return HotbarComponent; }

};
