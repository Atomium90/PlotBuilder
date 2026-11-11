#pragma once

#include "CoreMinimal.h"
#include "Destruction/DemolitionTypes.h"
#include "Flow/GameFlow.h"

/** How a line of the focus card reads: fine (teal), worth a look (amber), bad (coral). */
enum class EFocusTone : uint8
{
	Good,
	Warn,
	Bad
};

/** What the game knows about the thing under the crosshair, gathered by the widget; the card turns it into lines with FocusCard::Describe. */
struct FFocusCardFacts
{
	/** Something with hit points is under the crosshair. Without it the card is not shown. */
	bool bHasTarget = false;

	FString Name;
	EDemolitionMaterial Material = EDemolitionMaterial::Wood;
	EDurabilityState State = EDurabilityState::Intact;

	/** Hit points left, 0 to 1. */
	float HPRatio = 1.f;

	/** The storey of a piece of the house (0 is the ground floor). -1 for anything that is not a piece of the house. */
	int32 Storey = -1;

	/** What a piece of furniture is to the player ("placed in your home", "the client wants it"). Empty for a piece of the house. */
	FString Role;

	bool bInMission = false;

	/** The structure has given up on this piece: nothing holds it up and it is about to fall. */
	bool bCompromised = false;

	/** The tool in hand has a multiplier against this material (the tool x material matrix). Only for a tool that hits. */
	bool bHasToolMatch = false;
	FString ToolName;
	float ToolMultiplier = 1.f;

	/** A piece of furniture the client asked to get back: its value at full health, and the share of it the health curve leaves. */
	bool bIsSalvage = false;
	int32 Value = 0;
	float ValueShare = 1.f;

	/** The take-back key does something on this target right now. */
	bool bCanTakeBack = false;
};

/** The lines of the card, ready to draw. */
struct FFocusCardView
{
	bool bVisible = false;

	FString Name;

	/** Material, then the storey or the role of the furniture: "Brick · storey 1". */
	FString Sub;

	/** The pill: Intact, Damaged, Critical or Compromised. */
	FString StateText;
	EFocusTone StateTone = EFocusTone::Good;

	/** The health bar, left out at full health outside a mission. Fill is 0 to 1. */
	bool bShowBar = false;
	float Fill = 1.f;
	FString FillText;

	/** How well the tool in hand suits the material ("Sledgehammer: slow on brick"). */
	bool bShowTool = false;
	FString ToolText;
	EFocusTone ToolTone = EFocusTone::Good;

	/** The warning of a piece that is about to fall. */
	bool bShowWarning = false;
	FString WarningText;

	/** The value of a piece of furniture the client wants, now and at full health. */
	bool bShowValue = false;
	FString ValueNow;
	FString ValueText;

	/** The take-back key and its word: never in a mission. */
	bool bShowTakeBack = false;
};

/**
 *  What the focus card says, as a pure function of plain facts, so it is tested without a level or a widget (same idea as the game flow).
 */
namespace FocusCard
{
	/** A tool multiplier at or above this suits the material ("good on"). */
	constexpr float GoodMultiplier = 1.f;

	/** At or above this but under GoodMultiplier, the tool is slow on the material. Under it, it barely scratches it. */
	constexpr float SlowMultiplier = 0.5f;

	/** "Brick", "Concrete"... */
	FString MaterialName(EDemolitionMaterial Material);

	/** Where the card is shown: in a mission and in the home. Not in the hub, where the PC and the truck have nothing to read. */
	bool ShownIn(EGameFlowState State);

	FFocusCardView Describe(const FFocusCardFacts& Facts);
}
