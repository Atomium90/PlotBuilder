#include "Terminal.h"
#include "UI/GameHUDWidget.h"
#include "UI/PlayerMessages.h"

ATerminal::ATerminal()
{
	Prompt = NSLOCTEXT("Hub", "TerminalPrompt", "[E] Use the PC");
}

void ATerminal::Use(AActor* User)
{
	// The HUD owns every screen and the input mode: the PC only asks it to open its window.
	if (UGameHUDWidget* Hud = UGameHUDWidget::Find(this))
	{
		Hud->OpenMenu(UGameHUDWidget::TerminalMenuName);
		return;
	}

	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(NSLOCTEXT("Hub", "TerminalNoScreen", "The PC does not start: this level has no HUD with a terminal screen."), EMessageTone::Warning);
	}
}
