#include "Contracts/ContractPool.h"
#include "Contracts/ClientRow.h"
#include "Contracts/ContractEvaluator.h"
#include "Engine/DataTable.h"
#include "Generation/HouseArchetype.h"

const FClientRow* UContractPool::FindClient(FName ClientId) const
{
	return ClientTable ? ClientTable->FindRow<FClientRow>(ClientId, TEXT("UContractPool::FindClient"), false) : nullptr;
}

FContractDefinition UContractPool::DrawContract(const FContractTemplate& Template, FRandomStream& Stream) const
{
	FIntPoint MinCells = Template.MinFootprintCells;
	FIntPoint MaxCells = Template.MaxFootprintCells;
	int32 MinFloors = Template.MinFloors;
	int32 MaxFloors = Template.MaxFloors;

	if (const UHouseArchetype* Archetype = Template.Archetype)
	{
		// What the template leaves open comes from the archetype.
		MinCells.X = MinCells.X > 0 ? MinCells.X : Archetype->MinRoomCells.X;
		MinCells.Y = MinCells.Y > 0 ? MinCells.Y : Archetype->MinRoomCells.Y;
		MaxCells.X = MaxCells.X > 0 ? MaxCells.X : Archetype->MaxRoomCells.X;
		MaxCells.Y = MaxCells.Y > 0 ? MaxCells.Y : Archetype->MaxRoomCells.Y;

		const bool bHasFloorRange = Archetype->MinFloors > 0 && Archetype->MaxFloors >= Archetype->MinFloors;

		if (!bHasFloorRange)
		{
			// The plans of this archetype have a fixed number of storeys: the generator would ignore any other request.
			MinFloors = FMath::Max(1, Archetype->Floors);
			MaxFloors = MinFloors;
		}
		else if (MinFloors <= 0)
		{
			MinFloors = Archetype->MinFloors;
			MaxFloors = MaxFloors > 0 ? MaxFloors : Archetype->MaxFloors;
		}
	}

	MinCells.X = FMath::Max(1, MinCells.X);
	MinCells.Y = FMath::Max(1, MinCells.Y);
	MaxCells.X = FMath::Max(MinCells.X, MaxCells.X);
	MaxCells.Y = FMath::Max(MinCells.Y, MaxCells.Y);
	MinFloors = FMath::Max(1, MinFloors);
	MaxFloors = FMath::Max(MinFloors, MaxFloors);

	FContractDefinition Contract;

	// The order of the draws is fixed: it is what makes a stream give the same offers again.
	Contract.FootprintCells.X = Stream.RandRange(MinCells.X, MaxCells.X);
	Contract.FootprintCells.Y = Stream.RandRange(MinCells.Y, MaxCells.Y);
	Contract.Floors = Stream.RandRange(MinFloors, MaxFloors);

	const float TotalWeight = FMath::Max(0.f, Template.TotalWeight);
	const float PartialWeight = Contract.Floors >= 2 ? FMath::Max(0.f, Template.PartialWeight) : 0.f;
	const float RenovationWeight = FMath::Max(0.f, Template.RenovationWeight);
	const float TypeRoll = Stream.FRand() * (TotalWeight + PartialWeight + RenovationWeight);

	Contract.Type = EContractType::TotalDemolition;

	if (TypeRoll >= TotalWeight + PartialWeight && RenovationWeight > 0.f)
	{
		Contract.Type = EContractType::Renovation;
	}
	else if (TypeRoll >= TotalWeight && PartialWeight > 0.f)
	{
		Contract.Type = EContractType::PartialDemolition;
	}

	const int32 Salvage = Stream.RandRange(FMath::Max(0, Template.MinSalvage), FMath::Max(Template.MinSalvage, Template.MaxSalvage));
	const int32 Zones = Stream.RandRange(FMath::Max(0, Template.MinZones), FMath::Max(Template.MinZones, Template.MaxZones));

	Contract.Seed = Stream.RandRange(1, 99999);
	Contract.ClientId = Clients.IsEmpty() ? FName(TEXT("Client")) : Clients[Stream.RandRange(0, Clients.Num() - 1)];

	// The two conditions added later draw after everything above, so the contracts that already existed keep their size, type, seed and client for a given stream.
	const float KeepRoll = Stream.FRand();
	const int32 KeepPick = Stream.RandRange(0, FMath::Max(0, Template.KeepStandingRooms.Num() - 1));
	const float ForbiddenRoll = Stream.FRand();
	const int32 ForbiddenPick = Stream.RandRange(0, FMath::Max(0, Template.ForbiddenTools.Num() - 1));

	if (Salvage > 0)
	{
		FContractCondition& Condition = Contract.Conditions.AddDefaulted_GetRef();
		Condition.Type = EConditionType::SalvageFurniture;
		Condition.Count = Salvage;
	}

	if (Zones > 0)
	{
		FContractCondition& Condition = Contract.Conditions.AddDefaulted_GetRef();
		Condition.Type = EConditionType::ProtectedZone;
		Condition.Count = Zones;
	}

	// Only a total demolition asks to keep a room. A renovation already keeps the whole shell, and a partial demolition keeps whole storeys:
	// the room would be lumped in with them when the report counts what had to stay, without the cap of its own.
	if (Contract.Type == EContractType::TotalDemolition && !Template.KeepStandingRooms.IsEmpty() && KeepRoll < Template.KeepStandingChance)
	{
		FContractCondition& Condition = Contract.Conditions.AddDefaulted_GetRef();
		Condition.Type = EConditionType::KeepStanding;
		Condition.Count = 1;
		Condition.Param = FName(*StaticEnum<ERoomType>()->GetNameStringByValue(static_cast<int64>(Template.KeepStandingRooms[KeepPick])));
	}

	if (!Template.ForbiddenTools.IsEmpty() && ForbiddenRoll < Template.ForbiddenToolChance)
	{
		FContractCondition& Condition = Contract.Conditions.AddDefaulted_GetRef();
		Condition.Type = EConditionType::ForbiddenTool;
		Condition.Count = 1;
		Condition.Param = Template.ForbiddenTools[ForbiddenPick];
	}

	Contract.ArchetypeId = Template.Archetype ? FName(*Template.Archetype->GetName()) : NAME_None;
	Contract.ReputationRequired = Template.Tier;

	// The estimates come from the request: the generator may stretch the footprint a little, the debug log compares the two.
	Contract.AreaM2 = static_cast<float>(Contract.FootprintCells.X * Contract.FootprintCells.Y * Contract.Floors) * SquareMetresPerCell;
	Contract.EstimatedMinutes = UContractEvaluator::EstimateMinutes(Contract.Type, Contract.AreaM2, Salvage);
	Contract.BasePayout = FMath::RoundToInt(static_cast<float>(UContractEvaluator::EstimateBasePayout(Contract.Type, Contract.AreaM2)) * FMath::Max(0.f, Template.PayoutMultiplier) / 10.f) * 10;

	return Contract;
}

