# PlotBuilder — Technical Documentation (Week 1)

Short technical documentation deliverable for CDC_S1_Architecture_Gameplay_Physics_Unreal. Covers the overall architecture, the main classes/Components, how the interaction system works, the physics systems developed, and the main technical choices made.

## Architecture

`APlayerCharacter` carries three independent components, each with a single responsibility, plus a `UBuildModeComponent` layered on top of two of them:

| Class | Role |
|---|---|
| `APlayerCharacter` | Movement, camera, Enhanced Input bindings |
| `UInteractionComponent` | Traces from the view, tracks the focused actor, relays to `IInteractable` |
| `IInteractable` | Generic contract (`OnFocusBegin`/`OnFocusEnd`/`Interact`) - implementable in C++ or Blueprint |
| `UPhysicsManipulationComponent` | Push / Pull / Launch on the current focus, acting directly on its physics body |
| `UHotbarComponent` | Acquisition (Interact/Purchase) and instant placement for `APhysicsProp`; shared selection state for both Prop and Buildable flows |
| `UBuildModeComponent` | Dedicated placement flow for `ABuildablePiece`: ghost preview, rotation, snapping, obstruction check |
| `ABuildablePiece` | Construction piece (wall...): mesh + `Cost`, never simulates physics |
| `APhysicsProp` | Simulated physics object (crate, furniture...), manipulated via Push/Pull/Launch |
| `APlot` | Buildable area bounds; gates where `UBuildModeComponent` allows placement |
| `ADetectionZone` | Overlap-based trigger, reacts to any `IInteractable` actor entering/leaving |

No object-specific logic lives in `APlayerCharacter` - it only knows about its own components, which in turn only know about `IInteractable` and raw physics bodies, never about a specific object class.

## Interaction system

`UInteractionComponent` traces every tick from the controller's view point on a dedicated collision channel (`GameTraceChannel2`, named "Interactable" in the collision profile settings). If the trace hits an actor implementing `IInteractable`, it becomes the current focus (`OnFocusBegin`/`OnFocusEnd` fire on change) and `TryInteract()` calls `Interact()` on it. Both `ABuildablePiece` and `APhysicsProp` implement `Interact` to store themselves into the instigator's `UHotbarComponent` - the component itself has no idea what kind of object it just picked up.

## Shop catalog

`UHotbarComponent::ShopCatalog` is a `DataTable` of `FShopItemRow` (`DisplayName`, `ActorClass`, `Cost`) - one row per purchasable item, Buildable or Prop alike. `PurchaseRow`/`PurchaseByIndex` add a row's `ActorClass` straight to the hotbar with no world actor involved, as opposed to `Interact` picking up something already physically placed in the world. Adding a new purchasable item is a new DataTable row, not new code. `Cost` is stored on the row but not yet enforced - no currency/economy system exists yet, so purchasing is currently free.

## Physics: Push / Pull / Launch

`UPhysicsManipulationComponent` reads the sibling `UInteractionComponent`'s current focus (no separate trace) and acts on its physics body directly:
- **Grab** picks up the focus if it has a simulated body and nothing is already held.
- **Push** applies an outward impulse to the current focus.
- **Launch** releases whatever is held with a forward impulse.
- While held, a spring pulls the object towards a point in front of the view, damped against its current velocity to stop oscillation. The spring targets the body's **center of mass**, not its component origin - targeting the origin caused off-center meshes to spin continuously, since the pivot-to-center-of-mass offset rotates with the object and fed rotational energy back into the spring.

## Construction: Build Mode

Placing a `APhysicsProp` is instant (`UHotbarComponent::TryPlace`, grid-snapped). Placing a `ABuildablePiece` goes through a separate, toggleable **Build Mode** (`UBuildModeComponent`) instead, since blind instant placement was tested and found unusable:

