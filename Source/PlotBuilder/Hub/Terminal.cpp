#include "Terminal.h"
#include "UI/PlayerMessages.h"

ATerminal::ATerminal()
{
	Prompt = NSLOCTEXT("Hub", "TerminalPrompt", "[E] Use the PC");
}

void ATerminal::Use(AActor* User)
{
	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(NSLOCTEXT("Hub", "TerminalPlaceholder", "PC: the mission board comes on Nov 10."), EMessageTone::Info);
	}
}
