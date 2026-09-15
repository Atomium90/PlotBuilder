# PlotBuilder

Construction / simulation sandbox built with Unreal Engine 5.8. Individual capstone project (9 weeks, MSc Video Game Development, Gaming Campus).

Free-form construction with a modular piece kit (House Builder-style) on plots of land (`APlot`), manipulated with a single physical interaction tool (Push / Pull / Launch).

> Provisional README. Will be replaced by the full technical documentation at the end of Block 1.

## Current state — Week 1 (Gameplay, C++/Blueprint & Physics)

Specs in `Docs/CDC/`.

- [x] Playable character, camera, Enhanced Input
- [x] Generic interaction system (`IInteractable` / `UInteractionComponent`) + visual debug tool
- [ ] Manipulable physics objects, Push / Pull / Launch (`APhysicsProp`, `UPhysicsManipulationComponent`)
- [ ] Buildable pieces and plots (`ABuildablePiece`, `APlot`)
- [ ] Chaos demolition, triggers

## Setup

- Unreal Engine 5.8
- Git LFS required before cloning (`git lfs install`): `.uasset`/`.umap` files are tracked via LFS
- Open `PlotBuilder.uproject`, generate project files if needed, build

## Architecture (summary)

`APlayerCharacter` (movement, camera) carries a `UInteractionComponent` that detects the focused actor and relays to `IInteractable`, implemented by interactive objects. Full detail (classes, locked decisions) will land in the upcoming technical doc.
