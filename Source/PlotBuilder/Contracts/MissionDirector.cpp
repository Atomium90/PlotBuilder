#include "Contracts/MissionDirector.h"
#include "Debug/DebugDisplay.h"
#include "Buildables/BuildablePiece.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/Plot.h"
#include "GameFramework/GameModeBase.h"
#include "Generation/HouseArchetype.h"
#include "Generation/HouseGenerator.h"
#include "Contracts/ContractEvaluator.h"
#include "Contracts/ContractPool.h"
#include "Contracts/ContractSubsystem.h"
#include "Contracts/MissionBinding.h"
#include "Contracts/MissionResultSubsystem.h"
#include "Destruction/DurabilityComponent.h"
#include "Environment/MissionGround.h"
#include "Environment/ProtectedZone.h"
#include "Interaction/ShopItemRow.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "PlotBuilder.h"
#include "Props/PhysicsProp.h"
#include "TimerManager.h"

int32 AMissionDirector::DebugSeed = -1;

namespace
{
	constexpr int32 DirectorMessageKey = 7300;
	constexpr int32 MaxInputAttempts = 50;
	constexpr float InputRetryInterval = 0.1f;

	/** Tries GenerateValid gives the mission's house before the fallback seed: the generator's own default. */
	constexpr int32 MissionGenerateAttempts = 5;
}

AMissionDirector::AMissionDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

bool AMissionDirector::ParseType(const FString& Text, EContractType& OutType)
{
	if (Text.IsEmpty())
	{
		return false;
	}

	// The first letter decides.
	switch (FChar::ToLower(Text[0]))
	{
	case TEXT('t'):
		OutType = EContractType::TotalDemolition;
		return true;
	case TEXT('p'):
		OutType = EContractType::PartialDemolition;
		return true;
	case TEXT('r'):
		OutType = EContractType::Renovation;
		return true;
	default:
		return false;
	}
}

AMissionDirector* AMissionDirector::Find(const UObject* WorldContext)
{
	return Cast<AMissionDirector>(UGameplayStatics::GetActorOfClass(WorldContext, AMissionDirector::StaticClass()));
}

AHouseGenerator* AMissionDirector::ResolveGenerator()
{
	if (!Generator)
	{
		Generator = Cast<AHouseGenerator>(UGameplayStatics::GetActorOfClass(this, AHouseGenerator::StaticClass()));
	}

	return Generator;
}

AMissionGround* AMissionDirector::ResolveGround()
{
	if (!Ground)
	{
		Ground = AMissionGround::Find(this);
	}

	return Ground;
}

APlot* AMissionDirector::ResolvePlot() const
{
	if (Generator && Generator->Plot)
	{
		return Generator->Plot;
	}

	return Cast<APlot>(UGameplayStatics::GetActorOfClass(this, APlot::StaticClass()));
}

void AMissionDirector::BeginPlay()
{
	Super::BeginPlay();

	// The caution tape marks the plot in a mission: the cyan debug box would only be noise.
	if (APlot* Plot = ResolvePlot())
	{
		Plot->bShowDebugBounds = false;
	}

	if (ContractPool)
	{
		if (UContractSubsystem* Contracts = UContractSubsystem::Get(this))
		{
			Contracts->SetPool(ContractPool);
		}
	}

	if (bDisableBuilding)
	{
		ApplyMissionInput();
	}

	if (bStartOnBeginPlay)
	{
		StartMission();
	}
}

void AMissionDirector::ApplyMissionInput()
{
	if (APlayerCharacter* Character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Character->SetBuildingAllowed(false);
		GetWorldTimerManager().ClearTimer(InputTimer);
		return;
	}

	if (++InputAttempts > MaxInputAttempts)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AMissionDirector: no player character after %d tries, building stays on."), MaxInputAttempts);
		GetWorldTimerManager().ClearTimer(InputTimer);
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(InputTimer))
	{
		GetWorldTimerManager().SetTimer(InputTimer, this, &AMissionDirector::ApplyMissionInput, InputRetryInterval, true);
	}
}

