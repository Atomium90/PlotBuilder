#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameScreenWidget.h"
#include "Contracts/MissionNotebook.h"
#include "MissionNotebookWidget.generated.h"

class UBorder;
class UButton;
class UContractChipWidget;
class UDebriefLineWidget;
class UProgressBar;
class UPanelWidget;
class UTextBlock;

/**
 *  One group of lines of the notebook (the furniture to recover, a protected zone, the forbidden tool): a title, a count and the lines. It keeps
 *  its line widgets and only rewrites them when the data changes, so a scrolled list stays where the player left it.
 */
UCLASS(Abstract)
class UNotebookSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetSection(const FNotebookSection& Section);

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Notebook")
	TSubclassOf<UDebriefLineWidget> LineClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** Where the lines go. The Blueprint puts a scroll box here for the furniture list (5 lines visible, a scroll bar for the rest). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> LineHost;

private:

	UPROPERTY()
	TArray<TObjectPtr<UDebriefLineWidget>> Lines;
};

/**
 *  The mission notebook, opened with N at the middle of the screen. The left page is the contract: who asks, what the job is, the time, the
 *  base payout, what the client also asks. The right page is where the player stands, live: the share of the house that is down, then one section
 *  per thing the contract asks for. The game goes on while it is open.
 *
 *  Everything it shows comes from UMissionResultSubsystem::GetLiveNotebook (read twice a second at most) and from ContractTexts, so it says what the
 *  report will say. The widget Blueprint only lays it out (the named widgets below are required) and names the three small widget classes it repeats.
 */
UCLASS(Abstract)
class UMissionNotebookWidget : public UGameScreenWidget
{
	GENERATED_BODY()

public:

	UMissionNotebookWidget(const FObjectInitializer& ObjectInitializer);

	virtual void OnScreenOpened() override;

protected:

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Notebook")
	TSubclassOf<UNotebookSectionWidget> SectionClass;

	/** One line of "the client also asks" (a plain chip Blueprint with no frame). */
	UPROPERTY(EditDefaultsOnly, Category = "Notebook")
	TSubclassOf<UContractChipWidget> AskClass;

	// ---- the contract page
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> PortraitBackground;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PortraitInitials;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClientNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ClientRoleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TypeText;

	/** "Two-storey house, 150 m². Bring all of it down." */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EstimatedText;

	/** Time spent on site, minutes and seconds. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OnSiteText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PayoutText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> AsksHost;

	// ---- the live page
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CompletionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CompletionBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> SectionHost;

private:

	/** Writes the contract page. It does not change during a mission, so it is written once, when the notebook opens. */
	void BuildContractPage(const FNotebookData& Data);

	/** Writes the live page from the cached data. */
	void RefreshLivePage();

	UPROPERTY()
	TArray<TObjectPtr<UNotebookSectionWidget>> Sections;

	/** The kinds of the sections on screen, to tell when the contract's sections changed and the widgets must be made again. */
	TArray<ENotebookSection> SectionKinds;
};

/**
 *  The "Leave the site?" card, shown at the truck when the job is not finished. It lists, section by section, what is missing, as states (no money
 *  here: the debrief does that). Escape or the Keep working button stays; E or the Leave button leaves. The HUD opens it (UGameHUDWidget::AskLeave).
 */
UCLASS(Abstract)
class ULeaveConfirmWidget : public UGameScreenWidget
{
	GENERATED_BODY()

public:

	ULeaveConfirmWidget(const FObjectInitializer& ObjectInitializer);

	/** Fills the card from the notebook. OnLeave runs if the player leaves; staying only closes the card. */
	void Show(const FNotebookData& Data, const FSimpleDelegate& InOnLeave);

protected:

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, Category = "Leave")
	TSubclassOf<UDebriefLineWidget> RowClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CompletionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CompletionBar;

	/** One row per section of the notebook: a name and what it comes to. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowHost;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeaveButton;

private:

	UFUNCTION()
	void HandleStay();

	UFUNCTION()
	void HandleLeave();

	FSimpleDelegate OnLeave;
};
