#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlotBuilderHUD.generated.h"

/**
 *  Draws what UPlayerMessages holds: the prompt a little below the crosshair and the short messages near the
 *  bottom, centred, with a shadow so they read on any background. Plain canvas text on purpose: it needs no asset,
 *  works in every build and leaves WBP_HUD (money, hotbar) alone. APlotBuilderGameMode sets it as the HUD class.
 *
 *  Sizes follow the height of the screen (1080 is the reference), so the text keeps its proportions at any resolution.
 */
UCLASS()
class APlotBuilderHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

private:

	/** Screen height the sizes below are written for. */
	static constexpr float ReferenceHeight = 1080.f;

	/** Vertical position of the prompt and of the first message, as a share of the screen height. */
	static constexpr float PromptHeightShare = 0.60f;
	static constexpr float ToastHeightShare = 0.78f;

	/** Space between two lines, in lines of text. */
	static constexpr float LineSpacing = 1.35f;

	/** Size of the text at the reference height. */
	static constexpr float TextScale = 1.15f;
};