bool AMissionDirector::StartMission()
{
	AHouseGenerator* House = ResolveGenerator();

	if (!House)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AMissionDirector: no AHouseGenerator in the level, nothing to build."));
		return false;
	}

	// A new house is a new mission: what happened to the player in the last one does not count in this one.
	if (UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this))
	{
		Results->ResetCounters();
	}

	FString Options;

	if (const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		Options = GameMode->OptionsString;
	}

	// The contract the player took decides first; without one the travel options and the defaults do, as they always did.
	const UContractSubsystem* Contracts = UContractSubsystem::Get(this);
	const FContractDefinition* Contract = (Contracts && Contracts->HasActiveContract()) ? &Contracts->GetActiveContract() : nullptr;

	// Archetype first: it decides how many storeys a partial demolition can use.
	const FString ArchetypeOption = Contract ? (Contract->ArchetypeId.IsNone() ? FString() : Contract->ArchetypeId.ToString()) : UGameplayStatics::ParseOption(Options, TEXT("Archetype"));

	if (!ArchetypeOption.IsEmpty())
	{
		House->SelectArchetype(ArchetypeOption);
	}

	if (Contract)
	{
		// The contract carries its own storeys (below, as a generation override): a setting left on the generator by an earlier option must not leak into it.
		House->SetFloorsFromText(TEXT("default"));
	}
	else
	{
		// Storeys, for an archetype with a floor range (the apartment block): "Floors=8" forces the count, "Floors=random" lets the
		// seed draw it, "Floors=default" goes back to the archetype's. Without the option the generator keeps its own settings.
		const FString FloorsOption = UGameplayStatics::ParseOption(Options, TEXT("Floors"));

		if (!FloorsOption.IsEmpty() && !House->SetFloorsFromText(FloorsOption))
		{
			UE_LOG(LogPlotBuilder, Warning, TEXT("AMissionDirector: the Floors option '%s' is not a number, 'random' or 'default', it is ignored."), *FloorsOption);
		}
	}

	int32 Seed = DefaultSeed;

	if (DebugSeed >= 0)
	{
		Seed = DebugSeed;
	}
	else if (Contract)
	{
		Seed = Contract->Seed;
	}
	else if (UGameplayStatics::HasOption(Options, TEXT("Seed")))
	{
		Seed = UGameplayStatics::GetIntOption(Options, TEXT("Seed"), DefaultSeed);
	}

	EContractType Type = Contract ? Contract->Type : DefaultType;

	if (!Contract)
	{
		ParseType(UGameplayStatics::ParseOption(Options, TEXT("Type")), Type);
	}

	const int32 FromLevel = Contract ? Contract->PartialFromLevel : UGameplayStatics::GetIntOption(Options, TEXT("From"), DefaultPartialFromLevel);

	// The size and the storeys the contract asked for take the place of the archetype's own bounds for this house.
	const FGenerationOverrides Overrides = Contract ? FGenerationOverrides::Exactly(Contract->FootprintCells, Contract->Floors) : FGenerationOverrides();

	// The ground first: the house and its props need something to stand on while they are placed.
	if (AMissionGround* Terrain = ResolveGround())
	{
		Terrain->Generate(Seed);
	}

	if (!House->GenerateValid(Seed, MissionGenerateAttempts, Overrides))
	{
		const FString Message = FString::Printf(TEXT("AMissionDirector: no house could be built from seed %d, not even the fallback."), Seed);
		UE_LOG(LogPlotBuilder, Error, TEXT("%s"), *Message);

		if (GEngine)
		{
			PBDebug::Message(DirectorMessageKey, 30.f, FColor::Red, Message);
		}

		return false;
	}

	if (Contract)
	{
		// The estimates on the board come from the request; the plan may have stretched the footprint to what it can reach.
		const FIntPoint Built = House->GetFootprintCells();
		UE_LOG(LogPlotBuilder, Log, TEXT("AMissionDirector: contract %s. Footprint asked %dx%d, built %dx%d; storeys asked %d, built %d."),
			*UContractSubsystem::Describe(*Contract), Contract->FootprintCells.X, Contract->FootprintCells.Y, Built.X, Built.Y, Contract->Floors, House->GetFloorCount());
	}

	Rebind(Type, FromLevel, Contract ? Contract->CountOf(EConditionType::SalvageFurniture) : 0);
	return true;
}

