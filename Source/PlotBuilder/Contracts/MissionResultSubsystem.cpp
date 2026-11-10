#include "Contracts/MissionResultSubsystem.h"
#include "Buildables/BuildablePiece.h"
#include "Contracts/ContractEvaluator.h"
#include "Contracts/ContractSubsystem.h"
#include "Contracts/MissionBinding.h"
#include "Contracts/MissionDirector.h"
#include "Destruction/CrushDetectionComponent.h"
#include "Destruction/DemolitionRules.h"
#include "Destruction/DurabilityComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Environment/DropZone.h"
#include "Environment/Plot.h"
#include "Environment/ProtectedZone.h"
#include "Interaction/PhysicsManipulationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSubsystem.h"
#include "Props/PhysicsProp.h"
#include "UI/PlayerMessages.h"

const FName UMissionResultSubsystem::CrushSource(TEXT("Crush"));
const FName UMissionResultSubsystem::ExplosionSource(TEXT("Explosion"));

namespace
{
	/**
	 *  Whether an item of the house counts as destroyed at the end: its actor is gone (broken, fractured, demolished), or it is a
	 *  piece that has already lost its support and is falling (it is only a matter of seconds), or its hit points are at zero.
	 */
	bool MissionResultIsGone(const FBindingItem& Item)
	{
		const AActor* Actor = Item.Actor.Get();

		if (!IsValid(Actor))
		{
			return true;
		}

		if (const ABuildablePiece* Piece = Cast<ABuildablePiece>(Actor))
		{
			if (Piece->State == EBuildablePieceState::Collapsing)
			{
				return true;
			}
		}

		const UDurabilityComponent* Durability = Actor->FindComponentByClass<UDurabilityComponent>();
		return Durability && Durability->GetState() == EDurabilityState::Destroyed;
	}

	int64 MissionResultDestroyedWeight(const TArray<FBindingItem>& Items)
	{
		int64 Total = 0;

		for (const FBindingItem& Item : Items)
		{
			Total += MissionResultIsGone(Item) ? Item.Weight : 0;
		}

		return Total;
	}
}

UMissionResultSubsystem* UMissionResultSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UMissionResultSubsystem>() : nullptr;
}

void UMissionResultSubsystem::ResetCounters()
{
	CrushEvents = 0;
	ExplosionEvents = 0;
	bForbiddenToolUsed = false;
	NotebookStamp = -1.0;
	bSettled = false;
}

void UMissionResultSubsystem::SetForbiddenTool(FName ToolId)
{
	ForbiddenTool = ToolId;
	bForbiddenToolUsed = false;
}

void UMissionResultSubsystem::RecordToolUse(FName ToolId)
{
	if (ForbiddenTool.IsNone() || bForbiddenToolUsed || ToolId != ForbiddenTool)
	{
		return;
	}

	bForbiddenToolUsed = true;

	UE_LOG(LogPlotBuilder, Log, TEXT("UMissionResultSubsystem: the forbidden tool '%s' was used, the report will charge it."), *ForbiddenTool.ToString());

	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(FText::Format(NSLOCTEXT("Mission", "ForbiddenToolUsed", "The client forbade the {0}. It will show in the report."), FText::FromName(ForbiddenTool)), EMessageTone::Warning, 6.f);
	}
}

void UMissionResultSubsystem::RecordSafetyEvent(FName Source)
{
	if (Source == CrushSource)
	{
		++CrushEvents;
	}
	else if (Source == ExplosionSource)
	{
		++ExplosionEvents;
	}
	else
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UMissionResultSubsystem: unknown safety event '%s', not counted."), *Source.ToString());
	}
}

void UMissionResultSubsystem::SetRules(const UDemolitionRules* NewRules)
{
	CachedRules = NewRules;
}

const UDemolitionRules* UMissionResultSubsystem::FindRules() const
{
	if (const UDemolitionRules* Given = CachedRules.Get())
	{
		return Given;
	}

	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (const UCrushDetectionComponent* Crush = Pawn->FindComponentByClass<UCrushDetectionComponent>())
		{
			return Crush->Rules;
		}
	}

	return nullptr;
}

