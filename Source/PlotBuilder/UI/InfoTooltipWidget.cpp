#include "InfoTooltipWidget.h"
#include "Components/TextBlock.h"

void UInfoTooltipWidget::SetContent(const FText& Title, const FText& Body)
{
	TitleText->SetText(Title);
	BodyText->SetText(Body);
	BodyText->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}