void AMissionDirector::Rebind(EContractType Type, int32 FromLevel, int32 SalvageCount)
{
	AHouseGenerator* House = ResolveGenerator();
	APlot* Plot = ResolvePlot();

	if (!House || !Plot || House->GetPieceCount() == 0)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AMissionDirector::Rebind: no built house to bind."));
		return;
	}

	// The storeys the house was really built with: an archetype with a floor range can give several counts.
	const int32 StoreyCount = House->GetFloorCount() > 0 ? House->GetFloorCount() : (House->Archetype ? FMath::Max(1, House->Archetype->Floors) : 1);

	// A partial demolition with no storey given takes one from the seed the house was really built from.
	if (Type == EContractType::PartialDemolition && FromLevel <= 0 && StoreyCount >= 2)
	{
		FRandomStream Stream(HashCombine(static_cast<uint32>(House->GetEffectiveSeed()), 0x9A47u));
		FromLevel = Stream.RandRange(1, StoreyCount - 1);
	}

	// The weight of an item is the cost of its DT_Shop row. A class with no row (or a free one) weighs its own
	// value if it is a prop, 1 otherwise, and is listed in the summary: it is a hole in the shop table.
	struct FClassWeight
	{
		int32 Weight = 1;
		bool bMissing = false;
	};

	TMap<const UClass*, FClassWeight> WeightByClass;

	/** Items of each class that had no DT_Shop cost, by class name. */
	TMap<FName, int32> MissingRows;

	// A mirror variant (the half gable that climbs the other way) is never sold: Build Mode swaps it in for the piece
	// that names it as MirrorVariantClass. It is the same item for the shop, so it weighs what that piece costs.
	const auto FindShopRow = [this](UClass* Class) -> const FShopItemRow*
	{
		if (const FShopItemRow* Row = FShopItemRow::FindForClass(ShopCatalog, Class))
		{
			return Row;
		}

		if (!ShopCatalog)
		{
			return nullptr;
		}

		TArray<FShopItemRow*> Rows;
		ShopCatalog->GetAllRows<FShopItemRow>(TEXT("AMissionDirector::FindShopRow"), Rows);

		for (const FShopItemRow* Row : Rows)
		{
			const ABuildablePiece* Sold = (Row && Row->ActorClass) ? Cast<ABuildablePiece>(Row->ActorClass->GetDefaultObject()) : nullptr;

			if (Sold && Sold->MirrorVariantClass.Get() == Class)
			{
				return Row;
			}
		}

		return nullptr;
	};

	const auto WeightOf = [&FindShopRow, &WeightByClass, &MissingRows](UClass* Class, int32 FallbackValue)
	{
		FClassWeight* Known = WeightByClass.Find(Class);

		if (!Known)
		{
			const FShopItemRow* Row = FindShopRow(Class);

			FClassWeight Computed;
			Computed.bMissing = !Row || Row->Cost <= 0;
			Computed.Weight = Computed.bMissing ? FMath::Max(1, FallbackValue) : Row->Cost;
			Known = &WeightByClass.Add(Class, Computed);
		}

		if (Known->bMissing)
		{
			MissingRows.FindOrAdd(Class->GetFName()) += 1;
		}

		return Known->Weight;
	};

	TArray<FBindingItem> Items;

	for (const TWeakObjectPtr<ABuildablePiece>& WeakPiece : House->GetPieces())
	{
		ABuildablePiece* Piece = WeakPiece.Get();

		if (!IsValid(Piece))
		{
			continue;
		}

		FBindingItem& Item = Items.AddDefaulted_GetRef();
		Item.Actor = Piece;
		Item.bIsProp = false;
		Item.PieceClass = Piece->PieceClass;
		Item.bLoadBearing = Piece->IsLoadBearing();
		Item.Level = Piece->GetGridCoord().Z;
		Item.Weight = WeightOf(Piece->GetClass(), 0);
	}

	for (const TWeakObjectPtr<APhysicsProp>& WeakProp : House->GetProps())
	{
		APhysicsProp* Prop = WeakProp.Get();

		if (!IsValid(Prop))
		{
			continue;
		}

		FBindingItem& Item = Items.AddDefaulted_GetRef();
		Item.Actor = Prop;
		Item.bIsProp = true;
		Item.Level = House->GetPropLevel(Prop);
		Item.Weight = WeightOf(Prop->GetClass(), Prop->Value);
		Item.Value = Prop->Value;
	}

	// The scoring lives on the rules asset the pieces carry. Remember it now, while a piece stands: the report is made at the end, when the house may be gone.
	if (UMissionResultSubsystem* Results = UMissionResultSubsystem::Get(this))
	{
		for (const TWeakObjectPtr<ABuildablePiece>& WeakPiece : House->GetPieces())
		{
			const ABuildablePiece* Piece = WeakPiece.Get();
			const UDurabilityComponent* Durability = IsValid(Piece) ? Piece->GetDurabilityComponent() : nullptr;

			if (Durability && Durability->Rules)
			{
				Results->SetRules(Durability->Rules);
				break;
			}
		}
	}

	// The furniture the client wants back (salvage condition). Its own stream, so the generator's draws and the partial
	// demolition's are untouched; the props come in emission order, so the same house gives the same pick. A prop is
	// drawn with a weight of its value (at least 1): the sofa and the piano are likelier than the lamp.
	if (SalvageCount > 0)
	{
		FRandomStream SalvageStream(HashCombine(static_cast<uint32>(House->GetEffectiveSeed()), 0x5A17u));

		TArray<int32> PropIndices;

		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (Items[Index].bIsProp)
			{
				PropIndices.Add(Index);
			}
		}

		for (int32 Picked = 0; Picked < SalvageCount && !PropIndices.IsEmpty(); ++Picked)
		{
			int32 WeightSum = 0;

			for (const int32 Index : PropIndices)
			{
				WeightSum += FMath::Max(1, Items[Index].Value);
			}

			const int32 Roll = SalvageStream.RandRange(0, WeightSum - 1);
			int32 Passed = 0;
			int32 At = PropIndices.Num() - 1;

			for (int32 Slot = 0; Slot < PropIndices.Num(); ++Slot)
			{
				Passed += FMath::Max(1, Items[PropIndices[Slot]].Value);

				if (Roll < Passed)
				{
					At = Slot;
					break;
				}
			}

			FBindingItem& Chosen = Items[PropIndices[At]];
			Chosen.bSalvage = true;
			Chosen.Label = FName(*MissionBinding::ItemName(Chosen.Actor.Get()));
			PropIndices.RemoveAt(At);
		}
	}

	FString Note;
	FMissionSets Sets = MissionBinding::Compute(Type, FromLevel, StoreyCount, Items, &Note);

	Summary = FString::Printf(TEXT("%s\nHouse: seed %d, %d pieces, %d props, %d storey(s)"), *MissionBinding::Describe(Sets), House->GetEffectiveSeed(), House->GetPieceCount(), House->GetPropCount(), StoreyCount);

	if (!Note.IsEmpty())
	{
		Summary += TEXT("\n") + Note;
	}

	if (!ShopCatalog)
	{
		Summary += TEXT("\nNo ShopCatalog on the director: every weight fell back (1, or the prop's value).");
	}
	else if (!MissingRows.IsEmpty())
	{
		FString Names;

		for (const TPair<FName, int32>& Pair : MissingRows)
		{
			Names += FString::Printf(TEXT("%s%s x%d"), Names.IsEmpty() ? TEXT("") : TEXT(", "), *Pair.Key.ToString(), Pair.Value);
		}

		Summary += FString::Printf(TEXT("\nNo DT_Shop cost for %d class(es), weight fell back: %s"), MissingRows.Num(), *Names);
	}

	UE_LOG(LogPlotBuilder, Log, TEXT("%s"), *Summary);

	if (GEngine)
	{
		PBDebug::Message(DirectorMessageKey, 20.f, FColor::Cyan, Summary);
	}

	Plot->SetMissionSets(MoveTemp(Sets));

	// The house is built and bound now: the zones look again at what shares their volume, this time knowing which things are the house.
	for (const TObjectPtr<AProtectedZone>& Zone : Plot->GetProtectedZones())
	{
		if (Zone)
		{
			Zone->ScanVolume();
		}
	}
}

