#include "MissionBoardWidget.h"
#include "ContractCardWidget.h"
#include "ContractChipWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Contracts/ContractSubsystem.h"
#include "Contracts/ContractTexts.h"
#include "Contracts/ClientRow.h"
#include "PlotBuilder.h"

void UMissionBoardWidget::NativeDestruct()
{
	if (UContractSubsystem* Board = Contracts.Get())
	{
		Board->OnOffersChanged.RemoveAll(this);
		Board->OnActiveContractChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMissionBoardWidget::Refresh()
{
	UContractSubsystem* Board = UContractSubsystem::Get(this);

	if (!Board)
	{
		return;
	}

	// Listen once: the board is redrawn when a new set of offers is drawn, and the "taken" mark follows the active contract.
	if (Contracts.Get() != Board)
	{
		Contracts = Board;
		Board->OnOffersChanged.AddUObject(this, &UMissionBoardWidget::HandleBoardChanged);
		Board->OnActiveContractChanged.AddUObject(this, &UMissionBoardWidget::RefreshTaken);
	}

	HandleBoardChanged();
}

void UMissionBoardWidget::HandleBoardChanged()
{
	CardRow->ClearChildren();
	Cards.Reset();
	HoveredIndex = INDEX_NONE;

	const UContractSubsystem* Board = Contracts.Get();

	if (!Board || !CardClass)
	{
		ShowIdle();
		return;
	}

	const TArray<FContractDefinition>& Offers = Board->GetOffers();

	for (int32 Index = 0; Index < Offers.Num(); ++Index)
	{
		UContractCardWidget* Card = CreateWidget<UContractCardWidget>(this, CardClass);

		if (!Card)
		{
			continue;
		}

		Card->SetContract(Index, Offers[Index], Board->FindClient(Offers[Index].ClientId));
		Card->OnClicked.BindUObject(this, &UMissionBoardWidget::HandleCardClicked);
		Card->OnHoverChanged.BindUObject(this, &UMissionBoardWidget::HandleCardHovered);

		CardRow->AddChild(Card);
		Cards.Add(Card);

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Card->Slot))
		{
			BoxSlot->SetPadding(FMargin(Index > 0 ? CardGap * 0.5f : 0.f, 0.f, Index < Offers.Num() - 1 ? CardGap * 0.5f : 0.f, 0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Top);
		}
	}

	RefreshTaken();
	ShowIdle();
}

int32 UMissionBoardWidget::FindTakenIndex() const
{
	const UContractSubsystem* Board = Contracts.Get();

	if (!Board || !Board->HasActiveContract())
	{
		return INDEX_NONE;
	}

	const FContractDefinition& Active = Board->GetActiveContract();
	const TArray<FContractDefinition>& Offers = Board->GetOffers();

	// An offer is the active contract when it is the same job: same house seed, same client, same type.
	for (int32 Index = 0; Index < Offers.Num(); ++Index)
	{
		if (Offers[Index].Seed == Active.Seed && Offers[Index].ClientId == Active.ClientId && Offers[Index].Type == Active.Type)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UMissionBoardWidget::RefreshTaken()
{
	const int32 Taken = FindTakenIndex();

	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		Cards[Index]->SetTaken(Index == Taken);
	}

	if (HoveredIndex == INDEX_NONE)
	{
		ShowIdle();
	}
}

void UMissionBoardWidget::HandleCardClicked(const int32 Index)
{
	if (UContractSubsystem* Board = Contracts.Get())
	{
		Board->AcceptOffer(Index);
	}
}

void UMissionBoardWidget::HandleCardHovered(const int32 Index, const bool bHovered)
{
	if (bHovered)
	{
		HoveredIndex = Index;
		ShowDetails(Index);
	}
	else if (HoveredIndex == Index)
	{
		HoveredIndex = INDEX_NONE;
		ShowIdle();
	}
}

void UMissionBoardWidget::ShowDetails(const int32 Index)
{
	const UContractSubsystem* Board = Contracts.Get();

	if (!Board || !Board->GetOffers().IsValidIndex(Index) || !DetailClass)
	{
		return;
	}

	const FContractDefinition& Contract = Board->GetOffers()[Index];

	JobText->SetText(ContractTexts::Job(Contract));

	ConditionHost->ClearChildren();
	const TArray<ContractTexts::FConditionText> Conditions = ContractTexts::Conditions(Contract);

	for (const ContractTexts::FConditionText& Condition : Conditions)
	{
		if (UContractDetailWidget* Block = CreateWidget<UContractDetailWidget>(this, DetailClass))
		{
			Block->SetDetail(Condition.Title, Condition.Detail);
			ConditionHost->AddChild(Block);
		}
	}

	if (NoConditionText)
	{
		NoConditionText->SetVisibility(Conditions.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// The notch points at the middle of the hovered card, wherever the cards sit in the row. It is measured from the row, which is always
	// laid out (the strip may have been collapsed until now) and has the same left edge as the strip.
	if (Notch && Cards.IsValidIndex(Index))
	{
		const FGeometry& CardGeometry = Cards[Index]->GetCachedGeometry();
		const FVector2D CardCentre = CardGeometry.GetAbsolutePosition() + CardGeometry.GetAbsoluteSize() * 0.5;
		const FVector2D Local = CardRow->GetCachedGeometry().AbsoluteToLocal(CardCentre);

		Notch->SetRenderTranslation(FVector2D(Local.X - NotchHalfWidth, 0.0));
	}

	DetailBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	IdleBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UMissionBoardWidget::ShowIdle()
{
	const UContractSubsystem* Board = Contracts.Get();
	const int32 Taken = FindTakenIndex();

	if (Taken != INDEX_NONE && Board)
	{
		const FContractDefinition& Contract = Board->GetOffers()[Taken];
		const FClientRow* Client = Board->FindClient(Contract.ClientId);
		const FText Name = Client && !Client->DisplayName.IsEmpty() ? Client->DisplayName : FText::FromName(Contract.ClientId);

		IdleText->SetText(FText::Format(NSLOCTEXT("Contract", "BoardTaken", "{0} taken. Walk to the truck, or click another card to change."), Name));
	}
	else
	{
		IdleText->SetText(NSLOCTEXT("Contract", "BoardIdle", "Hover a card for the details. Click one to take the contract, then walk to the truck."));
	}

	DetailBox->SetVisibility(ESlateVisibility::Collapsed);
	IdleBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}
