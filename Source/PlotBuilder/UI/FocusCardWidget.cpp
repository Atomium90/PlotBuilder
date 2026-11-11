#include "FocusCardWidget.h"
#include "ItemLook.h"
#include "UIStyle.h"
#include "Buildables/BuildAnchor.h"
#include "Buildables/BuildablePiece.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Contracts/ContractEvaluator.h"
#include "Contracts/MissionBinding.h"
#include "Destruction/DemolitionRules.h"
#include "Destruction/DemolitionToolData.h"
#include "Destruction/DurabilityComponent.h"
#include "Engine/Texture2D.h"
#include "Environment/Plot.h"
#include "Flow/GameFlowSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Interaction/BuildModeComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/ToolbeltComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Props/PhysicsProp.h"

void UFocusCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The widget stays visible (and never takes a click) so that it keeps being ticked; only the card is shown and hidden.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	Card->SetVisibility(ESlateVisibility::Collapsed);
	LastKey.Reset();
}

void UFocusCardWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FFocusCardFacts Facts;
	FItemLook Look;

	if (bSuppressed || !GatherFacts(Facts, Look))
	{
		Facts = FFocusCardFacts();
	}

	const FFocusCardView View = FocusCard::Describe(Facts);

	if (!View.bVisible)
	{
		Card->SetVisibility(ESlateVisibility::Collapsed);
		LastKey.Reset();
		return;
	}

	Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	const FString Key = FString::Printf(TEXT("%s|%s|%s|%d|%s|%d|%s|%d|%s|%d|%s|%s|%d|%d|%d|%p"),
		*View.Name, *View.Sub, *View.StateText, static_cast<int32>(View.StateTone), *View.FillText, static_cast<int32>(View.bShowBar),
		*View.ToolText, static_cast<int32>(View.ToolTone), *View.WarningText, static_cast<int32>(View.bShowWarning),
		*View.ValueNow, *View.ValueText, static_cast<int32>(View.bShowValue), static_cast<int32>(View.bShowTool), static_cast<int32>(View.bShowTakeBack),
		static_cast<const void*>(Look.Icon));

	if (Key == LastKey)
	{
		return;
	}

	LastKey = Key;
	Show(View, Look);
}

bool UFocusCardWidget::GatherFacts(FFocusCardFacts& OutFacts, FItemLook& OutLook)
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const UInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UInteractionComponent>() : nullptr;
	AActor* Target = Interaction ? Interaction->GetCurrentFocus() : nullptr;
	const UDurabilityComponent* Durability = Target ? Target->FindComponentByClass<UDurabilityComponent>() : nullptr;

	// Only what can be hurt gets a card: not the PC, not the truck.
	if (!Durability)
	{
		return false;
	}

	// And only where there is something to read: a mission and the home, not the hub.
	const UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this);

	if (!Flow || !FocusCard::ShownIn(Flow->GetState()))
	{
		return false;
	}

	const ABuildablePiece* Piece = Cast<ABuildablePiece>(Target);
	APhysicsProp* Prop = Cast<APhysicsProp>(Target);
	const UHotbarComponent* Hotbar = Pawn->FindComponentByClass<UHotbarComponent>();
	const UBuildModeComponent* Build = Pawn->FindComponentByClass<UBuildModeComponent>();
	const UToolbeltComponent* Belt = Pawn->FindComponentByClass<UToolbeltComponent>();
	const UDemolitionRules* Rules = Durability->Rules;

	OutFacts.bHasTarget = true;
	OutFacts.bInMission = Flow->GetState() == EGameFlowState::OnSite;
	OutFacts.Material = Durability->GetMaterial();
	OutFacts.State = Durability->GetState();
	OutFacts.HPRatio = Durability->GetHPRatio();

	// The name and the look: the shop's row of the class when it has one, a guess from the class when it has none.
	if (const FShopItemRow* Row = Hotbar ? Hotbar->FindRowForClass(Target->GetClass()) : nullptr)
	{
		OutFacts.Name = Row->DisplayName.ToString();
		OutLook = FItemLook::FromRow(Style, *Row, nullptr);
	}
	else
	{
		FString Name = Target->GetClass()->GetName();
		Name.RemoveFromStart(TEXT("BP_"));
		Name.RemoveFromEnd(TEXT("_C"));

		FShopItemRow Guess;
		Guess.DisplayName = FText::FromString(Name);
		Guess.Category = Piece ? EShopItemCategory::Structure : EShopItemCategory::Prop;

		OutFacts.Name = Name;
		OutLook = FItemLook::FromRow(Style, Guess, nullptr);
	}

	if (Piece)
	{
		OutFacts.bCompromised = Piece->IsCompromised();

		// A piece placed by hand, outside the grid, has no storey to name.
		if (Piece->GetBuildAnchor() != EBuildAnchor::Legacy)
		{
			OutFacts.Storey = Piece->GetGridCoord().Z;
		}
	}

	if (Prop)
	{
		const UStaticMeshComponent* Mesh = Prop->GetMeshComponent();

		if (OutFacts.bInMission)
		{
			const APlot* Plot = FindPlot();
			const TWeakObjectPtr<AActor> Key(Prop);

			if (Plot && Plot->GetMissionSets().SalvageActors.Contains(Key))
			{
				OutFacts.bIsSalvage = true;
				OutFacts.Role = TEXT("the client wants it");
				OutFacts.Value = Prop->Value;
				OutFacts.ValueShare = UContractEvaluator::FurnitureValueMultiplier(Rules ? Rules->Scoring : FScoringParams(), OutFacts.HPRatio);
			}
		}
		else if (Mesh && !Mesh->IsSimulatingPhysics())
		{
			OutFacts.Role = TEXT("placed in your home");
		}
	}

	// The tool in hand against the material: only a tool that hits has a row in the matrix.
	if (OutFacts.bInMission && Belt && Rules)
	{
		if (const UDemolitionToolData* ToolData = Belt->GetEquippedToolData())
		{
			const FToolMaterialMultipliers* Row = Rules->ToolMaterialMatrix.Find(ToolData->Tool);
			const float* Multiplier = Row ? Row->Multipliers.Find(OutFacts.Material) : nullptr;

			if (Multiplier)
			{
				OutFacts.bHasToolMatch = true;
				OutFacts.ToolName = Belt->GetEquippedToolId().ToString();
				OutFacts.ToolMultiplier = *Multiplier;
			}
		}
	}

	// E takes a placed piece or a prop back where building is allowed, and does nothing else while the build mode owns the key.
	const bool bBuilding = Build && Build->IsBuildModeActive();

	if (Hotbar && Hotbar->IsAllowed() && !bBuilding)
	{
		OutFacts.bCanTakeBack = Prop || (Piece && Piece->State == EBuildablePieceState::Placed);
	}

	return true;
}