- A single reusable ghost actor (mesh/material swapped in place, never respawned) previews the selected piece, tinted green when placement is valid, red when blocked.
- One trace per tick decides the ghost's transform: hits a placed `ABuildablePiece` → snaps flush against it (face picked from where the hit lands within the target's own local bounding box, not just the raw impact normal, so aiming near an edge is enough - no sockets, purely geometric from mesh bounds); hits anything else solid → rests flush there instead (ground, level geometry, a Prop); hits nothing → falls back to a fixed distance in front of the view.
- Rotation (45° steps) is manual (two keys) and stacks on top of whatever base rotation the snap implies.
- One box overlap per tick (world statics, dynamics, pawns) at the candidate transform decides the blocked state - it ignores the ghost, the piece being snapped onto, and the player, and shrinks the test box by a small tolerance so two flush, touching pieces don't false-positive from floating-point error.
- A `APlot` in the level (optional - no Plot means no gating) additionally blocks any placement outside its bounds, drawn every tick as a debug box so the buildable area is visible without any extra content.
- The hotbar's selection is a single shared, cycle-able index (not a stack): buying/picking up an item selects it, scrolling cycles through the hotbar (skipping Props while Build Mode is active), and confirming a placement advances to the next item of the same class if the player bought/picked up several in a row.

## Chaos destruction

One wall (`GC_wall`) is set up as a hierarchical Geometry Collection: a single planar cut splits it into a bottom slab and a top section, then only the top section is fractured further (5-piece Voronoi) - a two-level cluster, not a flat single-pass fracture. Per-level Damage Threshold is tuned so a Launch-strength hit detaches the top from the bottom, which then shatters into its 5 fragments on landing rather than immediately (the impact with the ground supplies the second dose of strain needed). The bottom is marked Anchored so it should stay fixed to the ground; it does not currently survive the root cluster breaking, so the bottom piece falls too. This is intentionally kept as an introductory Chaos experiment per the CDC ("this experiment may remain simple... before studying them in greater depth the following week") - getting the Anchor to persist through a break is exactly the kind of thing Week 2's deeper Chaos work will address.

## Debugging tools

- `UInteractionComponent`: trace line (`bShowDebugTrace`) and/or the focused actor's name on screen (`bShowDebugFocusName`) - split so either can be toggled independently.
- `UPhysicsManipulationComponent`: force vector applied to the held/pushed object.
- `UHotbarComponent`: stored slots and current selection listed on screen every frame.
- `UBuildModeComponent`: currently previewed class and rotation on screen.
- `APlot`: bounds drawn as a debug box every tick (kept on by default - it's meant to be seen during play, not just for debugging).

All debug flags default to `false` except `APlot`'s bounds outline; enable the ones you want per test.

## Key technical choices

- **Component separation over one big Blueprint**: `UInteractionComponent` (generic detection) and `UPhysicsManipulationComponent` (physics manipulation) are deliberately two components, not one - detection and manipulation are different responsibilities, and neither needs to know how the other works.
- **C++ for the skeleton, Blueprint for variants**: base classes, interfaces, and components are C++; each buildable piece or prop is a Blueprint subclass assigning a mesh and tuning values, never new code.
- **Geometric snapping over sockets**: Build Mode's piece-to-piece and ground snapping is computed from each mesh's own bounding box rather than hand-placed sockets - no per-piece editor authoring, and the same bounds data doubles as the obstruction check's overlap shape.
- **The player doesn't block its own construction**: the obstruction check deliberately ignores the player's own capsule. An earlier version blocked on it too, which made it near-impossible to build next to yourself - no FPS building game self-blocks for this reason either.

## Known limitations

- Chaos: the Anchored bottom piece of `GC_wall` doesn't survive the root cluster breaking (see above) - an introductory experiment per the CDC's own framing ("may remain simple"), to be revisited in Week 2 once Chaos is covered in depth.
- `APlot` is a single, minimal bounds volume - no ownership, no cost tracking, no buy/sell (the long-term vision for that isn't in scope this week).
