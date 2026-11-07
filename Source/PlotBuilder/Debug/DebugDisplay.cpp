#include "Debug/DebugDisplay.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// File-private names are unique across the module (unity build).

#if WITH_EDITOR
	constexpr int32 DebugSwitchDefault = 1;
#else
	constexpr int32 DebugSwitchDefault = 0;
#endif

	/** Switching off also takes down what is already up: the text still counting down and the lines that stay for seconds. */
	void OnDebugSwitchChanged(IConsoleVariable* Variable)
	{
		if (Variable->GetInt() != 0 || !GEngine)
		{
			return;
		}

		GEngine->ClearOnScreenDebugMessages();

#if ENABLE_DRAW_DEBUG
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (const UWorld* World = Context.World())
			{
				FlushPersistentDebugLines(World);
				FlushDebugStrings(World);
			}
		}
#endif
	}

	TAutoConsoleVariable<int32> CVarDebugSwitch(
		TEXT("pb.Debug"),
		DebugSwitchDefault,
		TEXT("pb.Debug 0|1 - master switch of every debug display (on-screen text, debug lines and boxes, the stability heat-map). 0 for a demo: nothing of the developer's shows, the player's own messages stay. On in the editor, off in a packaged build. A display shows only if both this and its own switch (pb.ShowStability...) are on."),
		ECVF_Default);

	[[maybe_unused]] const bool bDebugSwitchHooked = []()
	{
		CVarDebugSwitch.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnDebugSwitchChanged));
		return true;
	}();
}

bool PBDebug::IsEnabled()
{
	return CVarDebugSwitch.GetValueOnAnyThread() != 0;
}

void PBDebug::Message(int32 Key, float Seconds, const FColor& Color, const FString& Text)
{
	if (GEngine && IsEnabled())
	{
		GEngine->AddOnScreenDebugMessage(Key, Seconds, Color, Text);
	}
}
