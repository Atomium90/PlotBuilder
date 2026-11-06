#pragma once

#include "CoreMinimal.h"
#include "ContractTypes.generated.h"

/** The three kinds of contract (GDD §6.2). They differ only in what must be destroyed and what must stay: see MissionBinding::Compute. */
UENUM(BlueprintType)
enum class EContractType : uint8
{
	/** The whole property goes. */
	TotalDemolition,
	/** The upper storeys go, from a given storey up. The storeys below stay. */
	PartialDemolition,
	/** The interior is stripped: everything that is not load-bearing, and all the furniture. The shell stays. */
	Renovation
};
