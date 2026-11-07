#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"
#include "Buildables/BuildablePiece.h"
#include "Contracts/MissionDirector.h"
#include "Destruction/CrushDetectionComponent.h"
#include "Destruction/Damageable.h"
#include "Destruction/DemolitionTypes.h"
#include "Environment/DropZone.h"
#include "Environment/MissionGround.h"
#include "Environment/ProtectedZone.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Progress/ProgressSubsystem.h"
#include "Props/PhysicsProp.h"
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

static FAutoConsoleCommandWithWorldAndArgs PbShowProgressCommand(
	TEXT("pb.ShowProgress"),
	TEXT("pb.ShowProgress - prints the saved progress (money, owned tools, parcel) on screen and in the log. It is kept across maps and across sessions in the Progress save slot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (const UProgressSubsystem* Progress = UProgressSubsystem::Get(World))
		{
			Progress->ShowOnScreen(10.f);
			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.ShowProgress: no progress subsystem (no game instance)"));
	}));

static FAutoConsoleCommandWithWorldAndArgs PbResetProgressCommand(
	TEXT("pb.ResetProgress"),
	TEXT("pb.ResetProgress - back to a new game: 500 money, the hands and the sledgehammer, no parcel. Writes the Progress slot and updates the player's hotbar. Run it before a demo."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& /*Args*/, UWorld* World)
	{
		UProgressSubsystem* Progress = UProgressSubsystem::Get(World);

		if (!Progress)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.ResetProgress: no progress subsystem (no game instance)"));
			return;
		}

		Progress->Reset();

		// The hotbar keeps its own copy of the money for the map: bring it back in line.
		if (APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			if (UHotbarComponent* Hotbar = Character->GetHotbarComponent())
			{
				Hotbar->SetMoney(Progress->GetMoney());
			}
		}

		Progress->ShowOnScreen(10.f);
	}));

static FAutoConsoleCommandWithWorldAndArgs PbShowCrushCommand(
	TEXT("pb.ShowCrush"),
	TEXT("pb.ShowCrush 0|1 - toggles the on-screen crush gauge: the energy summed over the rolling window against the stun threshold (green, yellow, orange, red when stunned)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (const APlayerCharacter* Character = GetDebugPlayerCharacter(World))
		{
			if (UCrushDetectionComponent* Crush = Character->GetCrushDetectionComponent())
			{
				Crush->SetShowGauge(ParseBoolArg(Args));
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
	TEXT("pb.GenerateHouse <seed> [forcedFailures] [keepUnstable] - clears, then rebuilds the level's AHouseGenerator house from the seed (default 1), re-rolling an invalid layout up to the fallback seed. Same seed, same house. forcedFailures (default 0) declares that many attempts failed, to test the re-roll: with 5 attempts, 2 = built on the 3rd try, 5 = fallback seed, 6 = nothing built. keepUnstable (default 0) = 1 leaves a rejected house in the world, without re-rolling, to inspect it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const int32 Seed = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		const int32 ForcedFailures = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
		const bool bKeepUnstable = Args.Num() > 2 && FCString::Atoi(*Args[2]) != 0;

		for (TActorIterator<AHouseGenerator> It(World); It; ++It)
		{
			// Always overwritten, so a forced-failure count or the keep flag never lingers into the next call.
			It->DebugForcedFailures = ForcedFailures;
			It->bKeepUnstableHouse = bKeepUnstable;
			It->GenerateValid(Seed);
			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.GenerateHouse: no AHouseGenerator in the level"));
	}));

static FAutoConsoleCommandWithWorldAndArgs PbSpawnHouseCommand(
	TEXT("pb.SpawnHouse"),
	TEXT("pb.SpawnHouse <archetype> [seed] - switches the level's AHouseGenerator to the first of its KnownArchetypes whose asset name contains <archetype> (any case), then builds a house from the seed (default 1) like pb.GenerateHouse. The archetype stays selected, so pb.TestHouses and pb.GenerateHouse then use it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.SpawnHouse: usage pb.SpawnHouse <archetype> [seed], e.g. pb.SpawnHouse bungalow 3"));
			return;
		}

		const int32 Seed = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;

		for (TActorIterator<AHouseGenerator> It(World); It; ++It)
		{
			if (It->SelectArchetype(Args[0]))
			{
				// Same reset as pb.GenerateHouse: a debug setting left from an earlier call never leaks into this one.
				It->DebugForcedFailures = 0;
				It->bKeepUnstableHouse = false;
				It->GenerateValid(Seed);
			}

			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.SpawnHouse: no AHouseGenerator in the level"));
	}));

