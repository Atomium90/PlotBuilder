#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusCard.h"
#include "FocusCardWidget.generated.h"

class APlot;
class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UUIStyle;
class UWidget;
struct FItemLook;

/**
 *  The card of the thing under the crosshair: its name, its material and storey, its state and health, how well the tool in hand suits it,
 *  what a piece of furniture the client wants is worth now, and the take-back key where it does something. It reads what the interaction
 *  component focuses (the same trace as the E prompt), gathers the facts, and FocusCard::Describe turns them into the lines; this class
 *  only draws them. The Blueprint (WBP_FocusCard) lays the named widgets out; every row is optional except the Card panel.
 *
 *  The widget itself stays visible and never takes a click: only the Card panel is shown and hidden. A collapsed widget is not ticked, so
 *  one that hid itself could never show again. UGameHUDWidget creates it (FocusCardClass) and tells it when a screen is open.
 */
UCLASS(Abstract)
class UFocusCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** A screen is open over the game (the shop, the PC, the notebook...): the card steps aside. */
	void SetSuppressed(const bool bInSuppressed) { bSuppressed = bInSuppressed; }

protected:

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The panel that holds everything: shown when something is under the crosshair, collapsed otherwise. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Card;

	/** The border of the card: its outline turns coral for a piece in trouble. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Frame;

	/** The category icon, tinted with the category colour. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** "Brick · storey 1". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubText;

	/** The pill with the state, and the text inside it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StatePill;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	/** The health bar, its row, and the percentage next to it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BarRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> FillBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FillText;

	/** The tool line: a dot coloured by how good the match is, and the sentence. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToolRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ToolDot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ToolText;

	/** The warning of a piece about to fall. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> WarningRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WarningText;

	/** The value of a piece of furniture the client wants: now, and "of 400 at this health". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ValueRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueNowText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	/** The take-back row: the key chip and its word, drawn by the Blueprint. Shown only where taking back works. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptRow;

	/** The style asset (DA_UIStyle): the category colours and icons, the panel colours. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	TObjectPtr<UUIStyle> Style;

	/** Teal: intact, or a tool that suits the material. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor GoodColor = FLinearColor::FromSRGBColor(FColor(88, 201, 180));

	/** Amber: damaged, or a slow tool. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor WarnColor = FLinearColor::FromSRGBColor(FColor(224, 184, 114));

	/** Coral: critical, compromised, or a tool that barely scratches. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor BadColor = FLinearColor::FromSRGBColor(FColor(255, 107, 94));

	/** The outline of the card around a piece in trouble. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor BadFrameColor = FLinearColor::FromSRGBColor(FColor(122, 61, 58));

private:

	/** Reads the game: false when nothing with hit points is under the crosshair. */
	bool GatherFacts(FFocusCardFacts& OutFacts, FItemLook& OutLook);

	/** Draws the lines. */
	void Show(const FFocusCardView& View, const FItemLook& Look);

	FLinearColor ToneColor(EFocusTone Tone) const;

	static void SetRow(UWidget* Row, bool bShown);

	/** The plot of the level, found once and kept (it is where the mission's furniture list lives). */
	const APlot* FindPlot();

	bool bSuppressed = false;

	/** What was drawn last: the card is only redrawn when a line changes. */
	FString LastKey;

	TWeakObjectPtr<APlot> CachedPlot;
	double NextPlotSearch = 0.0;
};
