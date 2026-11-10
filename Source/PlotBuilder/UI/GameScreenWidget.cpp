#include "GameScreenWidget.h"
#include "Input/Events.h"

void UGameScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The HUD gives the keyboard focus to the screen it opens; without this the screen would never see a key.
	SetIsFocusable(true);
}

FReply UGameScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bIsCloseKey = (bCloseOnEscape && Key == EKeys::Escape) || (CloseKey.IsValid() && Key == CloseKey);

	if (bIsCloseKey)
	{
		// The key that opened the screen may still be held: its auto-repeat must not close the screen again (see UShopWidget).
		if (!InKeyEvent.IsRepeat())
		{
			OnCloseRequested.Broadcast();
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
