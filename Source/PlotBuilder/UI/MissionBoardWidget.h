#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MissionBoardWidget.generated.h"

class UContractCardWidget;
class UContractDetailWidget;
class UContractSubsystem;
class UPanelWidget;
class UTextBlock;
class UWidget;

/**
 *  The Contracts tab of the PC terminal: the offers of UContractSubsystem as a row of cards, and under the row a strip that shows the
 *  details of the card the pointer is on (the job, then one block per condition, up to four) or, when no card is hovered, a line of
 *  help. A click on a card takes the contract (UContractSubsystem::AcceptOffer); the player then walks to the truck.
 *
 *  The strip has a fixed height so nothing jumps when it changes, and a small notch that points at the hovered card. The widget
 *  Blueprint only lays it out (the named widgets below are required) and names the two small widget classes it repeats.
 */
UCLASS(Abstract)
class UMissionBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Builds the cards from the board as it is now. The terminal calls it when it opens. */
	void Refresh();

protected:

	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<UContractCardWidget> CardClass;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<UContractDetailWidget> DetailClass;

	/** Space between two cards. */
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float CardGap = 28.f;

	/** Half the width of the notch, so it can be centred on the card. */
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	float NotchHalfWidth = 7.f;

	/** A horizontal box: the cards go in it. The strip under it has the same left edge and width, so the notch can be placed from it. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> CardRow;

	/** The strip when a card is hovered. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> DetailBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> JobText;

	/** Where the condition blocks go. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ConditionHost;

	/** Shown instead of the blocks when the contract has no condition. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> NoConditionText;

	/** The small triangle on top of the strip. Moved under the hovered card. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Notch;

	/** The strip when no card is hovered: a line of help. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> IdleBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> IdleText;

private:

	void HandleCardClicked(int32 Index);
	void HandleCardHovered(int32 Index, bool bHovered);

	/** The board changed (a new draw, a contract taken): the cards are built again. */
	void HandleBoardChanged();

	void ShowDetails(int32 Index);
	void ShowIdle();

	/** Marks the card of the active contract as taken. */
	void RefreshTaken();

	/** Index of the offer that is the active contract, or INDEX_NONE. */
	int32 FindTakenIndex() const;

	UPROPERTY()
	TArray<TObjectPtr<UContractCardWidget>> Cards;

	TWeakObjectPtr<UContractSubsystem> Contracts;

	int32 HoveredIndex = INDEX_NONE;
};