FContractDefinition AMissionDirector::GetMissionContract()
{
	if (const UContractSubsystem* Contracts = UContractSubsystem::Get(this))
	{
		if (Contracts->HasActiveContract())
		{
			return Contracts->GetActiveContract();
		}
	}

	// No contract was taken: one is made from the house that stands, so the report still has a base payout to work from.
	FContractDefinition Contract;
	Contract.ClientId = FName(TEXT("Debug"));

	if (const APlot* Plot = ResolvePlot())
	{
		Contract.Type = Plot->GetMissionSets().Type;
	}

	if (const AHouseGenerator* House = ResolveGenerator())
	{
		Contract.Seed = House->GetEffectiveSeed();
		Contract.FootprintCells = House->GetFootprintCells();
		Contract.Floors = House->GetFloorCount();

		if (House->Archetype)
		{
			Contract.ArchetypeId = FName(*House->Archetype->GetName());
		}
	}

	Contract.AreaM2 = static_cast<float>(Contract.FootprintCells.X * Contract.FootprintCells.Y * Contract.Floors) * UContractPool::SquareMetresPerCell;
	Contract.EstimatedMinutes = UContractEvaluator::EstimateMinutes(Contract.Type, Contract.AreaM2);
	Contract.BasePayout = UContractEvaluator::EstimateBasePayout(Contract.Type, Contract.AreaM2);

	return Contract;
}

void AMissionDirector::SetShowTargets(bool bShow)
{
	bShowTargets = bShow;
	SetActorTickEnabled(bShow);
}

void AMissionDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const APlot* Plot = ResolvePlot();

	if (!bShowTargets || !PBDebug::IsEnabled() || !Plot)
	{
		return;
	}

	const FMissionSets& Sets = Plot->GetMissionSets();
	const UWorld* World = GetWorld();

	const auto DrawSet = [World](const TArray<FBindingItem>& Set, const FColor& Color)
	{
		for (const FBindingItem& Item : Set)
		{
			if (const AActor* Actor = Item.Actor.Get())
			{
				const FBox Box = Actor->GetComponentsBoundingBox(true);

				if (Box.IsValid)
				{
					DrawDebugBox(World, Box.GetCenter(), Box.GetExtent(), Color, false, -1.f, 0, 1.5f);
				}
			}
		}
	};

	DrawSet(Sets.Targets, FColor::Green);
	DrawSet(Sets.MustStay, FColor(60, 120, 255));
	DrawSet(Sets.Salvage, FColor::Magenta);
}
