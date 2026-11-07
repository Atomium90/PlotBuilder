#include "Terminal.h"
#include "Engine/Engine.h"

ATerminal::ATerminal()
{
	Prompt = NSLOCTEXT("Hub", "TerminalPrompt", "[E] Use the PC");
}

void ATerminal::Use(AActor* User)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(PlaceholderMessageKey, 4.f, FColor::Cyan, TEXT("PC: the mission board comes on Nov 10."));
	}
}
