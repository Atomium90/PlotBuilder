#include "ProgressSubsystem.h"
#include "ProgressSave.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"

const FString UProgressSubsystem::SlotName = TEXT("Progress");

void UProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		Progress = Cast<UProgressSave>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	}

	if (Progress)
	{
		UE_LOG(LogPlotBuilder, Log, TEXT("UProgressSubsystem: loaded the %s slot. %s"), *SlotName, *Describe());
		return;
	}

	// No slot, or one that could not be read: a new game. It is written on the first change.
	Progress = Cast<UProgressSave>(UGameplayStatics::CreateSaveGameObject(UProgressSave::StaticClass()));
	Progress->ResetToDefaults();

	UE_LOG(LogPlotBuilder, Log, TEXT("UProgressSubsystem: no usable %s slot, starting a new game. %s"), *SlotName, *Describe());
}

UProgressSubsystem* UProgressSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UProgressSubsystem>() : nullptr;
}

int32 UProgressSubsystem::GetMoney() const
{
	return Progress->Money;
}

void UProgressSubsystem::SetMoney(int32 NewAmount)
{
	NewAmount = FMath::Max(NewAmount, 0);

	if (NewAmount == Progress->Money)
	{
		return;
	}

	Progress->Money = NewAmount;
	Persist();
}

void UProgressSubsystem::AddMoney(int32 Delta)
{
	SetMoney(Progress->Money + Delta);
}

bool UProgressSubsystem::OwnsTool(FName ToolId) const
{
	return Progress->OwnedTools.Contains(ToolId);
}

void UProgressSubsystem::GrantTool(FName ToolId)
{
	if (OwnsTool(ToolId))
	{
		return;
	}

	Progress->OwnedTools.Add(ToolId);
	Persist();
}

bool UProgressSubsystem::IsParcelOwned() const
{
	return Progress->bParcelOwned;
}

void UProgressSubsystem::SetParcelOwned(bool bOwned)
{
	if (bOwned == Progress->bParcelOwned)
	{
		return;
	}

	Progress->bParcelOwned = bOwned;
	Persist();
}

const FHomeSnapshot& UProgressSubsystem::GetHome() const
{
	return Progress->Home;
}

void UProgressSubsystem::SetHome(FHomeSnapshot&& NewHome)
{
	Progress->Home = MoveTemp(NewHome);
	Persist();
}

void UProgressSubsystem::ClearHome()
{
	Progress->Home = FHomeSnapshot();
	Persist();
}

void UProgressSubsystem::Reset()
{
	Progress->ResetToDefaults();
	Persist();
}

FString UProgressSubsystem::Describe() const
{
	TArray<FString> Tools;

	for (const FName& Tool : Progress->OwnedTools)
	{
		Tools.Add(Tool.ToString());
	}

	Tools.Sort();

	const FHomeSnapshot& Home = Progress->Home;
	const FString HomeText = Home.bValid
		? FString::Printf(TEXT("%d pieces, %d props, %d slots"), Home.Pieces.Num(), Home.Props.Num(), Home.Slots.Num())
		: FString(TEXT("none"));

	return FString::Printf(TEXT("Progress: money %d | tools %s | parcel %s | home %s | slot %s"),
		Progress->Money, *FString::Join(Tools, TEXT(", ")), Progress->bParcelOwned ? TEXT("owned") : TEXT("not owned"), *HomeText, *SlotName);
}

void UProgressSubsystem::ShowOnScreen(float Seconds) const
{
	const FString Text = Describe();

	UE_LOG(LogPlotBuilder, Log, TEXT("%s"), *Text);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(MessageKey, Seconds, FColor::Cyan, Text);
	}
}

void UProgressSubsystem::Persist()
{
	if (!UGameplayStatics::SaveGameToSlot(Progress, SlotName, UserIndex))
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UProgressSubsystem: could not write the %s slot, the progress is kept in memory only."), *SlotName);
	}

	OnProgressChanged.Broadcast();
}
