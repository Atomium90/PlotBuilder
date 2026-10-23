# PlotBuilder — Technical Documentation (Weeks 1–2)

Technical documentation deliverable for CDC_S1_Architecture_Gameplay_Physics_Unreal (Week 1) and the Week 2 construction work (grid-based building, structural portance, inventory, shop). Covers the overall architecture, the main classes and components, how the interaction system works, the physics and construction systems, the structural portance graph, the inventory and shop, the UI layer, the Chaos setup, the debugging tools, and the main technical choices made.

## Architecture

`APlayerCharacter` carries independent components, each with a single responsibility, plus a `UBuildModeComponent` layered on top of two of them. `APlot` bounds the buildable area and owns the structural portance graph.

| Class | Role |
|---|---|
| `APlayerCharacter` | Movement, camera, Enhanced Input bindings |
| `UInteractionComponent` | Traces from the view, tracks the focused actor, relays to `IInteractable` |
| `IInteractable` | Generic contract (`OnFocusBegin`/`OnFocusEnd`/`Interact`) - implementable in C++ or Blueprint |
| `UPhysicsManipulationComponent` | Push / Pull / Launch on the current focus, acting directly on its physics body |
| `UHotbarComponent` | Stacking grid inventory (36 slots: a 9-slot hotbar row plus a backpack), the item "on the cursor", money, shop purchase, acquisition (Interact/Purchase), instant placement for `APhysicsProp`, and the shared selection state for both Prop and Buildable flows |
| `UBuildModeComponent` | Placement flow for `ABuildablePiece`: ghost preview, rotation, obstruction and stability checks, through one of two placement paths (flush-snap or grid) |
| `ABuildablePiece` | Construction piece: mesh, `Cost`, build anchor, declared sockets, load-bearing flag; never simulates physics |
| `APhysicsProp` | Simulated physics object (crate, furniture...), manipulated via Push/Pull/Launch |
| `APlot` | Buildable area bounds and ground reference; gates where `UBuildModeComponent` allows placement; owns the structural graph |
| `UStructuralSupportComponent` | The portance graph and the slot-occupation table of a plot: every placed piece registers into it, stability is propagated from anchors, unstable pieces collapse |
| `DA_StructuralRules` | Data asset with the grid and portance tunables (cell size, floor height, roof step height, losses, thresholds) |
| `ADetectionZone` | Overlap-based trigger, reacts to any `IInteractable` actor entering/leaving |

No object-specific logic lives in `APlayerCharacter` - it only knows about its own components, which in turn only know about `IInteractable` and raw physics bodies, never about a specific object class.

## Interaction system

`UInteractionComponent` traces every tick from the controller's view point on a dedicated collision channel (`GameTraceChannel2`, named "Interactable" in the collision profile settings). If the trace hits an actor implementing `IInteractable`, it becomes the current focus (`OnFocusBegin`/`OnFocusEnd` fire on change) and `TryInteract()` calls `Interact()` on it. Both `ABuildablePiece` and `APhysicsProp` implement `Interact` to store themselves into the instigator's `UHotbarComponent` - the component itself has no idea what kind of object it just picked up. Door leaves also implement the interface, to open and close.

## Inventory and shop

### Inventory

`UHotbarComponent` holds the inventory as a flat array of `FInventorySlot` (`ActorClass`, `Scale`, `Count`, `StackSize`). The grid is `NumColumns` x `NumRows` (9 x 4): the slot at `Row * NumColumns + Column`, row 0 being the hotbar and the rows below the backpack. A separate cursor slot holds the stack currently "in hand".

Interaction follows the Minecraft model, implemented once in `OnSlotLeftClicked(SlotIndex)` and `OnSlotRightClicked(SlotIndex)`: a left click picks up, places, merges identical stacks or swaps different ones; a right click places a single item. The selection (`GetSelectedIndex`, `CycleSelection`) is bound to the 9 hotbar slots.

The component exposes read access (`GetSlots`, `GetCursorSlot`, `GetSelectedIndex`, `GetMoney`, `IsInfiniteMoney`) and four events without parameters (`OnInventoryChanged`, `OnSelectedSlotChanged`, `OnCursorSlotChanged`, `OnMoneyChanged`). The UI never polls: it re-reads the state through the getters whenever one of these events fires.