static FAutoConsoleCommandWithWorldAndArgs PbBindContractCommand(
	TEXT("pb.BindContract"),
	TEXT("pb.BindContract <total|partial|renovation> [fromLevel] - recomputes the target and must-stay sets of the current house for that contract type and prints them (counts and weights by piece class, props apart). fromLevel is the first storey a partial demolition removes (default: rolled from the seed). Needs the level's AMissionDirector. After pb.GenerateHouse or pb.SpawnHouse, run it again: the sets still describe the house that was cleared."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AMissionDirector* Director = World ? AMissionDirector::Find(World) : nullptr;

		if (!Director)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.BindContract: no AMissionDirector in the level"));
			return;
		}

		EContractType Type = EContractType::TotalDemolition;

		if (Args.IsEmpty() || !AMissionDirector::ParseType(Args[0], Type))
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.BindContract: usage pb.BindContract <total|partial|renovation> [fromLevel]"));
			return;
		}

		Director->Rebind(Type, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0);
	}));

static FAutoConsoleCommandWithWorldAndArgs PbShowTargetsCommand(
	TEXT("pb.ShowTargets"),
	TEXT("pb.ShowTargets 0|1 - draws a box on every piece and prop of the house: green = to destroy, blue = must stay, for the contract bound last. Needs the level's AMissionDirector."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (AMissionDirector* Director = World ? AMissionDirector::Find(World) : nullptr)
		{
			Director->SetShowTargets(ParseBoolArg(Args));
			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.ShowTargets: no AMissionDirector in the level"));
	}));

static FAutoConsoleCommandWithWorldAndArgs PbSetSeedCommand(
	TEXT("pb.SetSeed"),
	TEXT("pb.SetSeed <n> - the seed every mission started from now on builds its house from, whatever the travel options say (-1 clears it). If the level has a mission director, the house is rebuilt from it right away. Without an argument, prints the current one."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			UE_LOG(LogPlotBuilder, Log, TEXT("pb.SetSeed: current override %d (-1 = none)"), AMissionDirector::DebugSeed);
			return;
		}

		AMissionDirector::DebugSeed = FCString::Atoi(*Args[0]);
		UE_LOG(LogPlotBuilder, Log, TEXT("pb.SetSeed: override %d (-1 = none), used by every mission that starts from now on"), AMissionDirector::DebugSeed);

		if (AMissionDirector* Director = World ? AMissionDirector::Find(World) : nullptr)
		{
			Director->StartMission();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbGenerateGroundCommand(
	TEXT("pb.GenerateGround"),
	TEXT("pb.GenerateGround [seed] - rebuilds the level's AMissionGround (relief, sand and dirt, trees, rocks, clutter) from the seed (default: the one pb.SetSeed holds, else 1) and prints what it built. The house is not touched. Same seed, same ground."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AMissionGround* Ground = World ? AMissionGround::Find(World) : nullptr;

		if (!Ground)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.GenerateGround: no AMissionGround in the level"));
			return;
		}

		const int32 Fallback = AMissionDirector::DebugSeed >= 0 ? AMissionDirector::DebugSeed : 1;
		Ground->Generate(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : Fallback);
	}));

static FAutoConsoleCommandWithWorldAndArgs PbShowGroundCommand(
	TEXT("pb.ShowGround"),
	TEXT("pb.ShowGround 0|1 - draws the regions the ground keeps flat: white = flat (plot margin, zones, player start), orange = also kept clear of trees and clutter, blue = road, yellow = the plot itself. Needs the level's AMissionGround, built."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (AMissionGround* Ground = World ? AMissionGround::Find(World) : nullptr)
		{
			Ground->SetShowDebug(ParseBoolArg(Args));
			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.ShowGround: no AMissionGround in the level"));
	}));

namespace
{
	// pb.ShowZones: a readout redrawn every frame, so the zones can be checked without waiting for a debrief.
	// It lives in a core ticker rather than on the zones: no class carries debug state for it, and it stops
	// by itself when the world it was started in goes away.
	FTSTicker::FDelegateHandle ZonesReadoutHandle;
	TWeakObjectPtr<UWorld> ZonesReadoutWorld;

	bool TickZonesReadout(float /*DeltaTime*/)
	{
		UWorld* World = ZonesReadoutWorld.Get();

		if (!World || World->bIsTearingDown || !GEngine)
		{
			return false;
		}

		// One message per zone, on a fixed key so it replaces itself; the short duration clears a zone that is gone.
		// 7400 and up is free: 7100-7101 are the house generator's result and test summary, 7200 the plot's, 7300-7301 the director's and the ground's.
		int32 Key = 7400;

		for (TActorIterator<ADropZone> It(World); It; ++It)
		{
			const TArray<FDropItem> Items = It->GetItems();
			FString Text = FString::Printf(TEXT("Drop zone '%s': %d item(s), value %d"), *It->GetName(), Items.Num(), It->GetTotalValue());

			for (const FDropItem& Item : Items)
			{
				Text += FString::Printf(TEXT("\n  %s: value %d, %.0f%% HP"), Item.Prop.IsValid() ? *Item.Prop->GetName() : TEXT("?"), Item.Value, Item.HPRatio * 100.f);
			}

			GEngine->AddOnScreenDebugMessage(Key++, 0.2f, FColor::Green, Text);
		}

		for (TActorIterator<AProtectedZone> It(World); It; ++It)
		{
			GEngine->AddOnScreenDebugMessage(Key++, 0.2f, It->GetTotalHPLost() > 0.f ? FColor::Red : FColor::Yellow, It->Describe());
		}

		return true;
	}
}

