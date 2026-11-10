#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContractChipWidget.generated.h"

class UTextBlock;

/** A small chip with one short text: a condition of a contract on its card ("3 pieces", "No jackhammer"). The widget Blueprint only lays it out. */
UCLASS(Abstract)
class UContractChipWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetChip(const FText& Text);

protected:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;
};

/** One condition in the details strip: its title and the rule in a sentence or two. The widget Blueprint only lays it out. */
UCLASS(Abstract)
class UContractDetailWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetDetail(const FText& Title, const FText& Detail);

protected:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailText;
};
