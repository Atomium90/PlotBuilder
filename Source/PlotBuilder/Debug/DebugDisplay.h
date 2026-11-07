#pragma once

#include "CoreMinimal.h"

/**
 *  The master switch of every debug display: the on-screen text, the debug lines and boxes, the heat-map. One
 *  console variable, pb.Debug 0|1 (it can also go in the [SystemSettings] of an ini, or on the command line with
 *  -ExecCmds), decides whether any of them shows. It is on in the editor and off in a packaged build, so a demo build
 *  starts clean and a developer turns it on with pb.Debug 1.
 *
 *  What goes through here: text and drawings meant for the developer (summaries, readouts, zones, the heat-map).
 *  What the player has to read (the prompt of a door, a refused trip) goes through UPlayerMessages instead and is
 *  never touched by this switch. The two used to share GEngine->AddOnScreenDebugMessage, which is why neither could
 *  be turned off without the other.
 *
 *  A display has two switches: its own (pb.ShowStability, bShowDebug on an actor...) and this one. It shows only if
 *  both are on, and turning this one back on brings back what was on before. Anything that changes the world rather
 *  than draws (the heat-map swaps materials) has to look at IsEnabled itself and undo its change when it goes off.
 */
namespace PBDebug
{
	/** True while the debug displays are on (pb.Debug 1). Cheap: read it every frame if needed. */
	bool IsEnabled();

	/**
	 *  GEngine->AddOnScreenDebugMessage when the switch is on, nothing otherwise. Same arguments, so a call site only
	 *  changes its name. Key -1 adds a new line each time, any other key replaces the line with that key.
	 */
	void Message(int32 Key, float Seconds, const FColor& Color, const FString& Text);
}
