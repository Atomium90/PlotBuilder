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

## Setup

- Unreal Engine 5.8
- Git LFS required before cloning (`git lfs install`): `.uasset`/`.umap` files are tracked via LFS
- Open `PlotBuilder.uproject`, generate project files if needed, build

## Technical overview

`APlayerCharacter` carries a handful of single-responsibility components rather than one large Blueprint: `UInteractionComponent` detects and relays to whatever implements the generic `IInteractable` interface, `UPhysicsManipulationComponent` handles Push/Pull/Launch on physics bodies, `UHotbarComponent` handles acquisition and instant placement for `APhysicsProp`, and `UBuildModeComponent` layers a dedicated preview/snap/confirm flow on top of it for `ABuildablePiece`. A `APlot` bounds the buildable area, and one wall is set up as a Chaos-destructible Geometry Collection.

See [`Docs/TECHNICAL.md`](Docs/TECHNICAL.md) for the full technical documentation: every class's role, how each system works, the physics and Chaos setup, and the key technical choices made.
