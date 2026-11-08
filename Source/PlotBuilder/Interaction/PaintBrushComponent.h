#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/PlayerMessages.h"
#include "PaintBrushComponent.generated.h"

class APlot;
class UHotbarComponent;
class UInteractionComponent;
class UPaintPalette;
class UToolbeltComponent;

/**
 *  What the paint brush (slot 5 of the tool belt) does. The belt only decides when the brush can be held and forwards the
 *  click here, the same way it forwards the Hands to UPhysicsManipulationComponent; this component knows the rest: what
 *  the player is looking at, which colour to apply and how to say what is wrong.
 *
 *  A left click paints the placed piece under the crosshair with the colour of the paint bucket selected in the hotbar
 *  (APaintBucket). Nothing is used up for now. While the brush is in hand a line on screen names the colour, or says
 *  what is missing (no bucket selected, no palette on the plot).
 *
 *  Which colour to paint with is decided in one place, ResolveColor. Today it reads the hotbar; if the player ever owns
 *  too many colours for the hotbar (a backpack of paints opened from the bucket), only that function changes.
 *  The secondary use of the brush (right click) is left free for such a menu.
 *
 *  Whether the brush can be held at all is not decided here: building turns it on and off for a map
 *  (APlayerCharacter::SetBuildingAllowed), and the belt lock turns it off with everything else.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UPaintBrushComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPaintBrushComponent();

	/** Primary use of the brush: paints the placed piece in focus, or tells the player why it cannot. */
	void Paint();

	/**
	 *  The colour a paint bucket class stands for in this plot's palette, for the shop and the inventory to draw it.
	 *  False when the class is not a bucket or the plot has no palette (the widgets then use the Paint category colour).
	 */
	bool GetBucketColor(TSubclassOf<AActor> BucketClass, FLinearColor& OutColor, FText& OutName) const;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	enum class EBrushState : uint8
	{
		/** The plot has no palette (or none is set): there is nothing to paint with. */
		NoPalette,
		/** The selected hotbar slot is not a paint bucket. */
		NoBucket,
		/** A colour is ready. */
		Ready
	};

	/**
	 *  The one place that decides which colour the brush holds. Ready fills OutPaletteIndex with an entry of the plot's
	 *  palette (already wrapped into range). Today the answer comes from the bucket selected in the hotbar.
	 */
	EBrushState ResolveColor(int32& OutPaletteIndex) const;

	const UPaintPalette* GetPalette() const;

	/** Keeps the on-screen line of the brush in step with whether it is in hand and which colour it holds. Only re-sets the text when something changed. */
	void UpdatePrompt();

	/** Shows a short message to the player, if the message service exists. */
	void Say(const FText& Text, EMessageTone Tone) const;

	UPROPERTY()
	TObjectPtr<UToolbeltComponent> ToolbeltComponent;

	UPROPERTY()
	TObjectPtr<UHotbarComponent> HotbarComponent;

	UPROPERTY()
	TObjectPtr<UInteractionComponent> InteractionComponent;

	/** The level's plot, if any. Resolved once in BeginPlay, like build mode does. */
	UPROPERTY()
	TObjectPtr<APlot> CachedPlot;

	/** Whether this component owns the on-screen line right now, and what it was written for (a palette entry, or a negative code for a missing piece). */
	bool bPromptShown = false;
	int32 LastPromptKey = INDEX_NONE;
};
