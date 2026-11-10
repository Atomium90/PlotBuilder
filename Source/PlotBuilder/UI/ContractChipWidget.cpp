#include "ContractChipWidget.h"
#include "Components/TextBlock.h"

void UContractChipWidget::SetChip(const FText& Text)
{
	LabelText->SetText(Text);
}

void UContractDetailWidget::SetDetail(const FText& Title, const FText& Detail)
{
	TitleText->SetText(Title);
	DetailText->SetText(Detail);
}
