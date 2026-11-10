#pragma once

#include "CoreMinimal.h"
#include "GameScreenWidget.h"
#include "PCTerminalWidget.generated.h"

class UButton;
class UMissionBoardWidget;
class UParcelTabWidget;
class UTextBlock;
class UToolsTabWidget;
class UWidget;
class UWidgetSwitcher;

/**
 *  The PC of the office: one window with three tabs, Contracts, Tools and Parcel, and the balance on the right of the tab bar. Each tab
 *  is a widget of its own in the same switcher (the board, UToolsTabWidget, UParcelTabWidget); this window opens them, and redraws the
 *  balance when one of them made a purchase.
 *
 *  The HUD opens it (ATerminal::Use). It always opens on the Contracts tab with a fresh board. The widget Blueprint only lays it out
 *  (the named widgets below are required).
 */
UCLASS(Abstract)
class UPCTerminalWidget : public UGameScreenWidget
{
	GENERATED_BODY()

public:

	virtual void OnScreenOpened() override;

protected:

	virtual void NativeOnInitialized() override;

	/** The three tab contents, in the order of the tabs: Contracts, Tools, Parcel. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ContractsTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ToolsTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ParcelTab;

	/** The orange line under the active tab. Each is shown only while its tab is the active one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ContractsMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToolsMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ParcelMark;

	/** The Contracts tab, the first content of the switcher. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMissionBoardWidget> MissionBoard;

	/** The Tools tab. Optional so a window laid out before the tab existed still works (its page is then a placeholder). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UToolsTabWidget> ToolsPage;

	/** The Parcel tab. Optional for the same reason. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UParcelTabWidget> ParcelPage;

	/** The player's money (the infinity sign when it is infinite). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BalanceText;

private:

	void SelectTab(int32 Index);
	void RefreshBalance();

	UFUNCTION()
	void HandleContractsTab();

	UFUNCTION()
	void HandleToolsTab();

	UFUNCTION()
	void HandleParcelTab();
};
