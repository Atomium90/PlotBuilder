# PlotBuilder

Construction / simulation sandbox built with Unreal Engine 5.8. Individual capstone project (9 weeks, MSc Video Game Development, Gaming Campus).

Free-form construction with a modular piece kit (House Builder-style) on plots of land (`APlot`), manipulated with a single physical interaction tool (Push / Pull / Launch).

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
- [x] Full greybox piece kit: foundation, floor, walls (plain/door/window), interior walls, corner posts, stairs, and a full roof kit (slope, ridge, hip, valley, flat tile, gables)
- [x] Structural portance graph on `APlot` (`UStructuralSupportComponent`): stability propagated from anchors, load-bearing walls vs. partitions, collapse cascades on removal
- [x] Placing an unstable piece is allowed rather than blocked - the Build Mode ghost turns yellow as a warning (green/yellow/red), and an unstable placement collapses right after, same as a removal-triggered collapse
- [x] `RoofStep`: a second, independent vertical axis for roof pieces (climbing a slope, stacking a gable infill) that never changes what a floor level means for any other piece
- [x] Stacking grid inventory (`UHotbarComponent`): 36 slots (9-slot hotbar + backpack), Minecraft-style click interaction, a shop catalog with per-item stack size and category
- [x] Inventory/shop/controls-help UI (UMG): `WBP_HUD`, `WBP_Inventory`, `WBP_Hotbar`, `WBP_Shop`, `WBP_ControlsHelp`

In progress: a `Push`/`Pull`/`Launch` simplification (grab/drop + a dedicated construction-helper category) planned for Week 3.

## Setup

- Unreal Engine 5.8
- Git LFS required before cloning (`git lfs install`): `.uasset`/`.umap` files are tracked via LFS
- Open `PlotBuilder.uproject`, generate project files if needed, build

## Technical overview

`APlayerCharacter` carries a handful of single-responsibility components rather than one large Blueprint: `UInteractionComponent` detects and relays to whatever implements the generic `IInteractable` interface, `UPhysicsManipulationComponent` handles Push/Pull/Launch on physics bodies, `UHotbarComponent` is a stacking grid inventory that also handles acquisition and instant placement for `APhysicsProp`, and `UBuildModeComponent` layers a dedicated preview/snap/confirm flow on top of it for `ABuildablePiece` - either the Week 1 flush-snap path or a grid-based one (`EBuildAnchor`), depending on the piece. A `APlot` bounds the buildable area and owns `UStructuralSupportComponent`, the portance graph that every placed piece registers into; one wall is also set up as a Chaos-destructible Geometry Collection.

See [`Docs/TECHNICAL.md`](Docs/TECHNICAL.md) for the full technical documentation: every class's role, how each system works, the physics and Chaos setup, and the key technical choices made.
