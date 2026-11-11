#include "Flow/GameFlow.h"

EGameFlowState GameFlow::Resolve(const FGameFlowFacts& Facts)
{
	if (Facts.bInMission)
	{
		return EGameFlowState::OnSite;
	}

	if (Facts.bDebriefOpen)
	{
		return EGameFlowState::Debrief;
	}

	if (Facts.bInHome)
	{
		return EGameFlowState::InHome;
	}

	return Facts.bHasActiveContract ? EGameFlowState::ContractSelected : EGameFlowState::InHub;
}

const TCHAR* GameFlow::ToText(EGameFlowState State)
{
	switch (State)
	{
	case EGameFlowState::ContractSelected:
		return TEXT("ContractSelected");
	case EGameFlowState::OnSite:
		return TEXT("OnSite");
	case EGameFlowState::Debrief:
		return TEXT("Debrief");
	case EGameFlowState::InHome:
		return TEXT("InHome");
	default:
		return TEXT("InHub");
	}
}
