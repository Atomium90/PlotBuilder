#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ProgressChipWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 *  A ring that fills clockwise from the top. It draws itself (a circle track and an arc of the same width), so it needs no material and
 *  no texture. Give it a square size in the Blueprint (a SizeBox around it), then SetRatio.
 */
UCLASS()
class UProgressRingWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** 0 to 1. */
	void SetRatio(float NewRatio);

	float GetRatio() const { return Ratio; }

protected:

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** The part of the ring that is not filled. */
	UPROPERTY(EditAnywhere, Category = "Ring")
	FLinearColor TrackColor = FLinearColor(0.0185f, 0.0262f, 0.0423f, 1.f);

	/** The filled part (the orange of the accent). */
	UPROPERTY(EditAnywhere, Category = "Ring")
	FLinearColor FillColor = FLinearColor(1.f, 0.195f, 0.028f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Ring", meta = (ClampMin = "1"))
	float Thickness = 5.f;

	/** Straight segments the circle is drawn with: enough to look round at the size of a HUD chip. */
	UPROPERTY(EditAnywhere, Category = "Ring", meta = (ClampMin = "8"))
	int32 Segments = 72;

private:

	float Ratio = 0.f;
};

/**
 *  The progress chip of the mission HUD: a ring with the share of the house that is down and what it measures. It shows itself only in a
 *  mission and reads the same live data as the notebook (UMissionResultSubsystem::GetLiveNotebook, which is read twice a second at most,
 *  so a chip on screen all the time costs nothing). The widget Blueprint only lays it out (the named widgets below are required).
 */
UCLASS(Abstract)
class UProgressChipWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressRingWidget> Ring;

	/** The percentage inside the ring, without the sign. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PercentText;
};
