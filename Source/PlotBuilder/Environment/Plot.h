#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Plot.generated.h"

class UBoxComponent;

/**
 *  Minimal buildable area: a resizable box volume other systems can query with IsLocationInside.
 *  No ownership/cost tracking yet - just enough for Build Mode to keep placement inside a defined
 *  plot. Draws its own bounds every tick as a debug box so the area is visible in-game without any
 *  extra content (mesh/material).
 */
UCLASS()
class APlot : public AActor
{
	GENERATED_BODY()

public:

	APlot();

	UBoxComponent* GetBoundsVolume() const { return BoundsVolume; }

	/** Whether WorldLocation falls within this plot's bounds (axis-aligned box test, cheap). */
	UFUNCTION(BlueprintCallable, Category = "Plot")
	bool IsLocationInside(const FVector& WorldLocation) const;

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	bool bShowDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	FColor DebugColor = FColor::Cyan;

protected:

	virtual void Tick(float DeltaTime) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoundsVolume;
};
