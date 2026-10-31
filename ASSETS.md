# PlotBuilder — Asset inventory

Technical list of every external or self-generated asset currently imported into the project: what it is, where it came from, and where it lives in `Content/`. For licenses and the formal attribution text, see [`CREDITS.md`](CREDITS.md) - this file is about *where*, that one is about *under what terms*.

## Kenney Furniture Kit

| What | Imported at | Used by |
|---|---|---|
| `bedSingle` (mesh, cover, pillow, carpet/metal/wood materials) | `Content/Props/Kenney/FurnitureKit/bedSingle/` | `BP_Bed` |
| `cabinetBedDrawer` (mesh, door, drawer, metal/wood materials) | `Content/Props/Kenney/FurnitureKit/cabinetBedDrawer/` | not yet wired to a Blueprint |

Only two pieces imported so far, furniture work hasn't started in earnest yet - the GDD's "about thirty" is a ballpark, not a count to hit immediately.

## Kenney "Impact Sounds"

| What | Imported at | Used by |
|---|---|---|
| One `.wav` per `EDemolitionMaterial` (Wood, Plaster, Brick, Concrete, Glass, Metal) | `Content/Audio/Kenney/ImpactSounds/` | `DA_DemolitionRules::ImpactSounds` |

Source `.wav` files and the Kenney file each one maps to: `_AssetSources/Audio/Impact/README.md`.

## Sonniss GDC Game Audio Bundle 2026

| What | Imported at | Used by |
|---|---|---|
| 3 explosion sounds, isolated by hand from one longer clip in the bundle | `Content/Audio/SFX/Explosion/` (`SFX_Charge_Explosion_01/02/03`, wrapped in `SC_Charge_Explosion` with an `Att_ChargeExplosion` attenuation) | `AExplosiveCharge::ExplosionSound` |

Source `.wav` files: `_AssetSources/Audio/Explosion/` (no per-file README yet - see `CREDITS.md` for the source).

## Self-generated: demolition tool placeholders

| What | Imported at | Used by |
|---|---|---|
| `SM_Crowbar`, `SM_Sledgehammer`, `SM_Jackhammer`, `SM_Charge` (greybox meshes with sockets), `tools_palette` texture/materials | `Content/Props/Tools/` | `ADemolitionTool` subclasses (held mesh), `AExplosiveCharge` |

Script-generated (not hand-modeled or AI image/model generation) - `generate_tools.py` builds the shapes, `build_fbx.py` adds the sockets and converts to FBX. Source scripts and the original FBX/palette: `_Inbox/Tools/README.md`.

## Epic's Unreal Engine templates

| What | Imported at | Used by |
|---|---|---|
| First Person template base (game mode, player controller, colorway material instance) | `Content/FirstPerson/` | `APlayerCharacter`'s base pawn setup |
| Mannequin skeletal meshes, materials, textures and animations (Manny/Quinn) | `Content/Characters/Mannequins/` | the player's skeleton/animation base |

Ships with every new UE5 project from the First Person template - not separately downloaded.
