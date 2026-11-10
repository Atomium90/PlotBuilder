#pragma once

#include "CoreMinimal.h"
#include "GameScreenWidget.h"
#include "Contracts/ContractTypes.h"
#include "Contracts/MissionNotebook.h"
#include "DebriefWidget.generated.h"

class UBorder;
class UDebriefLineWidget;
class UDebriefStepWidget;
class UPanelWidget;
class UTextBlock;
class UUIStyle;
class UWidget;
struct FClientRow;

/**
 *  The client's report, shown at the hub when the player comes back from a mission. On the left the waterfall: it starts at the base
 *  payout and walks down, one step per cause, to what is paid, so it is plain where the money was lost. On the right, the cause that
 *  cost the most, the furniture the client asked for with its value, and the other terms of the contract (zones, the room to keep,
 *  the forbidden tool, safety). At the bottom the balance before and after.
 *
 *  The HUD fills it (UGameHUDWidget::ShowDebrief) and opens it; E, Enter or Escape closes it. The widget Blueprint only lays it out
 *  (the named widgets below are required) and names the two row classes it repeats.
 */
UCLASS(Abstract)
class UDebriefWidget : public UGameScreenWidget
{
	GENERATED_BODY()

public:

	UDebriefWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 *  Fills the screen from a report. Notebook is the final notebook of the mission (it lists the fate of each piece and zone), Client the
	 *  row of the client (may be null). Paid is what really reached the account, BalanceAfter the account now; with bInfiniteMoney
	 *  the balance is shown as the infinity sign.
	 */
	void ShowReport(const FDebriefResult& Result, const FNotebookData& Notebook, const FClientRow* Client, int32 Paid, int32 BalanceAfter, bool bInfiniteMoney);

protected:

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief")
	TObjectPtr<UUIStyle> Style;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief")
	TSubclassOf<UDebriefStepWidget> StepClass;

	UPROPERTY(EditDefaultsOnly, Category = "Debrief")
	TSubclassOf<UDebriefLineWidget> LineClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> PortraitBackground;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PortraitInitials;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClientNameText;

	/** "Total demolition, Two-storey house, 150 m²". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClientDetailText;

	/** What was paid. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PaidText;

	/** The waterfall rows go in it (a vertical box). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> StepHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BiggestLossText;

	/** The furniture block: hidden when the contract asked for none. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> FurnitureBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FurnitureFigureText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> FurnitureHost;

	/** The block of the other terms: hidden when there are none. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> OtherBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> OtherHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BalanceBeforeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BalanceAfterText;
};
