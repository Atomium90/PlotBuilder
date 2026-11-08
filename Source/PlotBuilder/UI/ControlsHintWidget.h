#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ControlsHintWidget.generated.h"

class UWidget;

/**
 *  The controls reminder in the top right corner: a card with the keys grouped by what they do, and, while it is
 *  folded, only a small "H  Controls" chip. It starts folded. H opens and folds it. The widget Blueprint only lays it
 *  out (the card, the chip, the groups); which groups show comes from what the map allows, read when the card opens.
 *
 *  This is a stand-in for a real onboarding and is meant to be thrown away, so it stays this simple.
 */
UCLASS(Abstract)
class UControlsHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 *  WBP_HUD's ToggleHelp function (called on H) still shows and hides this widget with SetVisibility, so that
	 *  Blueprint is left as it was: Collapsed or Hidden folds the card, any other value opens it. The widget itself
	 *  always stays on screen, because the folded state is the chip.
	 */
	virtual void SetVisibility(ESlateVisibility InVisibility) override;

	/** Opens or folds the card. */
	void SetOpen(bool bInOpen);

	bool IsOpen() const { return bOpen; }

protected:

	virtual void NativeConstruct() override;

	/** The full card with every group. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel;

	/** The small chip shown while the card is folded. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Reminder;

	/** The tool keys: hidden on a map where the tool belt is locked (the hub). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ToolsGroup;

	/** The build keys: hidden on a map where building is off (a mission). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BuildGroup;

private:

	/** Shows or hides the groups the current map has no use for. */
	void RefreshGroups();

	bool bOpen = false;
};
