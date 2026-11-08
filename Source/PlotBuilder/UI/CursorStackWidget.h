#pragma once

#include "CoreMinimal.h"
#include "ItemSlotWidget.h"
#include "CursorStackWidget.generated.h"

class UUIStyle;
class UHotbarComponent;

/**
 *  The stack the cursor carries while the player moves items in the bag: one slot widget that shows the cursor's stack
 *  of UHotbarComponent with the same look as the other slots. It shows itself while the cursor holds something and hides
 *  itself otherwise, on its own: nothing has to tell it that the cursor changed. WBP_HUD only moves it under the pointer.
 *  The widget Blueprint only lays it out, like a slot's.
 */
UCLASS(Abstract)
class UCursorStackWidget : public UItemSlotWidget
{
	GENERATED_BODY()

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool ResolveStack(FInventorySlot& OutStack) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Cursor Stack")
	TObjectPtr<UUIStyle> StackStyle;

	/** Width and height in pixels, the size of a slot of the bag. */
	UPROPERTY(EditDefaultsOnly, Category = "Cursor Stack")
	float StackSize = 54.f;

private:

	bool TryBind();
	void Unbind();

	/** Redraws the stack and shows the widget only while the cursor holds one. */
	void Show();

	UFUNCTION()
	void HandleCursorChanged();

	UPROPERTY()
	TObjectPtr<UHotbarComponent> CursorHotbar;

	bool bBound = false;
};
