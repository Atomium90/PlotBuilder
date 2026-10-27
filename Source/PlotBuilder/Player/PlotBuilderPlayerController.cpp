// Copyright Epic Games, Inc. All Rights Reserved.


#include "PlotBuilderPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "PlotBuilderCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "PlotBuilder.h"
#include "Widgets/Input/SVirtualJoystick.h"

APlotBuilderPlayerController::APlotBuilderPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = APlotBuilderCameraManager::StaticClass();
}

void APlotBuilderPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogPlotBuilder, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void APlotBuilderPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			// Earlier entries get a higher priority, so when two contexts bind the same key to
			// different actions, the one listed first in DefaultMappingContexts wins instead of it
			// being an unspecified tie (every context used to be added at priority 0).
			int32 Priority = DefaultMappingContexts.Num();

			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, Priority--);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool APlotBuilderPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
