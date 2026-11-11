#include "Flow/GameFlowSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Contracts/ContractSubsystem.h"
#include "Contracts/MissionDirector.h"
#include "Debug/DebugDisplay.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Home/HomeDirector.h"
#include "PlotBuilder.h"
#include "UI/GameHUDWidget.h"

void UGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGameFlowSubsystem::HandlePoll), PollSeconds);
}

void UGameFlowSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	PollHandle.Reset();

	Super::Deinitialize();
}

UGameFlowSubsystem* UGameFlowSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UGameFlowSubsystem>() : nullptr;
}

bool UGameFlowSubsystem::HandlePoll(float /*DeltaTime*/)
{
	Refresh();
	return true;
}

bool UGameFlowSubsystem::GatherFacts(FGameFlowFacts& OutFacts) const
{
	UGameInstance* Instance = GetGameInstance();
	UWorld* World = Instance ? Instance->GetWorld() : nullptr;

	if (!World)
	{
		return false;
	}

	FGameFlowFacts Read;
	Read.bInMission = AMissionDirector::Find(World) != nullptr;
	Read.bInHome = AHomeDirector::Find(World) != nullptr;

	const UGameHUDWidget* Hud = UGameHUDWidget::Find(World);
	Read.bDebriefOpen = Hud && Hud->GetCurrentMenu() == UGameHUDWidget::DebriefMenuName;

	const UContractSubsystem* Contracts = Instance->GetSubsystem<UContractSubsystem>();
	Read.bHasActiveContract = Contracts && Contracts->HasActiveContract();

	OutFacts = Read;
	return true;
}

void UGameFlowSubsystem::Refresh()
{
	FGameFlowFacts Read;

	// Between two levels there is no world to look at: the last state stands until the next level has started.
	if (!GatherFacts(Read))
	{
		return;
	}

	Facts = Read;

	const EGameFlowState NewState = GameFlow::Resolve(Facts);

	if (NewState == State)
	{
		return;
	}

	const EGameFlowState OldState = State;
	State = NewState;

	UE_LOG(LogPlotBuilder, Log, TEXT("UGameFlowSubsystem: %s -> %s."), GameFlow::ToText(OldState), GameFlow::ToText(NewState));

	OnStateChanged.Broadcast(OldState, NewState);
}

FString UGameFlowSubsystem::Describe() const
{
	const auto YesNo = [](const bool bValue) { return bValue ? TEXT("yes") : TEXT("no"); };

	return FString::Printf(TEXT("Game flow: %s (mission level: %s, home: %s, debrief open: %s, contract taken: %s)"),
		GameFlow::ToText(State), YesNo(Facts.bInMission), YesNo(Facts.bInHome), YesNo(Facts.bDebriefOpen), YesNo(Facts.bHasActiveContract));
}

void UGameFlowSubsystem::ShowOnScreen(float Seconds) const
{
	const FString Text = Describe();
	UE_LOG(LogPlotBuilder, Log, TEXT("%s"), *Text);

	PBDebug::Message(MessageKey, Seconds, FColor::Cyan, Text);
}
