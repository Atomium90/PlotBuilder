#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InfoTooltipWidget.generated.h"

class UTextBlock;

/** The small card shown while the pointer rests on a slot or a tile: a title and a few lines under it. The widget Blueprint only lays it out. */
UCLASS(Abstract)
class UInfoTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetContent(const FText& Title, const FText& Body);

protected:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BodyText;
};
