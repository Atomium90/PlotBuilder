#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"
#include "Player/PlayerCharacter.h"
#include "Interaction/HotbarComponent.h"
#include "Destruction/DemolitionTool.h"
#include "Generation/HouseGenerator.h"

// GDD §18.8's pb.* console commands - thin wrappers on debug code that already exists on
// APlayerCharacter (DebugShowStability, DebugDamageFocus), plus the two small additions below
// (ADemolitionTool::bShowHPDebug, UHotbarComponent::SetMoney). Exec UFUNCTIONs can't produce a
// dotted command name, so these register for real via IConsoleManager instead.
//
// pb.GiveTool is deliberately not here: UToolbeltComponent has no ownership/gating concept yet
// (every melee tool is already always spawned), so there is nothing for it to wrap until tool
// purchase is real (Sprint 3).

namespace
{
	APlayerCharacter* GetDebugPlayerCharacter(UWorld* World)
	{
		return World ? Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
	}

	bool ParseBoolArg(const TArray<FString>& Args)
	{
		return Args.Num() == 0 || Args[0] != TEXT("0");
	}
}

static FAutoConsoleCommandWithWorldAndArgs PbShowStabilityCommand(
	TEXT("pb.ShowStability"),
	TEXT("pb.ShowStability 0|1 - toggles the plot's stability heat-map."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			Character->DebugShowStability(ParseBoolArg(Args));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbShowHPCommand(
	TEXT("pb.ShowHP"),
	TEXT("pb.ShowHP 0|1 - toggles the on-screen 'HP left' print every tool hit makes."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* /*World*/)
	{
		ADemolitionTool::bShowHPDebug = ParseBoolArg(Args);
	}));

static FAutoConsoleCommandWithWorldAndArgs PbMoneyCommand(
	TEXT("pb.Money"),
	TEXT("pb.Money <n> - sets the local player's money directly, bypassing bInfiniteMoney."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			return;
		}

		if (APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			if (UHotbarComponent* Hotbar = Character->GetHotbarComponent())
			{
				Hotbar->SetMoney(FCString::Atoi(*Args[0]));
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbShowCollapseQueueCommand(
	TEXT("pb.ShowCollapseQueue"),
	TEXT("pb.ShowCollapseQueue 0|1 - toggles the on-screen collapse-queue readout."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			Character->DebugShowCollapseQueue(ParseBoolArg(Args));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbDamageAllCommand(
	TEXT("pb.DamageAll"),
	TEXT("pb.DamageAll <n> - applies n damage to the player's current interaction focus."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			return;
		}

		if (APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			Character->DebugDamageFocus(FCString::Atof(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbCollapseTestCommand(
	TEXT("pb.CollapseTest"),
	TEXT("pb.CollapseTest - applies lethal damage to the player's current interaction focus."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			Character->DebugDamageFocus(99999.f);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbGenerateHouseCommand(
	TEXT("pb.GenerateHouse"),
	TEXT("pb.GenerateHouse <seed> [forcedFailures] - clears, then rebuilds the level's AHouseGenerator house from the seed (default 1), re-rolling an invalid layout up to the fallback seed. Same seed, same house. forcedFailures (default 0) declares that many attempts failed, to test the re-roll: with 5 attempts, 2 = built on the 3rd try, 5 = fallback seed, 6 = nothing built."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const int32 Seed = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		const int32 ForcedFailures = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;

		for (TActorIterator<AHouseGenerator> It(World); It; ++It)
		{
			// Always overwritten, so a forced-failure count never lingers into the next call.
			It->DebugForcedFailures = ForcedFailures;
			It->GenerateValid(Seed);
			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.GenerateHouse: no AHouseGenerator in the level"));
	}));
