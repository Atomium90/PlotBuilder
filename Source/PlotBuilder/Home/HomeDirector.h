#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HomeDirector.generated.h"

class APhysicsProp;
class APlot;

/**
 *  The one actor Lvl_Home needs besides its plot: it keeps the player's house between visits (GDD §12, D24).
 *
 *  When the level starts it puts back what the player left, pieces first through APlot::PlacePiece (the placement
 *  function build mode and the generator also use, so every piece is registered in the structural graph), then the
 *  loose props, then the inventory once the pawn exists. When the player leaves through a travel point it writes
 *  what is on the plot to the progress save (UProgressSubsystem::OnBeforeLevelChange), so the snapshot is taken
 *  while the world is still whole.
 *
 *  What is saved: the graph's own list of pieces (class, cell, quarter turns, roof step, the scale they were placed
 *  with), every prop that was not already in the level, and the inventory grid. What is not: a piece placed by hand
 *  in the editor (the graph does not know it), a prop placed by hand in the editor (it is in the level again
 *  anyway), a Legacy-anchored piece (counted and reported), and anything when the level is left without a travel
 *  point (Escape, stopping PIE): pb.SaveHome takes a snapshot on demand.
 */
UCLASS()
class AHomeDirector : public AActor
{
	GENERATED_BODY()

public:

	AHomeDirector();

	/** The plot that holds the house. Empty = the first one in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Home")
	TObjectPtr<APlot> Plot;

	/** Put the saved house back when the level starts. Off to start from an empty plot (the snapshot stays in the save). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Home")
	bool bRestoreOnBeginPlay = true;

	/** Writes the pieces, props and inventory that are in the level to the progress save. Does nothing without a plot. */
	UFUNCTION(BlueprintCallable, Category = "Home")
	void SaveSnapshot();

	/** One line saying what the last restore or snapshot did, for the console commands. */
	const FString& GetSummary() const { return Summary; }

	/** The home director of this world, or nullptr (a world without a home). */
	static AHomeDirector* Find(const UObject* WorldContext);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	static constexpr int32 MaxInventoryAttempts = 50;
	static constexpr float InventoryRetryInterval = 0.1f;

	/** On-screen key of the summary line (7600 to 7699 belong to the progress and hub readouts). */
	static constexpr int32 SummaryMessageKey = 7640;

	/** Puts the saved pieces and props back. Fills Summary with what it did. */
	void RestorePlot();

	/** Puts the saved inventory back. The pawn may not exist yet when the level starts: retried until it does. */
	void ApplyInventory();

	/** Prints Summary on screen and in the log. */
	void Report(const FColor& Color) const;

	APlot* ResolvePlot() const;

	FDelegateHandle LeaveHandle;
	FTimerHandle InventoryTimer;
	int32 InventoryAttempts = 0;
	FString Summary;

	/** The props the level already held when it started: they come back with the level, so they are never saved. */
	TSet<TWeakObjectPtr<APhysicsProp>> LevelProps;
};
