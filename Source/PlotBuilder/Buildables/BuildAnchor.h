#pragma once

#include "CoreMinimal.h"
#include "BuildAnchor.generated.h"

/**
 *  How a piece is positioned in Build Mode. Legacy keeps the week 1 flush-against-bounds snap
 *  (UBuildModeComponent::TryFindSnapTransform) untouched - existing Props and any piece not yet
 *  migrated to the grid. The other three snap to an integer grid cell instead (see claude/Kit
 *  greybox specs des pieces et plan de remplacement.md): Cell for pieces that fill a whole floor
 *  cell (foundation, floor slab), Edge for pieces centered on a cell edge (walls, partitions),
 *  Vertex for pieces at a grid corner (posts). Also doubles as the occupancy slot type in
 *  UStructuralSupportComponent - Legacy pieces never occupy a slot.
 */
UENUM(BlueprintType)
enum class EBuildAnchor : uint8
{
	Legacy,
	Cell,
	Edge,
	Vertex,
};
