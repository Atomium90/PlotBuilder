# PlotBuilder

Sandbox built with Unreal Engine 5.8. Individual capstone project (9 weeks, MSc Video Game Development, Gaming Campus).

The player runs a small demolition company: take a contract at the hub, go to an isolated generated house, and tear it down with tools while a structural portance graph (Valheim-style) decides what falls and when. A plot of land can also be bought to build a home with a modular piece kit (House Builder-style), where every placed piece carries its weight through the same graph. Four tools (explosive charge, sledgehammer, crowbar, jackhammer) damage pieces, and a piece's damage feeds back into the graph.

> Provisional README. Will be replaced by the full technical documentation at the end of Block 1.

## What is playable

| Map | What it is |
|---|---|
| `Lvl_Hub` | The warehouse: the PC terminal, the truck to a mission, the way into the home (it opens once the player owns a parcel; `pb.SetParcel 1` stands in for the purchase) |
| `Lvl_Mission` | A seeded lot with a generated house. The contract (total, partial or renovation) is bound to the house, and the tools are the only way to act on it. There are no offers and no payment yet |
| `Lvl_Home` | The player's own plot: Build Mode and paint, kept between visits |
| `Lvl_DemoTest` | A test map with everything on: the tools, the portance cascade, Build Mode |

## Controls

| Action | Input |
|---|---|
| Move / Look / Jump | ZQSD / Mouse / Space |
| Interact | E |
| Use the equipped tool | Left Click (primary), Right Click (secondary) |
| Switch tool | Left / Right arrow |
| Build Mode on or off | Tab |
| Place | F |
| Rotate the ghost (Build Mode) | A / E |
| Change the build level / the roof step | Numpad + and - / O and P |
| Select in the hotbar | Mouse Wheel |
| Shop / Bag | B / I |
| Controls reminder | H (folded at the start, as a small chip in the top right corner) |

In the shop, the plus and minus of a row fill the cart (Shift moves ten at a time) and Confirm buys it all or nothing. In the bag, a left click picks up or places a stack and a right click places a single item.

Where the tool belt is locked (the hub), Left and Right Click are the older Grab and Push instead. What a map allows (building, demolition, the belt itself) is set per map.

## Setup

- Unreal Engine 5.8
- Git LFS required before cloning (`git lfs install`): `.uasset`/`.umap` files are tracked via LFS
- Open `PlotBuilder.uproject`, generate project files if needed, build
- Open `Content/Maps/Lvl_Hub` to play the loop (hub, mission, hub), `Lvl_Home` for construction, or `Lvl_DemoTest` for the demolition tools and the portance cascade, and press Play

## Repository layout

| Path | Content |
|---|---|
| `Source/PlotBuilder/` | C++ by area: `Player`, `Interaction` (inventory, shop cart, tool belt, Build Mode, paint brush), `Buildables` (pieces and sockets), `Environment` (the plot, the structural graph, zones, the mission ground), `Destruction` (durability, tools, rules, fragments), `Generation` (the house generator), `Contracts` (the mission director and the contract binding), `Hub`, `Home`, `Progress` (the saved progress), `Props`, `UI` (widget base classes, the style, the messages), `Debug` (the `pb.*` console commands) |
| `Content/Buildable/` | Piece Blueprints, greybox meshes, fracture collections, ghost materials |
| `Content/Generation/` | House archetypes and plans, the roof kit, the furnishing table |
| `Content/Destruction/` | The tool Blueprints and their data |
| `Content/Maps/` | The four levels above |
| `Content/UI/` | The widget Blueprints, the style Data Asset `DA_UIStyle`, fonts and icons |
| `Content/Input/` | Enhanced Input actions and mapping contexts |
| `Content/Environment/` | The plot, the dressing of the hub and the missions, textures |
| `Content/Props/` | Physics props, and the tool meshes under `Props/Tools` |
| `Content/Audio/` | Impact, explosion and jackhammer sounds |
| `Content/Debug/` | Debug Blueprints, including `BP_SocketDebugger` |
| `Docs/` | `GDD.md` (the design), `TECHNICAL.md`, `PROFILING.md`, and the specs in `CDC/` |

## Technical overview

`APlayerCharacter` carries a handful of single-responsibility components rather than one large Blueprint: `UInteractionComponent` detects and relays to whatever implements the generic `IInteractable` interface, `UPhysicsManipulationComponent` handles Push/Pull/Launch on physics bodies (and backs the Hands tool's Grab/Throw), `UHotbarComponent` is a stacking grid inventory that also handles the shop purchase (through the cart in `UShopCartComponent`), acquisition and instant placement for `APhysicsProp`, and `UBuildModeComponent` layers a dedicated preview/snap/confirm flow on top of it for `ABuildablePiece` - either the flush-snap path or a grid-based one (`EBuildAnchor`), depending on the piece. A `APlot` bounds the buildable area and owns `UStructuralSupportComponent`, the portance graph that every placed piece registers into: pieces declare sockets in their Blueprint (not on their mesh), stability propagates from the foundations' anchors and is reduced by a piece's own damage (`c(h)`), and a piece with no stable chain joins a staggered collapse queue instead of vanishing outright. `UToolbeltComponent` and its four demolition tools deal damage through the generic `IDamageable` interface, resolved per hit by each piece's own `UDurabilityComponent` against the tool x material matrix in `DA_DemolitionRules` - tools and the portance graph never reference each other directly. A destroyed piece breaks into one of the authored Chaos Geometry Collections of its mesh when it has any, and into generic shards otherwise.

Around that core, `AHouseGenerator` builds a validated, seeded house through the same placement function Build Mode uses, `AMissionDirector` sets up a mission level and binds the contract to the house, and the hub, the home and the saved progress (`UProgressSubsystem`) tie the levels together. The interface is UMG with its logic in C++ (a base class per widget, one `UUIStyle` Data Asset for the look).

See [`Docs/TECHNICAL.md`](Docs/TECHNICAL.md) for the full technical documentation: every class's role, how each system works, the physics, building, demolition and portance models, generation, missions, the interface, the Chaos setup, the debugging tools, and the key technical choices made.

## Credits

Furniture props, the mission and hub environment kits and the impact sounds: Kenney (kenney.nl, CC0). Storage shed and shelves: Quaternius (CC0). Ground textures: ambientCG (CC0). Explosion and jackhammer sounds: isolated from the Sonniss GDC Game Audio Bundle 2026 (sonniss.com). Fonts: Barlow and Barlow Condensed (SIL Open Font License). Demolition tool placeholder meshes, caution props and interface icons: self-generated. Character mannequins and the first-person template content: Epic Games' Unreal Engine templates.

Full attribution and licenses: [`CREDITS.md`](CREDITS.md). Where each asset is used in the project: [`ASSETS.md`](ASSETS.md).
