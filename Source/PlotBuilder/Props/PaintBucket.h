#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaintBucket.generated.h"

/**
 *  A bucket of paint, as an inventory item: never an actor in the world. Each colour is a Blueprint subclass that sets
 *  PaletteIndex (BP_PaintBucket_Red...), so the shop, the hotbar and the home snapshot treat a bucket like any other item
 *  (one class per item, one shop row per class) and the inventory UI tells the colours apart through the class, with no
 *  special case anywhere. The colour itself is not stored here but in the plot's UPaintPalette, so retuning the palette
 *  retunes every bucket and every painted house.
 *
 *  The brush (slot 5 of the tool belt, see UPaintBrushComponent) paints with the bucket selected in the hotbar. A bucket
 *  is not used up for now: it is an unlock of that colour. Making it finite later happens in one place, where the brush
 *  applies the paint, and the item stays the same. A shop row for a bucket should use StackSize 1.
 */
UCLASS(Blueprintable, NotPlaceable)
class APaintBucket : public AActor
{
	GENERATED_BODY()

public:

	/** Entry of the plot's UPaintPalette this bucket holds. Entry 0 is the neutral one: a bucket on 0 takes paint off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Paint", meta = (ClampMin = "0"))
	int32 PaletteIndex = 1;
};
