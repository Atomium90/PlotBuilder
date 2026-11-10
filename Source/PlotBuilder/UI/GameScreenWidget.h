#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "GameScreenWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScreenCloseRequested);

/**
 *  The base of the screens the HUD hosts one at a time (the PC terminal, the mission notebook, the debrief, the leave-the-site
 *  card). The HUD opens and closes them (UGameHUDWidget::OpenMenu) and is the only place that changes the input mode; a screen
 *  only says when it wants to be closed. Escape does by default, and CloseKey adds the key that opened it (N for the notebook).
 *
 *  Like the shop, it ignores the auto-repeat of those keys: the key that opened the screen may still be held, and its repeat must
 *  not close it again and flicker.
 *
 *  The widget Blueprint (or the derived C++ class) only lays the screen out.
 */
UCLASS(Abstract)
class UGameScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fired when the player presses Escape or CloseKey. The HUD closes the screen. */
	UPROPERTY(BlueprintAssignable, Category = "Screen")
	FOnScreenCloseRequested OnCloseRequested;

	/** The HUD has just shown the screen: read fresh data here. */
	virtual void OnScreenOpened() {}

	/** The HUD is about to hide the screen. */
	virtual void OnScreenClosed() {}

protected:

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** A second key that closes the screen, besides Escape. Invalid (none) by default. */
	UPROPERTY(EditDefaultsOnly, Category = "Screen")
	FKey CloseKey;

	/** Escape closes the screen. Off for a screen that treats Escape as one of its own answers (the leave card: Escape means stay). */
	UPROPERTY(EditDefaultsOnly, Category = "Screen")
	bool bCloseOnEscape = true;
};
