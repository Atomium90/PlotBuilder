#include "DemolitionRules.h"
#include "PlotBuilder.h"

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