static FAutoConsoleCommandWithWorldAndArgs PbShowZonesCommand(
	TEXT("pb.ShowZones"),
	TEXT("pb.ShowZones 0|1 - on-screen readout of the level's zones: each drop zone's items (count, value, health left), each protected zone's losses (by thing and by tool). Stops when the level ends."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ZonesReadoutHandle);
		ZonesReadoutHandle.Reset();
		ZonesReadoutWorld = World;

		if (World && ParseBoolArg(Args))
		{
			ZonesReadoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickZonesReadout));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbCollapseAllCommand(
	TEXT("pb.CollapseAll"),
	TEXT("pb.CollapseAll [fall|break] - the whole house of the level's AHouseGenerator goes in this one frame, for the worst-case profile (T5). fall (default): every piece is released at once under physics, with none of the collapse queue's stagger. break: every piece and prop takes lethal damage and fractures in place. Leaves a bookmark in an Unreal Insights trace. A real cascade (a charge, pb.CollapseTest) is the one that exercises the graph; this one stresses physics and the fragment cap."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bBreak = !Args.IsEmpty() && Args[0].Equals(TEXT("break"), ESearchCase::IgnoreCase);

		if (!Args.IsEmpty() && !bBreak && !Args[0].Equals(TEXT("fall"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.CollapseAll: usage pb.CollapseAll [fall|break]"));
			return;
		}

		AHouseGenerator* Generator = nullptr;

		if (World)
		{
			for (TActorIterator<AHouseGenerator> It(World); It; ++It)
			{
				Generator = *It;
				break;
			}
		}

		if (!Generator || Generator->GetPieceCount() == 0)
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("pb.CollapseAll: no house in the level (pb.SpawnHouse <archetype> [seed] builds one)"));
			return;
		}

		// Copies: destroying a piece must not be able to change the list being walked.
		const TArray<TWeakObjectPtr<ABuildablePiece>> Pieces = Generator->GetPieces();
		const TArray<TWeakObjectPtr<APhysicsProp>> Props = Generator->GetProps();

		TRACE_BOOKMARK(TEXT("pb.CollapseAll %s"), bBreak ? TEXT("break") : TEXT("fall"));
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("pb.CollapseAll");

		const double StartSeconds = FPlatformTime::Seconds();
		int32 PieceCount = 0;
		int32 PropCount = 0;

		for (const TWeakObjectPtr<ABuildablePiece>& WeakPiece : Pieces)
		{
			ABuildablePiece* Piece = WeakPiece.Get();

			if (!IsValid(Piece))
			{
				continue;
			}

			++PieceCount;

			if (bBreak)
			{
				FDamageInfo Damage;
				Damage.Amount = 1000000.f;
				Damage.ToolId = FName("Debug");
				Damage.HitLocation = Piece->GetActorLocation();
				IDamageable::Execute_ApplyDamage(Piece, Damage);
			}
			else
			{
				Piece->BeginCollapse();
			}
		}

		// Furniture already simulates physics: only the break mode has anything to do with it.
		if (bBreak)
		{
			for (const TWeakObjectPtr<APhysicsProp>& WeakProp : Props)
			{
				APhysicsProp* Prop = WeakProp.Get();

				if (IsValid(Prop) && Prop->Implements<UDamageable>())
				{
					FDamageInfo Damage;
					Damage.Amount = 1000000.f;
					Damage.ToolId = FName("Debug");
					Damage.HitLocation = Prop->GetActorLocation();
					IDamageable::Execute_ApplyDamage(Prop, Damage);
					++PropCount;
				}
			}
		}

		const FString Message = FString::Printf(TEXT("pb.CollapseAll %s: %d pieces%s in one frame, the call took %.1f ms"), bBreak ? TEXT("break") : TEXT("fall"), PieceCount, bBreak ? *FString::Printf(TEXT(" and %d props"), PropCount) : TEXT(""), (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
		UE_LOG(LogPlotBuilder, Log, TEXT("%s"), *Message);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(7500, 10.f, FColor::Orange, Message);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs PbTestHousesCommand(
	TEXT("pb.TestHouses"),
	TEXT("pb.TestHouses [firstSeed] [lastSeed] - builds every seed of the range (default 1 to 20, at most 100) twice with the level's AHouseGenerator and prints a summary: valid houses, re-rolls, same hash twice (T6), room types seen, props per house. Leaves the last house in the world."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const int32 FirstSeed = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		const int32 LastSeed = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : FirstSeed + 19;

		for (TActorIterator<AHouseGenerator> It(World); It; ++It)
		{
			It->TestSeeds(FirstSeed, LastSeed);
			return;
		}

		UE_LOG(LogPlotBuilder, Warning, TEXT("pb.TestHouses: no AHouseGenerator in the level"));
	}));