### Shop catalog

`UHotbarComponent::ShopCatalog` is a `DataTable` of `FShopItemRow` - one row per purchasable item, Buildable or Prop alike: `DisplayName`, `ActorClass`, `Cost`, a `Category` (the `EShopItemCategory` enum, used for the shop tabs and the colour of an item's icon), and a stack size. `PurchaseRow` checks the player's money, debits it and adds the item to the inventory with no world actor involved, as opposed to `Interact` picking up something already physically placed in the world. Money lives on the component, with an infinite-money mode for testing. Adding a purchasable item is a new DataTable row, not new code.

## Physics: Push / Pull / Launch

`UPhysicsManipulationComponent` reads the sibling `UInteractionComponent`'s current focus (no separate trace) and acts on its physics body directly:
- **Grab** picks up the focus if it has a simulated body and nothing is already held.
- **Push** applies an outward impulse to the current focus.
- **Launch** releases whatever is held with a forward impulse.
- While held, a spring pulls the object towards a point in front of the view, damped against its current velocity to stop oscillation. The spring targets the body's **center of mass**, not its component origin - targeting the origin caused off-center meshes to spin continuously, since the pivot-to-center-of-mass offset rotates with the object and fed rotational energy back into the spring.

## Construction: Build Mode

Placing a `APhysicsProp` is instant (`UHotbarComponent::TryPlace`, grid-snapped). Placing a `ABuildablePiece` goes through a separate, toggleable **Build Mode** (`UBuildModeComponent`), since blind instant placement was tested and found unusable. A single reusable ghost actor (mesh/material swapped in place, never respawned) previews the selected piece. It is tinted green when placement is comfortably stable, yellow when it would fall under the comfort threshold (fragile - it may or may not actually collapse, see "Unstable placement" below), and red when blocked.

Each `ABuildablePiece` declares a build anchor (`EBuildAnchor`: `Legacy`, `Cell`, `Edge`, `Vertex`) that selects one of two placement paths. `Legacy` is the default, so any piece not migrated keeps the Week 1 behaviour unchanged.

### Legacy path (Week 1): flush snap

- One trace per tick decides the ghost's transform: hits a placed `ABuildablePiece` → snaps flush against it (face picked from where the hit lands within the target's own local bounding box, not just the raw impact normal, so aiming near an edge is enough); hits anything else solid → rests flush there instead (ground, level geometry, a Prop); hits nothing → falls back to a fixed distance in front of the view.
- Rotation (45° steps) is manual (two keys) and stacks on top of whatever base rotation the snap implies.
- One box overlap per tick (world statics, dynamics, pawns) at the candidate transform decides the blocked state - it ignores the ghost, the piece being snapped onto, and the player, and shrinks the test box by a small tolerance so two flush, touching pieces don't false-positive from floating-point error.

### Grid path (Week 2)

The flush snap places a piece against another piece's bounding box, which cannot put a wall on a foundation's edge or a post on a vertex. The grid path places a piece on an integer grid instead, so that its sockets land exactly on its neighbours' sockets.

