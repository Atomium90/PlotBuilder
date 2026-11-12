#pragma once

#include "CoreMinimal.h"
#include "BuildAnchor.generated.h"

/**
 *  How a piece is positioned in Build Mode. Legacy keeps the original flush-against-bounds snap
 *  (UBuildModeComponent::TryFindSnapTransform) untouched - existing Props and any piece not yet
 *  migrated to the grid. The other three snap to an integer grid cell instead: Cell for pieces that fill a whole floor
 *  cell (foundation, floor slab), Edge for pieces centered on a cell edge (walls, partitions),
 *  Vertex for pieces at a grid corner (posts). Also doubles as the occupancy slot type in
 *  UStructuralSupportComponent - Legacy pieces never occupy a slot.
 *
 *  Ceiling is placed like a Cell piece (centred on a room cell) but has a slot table of its own: a lamp
 *  hangs under the slab above its cell, and the cell's own slot is already taken by the floor slab.
 *  Appended last so serialized values in existing Blueprints stay valid.
 */
UENUM(BlueprintType)
enum class EBuildAnchor : uint8
{
	Legacy,
	Cell,
	Edge,
	Vertex,
	Ceiling,
};
