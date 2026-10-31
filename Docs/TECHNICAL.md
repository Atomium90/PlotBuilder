# PlotBuilder — Technical Documentation

Technical documentation for PlotBuilder, companion to the GDD (the design source of truth): the construction and structural-portance systems (grid-based building, inventory, shop) and the demolition systems built on top of them (durability and materials, the four demolition tools, damage-coupled stability, the staggered collapse queue and fracture-on-destroy). Covers the overall architecture, the main classes and components, how the interaction system works, the physics and construction systems, demolition, the structural portance graph, the inventory and shop, the UI layer, the Chaos setup, the debugging tools, and the main technical choices made.

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
| `UStructuralSupportComponent` | The portance graph and the slot-occupation table of a plot: every placed piece registers into it, stability is propagated from anchors, unstable pieces collapse, damaged pieces transmit less of it, and a lost piece enters a staggered collapse queue instead of cascading instantly |
| `DA_StructuralRules` | Data asset with the grid and portance tunables (cell size, floor height, roof step height, losses, thresholds) |
| `ADetectionZone` | Overlap-based trigger, reacts to any `IInteractable` actor entering/leaving |
| `IDamageable` | Generic contract (`ApplyDamage(FDamageInfo)`) for anything a demolition tool can damage - tools never see a concrete class |
| `UDurabilityComponent` | HP, material, damage state (Intact/Damaged/Critical/Destroyed) for one `IDamageable` owner; also forwards the stability-coupling and collapse-queue constants it reads from `DA_DemolitionRules`, so `UStructuralSupportComponent` never needs to know that asset exists |
| `UToolbeltComponent` | Owned/equipped demolition tool slot (Hands + the four tools), separate from `UHotbarComponent`'s construction inventory |
| `ADemolitionTool` | Shared base for the three melee tools (sledgehammer, crowbar, jackhammer); `BP_Sledgehammer`/`BP_Crowbar`/`BP_Jackhammer` differ only by their `UDemolitionToolData` |
| `AExplosiveCharge` | The first tool: thrown, manually detonated, flat radial damage and impulse, no falloff |
| `AFragmentDebris` | Generic greybox shard spawned on fracture-on-destroy; short-lived, tagged `"Fragment"` for a future debris subsystem |
| `DA_DemolitionRules` | Data asset for every demolition tunable: HP per material/class, damage-state thresholds, the tool x material matrix, the stability-coupling curve, collapse-queue timing, fragment counts/impulse |

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

## Demolition

### Durability and materials

Anything a tool can damage implements `IDamageable`, a single-function contract (`ApplyDamage(FDamageInfo)`) - a tool resolves a hit, builds an `FDamageInfo` (amount, tool id, hit location, impulse) and calls it on whatever it traced, without ever knowing if that's a wall, a post or (later) a piece of furniture. `ABuildablePiece::ApplyDamage_Implementation` just forwards to its own `UDurabilityComponent`.

`UDurabilityComponent` owns the HP, the material (`EDemolitionMaterial`: Wood, Plaster, Brick, Concrete, Glass, Metal) and the damage state (`EDurabilityState`: Intact, Damaged, Critical, Destroyed), and knows nothing about its owner. `InitializeFromRules` computes `MaxHP` once from `DA_DemolitionRules` (`MaterialBaseHP[Material] x PieceClassFactor[PieceClass]`, the piece's role - foundation, exterior wall, stairs... - factoring HP independently of its material); `ApplyDamage` subtracts, recomputes the state against the rules' `DamagedThreshold`/`CriticalThreshold`, and broadcasts `OnDurabilityDamaged`/`OnDurabilityStateChanged`/`OnDurabilityDestroyed`. `ABuildablePiece` listens to all three purely to react (damage tint, fracture-on-destroy) - the component itself never touches the mesh or the structural graph.

The damage tint is a `Damage` scalar (`1 - GetHPRatio()`) written to the mesh's Custom Primitive Data (index 1), read by the piece's material - the same per-instance-data technique `UStructuralSupportComponent`'s stability heat-map uses (index 0), so neither needs its own material permutation per piece.

The tool x material matrix (`ToolMaterialMatrix` on `DA_DemolitionRules`, one `FToolMaterialMultipliers` row per `EDemolitionTool`) answers "how much does this tool's base damage actually do to this material" - `ComputeDamage(Tool, Material, BaseDamage)` is the one place that logic lives, called by every tool so a new tool or a re-tuned matchup is a data change, not new code.

### Toolbelt and tools

`UToolbeltComponent` is deliberately separate from `UHotbarComponent`: construction pieces and props are stock the player owns, demolition tools are equipped gear. Slot 0 is Hands (routes straight to the sibling `UPhysicsManipulationComponent`: primary Grabs, secondary Throws - a narrower pair than that component's own legacy Push/Pull/Launch, which stays reachable in `Lvl_Home` through `APlayerCharacter`'s locked-toolbelt gate). Slot 1 (Charge) spawns and throws an `AExplosiveCharge` on secondary use, then detonates it on the next press. Slots 2-4 are `ADemolitionTool` instances (Sledgehammer/Crowbar/Jackhammer), spawned once in `BeginPlay` and kept for the owner's lifetime, unlike the consumable charge.

`ADemolitionTool` is one C++ class for all three melee tools - `BP_Sledgehammer`/`BP_Crowbar`/`BP_Jackhammer` differ only by their `UDemolitionToolData` (base damage, rate, range, hit radius, wind-up, `bContinuous`, camera shake class) and their held mesh/socket offset. `PrimaryUse` traces from the owner's view, resolves `IDamageable` targets (the traced actor alone, or everything in a radius around the impact point if `HitRadius > 0`, mirroring the charge's own overlap loop), and gates re-use behind one busy window covering wind-up plus the rest of the `1/Rate` cycle. A continuous tool (the jackhammer) instead starts a looping timer at `1/Rate` on press and stops it on `ReleasePrimaryUse` (LMB up) - no separate base class needed, `bContinuous` branches inside the same `PrimaryUse`.