- **Units.** `CellSize` is 100 cm, one grid unit `u`; a floor cell is 2 `u` wide. Cell centres are at (even, even), edge midpoints at (odd, even) or (even, odd), vertices at (odd, odd). `FloorHeight` is 300 cm.
- **Anchors.** `Cell`: foundation, floor slab, roof tiles. `Edge`: walls, interior walls, openings, gables. `Vertex`: corner posts.
- **Where the ghost goes.** The view ray is intersected with a horizontal plane at the build level's height (the level is changed with two input actions), and the hit point is snapped to the nearest cell centre, edge midpoint or vertex of the anchor type. The pivot is what gets placed, never the centre of the bounds; the sockets are expressed relative to the pivot.
- **Heights.** The top of the level-0 foundation is at `GroundZ + 20`; a piece at level `k` has its pivot at that height plus `k x 300`. Roof pieces additionally carry a `VerticalOffset` (-20 cm) because the real top of a wall is 20 cm below the logical level, which includes the floor slab. `GroundZ` is the plot actor's own Z.
- **Wall side.** Exterior walls have their pivot on one face, so a wall sits entirely on one side of its edge instead of straddling it: it never overhangs the foundation, and two walls meeting at a corner overlap inside the footprint without needing a post. A `bFaceAnchoredEdge` flag selects this behaviour per piece; the side is the one that has floor under it, or the aimed side if both do. Interior walls stay centred on their edge.
- **Integer bookkeeping.** A placed piece stores its grid coordinate (cell and level) and its orientation in quarter turns. Everything downstream - occupancy, socket matching, stability - compares integers, never floats.
- **Occupancy.** `UStructuralSupportComponent` keeps a table of occupied slots, keyed by slot type (floor, edge, vertex), cell and level. One slot holds one piece. This replaces the physical overlap test between pieces, which would otherwise reject two walls overlapping at a corner.
- **Blocked or not.** The ghost is red when the position is outside the plot, the slot is occupied, or something other than a piece (the player, a prop, the environment) is in the way. The overlap box is cut at `GroundZ` so the buried part of a foundation never counts as an obstruction.
- **Risky placement.** A candidate under the comfort threshold is allowed, not blocked (yellow) - two different thresholds are at play: the ghost turns yellow as soon as stability drops under `ComfortThreshold` (0.5), but the piece only actually collapses if it's also under the stricter `CollapseThreshold` (0.1). A single horizontal link from an anchor, for instance, lands at exactly 0.4 - yellow, but stable, since 0.4 is still above 0.1. A candidate that does end up under 0.1 is placed, then collapses right after, exactly like a removal-triggered collapse.

## Structural portance

### The piece kit

Pieces are greybox meshes on a 200 cm cell, one mesh per piece; a Blueprint per piece assigns the mesh and sets the data (anchor, sockets, load-bearing flag, cost). Walls are 280 cm high, the floor slab is 20 cm, so one storey is 300 cm.

| Group | Pieces |
|---|---|
| Structure | foundation (also the ground-floor floor), floor slab, plain wall, wall with door opening, wall with window opening, corner post, door leaf (a separate Blueprint, not a placeable piece), stairs |
| Interior | interior wall (10 cm), interior wall with door opening |
| Roof (one slope, 1:2 = 26.6°) | slope, ridge, flat top tile, hip corner, valley corner, half gable, gable infill |

A door leaf is a Child Actor of the door-wall Blueprints, driven by a Timeline. Stairs are supported from below and, like other structural pieces, are load-bearing: a floor landing at the top of a flight can rest on them. Their collision is the mesh itself.

### Sockets