TArray<FContractDefinition> UContractPool::DrawOffers(int32 Count, FRandomStream& Stream, int32 MaxTier) const
{
	TArray<FContractDefinition> Offers;

	TArray<int32> Candidates;

	for (int32 Index = 0; Index < Templates.Num(); ++Index)
	{
		// A kind above the player's tier is not offered, whatever its weight.
		if (Templates[Index].Weight > 0.f && Templates[Index].Tier <= MaxTier)
		{
			Candidates.Add(Index);
		}
	}

	if (Candidates.IsEmpty())
	{
		return Offers;
	}

	TArray<int32> Remaining = Candidates;

	for (int32 Offer = 0; Offer < Count; ++Offer)
	{
		// Every kind has been used: they all come back.
		if (Remaining.IsEmpty())
		{
			Remaining = Candidates;
		}

		float WeightSum = 0.f;

		for (const int32 Index : Remaining)
		{
			WeightSum += Templates[Index].Weight;
		}

		const float Roll = Stream.FRand() * WeightSum;
		float Passed = 0.f;
		int32 PickedAt = Remaining.Num() - 1;

		for (int32 Slot = 0; Slot < Remaining.Num(); ++Slot)
		{
			Passed += Templates[Remaining[Slot]].Weight;

			if (Roll < Passed)
			{
				PickedAt = Slot;
				break;
			}
		}

		const int32 Picked = Remaining[PickedAt];
		Remaining.RemoveAt(PickedAt);

		Offers.Add(DrawContract(Templates[Picked], Stream));
	}

	return Offers;
}
