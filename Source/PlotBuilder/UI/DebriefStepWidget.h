#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Contracts/DebriefView.h"
#include "Contracts/MissionNotebook.h"
#include "DebriefStepWidget.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UUIStyle;

/**
 *  One row of the debrief waterfall: the name of the step and what happened on the left, and on the right a bar that spans the
 *  money the step moves, on a track whose width is the base payout. Every position is worked out here in pixels from the figures,
 *  so the bars of the rows line up exactly and a loss starts where the one above it ended. A thin connector drops from the end of
 *  the bar to the next row.
 *
 *  The widget Blueprint lays out the two texts and a canvas panel (the track) holding the bar, its figure and the connector; those
 *  three are placed by this class, not by the Blueprint.
 */
UCLASS(Abstract)
class UDebriefStepWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Draws Step on a track that stands for Base money. bConnector adds the drop to the next row; bBiggest highlights the loss that cost the most. */
	void SetStep(const FDebriefStep& Step, int32 Base, bool bConnector, bool bBiggest);

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	TObjectPtr<UUIStyle> Style;

	/** Width in pixels of the track: the base payout is this long. */
	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	float TrackWidth = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	float BarTop = 11.f;

	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	float BarHeight = 24.f;

	/** Where the connector starts (just under the bar) and how long it is (down to the bar of the next row). */
	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	float ConnectorTop = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	float ConnectorHeight = 22.f;

	/** The thinnest a bar is drawn, so a step that cost almost nothing is still seen. */
	UPROPERTY(EditDefaultsOnly, Category = "Waterfall")
	float MinBarWidth = 3.f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailText;

	/** The bar. It sits in a canvas panel and is placed and sized here. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Bar;

	/** The figure of the step: inside the bar for a total, to the left of the bar for a loss. Also in the canvas panel. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FigureText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Connector;
};

/**
 *  One line of the right column of the debrief: a piece of furniture, a zone, the forbidden tool, with its state and, for furniture,
 *  the value the client counts for it. The widget Blueprint only lays it out.
 */
UCLASS(Abstract)
class UDebriefLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetLine(const FNotebookLine& Line);

	/** A line made of words only (the safety events). */
	void SetPlain(const FString& Name, const FString& State, ENotebookState Status);

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Line")
	TObjectPtr<UUIStyle> Style;

	/** A small coloured dot that gives the state at a glance. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> StateDot;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StateText;

	/** "300 / 400", for furniture. Empty and hidden for the others. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FigureText;

	/** A bar under the line for what is damaged (a zone, the room to keep): shown only when the line has a damage share. The notebook uses it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DamageBar;
};
