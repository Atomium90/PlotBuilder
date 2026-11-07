#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlayerMessages.generated.h"

/** How a message reads: the colour it gets on screen. */
enum class EMessageTone : uint8
{
	Info,
	Warning,
	Error
};

/** One short message still up: what it says, how, and when it goes away (real-time seconds of the world). */
struct FPlayerToast
{
	FText Text;
	EMessageTone Tone = EMessageTone::Info;
	double ExpiresAt = 0.0;
};

/**
 *  What the game tells the player in words: the prompt of the thing in front of them ("[E] Go") and the short
 *  messages that explain a refusal ("You do not own a parcel yet"). Gameplay code only says what and when; it knows
 *  nothing about how it is drawn. APlotBuilderHUD draws it today, a widget can replace it by reading the same two
 *  getters, and no caller changes.
 *
 *  This is not debug output. Developer text goes through PBDebug::Message, which pb.Debug can switch off; this stays
 *  on whatever pb.Debug says, and it exists in every build.
 *
 *  If the level's HUD is not APlotBuilderHUD (the game mode's HUD class was changed), nothing would draw. The
 *  service says so once in the log and falls back to the engine's on-screen text, so the player still reads it in a
 *  development build instead of silently losing the prompts.
 */
UCLASS()
class UPlayerMessages : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** The messages of this world, or nullptr (no world). */
	static UPlayerMessages* Get(const UObject* WorldContext);

	/**
	 *  Shows Text as the prompt, owned by Owner (the focused actor). A new prompt replaces the old one. An empty text
	 *  clears it.
	 */
	void SetPrompt(const UObject* Owner, const FText& Text);

	/** Takes the prompt down, but only if Owner is the one who set it: the focus may already have moved to something else. */
	void ClearPrompt(const UObject* Owner);

	/** Shows a short message. The same text while it is still up only restarts its timer, so pressing E ten times shows one line. */
	void Notify(const FText& Text, EMessageTone Tone = EMessageTone::Warning, float Seconds = DefaultSeconds);

	/** The prompt line, empty when there is none. Drops a prompt whose owner is gone without clearing it. */
	const FText& GetPrompt();

	/** The messages still up, oldest first. Drops the ones that expired. */
	const TArray<FPlayerToast>& GetToasts();

private:

	static constexpr float DefaultSeconds = 4.f;
	static constexpr int32 MaxToasts = 4;

	/** Keys of the fallback lines (7600 to 7699 belong to the hub, progress and home readouts). */
	static constexpr int32 FallbackPromptKey = 7610;
	static constexpr int32 FallbackToastKey = 7620;

	/** True if the level's HUD draws these messages. Says so once in the log if not. */
	bool HasPresenter();

	double Now() const;

	TWeakObjectPtr<const UObject> PromptOwner;
	FText PromptText;
	TArray<FPlayerToast> Toasts;
	bool bWarnedNoPresenter = false;
};
