#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PlotBuilderHUD.generated.h"

/**
 *  Draws what UPlayerMessages holds: the prompt just above the hotbar and the short messages stacked above it,
 *  centred, with a shadow so they read on any background. Plain canvas text on purpose: it needs no asset,
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

	/**
	 *  Vertical position of the prompt, as a share of the screen height, used only while no hotbar says where it begins
	 *  (UPlayerMessages::GetBottomLimit). The short messages stack upwards from the line above it, the newest nearest the prompt.
	 */
	static constexpr float PromptHeightShare = 0.90f;

	/** Space between the prompt and the widgets below it, at the reference height. */
	static constexpr float PromptGap = 10.f;

	/** Space between two lines, in lines of text. */
	static constexpr float LineSpacing = 1.35f;

	/** Size of the text at the reference height. */
	static constexpr float TextScale = 1.15f;
};
