#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

/**
 *  "Press again to confirm" for a take-back that could bring other pieces down. The first Confirm on a target arms it and answers false (the
 *  caller says why); a second Confirm on the same target within Window seconds answers true and disarms. Another target, or a press that comes
 *  too late, arms again instead. Pure: the time is given, so it is tested with no level (Automation RunTests PlotBuilder.Hotbar).
 */
struct FTakeBackConfirm
{
	bool Confirm(const UObject* Target, double Now, double Window)
	{
		if (Target && Armed.Get() == Target && Now <= ArmedUntil)
		{
			Armed.Reset();
			return true;
		}

		Armed = Target;
		ArmedUntil = Now + Window;
		return false;
	}

private:

	TWeakObjectPtr<const UObject> Armed;
	double ArmedUntil = 0.0;
};