`AExplosiveCharge` is a physics actor (same mesh/collision setup as `APhysicsProp`): thrown with an initial velocity, settles under gravity, and detonates on a second press (`RequestDetonation`, after `DetonationDelay` for the beep/warning). `Detonate` applies flat damage (no falloff) to every `IDamageable` in `Radius` through the same `ComputeDamage` matrix lookup as the melee tools, plus a flat radial impulse to nearby simulating bodies. A translucent sphere scaled to `Radius` previews the blast area for the charge's whole armed lifetime, doubling as the gameplay indicator and the visual debug tool.

Hit feedback (camera shake + a per-material impact sound, both data-driven) and fracture are shared, not per-tool: every tool ends in the same `IDamageable::ApplyDamage` call, so `UDurabilityComponent` is the single place a hit becomes a state change regardless of which of the four tools caused it.

## Construction: Build Mode

Placing a `APhysicsProp` is instant (`UHotbarComponent::TryPlace`, grid-snapped). Placing a `ABuildablePiece` goes through a separate, toggleable **Build Mode** (`UBuildModeComponent`), since blind instant placement was tested and found unusable. A single reusable ghost actor (mesh/material swapped in place, never respawned) previews the selected piece. It is tinted green when placement is comfortably stable, yellow when it would fall under the comfort threshold (fragile - it may or may not actually collapse, see "Unstable placement" below), and red when blocked.

Each `ABuildablePiece` declares a build anchor (`EBuildAnchor`: `Legacy`, `Cell`, `Edge`, `Vertex`) that selects one of two placement paths. `Legacy` is the default, so any piece not migrated keeps the flush-snap behaviour unchanged.

### Legacy path: flush snap

- One trace per tick decides the ghost's transform: hits a placed `ABuildablePiece` → snaps flush against it (face picked from where the hit lands within the target's own local bounding box, not just the raw impact normal, so aiming near an edge is enough); hits anything else solid → rests flush there instead (ground, level geometry, a Prop); hits nothing → falls back to a fixed distance in front of the view.
- Rotation (45° steps) is manual (two keys) and stacks on top of whatever base rotation the snap implies.
- One box overlap per tick (world statics, dynamics, pawns) at the candidate transform decides the blocked state - it ignores the ghost, the piece being snapped onto, and the player, and shrinks the test box by a small tolerance so two flush, touching pieces don't false-positive from floating-point error.

### Grid path

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