FMissionSnapshot UMissionResultSubsystem::BuildSnapshot() const
{
	FMissionSnapshot Snapshot;

	const UWorld* World = GetWorld();
	const APlot* Plot = World ? Cast<APlot>(UGameplayStatics::GetActorOfClass(World, APlot::StaticClass())) : nullptr;

	if (!Plot)
	{
		return Snapshot;
	}

	const FMissionSets& Sets = Plot->GetMissionSets();

	Snapshot.TargetWeight = Sets.TargetWeight;
	Snapshot.TargetDestroyedWeight = MissionResultDestroyedWeight(Sets.Targets);
	Snapshot.MustStayWeight = Sets.MustStayWeight;
	Snapshot.MustStayDestroyedWeight = MissionResultDestroyedWeight(Sets.MustStay);
	Snapshot.HouseWeight = Sets.TargetWeight + Sets.MustStayWeight + Sets.SalvageWeight;

	// The furniture the client asked for: delivered only if it stands inside the client drop zone at this very moment.
	TMap<const AActor*, float> Delivered;

	if (const ADropZone* DropZone = Plot->GetClientDropZone())
	{
		for (const FDropItem& Drop : DropZone->GetItems())
		{
			if (const APhysicsProp* Prop = Drop.Prop.Get())
			{
				Delivered.Add(Prop, Drop.HPRatio);
			}
		}
	}

	// What the player holds right now, for the notebook ("In your hands").
	const AActor* Held = nullptr;

	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (const UPhysicsManipulationComponent* Hands = Pawn->FindComponentByClass<UPhysicsManipulationComponent>())
		{
			Held = Hands->GetHeldActor();
		}
	}

	for (const FBindingItem& Item : Sets.Salvage)
	{
		FSalvageResult& Result = Snapshot.Salvage.AddDefaulted_GetRef();
		Result.Name = Item.Label.IsNone() ? MissionBinding::ItemName(Item.Actor.Get()) : Item.Label.ToString();
		Result.Value = Item.Value;

		const bool bGone = MissionResultIsGone(Item);
		const float* InZone = bGone ? nullptr : Delivered.Find(Item.Actor.Get());
		Result.bDestroyed = bGone;
		Result.bDelivered = InZone != nullptr;
		Result.HPRatio = InZone ? *InZone : 0.f;
		Result.bCarried = !bGone && Held && Held == Item.Actor.Get();

		// Alive and not handed over: still on the plot (in the house or around it), or carried off it. Only a word in the report.
		Result.bOnSite = !bGone && !InZone && Plot->IsLocationInside(Item.Actor->GetActorLocation());
	}

	for (const TObjectPtr<AProtectedZone>& Zone : Plot->GetProtectedZones())
	{
		if (!Zone)
		{
			continue;
		}

		FZoneResult& Result = Snapshot.Zones.AddDefaulted_GetRef();
		Result.Name = Zone->GetName();
		Result.Label = Zone->DisplayName.ToString();
		Result.ToolDamageRatio = Zone->GetDamageRatio();

		// What the zone counts from the house itself (the weight of the pieces that fell in) comes in through RecordExternalDamage.
		Result.FallenWeight = static_cast<int64>(Zone->GetExternalDamage());
	}

	Snapshot.CrushEvents = CrushEvents;
	Snapshot.ExplosionEvents = ExplosionEvents;
	Snapshot.ForbiddenToolName = ForbiddenTool;
	Snapshot.bForbiddenToolUsed = bForbiddenToolUsed;

	return Snapshot;
}

