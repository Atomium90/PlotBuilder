#pragma once

#include "CoreMinimal.h"
#include "StructuralSocket.generated.h"

/**
 *  What a structural socket represents, and how it pairs up for the support graph (see the
 *  "Sockets" table in claude/Kit greybox specs des pieces et plan de remplacement.md, which this
 *  mirrors). Roof_* pairings aren't detailed in that doc's loss-class table - classified in
 *  UStructuralSupportComponent as horizontal: roof-to-roof contact alone never counts as support,
 *  it still needs to trace down to a wall. Anchor isn't paired with anything - a piece that has
 *  one is its own support (foundation, or anything resting directly on the Plot's ground).
 */
UENUM(BlueprintType)
enum class EStructuralSocketType : uint8
{
	Anchor,
	FloorEdge,
	/** A floor/foundation's top-face corner (diagonal from its center, not along an edge) - only pairs with WallBottom, never with another FloorCorner or FloorEdge, so two floor pieces that merely touch at a shared corner don't count as linked. Gives a corner post (sitting at a grid vertex) something to rest on when that vertex happens to be a floor corner rather than the midpoint of an edge. */
	FloorCorner,
	WallBottom,
	WallTop,
	WallSide,
	FloorBottom,
	RoofLow,
	RoofHigh,
	RoofSide,
};

/**
 *  One structural attachment point on a piece, declared as data rather than authored on the mesh
 *  itself - swapping a mesh for a final asset never touches this. Position alone (no separate
 *  direction) is enough to disambiguate every socket on the current piece kit.
 */
USTRUCT(BlueprintType)
struct FStructuralSocket
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	EStructuralSocketType Type = EStructuralSocketType::WallBottom;

	/** Position of this socket, in grid cells, relative to the piece's own pivot - rotated by the piece's own yaw before being resolved to a world cell. */
	UPROPERTY(EditDefaultsOnly, Category = "Structural")
	FIntVector CellOffset = FIntVector::ZeroValue;
};