void UFocusCardWidget::Show(const FFocusCardView& View, const FItemLook& Look)
{
	const FLinearColor StateColor = ToneColor(View.StateTone);

	if (NameText)
	{
		NameText->SetText(FText::FromString(View.Name));
	}

	if (SubText)
	{
		SubText->SetText(FText::FromString(View.Sub));
	}

	if (StateText)
	{
		StateText->SetText(FText::FromString(View.StateText.ToUpper()));
		StateText->SetColorAndOpacity(FSlateColor(StateColor));
	}

	if (StatePill)
	{
		StatePill->SetBrush(UUIStyle::MakeRounded(UUIStyle::WithAlpha(StateColor, 0.13f), 11.f, StateColor, 1.f));
	}

	if (Frame && Style)
	{
		Frame->SetBrush(UUIStyle::MakeRounded(UUIStyle::WithAlpha(Style->RailBackground, 0.92f), 10.f, View.StateTone == EFocusTone::Bad ? BadFrameColor : Style->Line, 1.f));
	}

	if (IconImage)
	{
		if (Look.Icon)
		{
			IconImage->SetBrushFromTexture(Look.Icon);
		}

		IconImage->SetColorAndOpacity(Look.Color);
	}

	SetRow(BarRow, View.bShowBar);

	if (FillBar)
	{
		FillBar->SetPercent(View.Fill);
		FillBar->SetFillColorAndOpacity(StateColor);
	}

	if (FillText)
	{
		FillText->SetText(FText::FromString(View.FillText));
	}

	SetRow(ToolRow, View.bShowTool);

	if (ToolText)
	{
		ToolText->SetText(FText::FromString(View.ToolText));
	}

	if (ToolDot)
	{
		ToolDot->SetColorAndOpacity(ToneColor(View.ToolTone));
	}

	SetRow(WarningRow, View.bShowWarning);

	if (WarningText)
	{
		WarningText->SetText(FText::FromString(View.WarningText));
	}

	SetRow(ValueRow, View.bShowValue);

	if (ValueNowText)
	{
		ValueNowText->SetText(FText::FromString(View.ValueNow));
	}

	if (ValueText)
	{
		ValueText->SetText(FText::FromString(View.ValueText));
	}

	SetRow(PromptRow, View.bShowTakeBack);
}

FLinearColor UFocusCardWidget::ToneColor(const EFocusTone Tone) const
{
	switch (Tone)
	{
	case EFocusTone::Warn:
		return WarnColor;
	case EFocusTone::Bad:
		return BadColor;
	default:
		return GoodColor;
	}
}

void UFocusCardWidget::SetRow(UWidget* Row, const bool bShown)
{
	if (Row)
	{
		Row->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

const APlot* UFocusCardWidget::FindPlot()
{
	if (CachedPlot.IsValid())
	{
		return CachedPlot.Get();
	}

	const UWorld* World = GetWorld();

	if (!World || World->GetTimeSeconds() < NextPlotSearch)
	{
		return nullptr;
	}

	NextPlotSearch = World->GetTimeSeconds() + 1.0;
	CachedPlot = Cast<APlot>(UGameplayStatics::GetActorOfClass(World, APlot::StaticClass()));

	return CachedPlot.Get();
}
