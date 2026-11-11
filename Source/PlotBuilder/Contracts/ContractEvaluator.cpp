#include "Contracts/ContractEvaluator.h"
#include "Destruction/PointCurve.h"

namespace
{
	/** What the tools cost to keep over the mission, and in words: "Jackhammer, 38 s (50 + 2 per second)". Only the tools that have a fee and worked count. */
	int32 EvaluatorUpkeepWanted(const FMissionSnapshot& Snapshot, FString& OutDetail, bool& bOutAnyTool)
	{
		int32 Wanted = 0;
		bOutAnyTool = false;

		for (const FToolUsage& Usage : Snapshot.ToolUsage)
		{
			if (Usage.Upkeep.IsFree() || Usage.ActiveSeconds <= 0.f)
			{
				continue;
			}

			bOutAnyTool = true;
			Wanted += Usage.Upkeep.Cost(Usage.ActiveSeconds);

			OutDetail += FString::Printf(TEXT("%s%s, %.0f s (%s)"), OutDetail.IsEmpty() ? TEXT("") : TEXT("; "), *Usage.ToolId.ToString(), Usage.ActiveSeconds, *Usage.Upkeep.Describe());
		}

		return Wanted;
	}
}

float UContractEvaluator::TypeFactor(EContractType Type)
{
	switch (Type)
	{
	case EContractType::PartialDemolition:
		return 0.6f;
	case EContractType::Renovation:
		return 0.5f;
	default:
		return 1.f;
	}
}

float UContractEvaluator::EstimateMinutes(EContractType Type, float AreaM2, int32 SalvageCount)
{
	return 0.06f * FMath::Max(0.f, AreaM2) * TypeFactor(Type) + MinutesPerSalvageItem * FMath::Max(0, SalvageCount);
}

int32 UContractEvaluator::EstimateBasePayout(EContractType Type, float AreaM2)
{
	return FMath::RoundToInt(12.f * FMath::Max(0.f, AreaM2) * TypeFactor(Type) / 10.f) * 10;
}

float UContractEvaluator::FurnitureValueMultiplier(const FScoringParams& Params, float HPRatio)
{
	const float HP = FMath::Clamp(HPRatio, 0.f, 1.f);

	// No curve: the value goes down in a straight line with the hit points.
	if (Params.FurnitureValuePoints.Num() < 2)
	{
		return HP;
	}

	return PointCurve::Evaluate(Params.FurnitureValuePoints, HP);
}

float UContractEvaluator::ComputeSingleZoneDamage(const FMissionSnapshot& Snapshot, const FZoneResult& Zone, const FScoringParams& Params)
{
	const int64 House = Snapshot.HouseWeight > 0 ? Snapshot.HouseWeight : Snapshot.TargetWeight + Snapshot.MustStayWeight;
	const float Fallen = House > 0 ? static_cast<float>(Zone.FallenWeight) / static_cast<float>(House) * Params.ZoneFallFactor : 0.f;

	return FMath::Clamp(Zone.ToolDamageRatio + Fallen, 0.f, 1.f);
}

float UContractEvaluator::ComputeZoneDamage(const FMissionSnapshot& Snapshot, const FScoringParams& Params)
{
	float Total = 0.f;

	for (const FZoneResult& Zone : Snapshot.Zones)
	{
		Total += ComputeSingleZoneDamage(Snapshot, Zone, Params);
	}

	return FMath::Min(Total, 1.f);
}

float UContractEvaluator::ComputeCompletion(const FMissionSnapshot& Snapshot)
{
	return Snapshot.TargetWeight > 0 ? FMath::Clamp(static_cast<float>(Snapshot.TargetDestroyedWeight) / static_cast<float>(Snapshot.TargetWeight), 0.f, 1.f) : 1.f;
}

