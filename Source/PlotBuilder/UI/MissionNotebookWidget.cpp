#include "MissionNotebookWidget.h"
#include "ContractChipWidget.h"
#include "DebriefStepWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Contracts/ClientRow.h"
#include "Contracts/ContractSubsystem.h"
#include "Contracts/ContractTexts.h"
#include "Contracts/MissionResultSubsystem.h"
#include "Engine/World.h"
#include "Input/Events.h"

namespace
{
	/** Seconds as minutes and seconds ("04:12"). */
	FString NotebookClock(const double Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}
}

void UNotebookSectionWidget::SetSection(const FNotebookSection& Section)
{
	TitleText->SetText(FText::FromString(Section.Title));

	if (CountText)
	{
		CountText->SetText(FText::FromString(Section.Count));
	}

	if (!LineClass)
	{
		return;
	}

	// Keep the widgets that are still needed: a scrolled list must not jump back to the top twice a second.
	while (Lines.Num() > Section.Lines.Num())
	{
		Lines.Pop()->RemoveFromParent();
	}

	while (Lines.Num() < Section.Lines.Num())
	{
		UDebriefLineWidget* Row = CreateWidget<UDebriefLineWidget>(this, LineClass);

		if (!Row)
		{
			return;
		}

		LineHost->AddChild(Row);
		Lines.Add(Row);
	}

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		Lines[Index]->SetLine(Section.Lines[Index]);
	}
}

UMissionNotebookWidget::UMissionNotebookWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// N opens it, so N closes it too (Escape does as for every screen).
	CloseKey = EKeys::N;
}

void UMissionNotebookWidget::OnScreenOpened()
{
	Super::OnScreenOpened();

	UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this);

	if (!Results)
	{
		return;
	}

	BuildContractPage(Results->GetLiveNotebook(true));
	RefreshLivePage();
}

void UMissionNotebookWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsVisible())
	{
		RefreshLivePage();
	}
}

void UMissionNotebookWidget::BuildContractPage(const FNotebookData& Data)
{
	const FContractDefinition& Contract = Data.Contract;

	const UContractSubsystem* Contracts = UContractSubsystem::Get(this);
	const FClientRow* Client = Contracts ? Contracts->FindClient(Contract.ClientId) : nullptr;
	const FText Name = Client && !Client->DisplayName.IsEmpty() ? Client->DisplayName : FText::FromName(Contract.ClientId);

	ClientNameText->SetText(Name);
	ClientRoleText->SetText(Client ? Client->Role : FText::GetEmpty());
	PortraitBackground->SetBrushColor(Client ? Client->Tint : FLinearColor(0.04f, 0.07f, 0.12f, 1.f));
	PortraitInitials->SetText(FText::FromString(ContractTexts::Initials(Name.ToString())));
	TypeText->SetText(ContractTexts::TypeLabel(Contract.Type));
	SummaryText->SetText(ContractTexts::Summary(Contract));
	EstimatedText->SetText(FText::Format(NSLOCTEXT("Notebook", "Estimated", "~{0} min"), FText::FromString(ContractTexts::MinutesString(Contract.EstimatedMinutes))));
	PayoutText->SetText(FText::AsNumber(Contract.BasePayout));

	// What the client also asks, one line each.
	AsksHost->ClearChildren();

	if (AskClass)
	{
		for (const ContractTexts::FConditionText& Condition : ContractTexts::Conditions(Contract))
		{
			if (UContractChipWidget* Ask = CreateWidget<UContractChipWidget>(this, AskClass))
			{
				Ask->SetChip(Condition.Detail);
				AsksHost->AddChild(Ask);
			}
		}
	}
}

void UMissionNotebookWidget::RefreshLivePage()
{
	UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this);

	if (!Results)
	{
		return;
	}

	const FNotebookData& Data = Results->GetLiveNotebook();

	CompletionText->SetText(FText::Format(NSLOCTEXT("Notebook", "Completion", "{0} %"), FText::AsNumber(FMath::FloorToInt(Data.Completion * 100.f))));
	CompletionBar->SetPercent(Data.Completion);

	const UWorld* World = GetWorld();
	OnSiteText->SetText(FText::FromString(NotebookClock(World ? World->GetTimeSeconds() : 0.0)));

	// The sections are made again only if the contract's sections are not the ones on screen.
	TArray<ENotebookSection> Kinds;

	for (const FNotebookSection& Section : Data.Sections)
	{
		Kinds.Add(Section.Kind);
	}

	if (Kinds != SectionKinds && SectionClass)
	{
		SectionHost->ClearChildren();
		Sections.Reset();
		SectionKinds = Kinds;

		for (int32 Index = 0; Index < Kinds.Num(); ++Index)
		{
			if (UNotebookSectionWidget* Section = CreateWidget<UNotebookSectionWidget>(this, SectionClass))
			{
				SectionHost->AddChild(Section);
				Sections.Add(Section);
			}
		}
	}

	for (int32 Index = 0; Index < Sections.Num() && Index < Data.Sections.Num(); ++Index)
	{
		Sections[Index]->SetSection(Data.Sections[Index]);
	}
}

ULeaveConfirmWidget::ULeaveConfirmWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Escape means "keep working" here and is handled below; the card closes itself through the two answers.
	bCloseOnEscape = false;
}

void ULeaveConfirmWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (StayButton)
	{
		StayButton->OnClicked.AddUniqueDynamic(this, &ULeaveConfirmWidget::HandleStay);
	}

	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddUniqueDynamic(this, &ULeaveConfirmWidget::HandleLeave);
	}
}

void ULeaveConfirmWidget::Show(const FNotebookData& Data, const FSimpleDelegate& InOnLeave)
{
	OnLeave = InOnLeave;

	CompletionText->SetText(FText::Format(NSLOCTEXT("Notebook", "LeaveCompletion", "{0} %"), FText::AsNumber(FMath::FloorToInt(Data.Completion * 100.f))));
	CompletionBar->SetPercent(Data.Completion);

	RowHost->ClearChildren();

	if (!RowClass)
	{
		return;
	}

	for (const FNotebookSection& Section : Data.Sections)
	{
		if (UDebriefLineWidget* Row = CreateWidget<UDebriefLineWidget>(this, RowClass))
		{
			Row->SetPlain(Section.SummaryName, Section.Summary, Section.Status);
			RowHost->AddChild(Row);
		}
	}
}

FReply ULeaveConfirmWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape || Key == EKeys::E)
	{
		// The key that opened the card (E at the truck) may still be held: its auto-repeat answers nothing.
		if (!InKeyEvent.IsRepeat())
		{
			if (Key == EKeys::E)
			{
				HandleLeave();
			}
			else
			{
				HandleStay();
			}
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void ULeaveConfirmWidget::HandleStay()
{
	OnLeave.Unbind();
	OnCloseRequested.Broadcast();
}

void ULeaveConfirmWidget::HandleLeave()
{
	// The HUD gives the game its input back first, then the trip goes on.
	const FSimpleDelegate Leave = OnLeave;
	OnLeave.Unbind();
	OnCloseRequested.Broadcast();
	Leave.ExecuteIfBound();
}
