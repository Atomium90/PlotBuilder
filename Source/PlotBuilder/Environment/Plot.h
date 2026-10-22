#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Plot.generated.h"

class UBoxComponent;
class UStructuralSupportComponent;

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

	UFUNCTION(BlueprintPure, Category = "Plot")
	UStructuralSupportComponent* GetStructuralSupport() const { return StructuralSupport; }

	/** Whether WorldLocation falls within this plot's bounds (axis-aligned box test, inclusive with a small tolerance so a vertex/edge exactly on the boundary still counts). */
	UFUNCTION(BlueprintCallable, Category = "Plot")
	bool IsLocationInside(const FVector& WorldLocation) const;

	/** World point a grid-anchored piece's (0,0) cell resolves to - the plot's own pivot, which authoring convention places at the center of a cell (see claude/Kit greybox specs des pieces et plan de remplacement.md). */
	UFUNCTION(BlueprintPure, Category = "Plot")
	FVector GetGridOrigin() const { return GetActorLocation(); }

	/** The plot's ground surface Z - the plot's own pivot, by the same authoring convention (checked on BeginPlay, see the warning logged there if it doesn't hold). Assumes a flat plot. */
	UFUNCTION(BlueprintPure, Category = "Plot")
	float GetGroundZ() const { return GetActorLocation().Z; }

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	bool bShowDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Plot|Debug")
	FColor DebugColor = FColor::Cyan;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoundsVolume;

	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStructuralSupportComponent> StructuralSupport;
};
