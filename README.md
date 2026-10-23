# PlotBuilder

Construction / simulation sandbox built with Unreal Engine 5.8. Individual capstone project (9 weeks, MSc Video Game Development, Gaming Campus).

Free-form construction with a modular piece kit (House Builder-style) on plots of land (`APlot`), where every placed piece carries its weight through a structural portance graph (Valheim-style), manipulated with a single physical interaction tool (Push / Pull / Launch).

> Provisional README. Will be replaced by the full technical documentation at the end of Block 1.

## Current state — Week 1 (Gameplay, C++/Blueprint & Physics)

Specs in `Docs/CDC/`.

- [x] Playable character, camera, Enhanced Input
- [x] Generic interaction system (`IInteractable` / `UInteractionComponent`) + visual debug tools
- [x] Manipulable physics objects, Push / Pull / Launch (`APhysicsProp`, `UPhysicsManipulationComponent`)
- [x] Buildable pieces and a plot (`ABuildablePiece`, `UHotbarComponent`, `UBuildModeComponent`, `APlot`)
- [x] Overlap trigger with a dedicated collision channel (`ADetectionZone`)
- [x] Chaos destruction (fractured, clustered wall)

## Current state — Week 2 (Construction, Portance)

- [x] Grid-based Build Mode (`EBuildAnchor`: Cell/Edge/Vertex) alongside the Week 1 flush-snap path, which keeps working unchanged for any piece not migrated to the grid
- [x] Full greybox piece kit: foundation, floor, walls (plain/door/window), interior walls, corner posts, stairs, a door leaf, and a full roof kit (slope, ridge, hip, valley, flat tile, gables)
- [x] Structural portance graph on `APlot` (`UStructuralSupportComponent`): stability propagated from anchors, load-bearing walls vs. partitions, collapse cascades on removal
- [x] Placing a risky piece is allowed rather than blocked - the Build Mode ghost turns yellow below a comfort threshold (green/yellow/red); it only actually collapses right after if it's also below the stricter collapse threshold, same as a removal-triggered collapse
- [x] `RoofStep`: a second, independent vertical axis for roof pieces (climbing a slope, stacking a gable infill) that never changes what a floor level means for any other piece
- [x] Stacking grid inventory (`UHotbarComponent`): 36 slots (9-slot hotbar + backpack), Minecraft-style click interaction, a shop catalog with per-item stack size and category
- [x] Socket debug view (`BP_SocketDebugger`): every declared socket drawn at its logical position, to check that pieces meet where the graph expects
- [x] Temporary UI (UMG): permanent HUD with a hotbar and the balance, an inventory screen, a shop with category tabs, a controls overlay

Next: a `Push`/`Pull`/`Launch` simplification (grab/drop + a dedicated construction-helper category) planned for Week 3; automatic roof generation from the walls' footprint, reusing the same roof pieces, is planned after the kit.

## Controls

| Action | Input |
|---|---|
| Move / Look / Jump | ZQSD / Mouse / Space |
| Interact | E |
| Grab / Pull | Left Click |
| Push / Launch | Right Click |
| Place a prop | F |
| Toggle Build Mode | Tab |
| Rotate the ghost (Build Mode) | A / E |
| Cycle the hotbar | Mouse Wheel |
| Shop | B |
| Inventory | I |
| Controls overlay | H |

In the inventory: a click picks up or places a stack, a right click places a single item. In the shop: a click buys one item, a Shift-click buys ten.

## Setup

- Unreal Engine 5.8
- Git LFS required before cloning (`git lfs install`): `.uasset`/`.umap` files are tracked via LFS
- Open `PlotBuilder.uproject`, generate project files if needed, build
- Open `Content/Maps/Lvl_Construction` and press Play

## Repository layout

| Path | Content |
|---|---|
| `Source/` | C++ classes: the player and its components, `ABuildablePiece`, `APlot`, `UStructuralSupportComponent` |
| `Content/Buildable/` | Piece Blueprints, greybox meshes (`Greybox/Meshes`), ghost materials (`Preview`) |
| `Content/Maps/` | `Lvl_Construction`, the active level |
| `Content/UI/` | UMG widgets and the shared style (`BFL_UIStyle`) |
| `Content/Input/` | Enhanced Input actions and mapping contexts |
| `Content/Environment/` | The plot and the detection zone |
| `Content/Props/` | Physics props |
| `Content/Debug/` | Debug Blueprints, including `BP_SocketDebugger` |
| `Docs/` | `TECHNICAL.md` and the specs in `CDC/` |

## Technical overview

`APlayerCharacter` carries a handful of single-responsibility components rather than one large Blueprint: `UInteractionComponent` detects and relays to whatever implements the generic `IInteractable` interface, `UPhysicsManipulationComponent` handles Push/Pull/Launch on physics bodies, `UHotbarComponent` is a stacking grid inventory that also handles the shop purchase, acquisition and instant placement for `APhysicsProp`, and `UBuildModeComponent` layers a dedicated preview/snap/confirm flow on top of it for `ABuildablePiece` - either the Week 1 flush-snap path or a grid-based one (`EBuildAnchor`), depending on the piece. A `APlot` bounds the buildable area and owns `UStructuralSupportComponent`, the portance graph that every placed piece registers into: pieces declare sockets in their Blueprint (not on their mesh), stability propagates from the foundations' anchors, and a piece with no stable chain collapses. One wall is also set up as a Chaos-destructible Geometry Collection.

See [`Docs/TECHNICAL.md`](Docs/TECHNICAL.md) for the full technical documentation: every class's role, how each system works, the physics, building and portance models, the Chaos setup, the debugging tools, and the key technical choices made.

## Credits

Furniture props: Kenney Furniture Kit (kenney.nl). Character mannequins and the first-person template content: Epic Games' Unreal Engine templates.
