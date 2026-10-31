# PlotBuilder

Construction / simulation sandbox built with Unreal Engine 5.8. Individual capstone project (9 weeks, MSc Video Game Development, Gaming Campus).

Free-form construction with a modular piece kit (House Builder-style) on plots of land (`APlot`), where every placed piece carries its weight through a structural portance graph (Valheim-style). Block 1 pivots the core loop to demolition contracts: four tools (explosive charge, sledgehammer, crowbar, jackhammer) tear down pieces whose damage feeds back into that same portance graph, which decides what falls and when.

> Provisional README. Will be replaced by the full technical documentation at the end of Block 1.

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

In `Lvl_DemoTest`, the toolbelt (Hands + the 4 demolition tools) takes over Left/Right Click as primary/secondary use instead of Grab/Push; `Lvl_Home` keeps the legacy Grab/Push bindings above. Exact toolbelt slot keys are still being finalized (GDD §14).

## Setup

- Unreal Engine 5.8
- Git LFS required before cloning (`git lfs install`): `.uasset`/`.umap` files are tracked via LFS
- Open `PlotBuilder.uproject`, generate project files if needed, build
- Open `Content/Maps/Lvl_Home` for construction/Build Mode, or `Content/Maps/Lvl_DemoTest` for the demolition tools and portance cascade, and press Play

## Repository layout

| Path | Content |
|---|---|
| `Source/` | C++ classes: the player and its components, `ABuildablePiece`, `APlot`, `UStructuralSupportComponent`, and the `Destruction/` module (durability, tools, demolition rules, fragments) |
| `Content/Buildable/` | Piece Blueprints, greybox meshes (`Greybox/Meshes`), ghost materials (`Preview`) |
| `Content/Maps/` | `Lvl_Home` (construction/Build Mode), `Lvl_DemoTest` (demolition tools and portance cascade) |
| `Content/UI/` | UMG widgets and the shared style (`BFL_UIStyle`) |
| `Content/Input/` | Enhanced Input actions and mapping contexts |
| `Content/Environment/` | The plot and the detection zone |
| `Content/Props/` | Physics props, and the demolition tool meshes under `Props/Tools` |
| `Content/Debug/` | Debug Blueprints, including `BP_SocketDebugger` |
| `Docs/` | `TECHNICAL.md` and the specs in `CDC/` |

## Technical overview

`APlayerCharacter` carries a handful of single-responsibility components rather than one large Blueprint: `UInteractionComponent` detects and relays to whatever implements the generic `IInteractable` interface, `UPhysicsManipulationComponent` handles Push/Pull/Launch on physics bodies (and backs the Hands tool's Grab/Throw in `Lvl_DemoTest`), `UHotbarComponent` is a stacking grid inventory that also handles the shop purchase, acquisition and instant placement for `APhysicsProp`, and `UBuildModeComponent` layers a dedicated preview/snap/confirm flow on top of it for `ABuildablePiece` - either the flush-snap path or a grid-based one (`EBuildAnchor`), depending on the piece. A `APlot` bounds the buildable area and owns `UStructuralSupportComponent`, the portance graph that every placed piece registers into: pieces declare sockets in their Blueprint (not on their mesh), stability propagates from the foundations' anchors and is reduced by a piece's own damage (`c(h)`), and a piece with no stable chain joins a staggered collapse queue instead of vanishing outright. `UToolbeltComponent` and its four demolition tools deal damage through the generic `IDamageable` interface, resolved per hit by each piece's own `UDurabilityComponent` against the tool x material matrix in `DA_DemolitionRules` - tools and the portance graph never reference each other directly. One wall is also set up as a Chaos-destructible Geometry Collection, separate from the demolition tools' own fracture-on-destroy.

See [`Docs/TECHNICAL.md`](Docs/TECHNICAL.md) for the full technical documentation: every class's role, how each system works, the physics, building, demolition and portance models, the Chaos setup, the debugging tools, and the key technical choices made.

## Credits

Furniture props: Kenney Furniture Kit (kenney.nl). Impact sounds: Kenney "Impact Sounds" (kenney.nl, CC0). Explosion sounds: isolated from the Sonniss GDC Game Audio Bundle 2026 (sonniss.com). Demolition tool placeholder meshes: self-generated (`_Inbox/Tools`). Character mannequins and the first-person template content: Epic Games' Unreal Engine templates.

Full attribution and licenses: [`CREDITS.md`](CREDITS.md). Where each asset is used in the project: [`ASSETS.md`](ASSETS.md).