The graph is recomputed on every placement and removal, but no longer inline: `RegisterPiece`/`UnregisterPiece` just mark the graph dirty (`MarkDirty`), and `TickComponent` does the actual relaxation at most once per frame. A cascade that unregisters N pieces in the same frame (collapse releases, see below) recomputes once, not N times. The same evaluation (with the candidate piece's sockets, in integer coordinates) answers `WouldBeStable`, which drives the ghost's yellow state.

### Damage coupling (`c(h)`)

A piece's own damage now affects what it transmits upward, not just whether it collapses. `ComputeStability`/`RecomputeStability` multiply a neighbour's stability by `c(h)` before subtracting the link's loss - `c(h)` is `UDemolitionRules::ComputeStabilityCoupling(HPRatio)`, a curve from `DA_DemolitionRules::StabilityTransmission` (falls back to a hardcoded piecewise-linear approximation through `(0, 0.3)`, `(0.35, 0.6)`, `(0.75, 1.0)` if unassigned). `UStructuralSupportComponent` reads it through `UDurabilityComponent::GetStabilityCoupling()` and never references `DA_DemolitionRules` directly - the graph doesn't know damage internals exist, only that a piece can report a coupling factor.

### Collapse queue and fracture

A piece no longer collapses the instant it's found unstable. `UnregisterPiece` records the grid cells it vacates (`RecentlyVacatedCells` - cells, not the piece itself, since it may already be mid-`Destroy()`). `RecomputeStability` runs a BFS (`ComputeHopDistances`) from those cells over the same socket-adjacency graph the relaxation walks, but over any link, not just load-bearing ones - hop distance is about physical distance, not who transmits stability to whom. Every newly-unstable piece found this pass gets a release time, `t = Now + BaseDelay + compressed(hops x HopDelay + jitter)`, where the compression scales down a big cascade's hop spread to fit `MaxCascadeSpread` without touching `BaseDelay`, a fixed reaction time shared by the whole batch. `TickComponent` drains this queue every tick, independent of the dirty-flag recompute, calling `BeginCollapse()` on whatever piece's release time has come up.

`BeginCollapse` is what actually detaches a piece: it unregisters from the graph (which can in turn destabilise whatever rested on it, picked up by the *next* recompute, so a cascade naturally continues wave by wave), starts simulating physics, and gives itself a horizontal impulse away from the centroid of its still-registered neighbours (`UStructuralSupportComponent::ComputeAwayFromNeighbourCentroid`, strength from `DA_DemolitionRules::CollapseImpulseStrength`) so a falling wall leans/slides outward instead of dropping straight down. It fractures on its first significant impact (`OnComponentHit` above `MinFractureImpactSpeed`) or, failing that, after a timeout (`CollapseLifeSpan`) - either way it is replaced by fragments, never just destroyed outright.

`ABuildablePiece::Fracture` is the one fracture entry point, used both by a direct tool-kill (fractures in place, no fall) and by a cascade release (fractures after falling). It spawns a generic, procedural fragment set per `EPieceClass` (3-6 `AFragmentDebris`, GDD §7.8's v1 - not an authored per-mesh Geometry Collection, that's `UFractureLibrary`, not built yet), each with a random offset/rotation and an outward impulse; the same direction a piece fell away from (`CollapseBiasDirection`, zero for a direct kill) is blended into that outward impulse before scaling, so fragments scatter the way the piece fell, not independently of it. A fragment times out and scales to zero over `FragmentFadeDuration` rather than fading its material's opacity, avoiding a translucent material entirely.

## Chaos destruction

One wall (`GC_wall`) is set up as a hierarchical Geometry Collection: a single planar cut splits it into a bottom slab and a top section, then only the top section is fractured further (5-piece Voronoi) - a two-level cluster, not a flat single-pass fracture. Per-level Damage Threshold is tuned so a Launch-strength hit detaches the top from the bottom, which then shatters into its 5 fragments on landing rather than immediately (the impact with the ground supplies the second dose of strain needed). The bottom is marked Anchored so it should stay fixed to the ground; it does not currently survive the root cluster breaking, so the bottom piece falls too. This is intentionally kept as a simple introductory Chaos experiment - getting the Anchor to persist through a break is exactly the kind of thing a deeper Chaos pass will address later.

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

- `UInteractionComponent`: trace line (`bShowDebugTrace`) and/or the focused actor's name on screen (`bShowDebugFocusName`) - split so either can be toggled independently. `bShowDebugDurability` adds the focused actor's HP/material/state if it has a `UDurabilityComponent`.
- `UPhysicsManipulationComponent`: force vector applied to the held/pushed object.
- `UBuildModeComponent`: currently previewed class and rotation on screen; the ghost's green/yellow/red state; in the grid path, the snapped integer coordinates and the reason a placement is refused.
- `BP_SocketDebugger`: draws every declared socket of every piece as a coloured sphere (one colour and size per socket type), at its logical position. Two compatible sockets at the same point are drawn as concentric spheres; a lone sphere where a partner is expected means a wrong offset or a mispositioned piece.
- `APlot`: bounds drawn as a debug box every tick (kept on by default - it's meant to be seen during play, not just for debugging).
- `UStructuralSupportComponent`: a stability heat-map (`SetShowDebug`/`bShowDebug` - swaps every registered piece's mesh to a material reading Stability from Custom Primitive Data) and an on-screen collapse-queue readout (`SetShowCollapseQueueDebug`/`bShowCollapseQueueDebug` - piece name + seconds to release, for every pending release).
- `UToolbeltComponent`: equipped-slot name on screen (`bShowDebug`) - a stand-in until there's a real HUD indicator.
- `APlayerCharacter` console commands (`~` to open, type and Enter): `ListShop`/`Buy <index>` (the shop catalog); `DebugDamageFocus <amount>` (applies damage to the current focus if it's `IDamageable`, a stand-in for a tool); `DebugShowStability 0|1` and `DebugShowCollapseQueue 0|1` (the two toggles above, from anywhere without touching a Blueprint default).

All debug flags default to `false` except `APlot`'s bounds outline and `UToolbeltComponent::bShowDebug`; enable the ones you want per test.

## Key technical choices

- **Component separation over one big Blueprint**: `UInteractionComponent` (generic detection) and `UPhysicsManipulationComponent` (physics manipulation) are deliberately two components, not one - detection and manipulation are different responsibilities, and neither needs to know how the other works.
- **C++ for the skeleton, Blueprint for variants**: base classes, interfaces, and components are C++; each buildable piece or prop is a Blueprint subclass assigning a mesh and tuning values, never new code.
- **Two snapping strategies, chosen per piece**: the flush snap, computed from each mesh's own bounding box (no per-piece authoring), stays for pieces that do not need a grid; the grid path is used for the construction kit, where pieces must meet on exact sockets. A piece opts in through its build anchor, so nothing built around the flush snap had to change.
- **Sockets are data in the Blueprint, not sockets on the mesh**: a mesh socket follows the mesh, so replacing a greybox mesh by a final one would break the graph. Keeping them in the piece's data, relative to its pivot, makes a mesh swap a reference change.
- **Integer coordinates everywhere in the portance logic**: a single conversion from a piece to integer socket coordinates, shared by Build Mode, the debug view and the graph, instead of comparing floating-point positions.
- **Portance as a propagation graph, not a physics simulation**: stability is cheap, predictable, readable on screen (the ghost's colour) and easy to explain, at the cost of not simulating real loads.
- **A second vertical axis for roofs rather than a finer floor grid**: roofs rise in 100 cm steps while storeys are 300 cm; `RoofStep` keeps `FloorHeight` meaning one thing for every other piece.
- **Greybox first, assets later**: every piece is a simple, thick, closed mesh with its pivot and thickness fixed by the design, so the logic does not depend on final art and replacing meshes is a data change.
- **The player doesn't block its own construction**: the obstruction check deliberately ignores the player's own capsule. An earlier version blocked on it too, which made it near-impossible to build next to yourself - no FPS building game self-blocks for this reason either.
- **UI driven by events, not tick**: the component broadcasts changes and widgets re-read state, so the UI cannot drift from the inventory.
- **Tools never see a concrete class**: every demolition tool resolves a hit to `IDamageable` and stops there - a tool added later (or a breakable prop) needs no change to `ADemolitionTool`/`AExplosiveCharge`, only an `ApplyDamage_Implementation`.
- **The structural graph doesn't know damage exists, and damage doesn't know the graph exists**: `c(h)` lives on `DA_DemolitionRules`, read through a one-float forwarder (`UDurabilityComponent::GetStabilityCoupling`) - moving it onto `DA_StructuralRules` instead would have made the graph need to know about HP ratios, breaking that split for a value that is really "how damaged is this material", not a portance concept.
- **Generic per-class fragments over per-mesh fracture, for now**: GDD §7.8 describes per-mesh debris; v1 ships one procedural shard shape per `EPieceClass` instead, a deliberate simplification so every piece fractures without authoring a Geometry Collection per mesh first. Per-mesh fracture (`UFractureLibrary`) is not built yet; both versions tag their fragments `"Fragment"`, so whatever debris subsystem consumes them later doesn't care which version produced them.
- **Collapse is a queue, not an instant cascade**: a piece becomes unstable on one recompute and only detaches at its own staggered release time, so a multi-piece failure reads as waves instead of everything vanishing on the same frame - the direction bias and the fracture delay both lean on that same release moment existing as a distinct event.

## Known limitations

- Chaos: the Anchored bottom piece of `GC_wall` doesn't survive the root cluster breaking (see above) - an intentionally simple introductory experiment, to be revisited once Chaos is covered in depth.
- Roofs: one slope only, rectilinear footprints only, no overhang, and an odd-width roof ends in a flat top tile rather than a continuous slope. The ridge is a decorative piece placed by hand; there is no automatic roof generation yet, and no rafters (roof span is limited only by the portance rules).
- Grid placement ignores the real thickness of neighbouring pieces: occupancy is checked per slot, not per volume. A stair or a floor slab placed at the edge of a house overlaps the 20 cm of the exterior wall beside it (the wall's pivot is on one face, so it lies inside the neighbouring cell). **Planned fix (Nov 5):** a purely visual retreat. Slab edges are pulled back ~1 cm inside the wall's outer plane, and stairs and slabs lose the wall thickness on the wall side, read from the occupation table and recomputed when a wall is placed or destroyed. Nothing changes in the portance logic.
- Wall, floor, wall: from outside, the floor slab edge shows between the two storeys (a storey is 300 cm, a wall 280 cm, so a 20 cm band is left uncovered). **Planned fix (Oct 29):** exterior walls return to 300 cm, extended upward with the pivot unchanged; the roof pieces' `VerticalOffset` (−20 cm) is re-checked and removed or adjusted. Purely visual, no impact on portance. The change exposes coplanar faces (wall and slab edge), handled by the fix above.
- Coplanar faces flicker once pieces are tinted individually: two walls meeting at a corner share a plane and were only invisible because they used the same material; the per-piece damage tint makes the overlap visible. **Planned fix (Oct 31):** a parity bias. Pieces of even orientation are made ~0.3 % longer and odd ones ~0.3 % shorter, so end faces and tops at a corner are no longer in the same plane (driven by `DA_StructuralRules`). A depth offset in the material is the last resort, to be evaluated in Block 2 (GPU cost, interaction with virtual geometry).
- `APlot` is still a single bounds volume - no ownership, no cost tracking, no buy/sell (the long-term vision for that is out of scope for now).
- The UI is a temporary layer: items are shown as category-coloured squares with their name rather than icons, and there are no animations.
- **No vertical link between two walls stacked directly on each other.** `AreSocketsLinked` only pairs `WallBottom` with `FloorEdge`/`FloorCorner` and `WallTop` with `FloorBottom`/`RoofLow` - a wall only ever gets vertical stability through an intervening floor slab's sockets, never straight from the wall below it. Found via a two-storey test house with a stairwell opening (deliberately floor-less at both levels): the exterior walls bordering that opening couldn't be re-placed upstairs even though they rest squarely on a stable, load-bearing wall below. **Planned fix:** add a `WallTop`/`WallBottom` pairing (same cell, one level up), through the same `VerticalLoss`/`c(h)` arithmetic as every other vertical link - not a free ride, just closing the missing case. Distinct from the next point below: a sub-anchor slab can't be placed inside a deliberately open stairwell hole, so it doesn't cover this case.
- **Tall buildings decay toward zero stability by design, independent of the point above.** A plain vertical stack (wall, slab, wall, slab...) loses at least `VerticalLoss` per vertical hop even at full HP; a 5-storey stack already sits at or below `CollapseThreshold` at today's tuning, so a tall building cannot exist even undamaged. **Planned fix (GDD §8):** a sub-anchor slab variant - same footprint and the same 20 cm height as a normal slab, so storeys stay 300 cm - that resets stability to 1.0 like the foundation's `Anchor`, provided the chain below it is valid. Placed every `SubAnchorInterval` slabs by the generator, or by hand in Build Mode. Not yet implemented.
- Fracture-on-destroy is generic per `EPieceClass` (3-6 procedural shards), not per-mesh - a deliberate v1 simplification (GDD §7.8); per-mesh Geometry Collections (`UFractureLibrary`) are not built yet.
- `Lvl_DemoTest`'s test house is not persisted - grid-anchored pieces only compute their `GridCoord` through `UBuildModeComponent::TryPlace`, so an `APlot::BeginPlay` auto-registration pass would silently register every hand-placed piece at `(0,0,0)`. The real fix needs the inverse of the grid-placement math (camera raycast + edge disambiguation) and overlaps with the house generator's own placement API - not attempted yet, so the test house is rebuilt each session via Build Mode.
- `GrabAction`/`PushAction` (the legacy physics-manipulation bindings) live in `IMC_Universal`, globally active in every level, alongside the toolbelt's own `PrimaryUse`/`SecondaryUse` in `IMC_Mission` - both input contexts stay active everywhere ("option B" per-level IMC gating is still deferred). Worked around with `UToolbeltComponent::IsLocked()`: `Lvl_Home`'s Level Blueprint locks the toolbelt so the legacy bindings are the only ones live there, and `Lvl_DemoTest` leaves it unlocked so the toolbelt wins. A real per-level Enhanced Input setup, not a lock flag, is the eventual fix.