A piece does not store its connections on its mesh. Its Blueprint declares an array of `FStructuralSocket` (a type, a `CellOffset` relative to the piece's pivot in grid units, and a `RoofStep`), so swapping a mesh never touches the portance logic. The graph does not read the mesh at all.

| Socket | Carried by | Connects to |
|---|---|---|
| `Anchor` | foundation | the terrain; stability 1.0 |
| `FloorEdge` | foundation, floor slab (4 edge midpoints) | `WallBottom`; a neighbouring `FloorEdge` |
| `FloorCorner` | foundation, floor slab (4 corners) | the `WallBottom` of a post only |
| `WallBottom` / `WallTop` / `WallSide` | walls, post | the floor below / the floor or roof above / the neighbouring wall |
| `FloorBottom` | floor slab (4 edges and 4 corners) | `WallTop` |
| `RoofLow` / `RoofHigh` / `RoofSide` | roof tiles | the wall below or the previous tile / the next tile / a lateral neighbour |

`FloorCorner` is a separate type so that two slabs touching only at a corner are not considered joined. Door slots and the door leaf carry no structural socket.

Socket positions are turned into world-independent integer coordinates by one shared function (the piece's grid coordinate plus its offset rotated by its orientation), used by Build Mode, the debug view and the graph, so that a mismatch is visible before it is misinterpreted by the portance logic.

### Roof step

Roof pieces rise in 100 cm steps, which is not a divisor of the 300 cm storey. Sockets therefore carry a second, independent vertical axis, `RoofStep`, and so do placed pieces; two sockets only match if their absolute `RoofStep` values are strictly equal. It serves two cases: a tile climbing a slope (`RoofLow` and `RoofHigh` at different steps on the same piece) and a gable infill stacking on another one. It is 0 on every non-roof socket, so it never changes what a floor level means for any other piece. The height of one step is a tunable (`RoofStepHeight`, 100 cm).

### Propagation

Each placed piece is a node and each pair of matching sockets a link. Stability is a value from 0 to 1 that starts at 1.0 on `Anchor` and propagates outwards: a piece takes the best value among its neighbours minus a loss that depends on the link's class (vertical, horizontal, or roof). Below a threshold, or when no chain of links leads back to an anchor, the piece collapses; its removal can in turn cut other pieces off, which is how a cascade happens. The losses and thresholds live in `DA_StructuralRules`, so they are tuned without recompiling.

Load-bearing is a flag on the piece, not a mesh: a load-bearing wall transmits stability to what rests on it (floor, roof), while a partition receives its stability from the floor it stands on and transmits nothing upward. Walls with openings and roof pieces are load-bearing, interior walls are not. Roof pieces being load-bearing is what lets stability climb tile by tile up a slope (`RoofHigh` to the next tile's `RoofLow`) - a roof is stable only while an unbroken chain of links leads from a tile's `RoofLow` back to the `WallTop` of a load-bearing wall.

Pair classes that are not obvious: `WallTop` to `RoofLow` is vertical; the roof-to-roof pairs (`RoofLow`/`RoofLow`, `RoofHigh`/`RoofHigh`, `RoofHigh`/`RoofLow`, `RoofSide`/`RoofSide`) are horizontal, so a roof held only by other roof tiles, never reaching a load-bearing wall, is not stable.

The graph is recomputed on every placement and removal. The same evaluation (with the candidate piece's sockets, in integer coordinates) answers `WouldBeStable`, which drives the ghost's yellow state.

## Chaos destruction

One wall (`GC_wall`) is set up as a hierarchical Geometry Collection: a single planar cut splits it into a bottom slab and a top section, then only the top section is fractured further (5-piece Voronoi) - a two-level cluster, not a flat single-pass fracture. Per-level Damage Threshold is tuned so a Launch-strength hit detaches the top from the bottom, which then shatters into its 5 fragments on landing rather than immediately (the impact with the ground supplies the second dose of strain needed). The bottom is marked Anchored so it should stay fixed to the ground; it does not currently survive the root cluster breaking, so the bottom piece falls too. This is intentionally kept as an introductory Chaos experiment per the CDC ("this experiment may remain simple... before studying them in greater depth the following week") - getting the Anchor to persist through a break is exactly the kind of thing Week 2's deeper Chaos work will address.

## User interface

A temporary UMG layer, built to be restyled later. A `WBP_HUD` is created by the player controller when it possesses the pawn and stays on screen; it holds the hotbar, the money, the help overlay and the menus, and is the only place that changes the input mode.

| Widget | Role |
|---|---|
| `WBP_HUD` | Permanent container; opens and closes one menu at a time, shows the cursor, ignores move and look input while a menu is open, moves the item stack with the mouse |
| `WBP_Slot` | One inventory cell; used by the hotbar and the inventory; shows a category-coloured square, the name and the count; forwards clicks to the component |
| `WBP_Hotbar` | The 9 bottom slots |
| `WBP_Inventory` | The backpack and the hotbar row; opened with `I` |
| `WBP_CursorStack` | The stack carried by the cursor |
| `WBP_Shop`, `WBP_ShopCard` | The catalogue: tabs per category, one card per `DT_Shop` row, click to buy 1, Shift-click for 10; opened with `B` |
| `WBP_ControlsHelp` | Controls overlay |
| `BFL_UIStyle` | Function library holding every colour and size, so a restyle is made in one place |

Widgets hold no inventory state of their own. They bind to the component's events and re-read its getters, so nothing runs on tick (the only exception is the cursor stack following the mouse).

## Debugging tools

- `UInteractionComponent`: trace line (`bShowDebugTrace`) and/or the focused actor's name on screen (`bShowDebugFocusName`) - split so either can be toggled independently.
- `UPhysicsManipulationComponent`: force vector applied to the held/pushed object.
- `UBuildModeComponent`: currently previewed class and rotation on screen; the ghost's green/yellow/red state; in the grid path, the snapped integer coordinates and the reason a placement is refused.
- `BP_SocketDebugger`: draws every declared socket of every piece as a coloured sphere (one colour and size per socket type), at its logical position. Two compatible sockets at the same point are drawn as concentric spheres; a lone sphere where a partner is expected means a wrong offset or a mispositioned piece.
- `APlot`: bounds drawn as a debug box every tick (kept on by default - it's meant to be seen during play, not just for debugging).

All debug flags default to `false` except `APlot`'s bounds outline; enable the ones you want per test.

## Key technical choices

- **Component separation over one big Blueprint**: `UInteractionComponent` (generic detection) and `UPhysicsManipulationComponent` (physics manipulation) are deliberately two components, not one - detection and manipulation are different responsibilities, and neither needs to know how the other works.
- **C++ for the skeleton, Blueprint for variants**: base classes, interfaces, and components are C++; each buildable piece or prop is a Blueprint subclass assigning a mesh and tuning values, never new code.
- **Two snapping strategies, chosen per piece**: the Week 1 flush snap, computed from each mesh's own bounding box (no per-piece authoring), stays for pieces that do not need a grid; the grid path is used for the construction kit, where pieces must meet on exact sockets. A piece opts in through its build anchor, so nothing built for Week 1 had to change.
- **Sockets are data in the Blueprint, not sockets on the mesh**: a mesh socket follows the mesh, so replacing a greybox mesh by a final one would break the graph. Keeping them in the piece's data, relative to its pivot, makes a mesh swap a reference change.
- **Integer coordinates everywhere in the portance logic**: a single conversion from a piece to integer socket coordinates, shared by Build Mode, the debug view and the graph, instead of comparing floating-point positions.
- **Portance as a propagation graph, not a physics simulation**: stability is cheap, predictable, readable on screen (the ghost's colour) and easy to explain, at the cost of not simulating real loads.
- **A second vertical axis for roofs rather than a finer floor grid**: roofs rise in 100 cm steps while storeys are 300 cm; `RoofStep` keeps `FloorHeight` meaning one thing for every other piece.
- **Greybox first, assets later**: every piece is a simple, thick, closed mesh with its pivot and thickness fixed by the design, so the logic does not depend on final art and replacing meshes is a data change.
- **The player doesn't block its own construction**: the obstruction check deliberately ignores the player's own capsule. An earlier version blocked on it too, which made it near-impossible to build next to yourself - no FPS building game self-blocks for this reason either.
- **UI driven by events, not tick**: the component broadcasts changes and widgets re-read state, so the UI cannot drift from the inventory.

## Known limitations

- Chaos: the Anchored bottom piece of `GC_wall` doesn't survive the root cluster breaking (see above) - an introductory experiment per the CDC's own framing ("may remain simple"), to be revisited once Chaos is covered in depth.
- Roofs: one slope only, rectilinear footprints only, no overhang, and an odd-width roof ends in a flat top tile rather than a continuous slope. The ridge is a decorative piece placed by hand; there is no automatic roof generation yet, and no rafters (roof span is limited only by the portance rules).
- Grid placement ignores the real thickness of neighbouring pieces: occupancy is checked per slot, not per volume. A stair or a floor slab placed at the edge of a house overlaps the 20 cm of the exterior wall beside it (the wall's pivot is on one face, so it lies inside the neighbouring cell). Nothing is visible mid-house, where no wall is adjacent. Candidate fixes: shorten the meshes on the wall side, or offset the piece by the wall thickness when the neighbouring edge holds a wall.
- Wall, floor, wall: from outside, the floor slab edge shows between the two storeys. A storey is 300 cm and a wall 280 cm, so a 20 cm band of slab is left uncovered. Candidate fix: extend the exterior walls 20 cm downward to cover the slab edge (purely visual, no impact on portance).
- `APlot` is still a single bounds volume - no ownership, no cost tracking, no buy/sell (the long-term vision for that is out of scope for now).
- The UI is a temporary layer: items are shown as category-coloured squares with their name rather than icons, and there are no animations.
