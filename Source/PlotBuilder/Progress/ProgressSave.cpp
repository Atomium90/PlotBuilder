#include "ProgressSave.h"

void UProgressSave::ResetToDefaults()
{
	Money = DefaultMoney;

	ResetTools();

	bParcelOwned = false;

	Home = FHomeSnapshot();
}

void UProgressSave::ResetTools()
{
	OwnedTools.Reset();
	OwnedTools.Add(FName("Hands"));
	OwnedTools.Add(FName("Sledgehammer"));

	Charges = 0;
}
