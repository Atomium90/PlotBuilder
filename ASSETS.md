# PlotBuilder — Asset inventory

Technical list of every external or self-generated asset currently imported into the project: what it is, where it came from, and where it lives in `Content/`. For licenses and the formal attribution text, see [`CREDITS.md`](CREDITS.md) - this file is about *where*, that one is about *under what terms*.

## Kenney Furniture Kit

| What | Imported at | Used by |
|---|---|---|
| 41 props, each with its mesh and its materials: `bathroomCabinet`, `bathroomMirror`, `bathroomSink`, `bathroomSinkSquare`, `bathtub`, `bedBunk`, `bedDouble`, `bedSingle`, `bookcaseClosed`, `bookcaseOpen`, `bookcaseOpenLow`, `cabinetBed`, `cabinetBedDrawer`, `cabinetTelevision`, `chair`, `chairDesk`, `chairModernFrameCushion`, `computerKeyboard`, `computerScreen`, `desk`, `deskCorner`, `kitchenCabinet`, `kitchenFridge`, `kitchenFridgeLarge`, `kitchenFridgeSmall`, `kitchenSink`, `kitchenStove`, `kitchenStoveElectric`, `lampRoundTable`, `loungeChair`, `loungeSofa`, `loungeSofaLong`, `shower`, `showerRound`, `sideTable`, `sideTableDrawers`, `table`, `tableCoffee`, `tableCoffeeGlass`, `toilet`, `toiletSquare` | `Content/Props/Kenney/FurnitureKit/<name>/` | 39 `BP_Prop_*` Blueprints in `Content/Props/Blueprints/` (children of `BP_PhysicsProp_Base`, which only carry the mesh and its collision), then the rows of `DT_FurnishingRules` (what the house generator places in each room) and `DT_Shop` |

`computerKeyboard` and `computerScreen` are imported and have no Blueprint yet. The GDD's "about thirty" is a ballpark, not a target: the furnishing tables use what the kit offers for the five room types.

## Kenney Car Kit and Survival Kit (mission environment)

Imported with Import Uniform Scale set per mesh (the kits are small), pivots already at the foot. Each kit has one `T_*_Colormap` texture and one `M_*` material, built by hand and shared by all its meshes (the GLBs reference an external `Textures/colormap.png` instead of embedding it).

| What | Imported at | Used by |
|---|---|---|
| Car Kit: `SM_Truck` (x1.7), `SM_TractorShovel` (x1.7), `SM_Cone` (x1.2), `T_CarKit_Colormap`, `M_CarKit` | `Content/Environment/Kenney/CarKit/` | level dressing in `Lvl_Mission` (truck next to the drop zone, cones along the road) |
| Survival Kit: `SM_Tree`, `SM_TreeTall`, `SM_TreeAutumn`, `SM_TreeAutumnTall` (x5), `SM_RockA/B/C`, `SM_RockSandA/B/C` (x3), `SM_GrassPatch`, `SM_Box`, `SM_BoxLarge`, `SM_ResourcePlanks` (x3), `SM_Barrel` (x2.6), `SM_ResourceWood` (x5), `SM_Workbench`, `SM_WorkbenchAnvil`, `T_SurvivalKit_Colormap`, `M_SurvivalKit` | `Content/Environment/Kenney/SurvivalKit/` | the scatter rows of `AMissionGround` (Trees, Bushes, Rocks, Site clutter); the two workbenches dress `Lvl_Hub`. The grass mesh has no collision, the others keep the single convex hull generated at import |

Sources: `_AssetSources/Environment/Kenney/CarKit/` and `SurvivalKit/` (not tracked).

## Kenney Factory Kit (hub)

| What | Imported at | Used by |
|---|---|---|
| 22 meshes: `SM_FactoryBoxSmall/Wide/Long/Large`, `SM_FactoryCone`, `SM_FactoryArrow`, `SM_FactoryDoor`, `SM_FactoryDoorWideClosed/Half/Open`, `SM_FactoryWarningOrange`, `SM_FactoryWarningTraffic`, `SM_FactoryIndicatorSpecialArea/Arrow/Cross/Lines`, `SM_FactoryCatwalkStraight/Corner/Cross/Junction/Stairs/StairsLoop`, plus `T_FactoryKit_Colormap` and `M_FactoryKit` (one palette texture and one material, like the other Kenney kits) | `Content/Environment/Kenney/FactoryKit/` | dressing of `Lvl_Hub`. The catwalk pieces are imported and not placed yet |

Source: `_AssetSources/Environment/Kenney/FactoryKit/` (not tracked).

## Quaternius (storage shed and shelves)

| What | Imported at | Used by |
|---|---|---|
| `SM_ShedWoodA` (x6), `SM_ShedWoodB` (x6), `SM_ShedBrick` (x7, brick walls with a tile roof; first imported at x3 and x3.5, too small in game, doubled), plus 7 shared flat-colour material instances `MI_ShedWood`, `MI_ShedWoodLight`, `MI_ShedMain`, `MI_ShedWalls`, `MI_ShedStone`, `MI_ShedStoneLight`, `MI_ShedMetalLight` (the three GLBs each came with their own copy of the two wood materials, merged into one) | `Content/Environment/Quaternius/StorageShed/` | `BP_Shed_WoodA`, `BP_Shed_WoodB`, `BP_Shed_Brick` in `Content/Environment/Blueprints/` (children of `BP_PhysicsProp_Base` only for its durability and break handling), inside an `AProtectedZone` |
| `SM_ShelfSmall`, `SM_ShelfLarge`, `SM_ShelfTall`, plus 4 shared flat-colour material instances `MI_ShelfMain`, `MI_ShelfAccent`, `MI_ShelfDarkGrey`, `MI_ShelfWhite` | `Content/Environment/Quaternius/Shelf/` | dressing of `Lvl_Hub` |

