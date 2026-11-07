// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlotBuilderGameMode.h"
#include "UI/PlotBuilderHUD.h"

APlotBuilderGameMode::APlotBuilderGameMode()
{
	// Draws the prompts and short messages of UPlayerMessages. A game mode Blueprint keeps this unless it sets its own HUD Class.
	HUDClass = APlotBuilderHUD::StaticClass();
}
