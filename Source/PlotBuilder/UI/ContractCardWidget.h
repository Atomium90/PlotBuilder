#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Contracts/ContractTypes.h"
#include "ContractCardWidget.generated.h"

class UBorder;
class UContractChipWidget;
class UImage;
class UPanelWidget;
class UTextBlock;
class UUIStyle;
class UWidget;
struct FClientRow;

DECLARE_DELEGATE_OneParam(FOnContractCardClicked, int32 /*Index*/);
DECLARE_DELEGATE_TwoParams(FOnContractCardHovered, int32 /*Index*/, bool /*bHovered*/);

/**
 *  One offer of the board, as a card: who asks, what kind of job and where, how long and how big, the conditions as chips, and the
 *  base payout. Hovering it tells the board to show its details; clicking it takes the contract. It does not move on hover (a card
 *  that lifted would leave the pointer and flicker): the frame changes colour instead, orange when hovered, green once taken.
 *
 *  The widget Blueprint only lays it out (the named widgets below are required), and names the chip class it repeats.
 */
UCLASS(Abstract)
class UContractCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fills the card. Index is the card's place on the board, handed back by the two delegates. Client may be null (the id is shown). */
	void SetContract(int32 InIndex, const FContractDefinition& Contract, const FClientRow* Client);

	/** The contract of this card is the one the player took. */
	void SetTaken(bool bInTaken);

	int32 GetIndex() const { return Index; }

	FOnContractCardClicked OnClicked;
	FOnContractCardHovered OnHoverChanged;

protected:

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditDefaultsOnly, Category = "Card")
	TObjectPtr<UUIStyle> Style;

	UPROPERTY(EditDefaultsOnly, Category = "Card")
	TSubclassOf<UContractChipWidget> ChipClass;

	/** The rounded background and outline of the card. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Frame;

	/** The coloured square behind the portrait. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> PortraitBackground;

	/** The letters shown on it while the client has no portrait. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PortraitInitials;

	/** The portrait itself, hidden while the client has none. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TypeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SiteText;

	/** The number of minutes, without the unit (the Blueprint writes "min" next to it). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MinutesText;

	/** The number of square metres, without the unit. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AreaText;

	/** Where the condition chips go (a wrap box, so they fall on a second line when there are four). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ChipHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PayoutText;

	/** "Base payout", and "Click to take" while the card is hovered. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HintText;

	/** The "TAKEN" badge. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> TakenBadge;

private:

	void RefreshFrame();

	int32 Index = INDEX_NONE;
	bool bHovered = false;
	bool bTaken = false;
};
