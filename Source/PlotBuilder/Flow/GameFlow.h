#pragma once

#include "CoreMinimal.h"

/**
 *  Where the player is in the loop: hub, contract picked, on site, debrief, and the player's own home next to it (GDD §4).
 *  The state is not stored anywhere as a number that has to be kept in step: it is worked out from facts the game already
 *  has (is there a mission director in this level, is a contract active...), so it can never disagree with them.
 */
enum class EGameFlowState : uint8
{
	/** At the hub with nothing picked: the board is open to choose from. */
	InHub,
	/** At the hub, a contract is taken and the truck is waiting. */
	ContractSelected,
	/** In a mission level: the house is there to be taken down. */
	OnSite,
	/** The report of the mission just ended is on screen (at the hub). */
	Debrief,
	/** In the player's own home. */
	InHome
};

/** What the game says about the current moment. Whoever gathers them fills this; the tests fill it by hand. */
struct FGameFlowFacts
{
	/** The level holds a mission director. */
	bool bInMission = false;

	/** The level holds a home director. */
	bool bInHome = false;

	/** The debrief screen is the one open on the HUD. */
	bool bDebriefOpen = false;

	/** A contract is taken and not completed yet. */
	bool bHasActiveContract = false;
};

namespace GameFlow
{
	/**
	 *  The state of the loop for those facts. In order of priority: a mission level is OnSite whatever else is true; then an open
	 *  debrief is Debrief; then the home is InHome; then a taken contract is ContractSelected; anything else is InHub.
	 */
	EGameFlowState Resolve(const FGameFlowFacts& Facts);

	/** The state as the debug readout writes it ("InHub", "OnSite"...). */
	const TCHAR* ToText(EGameFlowState State);
}
