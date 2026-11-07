#include "UI/PlayerMessages.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "PlotBuilder.h"
#include "UI/PlotBuilderHUD.h"

UPlayerMessages* UPlayerMessages::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UPlayerMessages>() : nullptr;
}

double UPlayerMessages::Now() const
{
	// Real time, so a message does not freeze with the game when it is paused.
	return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
}

bool UPlayerMessages::HasPresenter()
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;

	// No controller yet (the level is starting): assume the HUD will be there, the next call checks again.
	if (!Controller || Cast<APlotBuilderHUD>(Controller->GetHUD()))
	{
		return true;
	}

	if (!bWarnedNoPresenter)
	{
		bWarnedNoPresenter = true;
		UE_LOG(LogPlotBuilder, Warning, TEXT("UPlayerMessages: the HUD is %s, not APlotBuilderHUD, so the prompts and messages are not drawn. Set the game mode's HUD Class to APlotBuilderHUD. Falling back to on-screen debug text."),
			*GetNameSafe(Controller->GetHUD()));
	}

	return false;
}

void UPlayerMessages::SetPrompt(const UObject* Owner, const FText& Text)
{
	if (Text.IsEmpty())
	{
		ClearPrompt(Owner);
		return;
	}

	PromptOwner = Owner;
	PromptText = Text;

	if (!HasPresenter() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(FallbackPromptKey, 600.f, FColor::Yellow, Text.ToString());
	}
}

const FText& UPlayerMessages::GetPrompt()
{
	if (!PromptText.IsEmpty() && !PromptOwner.IsValid())
	{
		PromptText = FText::GetEmpty();
	}

	return PromptText;
}

void UPlayerMessages::ClearPrompt(const UObject* Owner)
{
	if (PromptOwner.Get() != Owner)
	{
		return;
	}

	PromptOwner.Reset();
	PromptText = FText::GetEmpty();

	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(FallbackPromptKey);
	}
}

void UPlayerMessages::Notify(const FText& Text, EMessageTone Tone, float Seconds)
{
	if (Text.IsEmpty())
	{
		return;
	}

	const double ExpiresAt = Now() + Seconds;

	// The same text still up: keep one line and restart its timer.
	for (FPlayerToast& Toast : Toasts)
	{
		if (Toast.Text.EqualTo(Text))
		{
			Toast.Tone = Tone;
			Toast.ExpiresAt = ExpiresAt;
			return;
		}
	}

	if (Toasts.Num() >= MaxToasts)
	{
		Toasts.RemoveAt(0);
	}

	FPlayerToast& Added = Toasts.AddDefaulted_GetRef();
	Added.Text = Text;
	Added.Tone = Tone;
	Added.ExpiresAt = ExpiresAt;

	if (!HasPresenter() && GEngine)
	{
		const FColor Color = Tone == EMessageTone::Error ? FColor::Red : Tone == EMessageTone::Warning ? FColor::Orange : FColor::White;
		GEngine->AddOnScreenDebugMessage(FallbackToastKey, Seconds, Color, Text.ToString());
	}
}

const TArray<FPlayerToast>& UPlayerMessages::GetToasts()
{
	const double Time = Now();
	Toasts.RemoveAll([Time](const FPlayerToast& Toast) { return Toast.ExpiresAt <= Time; });
	return Toasts;
}
