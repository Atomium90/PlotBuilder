#include "ContractCardWidget.h"
#include "ContractChipWidget.h"
#include "UIStyle.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WrapBoxSlot.h"
#include "Contracts/ClientRow.h"
#include "Contracts/ContractTexts.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"

void UContractCardWidget::SetContract(const int32 InIndex, const FContractDefinition& Contract, const FClientRow* Client)
{
	Index = InIndex;

	const FText Name = Client && !Client->DisplayName.IsEmpty() ? Client->DisplayName : FText::FromName(Contract.ClientId);

	NameText->SetText(Name);
	RoleText->SetText(Client ? Client->Role : FText::GetEmpty());
	TypeText->SetText(ContractTexts::TypeLabel(Contract.Type));
	SiteText->SetText(ContractTexts::SiteName(Contract));
	MinutesText->SetText(FText::FromString(ContractTexts::MinutesString(Contract.EstimatedMinutes)));
	AreaText->SetText(FText::AsNumber(FMath::RoundToInt(Contract.AreaM2)));
	PayoutText->SetText(FText::AsNumber(Contract.BasePayout));

	// A portrait if the client has one, else the initials on the client's colour.
	PortraitBackground->SetBrushColor(Client ? Client->Tint : FLinearColor(0.23f, 0.29f, 0.39f, 1.f));
	PortraitInitials->SetText(FText::FromString(ContractTexts::Initials(Name.ToString())));

	UTexture2D* Portrait = Client ? Client->Portrait.LoadSynchronous() : nullptr;

	if (PortraitImage)
	{
		if (Portrait)
		{
			PortraitImage->SetBrushFromTexture(Portrait);
		}

		PortraitImage->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	PortraitInitials->SetVisibility(Portrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	ChipHost->ClearChildren();

	if (ChipClass)
	{
		for (const ContractTexts::FConditionText& Condition : ContractTexts::Conditions(Contract))
		{
			if (UContractChipWidget* Chip = CreateWidget<UContractChipWidget>(this, ChipClass))
			{
				Chip->SetChip(Condition.Chip);
				ChipHost->AddChild(Chip);

				// The chips are side by side with a small gap and fall on a second line when there is no room.
				if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Chip->Slot))
				{
					WrapSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 6.f));
				}
			}
		}
	}

	bHovered = false;
	bTaken = false;
	RefreshFrame();
}

void UContractCardWidget::SetTaken(const bool bInTaken)
{
	bTaken = bInTaken;
	RefreshFrame();
}

void UContractCardWidget::RefreshFrame()
{
	TakenBadge->SetVisibility(bTaken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (!Style)
	{
		return;
	}

	// Orange when hovered, green once taken, a quiet outline otherwise. The card itself never moves.
	const FLinearColor Outline = bHovered ? Style->Accent : (bTaken ? Style->Success : Style->SlotBorder);
	Frame->SetBrush(UUIStyle::MakeRounded(Style->PanelBackground, 14.f, Outline, (bHovered || bTaken) ? 2.f : 1.f));

	HintText->SetText(bHovered ? NSLOCTEXT("Contract", "ClickToTake", "Click to take") : NSLOCTEXT("Contract", "BasePayout", "Base payout"));
	HintText->SetColorAndOpacity(FSlateColor(bHovered ? Style->Accent : Style->TextMuted));
}

void UContractCardWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	bHovered = true;
	RefreshFrame();
	OnHoverChanged.ExecuteIfBound(Index, true);
}

void UContractCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	bHovered = false;
	RefreshFrame();
	OnHoverChanged.ExecuteIfBound(Index, false);
}

FReply UContractCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnClicked.ExecuteIfBound(Index);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
