#include "DebriefWidget.h"
#include "DebriefStepWidget.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Contracts/ClientRow.h"
#include "Contracts/ContractTexts.h"
#include "Contracts/DebriefView.h"
#include "Input/Events.h"

namespace
{
	FText DebriefMoney(const int32 Amount, const bool bInfinite)
	{
		return bInfinite ? FText::FromString(FString(TEXT("∞"))) : FText::AsNumber(Amount);
	}
}

UDebriefWidget::UDebriefWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// E is the "Continue" of the screen; Escape closes it too (UGameScreenWidget).
	CloseKey = EKeys::E;
}

FReply UDebriefWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		if (!InKeyEvent.IsRepeat())
		{
			OnCloseRequested.Broadcast();
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDebriefWidget::ShowReport(const FDebriefResult& Result, const FNotebookData& Notebook, const FClientRow* Client, const int32 Paid, const int32 BalanceAfter, const bool bInfiniteMoney)
{
	const FContractDefinition& Contract = Notebook.Contract;

	// Who asked, and what the job was.
	const FText Name = Client && !Client->DisplayName.IsEmpty() ? Client->DisplayName : FText::FromName(Contract.ClientId);

	ClientNameText->SetText(Name);
	ClientDetailText->SetText(FText::Format(NSLOCTEXT("Debrief", "ClientDetail", "{0}  |  {1}, {2} m²"), ContractTexts::TypeLabel(Contract.Type), ContractTexts::SiteName(Contract), FText::AsNumber(FMath::RoundToInt(Contract.AreaM2))));
	PortraitBackground->SetBrushColor(Client ? Client->Tint : FLinearColor(0.23f, 0.29f, 0.39f, 1.f));
	PortraitInitials->SetText(FText::FromString(ContractTexts::Initials(Name.ToString())));

	PaidText->SetText(FText::AsNumber(Paid));

	// The waterfall.
	const FDebriefView View = MissionDebrief::Build(Result, Notebook);

	StepHost->ClearChildren();

	if (StepClass)
	{
		for (int32 Index = 0; Index < View.Steps.Num(); ++Index)
		{
			if (UDebriefStepWidget* Row = CreateWidget<UDebriefStepWidget>(this, StepClass))
			{
				// Every step but the last is joined to the next by a connector.
				Row->SetStep(View.Steps[Index], View.BasePayout, Index < View.Steps.Num() - 1, Index == View.BiggestLossIndex);
				StepHost->AddChild(Row);
			}
		}
	}

	// The cause that cost the most.
	if (View.Steps.IsValidIndex(View.BiggestLossIndex))
	{
		const FDebriefStep& Biggest = View.Steps[View.BiggestLossIndex];
		BiggestLossText->SetText(FText::Format(NSLOCTEXT("Debrief", "BiggestLoss", "{0}, {1} of {2}"), FText::FromString(Biggest.Name), FText::AsNumber(Biggest.Amount()), FText::AsNumber(View.BasePayout)));
	}
	else
	{
		BiggestLossText->SetText(NSLOCTEXT("Debrief", "NothingLost", "Nothing lost: a clean job"));
	}

	// The furniture the client asked for, piece by piece.
	FurnitureHost->ClearChildren();
	const FNotebookSection* Furniture = Notebook.FindSection(ENotebookSection::Furniture);

	FurnitureBox->SetVisibility(Furniture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (Furniture && LineClass)
	{
		FurnitureFigureText->SetText(FText::FromString(Furniture->Figure));

		for (const FNotebookLine& Line : Furniture->Lines)
		{
			if (UDebriefLineWidget* Row = CreateWidget<UDebriefLineWidget>(this, LineClass))
			{
				Row->SetLine(Line);
				FurnitureHost->AddChild(Row);
			}
		}
	}

	// The other terms of the contract: the zones, the room to keep, the forbidden tool, and what happened to the player.
	OtherHost->ClearChildren();
	int32 OtherCount = 0;

	if (LineClass)
	{
		for (const FNotebookSection& Section : Notebook.Sections)
		{
			if (Section.Kind == ENotebookSection::Furniture)
			{
				continue;
			}

			for (const FNotebookLine& Line : Section.Lines)
			{
				if (UDebriefLineWidget* Row = CreateWidget<UDebriefLineWidget>(this, LineClass))
				{
					Row->SetPlain(Line.Name, Line.State, Line.Status);
					OtherHost->AddChild(Row);
					++OtherCount;
				}
			}
		}

		if (Result.CrushEvents > 0 || Result.ExplosionEvents > 0)
		{
			if (UDebriefLineWidget* Row = CreateWidget<UDebriefLineWidget>(this, LineClass))
			{
				FString What = Result.CrushEvents > 0 ? FString::Printf(TEXT("Crushed %d time(s) by the collapse"), Result.CrushEvents) : FString();
				What += Result.ExplosionEvents > 0 ? FString::Printf(TEXT("%sCaught %d time(s) by an explosion"), What.IsEmpty() ? TEXT("") : TEXT(". "), Result.ExplosionEvents) : FString();

				Row->SetPlain(TEXT("Safety"), What, ENotebookState::Warning);
				OtherHost->AddChild(Row);
				++OtherCount;
			}
		}
	}

	OtherBox->SetVisibility(OtherCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// The account before and after.
	BalanceBeforeText->SetText(DebriefMoney(BalanceAfter - Paid, bInfiniteMoney));
	BalanceAfterText->SetText(DebriefMoney(BalanceAfter, bInfiniteMoney));
}