FString UMissionResultSubsystem::DescribeSnapshot(const FMissionSnapshot& Snapshot)
{
	FString Text = FString::Printf(TEXT("Snapshot: to destroy %lld of %lld | to keep, destroyed %lld of %lld | house weight %lld | crush %d, explosion %d"),
		Snapshot.TargetDestroyedWeight, Snapshot.TargetWeight, Snapshot.MustStayDestroyedWeight, Snapshot.MustStayWeight,
		Snapshot.HouseWeight, Snapshot.CrushEvents, Snapshot.ExplosionEvents);

	if (!Snapshot.ForbiddenToolName.IsNone())
	{
		Text += FString::Printf(TEXT(" | forbidden tool %s: %s"), *Snapshot.ForbiddenToolName.ToString(), Snapshot.bForbiddenToolUsed ? TEXT("used") : TEXT("not used"));
	}

	for (const FSalvageResult& Item : Snapshot.Salvage)
	{
		const TCHAR* Fate = Item.bDelivered ? TEXT("delivered") : (Item.bDestroyed ? TEXT("destroyed") : (Item.bOnSite ? TEXT("left on the plot") : TEXT("left outside the plot")));

		Text += FString::Printf(TEXT("\n  Salvage %s: value %d, %s%s"), *Item.Name, Item.Value, Fate,
			Item.bDelivered ? *FString::Printf(TEXT(" at %.0f%% HP"), Item.HPRatio * 100.f) : TEXT(""));
	}

	for (const FZoneResult& Zone : Snapshot.Zones)
	{
		Text += FString::Printf(TEXT("\n  Zone %s: tools took %.0f%%, weight fallen in %lld"), *Zone.Name, Zone.ToolDamageRatio * 100.f, Zone.FallenWeight);
	}

	return Text;
}

const FNotebookData& UMissionResultSubsystem::GetLiveNotebook(bool bForceRefresh)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;

	if (!bForceRefresh && NotebookStamp >= 0.0 && Now - NotebookStamp < NotebookRefreshSeconds)
	{
		return CachedNotebook;
	}

	NotebookStamp = Now;

	AMissionDirector* Director = AMissionDirector::Find(this);

	if (!Director)
	{
		CachedNotebook = FNotebookData();
		return CachedNotebook;
	}

	const UDemolitionRules* Rules = FindRules();
	CachedNotebook = MissionNotebook::Build(Director->GetMissionContract(), BuildSnapshot(), Rules ? Rules->Scoring : FScoringParams());

	return CachedNotebook;
}

bool UMissionResultSubsystem::Settle(FDebriefResult& OutResult)
{
	if (bSettled)
	{
		return false;
	}

	AMissionDirector* Director = AMissionDirector::Find(this);

	if (!Director)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UMissionResultSubsystem::Settle: needs the level's AMissionDirector, nothing was paid."));
		return false;
	}

	FString Text;
	OutResult = Evaluate(Director->GetMissionContract(), Text);
	bSettled = true;

	int32 Due = 0;

	if (UContractSubsystem* Contracts = UContractSubsystem::Get(this))
	{
		// The debrief lists the fate of each piece next to the report: the notebook as it stands at this very moment goes with it.
		Due = Contracts->CompleteActiveContract(OutResult, GetLiveNotebook(true));
	}

	if (Due > 0)
	{
		if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
		{
			Progress->AddMoney(Due);
		}
	}

	UE_LOG(LogPlotBuilder, Log, TEXT("UMissionResultSubsystem::Settle: payout %d, %d paid.\n%s"), OutResult.Payout, Due, *Text);

	return true;
}

FDebriefResult UMissionResultSubsystem::Evaluate(const FContractDefinition& Contract, FString& OutText) const
{
	const FMissionSnapshot Snapshot = BuildSnapshot();
	const UDemolitionRules* Rules = FindRules();

	if (!Rules)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UMissionResultSubsystem: no rules asset known (no piece gave one, and the player's crush component has none), the scoring runs on the default values."));
	}

	const FDebriefResult Result = UContractEvaluator::Evaluate(Contract, Snapshot, Rules ? Rules->Scoring : FScoringParams());

	OutText = DescribeSnapshot(Snapshot) + TEXT("\n") + Result.ToString();
	return Result;
}
