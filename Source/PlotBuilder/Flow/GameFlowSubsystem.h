#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Flow/GameFlow.h"
#include "GameFlowSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameFlowChanged, EGameFlowState /*Old*/, EGameFlowState /*New*/);

/**
 *  The state of the loop for the whole session (see EGameFlowState). It lives on the game instance, like the contract and progress
 *  subsystems, so it follows the player from the hub to a mission and back.
 *
 *  Nothing is recorded as it happens. A few times a second the subsystem reads the facts the game already has (a mission director
 *  in the level, a home director, a contract taken, the debrief screen open) and works the state out of them with GameFlow::Resolve.
 *  A change is logged and announced on OnStateChanged. The state therefore cannot drift from the game, and no class has to remember
 *  to tell it anything.
 *
 *  It only says where the player is. It locks nothing: what a map allows is still the three independent switches of the player
 *  (the belt lock, building, destruction), which a map sets on its own.
 */
UCLASS()
class UGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** On-screen message key of ShowOnScreen (7330, after the contract readouts 7310 to 7329). */
	static constexpr int32 MessageKey = 7330;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The subsystem of the game instance that owns WorldContext, or nullptr (no world, or a world without a game instance). */
	static UGameFlowSubsystem* Get(const UObject* WorldContext);

	/** The state as of the last look (at most PollSeconds old). Cheap, safe to ask every frame. */
	EGameFlowState GetState() const { return State; }

	/** The facts behind the state. */
	const FGameFlowFacts& GetFacts() const { return Facts; }

	/** Looks at the game now instead of waiting for the next poll. Does nothing while no world exists (a level change in progress): the last state stands. */
	void Refresh();

	/** One line: the state and the facts it was worked out from. */
	FString Describe() const;

	/** Prints Describe() on screen for Seconds (pb.State). */
	void ShowOnScreen(float Seconds) const;

	/** Fired when the state changes, with the old and the new one. */
	FOnGameFlowChanged OnStateChanged;

private:

	/** How often the facts are read. A quarter of a second is far below what a player can tell, and cheap enough to ask for the actors of a class. */
	static constexpr float PollSeconds = 0.25f;

	bool HandlePoll(float DeltaTime);

	/** Reads the facts from the current world. False, OutFacts untouched, when there is no world. */
	bool GatherFacts(FGameFlowFacts& OutFacts) const;

	FTSTicker::FDelegateHandle PollHandle;
	EGameFlowState State = EGameFlowState::InHub;
	FGameFlowFacts Facts;
};
