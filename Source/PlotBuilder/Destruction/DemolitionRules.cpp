#include "DemolitionRules.h"
#include "PlotBuilder.h"
#include "Curves/CurveFloat.h"

float UDemolitionRules::ComputeMaxHP(EDemolitionMaterial Material, EPieceClass PieceClass) const
{
	const float* BaseHP = MaterialBaseHP.Find(Material);
	const float* Factor = PieceClassFactor.Find(PieceClass);

	if (!BaseHP)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UDemolitionRules::ComputeMaxHP: no MaterialBaseHP entry for material %d, falling back to 100"), (int32)Material);
	}

	if (!Factor)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UDemolitionRules::ComputeMaxHP: no PieceClassFactor entry for piece class %d, falling back to 100"), (int32)PieceClass);
	}

	if (!BaseHP || !Factor)
	{
		return 100.f;
	}

	return *BaseHP * *Factor;
}

float UDemolitionRules::ComputeDamage(EDemolitionTool Tool, EDemolitionMaterial Material, float BaseDamage) const
{
	const FToolMaterialMultipliers* ToolRow = ToolMaterialMatrix.Find(Tool);
	const float* Multiplier = ToolRow ? ToolRow->Multipliers.Find(Material) : nullptr;

	if (!Multiplier)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UDemolitionRules::ComputeDamage: no ToolMaterialMatrix entry for tool %d / material %d, falling back to a 1.0 multiplier"), (int32)Tool, (int32)Material);
		return BaseDamage;
	}

	return BaseDamage * *Multiplier;
}

USoundBase* UDemolitionRules::GetImpactSound(EDemolitionMaterial Material) const
{
	const TObjectPtr<USoundBase>* Sound = ImpactSounds.Find(Material);
	return Sound ? *Sound : nullptr;
}

int32 UDemolitionRules::ComputeFragmentCount(EPieceClass PieceClass) const
{
	const int32* Count = FragmentCountPerClass.Find(PieceClass);

	if (!Count)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("UDemolitionRules::ComputeFragmentCount: no FragmentCountPerClass entry for piece class %d, falling back to 4"), (int32)PieceClass);
		return 4;
	}

	return FMath::Clamp(*Count, 3, 6);
}

FLinearColor UDemolitionRules::GetFragmentTint(EDemolitionMaterial Material) const
{
	const FLinearColor* Tint = FragmentTintColor.Find(Material);
	return Tint ? *Tint : FLinearColor::Gray;
}

float UDemolitionRules::ComputeStabilityCoupling(float HPRatio) const
{
	if (StabilityTransmission)
	{
		return StabilityTransmission->GetFloatValue(HPRatio);
	}

	if (HPRatio >= 0.75f)
	{
		return 1.f;
	}

	if (HPRatio >= 0.35f)
	{
		return FMath::GetMappedRangeValueClamped(FVector2D(0.35f, 0.75f), FVector2D(0.6f, 1.f), HPRatio);
	}

	return FMath::GetMappedRangeValueClamped(FVector2D(0.f, 0.35f), FVector2D(0.3f, 0.6f), HPRatio);
}
