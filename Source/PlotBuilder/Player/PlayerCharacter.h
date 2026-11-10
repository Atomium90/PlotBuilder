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
class UBuildModeComponent;
class UToolbeltComponent;
class UPaintBrushComponent;
class UCrushDetectionComponent;
class UShopCartComponent;
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

	/** Preview + confirm placement flow for ABuildablePiece, separate from the instant Prop placement */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UBuildModeComponent* BuildModeComponent;

	/** Equipped demolition tool slot, separate from the hotbar - see GDD §18.2 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UToolbeltComponent* ToolbeltComponent;

	/** What the paint brush (tool belt slot 5) does: paints the piece in focus with the bucket selected in the hotbar */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UPaintBrushComponent* PaintBrushComponent;

	/** The shop cart: what the player means to buy, confirmed in one purchase. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UShopCartComponent* ShopCartComponent;

	/** Accumulated crush energy from Falling pieces, stun gating for movement/camera/tool input - see GDD §7.9 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCrushDetectionComponent* CrushDetectionComponent;

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

	/** Grab Input Action: hold to Pull, release to Launch - legacy full-featured binding, only acts while the toolbelt is locked (Lvl_Home); see LegacyGrabInput */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* GrabAction;

	/** Push Input Action - legacy full-featured binding (Launch if holding, otherwise pushes the focus), only acts while the toolbelt is locked (Lvl_Home); see LegacyPushInput */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* PushAction;

	/** Interact Input Action: generic interact, also picks up storable objects into the hotbar */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* InteractAction;

	/** Place Input Action: places the selected hotbar item (Prop instantly, or confirms the Build Mode preview) */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* PlaceAction;

	/** Toggle Build Mode Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ToggleBuildModeAction;

	/** Rotate Preview Input Action (Axis1D: +1/-1 per step, e.g. two keys with a Negate modifier) */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* RotatePreviewAction;

	/** Change Build Level Input Action (Axis1D: +1/-1 per step, e.g. two keys with a Negate modifier) - no-op outside Build Mode, and for a Legacy piece */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ChangeBuildLevelAction;

	/** Change Roof Step Input Action (Axis1D: +1/-1 per step, e.g. two keys with a Negate modifier) - no-op outside Build Mode */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ChangeRoofStepAction;

	/** Cycle Hotbar Selection Input Action (Axis1D: +1/-1 per step, e.g. mouse wheel) */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* CycleSelectionAction;

	/** Toggle Inventory Input Action - see ToggleInventory() below. */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ToggleInventoryAction;

	/** Toggle Shop Input Action - see ToggleShop() below. */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ToggleShopAction;

	/** Cycle Equipped Tool Input Action (Axis1D: -1 / +1 per press, the Left and Right arrow keys) - mission context only, see IMC_Mission */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* CycleToolAction;

	/** Tool Primary Use Input Action (e.g. Left Click) - mission context only */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ToolPrimaryUseAction;

	/** Tool Secondary Use Input Action (e.g. Right Click) - mission context only */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* ToolSecondaryUseAction;

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

	/** Place Input Action handler: confirms the Build Mode preview if active, otherwise instantly places a Prop */
	void PlaceInput();

	/** Rotate Preview Input Action handler */
	void RotatePreviewInput(const FInputActionValue& Value);

	/** Change Build Level Input Action handler */
	void ChangeBuildLevelInput(const FInputActionValue& Value);

	/** Change Roof Step Input Action handler */
	void ChangeRoofStepInput(const FInputActionValue& Value);

	/** Cycle Hotbar Selection Input Action handler */
	void CycleSelectionInput(const FInputActionValue& Value);

	/** Cycle Equipped Tool Input Action handler */
	void CycleToolInput(const FInputActionValue& Value);

	/** Grab Input Action handler - forwards to PhysicsManipulationComponent only while the toolbelt is locked (Lvl_Home); mission levels Grab through the toolbelt's Hands slot instead. */
	void LegacyGrabInput();

	/** Push Input Action handler - same gating as LegacyGrabInput. */
	void LegacyPushInput();

	/** Tool Primary Use Input Action handler - gates on CrushDetectionComponent->IsStunned() before forwarding to ToolbeltComponent::PrimaryUse, same gate-then-forward shape as LegacyGrabInput/LegacyPushInput. */
	void ToolPrimaryUseInput();

	/** Tool Primary Use (release) Input Action handler - same stun gate as ToolPrimaryUseInput. */
	void ToolReleasePrimaryUseInput();

	/** Tool Secondary Use Input Action handler - same stun gate as ToolPrimaryUseInput. */
	void ToolSecondaryUseInput();

	/**
	 *  Toggle Inventory Input Action handler - no C++ state to flip (nothing here owns widget
	 *  visibility), implement in the Blueprint to show/hide the inventory widget. Call
	 *  UHotbarComponent::ReturnCursorSlot() on close so a stack left on the cursor isn't lost.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Input")
	void ToggleInventory();

	/** Toggle Shop Input Action handler - same as ToggleInventory, implement in the Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Input")
	void ToggleShop();

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

	/** Returns BuildModeComponent subobject **/
	UBuildModeComponent* GetBuildModeComponent() const { return BuildModeComponent; }

	/** Returns ToolbeltComponent subobject **/
	UToolbeltComponent* GetToolbeltComponent() const { return ToolbeltComponent; }

	/** Returns ShopCartComponent subobject **/
	UShopCartComponent* GetShopCartComponent() const { return ShopCartComponent; }

	/** Returns CrushDetectionComponent subobject **/
	UCrushDetectionComponent* GetCrushDetectionComponent() const { return CrushDetectionComponent; }

	/**
	 *  Switches the building side of the game on or off in one call: Build Mode, and the hotbar's picking up,
	 *  buying and placing (see UBuildModeComponent::SetAllowed, UHotbarComponent::SetAllowed), and the paint brush
	 *  (UToolbeltComponent::SetPaintingAllowed: painting is part of building). A mission level
	 *  turns it off, leaving the tool belt and the hands as the only way to act on the house. On by default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player")
	void SetBuildingAllowed(bool bAllowed);

	/**
	 *  Switches the demolition side on or off: the charge and the melee tools can no longer be equipped (see
	 *  UToolbeltComponent::SetDestructionAllowed). Independent of SetBuildingAllowed and of the belt lock, so a map picks
	 *  any mix: the home allows building only, a renovation both, the hub neither. On by default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player")
	void SetDestructionAllowed(bool bAllowed);

	/**
	 *  Makes the tools something to buy: a melee tool is usable only once the player owns it, and a charge takes one from the stock
	 *  (see UToolbeltComponent::SetOwnershipEnforced). Off by default so a test map keeps every tool. Independent of the other switches.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player")
	void SetToolOwnershipEnforced(bool bEnforced);

	/** Debug console command (open the console with ~, type "ListShop" + Enter): prints every ShopCatalog row's index, display name and cost, so Buy's index is known without any UI. */
	UFUNCTION(Exec, Category = "Debug")
	void ListShop() const;

	/** Debug console command (console: "Buy <index>" + Enter): purchases the ShopCatalog row at ItemIndex (see ListShop for indices) - a stand-in for a real shop UI. */
	UFUNCTION(Exec, Category = "Debug")
	void Buy(int32 ItemIndex);

	/** Debug console command (console: "DebugDamageFocus <amount>" + Enter): applies Amount of damage to InteractionComponent's current focus if it implements IDamageable - a stand-in for an actual tool. */
	UFUNCTION(Exec, Category = "Debug")
	void DebugDamageFocus(float Amount);

	/** Debug console command (console: "DebugShowStability 1" or "0" + Enter): toggles the level's APlot's stability heat-map (UStructuralSupportComponent::SetShowDebug) - a stand-in for the GDD §18.8 pb.ShowStability proposal, scoped to just this one toggle for now. */
	UFUNCTION(Exec, Category = "Debug")
	void DebugShowStability(bool bShow);

	/** Debug console command (console: "DebugShowCollapseQueue 1" or "0" + Enter): toggles the level's APlot's on-screen collapse-queue readout (UStructuralSupportComponent::SetShowCollapseQueueDebug) - see GDD §7.7's staggered collapse. */
	UFUNCTION(Exec, Category = "Debug")
	void DebugShowCollapseQueue(bool bShow);

	/** Debug console command (console: "DebugShowFragmentCount 1" or "0" + Enter): toggles UDebrisSubsystem's on-screen active-fragments-vs-cap readout - GDD's debug table, "Fragment counter". */
	UFUNCTION(Exec, Category = "Debug")
	void DebugShowFragmentCount(bool bShow);

};