Sources: `_AssetSources/Environment/Quaternius/StorageShed/` and `Shelf/` (not tracked). The shed source files carry a node scale of 100 and a -90 degree rotation; Unreal bakes both, the meshes come in upright, at 846 x 880 x 457, 1076 x 969 x 457 and 1206 x 966 x 351 cm with the doubled scale.

## ambientCG textures

Only the colour, normal and roughness maps of each 2K JPG set are imported (the displacement and ambient occlusion maps are not used).

| What | Imported at | Used by |
|---|---|---|
| `Concrete017` as `T_Concrete_C/N/R` | `Content/Environment/Textures/Ground/` | `M_HubFloor`, the floor of `Lvl_Hub` (`Content/Environment/Materials/`) |
| `Ground054` as `T_Sand_C/N/R`, `Ground109` as `T_Dirt_C/N/R`, `Road007` as `T_Road_C/N/R` | `Content/Environment/Textures/Ground/` | `M_MissionGround`, the ground of the mission levels: sand and dirt on the lot, the road look on the flat zones marked as a road |

Sources: `_AssetSources/Environment/AmbientCG/` (not tracked).

## Self-generated: caution pole and tape

| What | Imported at | Used by |
|---|---|---|
| `SM_CautionPole` (112 cm), `SM_CautionTape` (one 100 cm module, striped by geometry), both on `MI_ToolsPalette` | `Content/Environment/Tape/` | `APlot::PoleMesh` and `APlot::TapeMesh` (the plot perimeter in mission levels) |

Script-generated: `_Inbox/Environment/generate_site_props.py`. Unreal reads glTF as (X, Z, Y), so the tape length is authored along glTF X.

## Kenney "Impact Sounds"

| What | Imported at | Used by |
|---|---|---|
| One `.wav` per `EDemolitionMaterial` (Wood, Plaster, Brick, Concrete, Glass, Metal) | `Content/Audio/Kenney/ImpactSounds/` | `DA_DemolitionRules::ImpactSounds` |

Source `.wav` files and the Kenney file each one maps to: `_AssetSources/Audio/Impact/README.md`.

## Sonniss GDC Game Audio Bundle 2026

| What | Imported at | Used by |
|---|---|---|
| 3 explosion sounds, isolated by hand from one longer clip in the bundle | `Content/Audio/SFX/Explosion/` (`SFX_Charge_Explosion_01/02/03`, wrapped in `SC_Charge_Explosion` with an `Att_ChargeExplosion` attenuation) | `AExplosiveCharge::ExplosionSound` |
| Jackhammer drill loop, a 4.4s extract looped with a 0.3s crossfade and normalized | `Content/Audio/SFX/Jackhammer/` (`SFX_Jackhammer_Loop`) | `DA_JackHammer::ContinuousLoopSound` |

Source `.wav` files: `_AssetSources/Audio/Explosion/` and `_AssetSources/Audio/Jackhammer/` (no per-file README yet - see `CREDITS.md` for the source).

## Self-generated: demolition tool placeholders

| What | Imported at | Used by |
|---|---|---|
| `SM_Crowbar`, `SM_Sledgehammer`, `SM_Jackhammer`, `SM_Charge` (greybox meshes with sockets), `tools_palette` texture/materials | `Content/Props/Tools/` | `ADemolitionTool` subclasses (held mesh), `AExplosiveCharge` |

Script-generated (not hand-modeled or AI image/model generation) - `generate_tools.py` builds the shapes, `build_fbx.py` adds the sockets and converts to FBX. Source scripts and the original FBX/palette: `_Inbox/Tools/README.md`.

## Interface: fonts and icons

| What | Imported at | Used by |
|---|---|---|
| Barlow Regular and Bold, Barlow Condensed Bold (font faces), composed into `F_Barlow` and `F_BarlowCondensed` | `Content/UI/Fonts/` | every text of the widget Blueprints |
| 14 pictograms `T_Icon_*` (5 shop categories, 6 tools, padlock, search, close), white on transparent, tinted by the widgets | `Content/UI/Icons/` | `DA_UIStyle`, referenced by the shop, the inventory, the hotbar and the tool belt |

The fonts come from Google Fonts (see `CREDITS.md`). The icons are self-generated: `make_icons.py` draws them as SVG paths and renders them to 128 px PNGs (`_Inbox/UI_Icons/`).

## Epic's Unreal Engine templates

| What | Imported at | Used by |
|---|---|---|
| First Person template base (game mode, player controller, colorway material instance) | `Content/FirstPerson/` | `APlayerCharacter`'s base pawn setup |
| Mannequin skeletal meshes, materials, textures and animations (Manny/Quinn) | `Content/Characters/Mannequins/` | the player's skeleton/animation base |

Ships with every new UE5 project from the First Person template - not separately downloaded.