FDebriefResult UContractEvaluator::Evaluate(const FContractDefinition& Contract, const FMissionSnapshot& Snapshot, const FScoringParams& Params)
{
	FDebriefResult Result;
	Result.BasePayout = Contract.BasePayout;

	Result.Completion = ComputeCompletion(Snapshot);
	Result.Collateral = Snapshot.MustStayWeight > 0 ? FMath::Clamp(static_cast<float>(Snapshot.MustStayDestroyedWeight) / static_cast<float>(Snapshot.MustStayWeight), 0.f, 1.f) : 0.f;
	Result.ZoneDamage = ComputeZoneDamage(Snapshot, Params);

	float ValueAsked = 0.f;
	float ValueKept = 0.f;
	int32 Delivered = 0;
	int32 Destroyed = 0;
	int32 OnSite = 0;

	for (const FSalvageResult& Item : Snapshot.Salvage)
	{
		ValueAsked += static_cast<float>(Item.Value);

		if (Item.bDelivered)
		{
			ValueKept += static_cast<float>(Item.Value) * FurnitureValueMultiplier(Params, Item.HPRatio);
			++Delivered;
		}

		Destroyed += Item.bDestroyed ? 1 : 0;
		OnSite += Item.bOnSite ? 1 : 0;
	}

	Result.FurnitureLoss = ValueAsked > 0.f ? FMath::Clamp(1.f - ValueKept / ValueAsked, 0.f, 1.f) : 0.f;
	Result.Safety = FMath::Min(Params.SafetyCap, Snapshot.CrushEvents * Params.CrushPenalty + Snapshot.ExplosionEvents * Params.ExplosionPenalty);
	Result.CrushEvents = Snapshot.CrushEvents;
	Result.ExplosionEvents = Snapshot.ExplosionEvents;

	// Once for the whole mission, whatever the number of uses.
	Result.ForbiddenTool = Snapshot.bForbiddenToolUsed ? Params.ForbiddenToolPenalty : 0.f;

	const float CollateralShare = Params.CollateralCoefficient * Result.Collateral;
	const float ZoneShare = Params.ZoneCoefficient * Result.ZoneDamage;
	const float FurnitureShare = Params.FurnitureCoefficient * Result.FurnitureLoss;
	const float Uncapped = CollateralShare + ZoneShare + FurnitureShare + Result.Safety + Result.ForbiddenTool;
	const float Cap = FMath::Clamp(Params.PenaltyCap, 0.f, 1.f);

	Result.bPenaltyCapped = Uncapped > Cap;
	Result.Penalty = FMath::Min(Uncapped, Cap);

	// Money is rounded one term at a time, so the lines add up to the payout to the unit.
	Result.Earned = FMath::RoundToInt(static_cast<float>(Contract.BasePayout) * Result.Completion);

	const float Earned = static_cast<float>(Result.Earned);
	const int32 CollateralMoney = FMath::RoundToInt(Earned * CollateralShare);
	const int32 ZoneMoney = FMath::RoundToInt(Earned * ZoneShare);
	const int32 FurnitureMoney = FMath::RoundToInt(Earned * FurnitureShare);
	const int32 SafetyMoney = FMath::RoundToInt(Earned * Result.Safety);
	const int32 ForbiddenMoney = FMath::RoundToInt(Earned * Result.ForbiddenTool);
	const int32 TermsMoney = CollateralMoney + ZoneMoney + FurnitureMoney + SafetyMoney + ForbiddenMoney;

	// The cap can only give money back, never take more than the terms did.
	const int32 PenaltyMoney = Result.bPenaltyCapped ? FMath::Min(TermsMoney, FMath::RoundToInt(Earned * Cap)) : TermsMoney;

	// The upkeep of the tools comes off what is left once the penalty has been taken, and never goes below zero: the job is always paid.
	const int32 AfterPenalty = FMath::Max(0, Result.Earned - PenaltyMoney);

	FString UpkeepDetail;
	bool bAnyUpkeepTool = false;
	const int32 UpkeepWanted = EvaluatorUpkeepWanted(Snapshot, UpkeepDetail, bAnyUpkeepTool);

	Result.Upkeep = FMath::Min(UpkeepWanted, AfterPenalty);
	Result.Payout = AfterPenalty - Result.Upkeep;

	const auto AddLine = [&Result](const TCHAR* Label, const FString& Detail, float Value, int32 Money)
	{
		FDebriefLine& Line = Result.Lines.AddDefaulted_GetRef();
		Line.Label = Label;
		Line.Detail = Detail;
		Line.Value = Value;
		Line.Money = Money;
	};

	AddLine(TEXT("Completion"),
		FString::Printf(TEXT("destroyed %lld of %lld, %.0f%% of the %d base payout"), Snapshot.TargetDestroyedWeight, Snapshot.TargetWeight, Result.Completion * 100.f, Contract.BasePayout),
		Result.Completion, Result.Earned);

	AddLine(TEXT("Collateral"),
		FString::Printf(TEXT("%lld of %lld that had to stay was destroyed (x%.2f)"), Snapshot.MustStayDestroyedWeight, Snapshot.MustStayWeight, Params.CollateralCoefficient),
		Result.Collateral, -CollateralMoney);

	AddLine(TEXT("Protected zones"),
		FString::Printf(TEXT("%d zone(s), damage %.0f%% (x%.2f)"), Snapshot.Zones.Num(), Result.ZoneDamage * 100.f, Params.ZoneCoefficient),
		Result.ZoneDamage, -ZoneMoney);

	AddLine(TEXT("Furniture"),
		FString::Printf(TEXT("kept %.0f of %.0f value, %d of %d delivered, %d destroyed, %d left on the plot (x%.2f)"), ValueKept, ValueAsked, Delivered, Snapshot.Salvage.Num(), Destroyed, OnSite, Params.FurnitureCoefficient),
		Result.FurnitureLoss, -FurnitureMoney);

	AddLine(TEXT("Safety"),
		FString::Printf(TEXT("%d crush, %d explosion (cap %.0f%%)"), Snapshot.CrushEvents, Snapshot.ExplosionEvents, Params.SafetyCap * 100.f),
		Result.Safety, -SafetyMoney);

	// Only a contract that forbade a tool has the line (or a mission where a tool was forbidden by hand and used).
	const FContractCondition* ForbiddenCondition = Contract.FindCondition(EConditionType::ForbiddenTool);

	if (ForbiddenCondition || Snapshot.bForbiddenToolUsed)
	{
		const FName Tool = !Snapshot.ForbiddenToolName.IsNone() ? Snapshot.ForbiddenToolName : (ForbiddenCondition ? ForbiddenCondition->Param : NAME_None);

		AddLine(TEXT("Forbidden tool"),
			Snapshot.bForbiddenToolUsed
				? FString::Printf(TEXT("the %s was used, %.0f%% of what was earned (once for the mission)"), *Tool.ToString(), Params.ForbiddenToolPenalty * 100.f)
				: FString::Printf(TEXT("the %s was not used"), *Tool.ToString()),
			Result.ForbiddenTool, -ForbiddenMoney);
	}

	if (Result.bPenaltyCapped)
	{
		AddLine(TEXT("Penalty cap"),
			FString::Printf(TEXT("the terms add up to %.0f%%, the cap is %.0f%%"), Uncapped * 100.f, Cap * 100.f),
			Result.Penalty, TermsMoney - PenaltyMoney);
	}

	// Only a mission where a tool with a fee worked has the line.
	if (bAnyUpkeepTool)
	{
		if (UpkeepWanted > Result.Upkeep)
		{
			UpkeepDetail += FString::Printf(TEXT(", %d of %d taken (never below zero)"), Result.Upkeep, UpkeepWanted);
		}

		AddLine(TEXT("Upkeep"), UpkeepDetail, Result.Earned > 0 ? static_cast<float>(Result.Upkeep) / static_cast<float>(Result.Earned) : 0.f, -Result.Upkeep);
	}

	// The share of the base payout that is actually paid.
	AddLine(TEXT("Payout"), FString(), Contract.BasePayout > 0 ? static_cast<float>(Result.Payout) / static_cast<float>(Contract.BasePayout) : 0.f, Result.Payout);

	return Result;
}

FString FDebriefResult::ToString() const
{
	FString Text = FString::Printf(TEXT("Debrief (base payout %d)"), BasePayout);

	for (const FDebriefLine& Line : Lines)
	{
		Text += FString::Printf(TEXT("\n  %-16s %6.1f%% %+7d   %s"), *Line.Label, Line.Value * 100.f, Line.Money, *Line.Detail);
	}

	return Text;
}
