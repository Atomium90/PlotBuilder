#include "ProgressSave.h"

void UProgressSave::ResetToDefaults()
{
	Money = DefaultMoney;

	OwnedTools.Reset();
	OwnedTools.Add(FName("Hands"));
	OwnedTools.Add(FName("Sledgehammer"));

	bParcelOwned = false;

	Home = FHomeSnapshot();
}
