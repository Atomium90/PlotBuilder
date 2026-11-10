#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameHUDWidget.generated.h"

class UControlsHintWidget;
class UCursorStackWidget;
class UDebriefWidget;
class UHotbarComponent;
class UInventoryWidget;
class ULeaveConfirmWidget;
class UMissionNotebookWidget;
class UPCTerminalWidget;
class UShopWidget;
class UTextBlock;
class UWidget;
struct FNotebookData;

/**
 *  The host of everything the player sees on top of the game: the money, the controls hint, the cursor stack, and the screens
 *  that open over the game one at a time (the shop, the bag, then the PC terminal, the notebook, the debrief...). It is the
 *  C++ base of WBP_HUD, which only lays the widgets out (the named widgets below are required); the logic that used to live in
 *  its graph is here.
 *
 *  The controller Blueprint calls ToggleMenu(Menu) and ToggleHelp() on it by name, so those two keep their names and their
 *  parameter. A menu is named by an FName ("Shop", "Inventory"...). Opening a screen puts the game in UI-only input, shows the
 *  cursor and ignores move and look; closing it undoes all of that. This is the only place that changes the input mode, so two
 *  screens can never fight over it. Only one screen is open at a time: opening another closes the first.
 *
 *  Everything else of the HUD (the hotbar, the tool belt, the prompts) draws and updates itself and is not touched here.
 */
UCLASS(Abstract)
class UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** The names of the menus. The controller passes the first two; the others are opened by the thing they belong to (the PC, the hub). */
	static const FName ShopMenuName;
	static const FName InventoryMenuName;
	static const FName TerminalMenuName;
	static const FName DebriefMenuName;
	static const FName NotebookMenuName;
	static const FName LeaveMenuName;

	/** The HUD of the first local player in WorldContext's world, or nullptr (no HUD yet, or a world without one). Not for per-frame use. */
	static UGameHUDWidget* Find(const UObject* WorldContext);

	/** Opens the menu, or closes it if it is the one that is open. The shop and the bag do nothing on a map where building is off. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleMenu(FName Menu);

	/** Opens the menu (closing the one that is open). Does nothing, with a warning, for a name no screen has. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void OpenMenu(FName Menu);

	/** Closes the open menu, if any: the cursor stack goes back to the bag, the game gets its input back. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void CloseMenu();

	/** Opens or folds the controls card. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleHelp();

	/** Redraws the money chip from the player's hotbar. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdateMoney();

	/**
	 *  Fills the debrief with the report of the last mission and opens it. By default only a report that has not been shown yet is
	 *  shown, once; with bOnlyIfUnseen false the last report is shown again (pb.ShowDebrief). False when there is none or no debrief screen.
	 */
	bool ShowDebrief(bool bOnlyIfUnseen = true);

	/**
	 *  Opens the "Leave the site?" card on that notebook; OnLeave runs if the player answers Leave (staying only closes the card). False, nothing
	 *  opened, when this HUD has no such card: the caller then falls back to its own warning.
	 */
	bool AskLeave(const FNotebookData& Data, const FSimpleDelegate& OnLeave);

	bool IsMenuOpen() const { return !CurrentMenu.IsNone(); }

	FName GetCurrentMenu() const { return CurrentMenu; }

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopWidget> ShopMenu;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInventoryWidget> InventoryMenu;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UControlsHintWidget> HelpWidget;

	/** The stack the cursor carries in the bag. It sits in a canvas panel and is moved under the pointer while a menu is open. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCursorStackWidget> CursorStackWidget;

	/** The number in the money chip (the infinity sign when money is infinite). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MoneyText;

	/** The PC terminal (contracts, tools, parcel). Optional so a HUD without it still works. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPCTerminalWidget> TerminalMenu;

	/** The debrief shown at the hub. Optional too. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UDebriefWidget> DebriefMenu;

	/** The mission notebook (N). Optional. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMissionNotebookWidget> NotebookMenu;

	/** The "Leave the site?" card. Optional. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULeaveConfirmWidget> LeaveMenu;

private:

	/** Finds the player's hotbar and listens to its money. False while the pawn does not exist yet (the HUD is built before it). */
	bool TryBind();
	void Unbind();

	/** The player's hotbar component, or nullptr. */
	UHotbarComponent* FindHotbar() const;

	/** Moves the cursor stack under the pointer. */
	void FollowPointer() const;

	UFUNCTION()
	void HandleMoneyChanged();

	/** A screen asked to be closed (Escape, or its own key). */
	UFUNCTION()
	void HandleCloseRequested();

	/** The widget of a menu name, or nullptr. */
	UUserWidget* FindScreen(FName Menu) const;

	/** Which menu is open now, or none. */
	FName CurrentMenu;

	UPROPERTY()
	TObjectPtr<UHotbarComponent> Hotbar;

	bool bBound = false;
};
