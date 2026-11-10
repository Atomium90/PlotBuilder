#include "Contracts/ContractSubsystem.h"
#include "Contracts/ContractPool.h"
#include "Debug/DebugDisplay.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PlotBuilder.h"

void UContractSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// A different board every time the game starts. pb.RefreshOffers <seed> brings a known one back for a test.
	OfferStream.Initialize(static_cast<int32>(FDateTime::UtcNow().GetTicks() & 0x7FFFFFFF));
}

UContractSubsystem* UContractSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UContractSubsystem>() : nullptr;
}

void UContractSubsystem::SetPool(UContractPool* NewPool)
{
	if (!NewPool || NewPool == Pool)
	{
		return;
	}

	Pool = NewPool;
	bOffersStale = true;
}

const FClientRow* UContractSubsystem::FindClient(FName ClientId) const
{
	return Pool ? Pool->FindClient(ClientId) : nullptr;
}

void UContractSubsystem::SetPlayerTier(int32 NewTier)
{
	NewTier = FMath::Max(0, NewTier);

	if (NewTier == PlayerTier)
	{
		return;
	}

	PlayerTier = NewTier;
	bOffersStale = true;
}

void UContractSubsystem::RefreshOffers(int32 Seed)
{
	if (!Pool)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UContractSubsystem: no contract pool, no offers. Assign ContractPool on the hub director or the mission director of the level."));
		return;
	}

	if (Seed >= 0)
	{
		OfferStream.Initialize(Seed);
	}

	Offers = Pool->DrawOffers(OfferCount, OfferStream, PlayerTier);
	bOffersStale = false;

	if (Offers.IsEmpty())
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UContractSubsystem: the pool '%s' has no template with a weight at tier %d or under, the board stays empty."), *Pool->GetName(), PlayerTier);
	}

	OnOffersChanged.Broadcast();
}

void UContractSubsystem::EnsureFreshOffers()
{
	if (Pool && (bOffersStale || Offers.IsEmpty()))
	{
		RefreshOffers();
	}
}

bool UContractSubsystem::AcceptOffer(int32 Index)
{
	if (!Offers.IsValidIndex(Index))
	{
		return false;
	}

	Active = Offers[Index];
	bHasActive = true;
	OnActiveContractChanged.Broadcast();
	return true;
}

void UContractSubsystem::ClearActiveContract()
{
	if (!bHasActive)
	{
		return;
	}

	Active = FContractDefinition();
	bHasActive = false;
	OnActiveContractChanged.Broadcast();
}

int32 UContractSubsystem::CompleteActiveContract(const FDebriefResult& Result)
{
	// Money is due only for a contract the player took, and only once: the contract is closed right here.
	const int32 Due = bHasActive ? FMath::Max(0, Result.Payout) : 0;

	LastResult = Result;
	bHasLastResult = true;
	bResultUnseen = true;

	// The player is back at the hub with a new board to read.
	bOffersStale = true;

	Active = FContractDefinition();
	bHasActive = false;
	OnActiveContractChanged.Broadcast();

	return Due;
}

bool UContractSubsystem::ConsumeUnseenResult(FDebriefResult& OutResult)
{
	if (!bResultUnseen)
	{
		return false;
	}

	bResultUnseen = false;
	OutResult = LastResult;
	return true;
}

FString UContractSubsystem::Describe(const FContractDefinition& Contract)
{
	FString Conditions;

	for (const FContractCondition& Condition : Contract.Conditions)
	{
		const UEnum* ConditionEnum = StaticEnum<EConditionType>();

		// A count means something for the furniture and the zones; the other two name a room or a tool.
		const bool bNamesSomething = Condition.Type == EConditionType::KeepStanding || Condition.Type == EConditionType::ForbiddenTool;
		const FString Detail = bNamesSomething ? FString::Printf(TEXT("(%s)"), *Condition.Param.ToString()) : FString::Printf(TEXT(" x%d"), Condition.Count);

		Conditions += FString::Printf(TEXT("%s%s%s"), Conditions.IsEmpty() ? TEXT("") : TEXT(", "), *ConditionEnum->GetNameStringByValue(static_cast<int64>(Condition.Type)), *Detail);
	}

	return FString::Printf(TEXT("%s | %s | %s | %dx%d cells, %d storey(s) = %.0f m2 | ~%.1f min | pay %d | tier %d | seed %d | %s"),
		*Contract.ClientId.ToString(),
		*StaticEnum<EContractType>()->GetNameStringByValue(static_cast<int64>(Contract.Type)),
		Contract.ArchetypeId.IsNone() ? TEXT("(level archetype)") : *Contract.ArchetypeId.ToString(),
		Contract.FootprintCells.X, Contract.FootprintCells.Y, Contract.Floors, Contract.AreaM2,
		Contract.EstimatedMinutes, Contract.BasePayout, Contract.ReputationRequired, Contract.Seed,
		Conditions.IsEmpty() ? TEXT("no condition") : *Conditions);
}

FString UContractSubsystem::DescribeBoard() const
{
	if (Offers.IsEmpty())
	{
		return FString::Printf(TEXT("Contract board (tier %d): no offers."), PlayerTier);
	}

	FString Text = FString::Printf(TEXT("Contract board (tier %d):"), PlayerTier);

	for (int32 Index = 0; Index < Offers.Num(); ++Index)
	{
		Text += FString::Printf(TEXT("\n  %d. %s"), Index + 1, *Describe(Offers[Index]));
	}

	Text += bHasActive ? FString::Printf(TEXT("\n  Active: %s"), *Describe(Active)) : FString(TEXT("\n  Active: none"));
	return Text;
}

void UContractSubsystem::ShowOnScreen(float Seconds) const
{
	UE_LOG(LogPlotBuilder, Log, TEXT("%s"), *DescribeBoard());

	if (!GEngine)
	{
		return;
	}

	for (int32 Index = 0; Index < Offers.Num(); ++Index)
	{
		PBDebug::Message(MessageKey + Index, Seconds, FColor::Cyan, FString::Printf(TEXT("%d. %s"), Index + 1, *Describe(Offers[Index])));
	}

	PBDebug::Message(MessageKey + OfferCount, Seconds, FColor::Green, bHasActive ? FString::Printf(TEXT("Active: %s"), *Describe(Active)) : FString(TEXT("Active: none")));
}
