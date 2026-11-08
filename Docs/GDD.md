# PlotBuilder — Game Design Document

> **Working title:** TBD (code name `PlotBuilder`, kept because every class and the repo already use it).
> **Engine:** Unreal Engine 5.8 — C++ skeleton, Blueprint variants.
> **Format:** Individual capstone, GTech 4 (MSc Video Game Development, Gaming Campus).
> **Audience:** the author (private working document, not a pitch deck).
> **Version:** 0.2 — written on 2026-10-24 (Q&A phase); bonus tier, home and hub decisions added on 2026-10-29 (see §23.1, D22–D27); stairwell wall link, sub-anchor slab and four backlog items added on 2026-10-31 (D28).

---

## 0. How to read this document

### 0.1 Status tags

Every decision carries a tag so that nothing is mistaken for something it is not.

| Tag | Meaning |
|---|---|
| `[LOCKED]` | Decided explicitly by the author. Changing it requires a conscious decision, logged in §23. |
| `[PROPOSAL]` | A concrete suggestion made while writing this GDD. Reasonable, but not validated yet. |
| `[TBD]` | A known gap. A default is given so work can start; the real answer comes later. |
| `[OPTIONAL++]` | Wanted if time allows. Never blocks anything. |

### 0.2 Block tags

| Tag | Block | Deadline | Theme |
|---|---|---|---|
| `B1` | Block 1 | **2026-11-13** (graded delivery + oral defence) | Gameplay, C++/Blueprint, advanced physics |
| `B2` | Block 2 | **2027-02-19** `[PROPOSAL: mapping]` | Real-time rendering |
| `B3` | Block 3 | **2027-05-07** `[PROPOSAL: mapping]` | Staging and player experience |

The two later dates were given by the author without naming the blocks; mapping them to B2 and B3 in order is an assumption (see §22).

### 0.3 Numbers

Every numeric value in this document is a **starting value to be tuned**, not a specification. They exist so that the systems can be built and so that the maths of the design can be sanity-checked. The authoritative values will live in Data Assets (`DA_*`) and Data Tables (`DT_*`), never in code.

---

## 1. Executive summary

**One-line pitch.** You run a small demolition company: take a contract, drive to a lone building, take it apart intelligently with a handful of tools while a structural system punishes careless work, get paid by how well you followed the client's brief, and eventually use the money (and the salvaged furniture) to build a home of your own.

**Why this concept.**
- It reuses, rather than discards, the Weeks 1–2 work: the modular piece kit, the grid, the structural portance graph, Chaos, the physics manipulation tool.
- It inverts the usual construction-sandbox loop (build → sell) into *dismantle → get paid → optionally build*, which differentiates it from Minecraft-style and House-Builder-style games, as intended.
- It makes the technical showpiece (a structural graph driving collapse) the **core of the gameplay** rather than a hidden backend. The player *feels* the portance system: you have to think about what holds what.
- It fits the CDC (`CDC_GTech4_Fil_Rouge`): systemic interactions, physics, environmental reactions and experimentation take priority over scripted progression; technical quality takes priority over content quantity.

**What the player does, minute to minute.** Reads a building, decides what to attack and in what order, hits things with a tool, hears and sees the structure complain, runs before it comes down, and then walks away from a pile of rubble with a client report.

**What the game is not.** It is not a management game (the player spends as little time as possible in menus), not a realistic structural-engineering simulation, not multiplayer, and not a narrative-driven adventure (narrative arrives in B3 and stays light).

---

## 2. Design pillars

| # | Pillar | What it means in practice | What it rules out |
|---|---|---|---|
| P1 | **Think, don't smash** `[LOCKED]` | Demolition is ordered by the structural graph. Removing a wall has consequences; attacking from the bottom is risky, hard and slow. | A free-for-all where every hit is equally good; one-hit everything. |
| P2 | **Readable destruction** `[LOCKED]` | Legibility wins over realism, while staying credible. The graph decides *what falls and when*; Chaos only makes the result look physical. Warnings precede collapses. | Opaque emergent chaos the player cannot learn from. |
| P3 | **Satisfying, not tedious** `[LOCKED]` | Staggered, cascading, sideways collapses; a house takes 5–10 minutes (20–25 for the biggest). | Long grinds; everything falling in one frame. |
| P4 | **Time in the game, not in menus** `[LOCKED]` | Hub is tiny and fast: pick one of three offers, go. Purchases happen on site where relevant. | Spreadsheet management, deep inventory micromanagement. |
| P5 | **Technical mastery is the product** `[LOCKED]` | Systems are modular, data-driven, debuggable and explainable orally. Content variety comes from data on top of working systems. | Content quantity for its own sake; one-off hacks. |

---

## 3. Scope, schedule and cut line

### 3.1 Calendar

| Milestone | Date | Notes |
|---|---|---|
| Today | 2026-10-24 | Weeks 1–2 work exists (see README / TECHNICAL). |
| **Block 1 delivery + defence** | **2026-11-13** | Playable gameplay prototype, tech doc, AI statement. ~20 working days left. |
| Block 2 delivery | 2027-02-19 `[PROPOSAL]` | Availability will likely be lower (possible job). |
| Block 3 delivery | 2027-05-07 `[PROPOSAL]` | Backend is stable by then, so visuals/narrative should be calmer to produce. |

**Capacity for B1:** the author is available every day, ~7–8 h/day, i.e. roughly **140–160 h** between now and Nov 13. Capacity for B2/B3 is `[TBD]`; plan by priority order with cut lines rather than by hours.

### 3.2 Block 1 cut line (Nov 13)

**Must have (indispensable)**
- One mission map with **2–3 house templates** (generated hybrid, or hand-built as a fallback — see §19.1 gates).
- The **four tools**: explosive charge, sledgehammer, jackhammer, crowbar.
- Demolition with **hit points, materials and a tool × material matrix**.
- **Structural cascade** driven by the portance graph: staggered, legible, with warnings.
- **Crush penalty** for the player.
- **Basic contract conditions**: protected zone, furniture to salvage.
- **Client report + payment**, and the loop **hub → mission → hub** (the hub may be a minimal blockout).

**Should have (if time allows, still B1)**
- Hub dressed beyond a blockout (pure placeholder meshes are acceptable).
- Furniture quality variants as data (cheap once the furnishing pipeline exists).

**Explicitly moved to later blocks** (the author's own classification)
- **B2:** the warehouse hub (asset-driven dressing), quality variants of furniture/materials as visual work, all rendering/VFX/profiling-heavy work, atmosphere.
- **B3:** dialogues and price negotiation, evolving clients, reputation system, staging.
- **Dropped for now (2026-10-29):** the "keep for myself" drop zone, the warehouse storage limit, hub dressing beyond a navigable v1, the mission background.

**Bonus tier (added 2026-10-29).** The author is running ahead of the plan and the estimates are conservative (observed pace ≈ 0.7×), so the following were added to Sprints 2–3. They sit at the **bottom of every sprint's cut order**: they are the first things dropped if time is short, and they never gate a decision gate.

| Area | Items |
|---|---|
| Contracts | Conditions *keep standing* and *forbidden tool*; furniture value curve and "client item left inside" detection; a single `Tier` integer filtering offers; game-state flow (`InHub` → … → `Debrief`). |
| Content | Window glass piece; extended Kenney Furniture Kit props (up to ~30, fewer is fine); third archetype (small apartment block); heavy-carry slowdown. |
| Economy | Tools bought at the PC terminal (start values of §11.2); per-use maintenance fee on the jackhammer; parcel purchase at the PC terminal (40 000 default). |
| Home | `Lvl_Home` reached from the hub access point (a simple map switch), build mode there, **persistent across map switches** (pieces, props, inventory). One block: shipped together or cut together. |
| Hub | Navigable v1: hall, mezzanine office with the PC, big door with the truck trigger, access-point trigger. Boxes or an asset pack; no dressing. |
| Tooling | Console commands of §18.8, damage log, crush accumulator display. |
| Backlog items (2026-10-31) | **Sticky charge** (§7.4), **jackhammer drill-loop sound** (§15.1), **paint tool** (§12), **tool wheel** (§14). Cut before every other bonus item, in this order: tool wheel, paint tool, jackhammer sound, sticky charge. |

The **stairwell wall link** and the **sub-anchor slab** (§8) are *not* bonus: they fix structural limits (stairwell walls with no slab, tall buildings that cannot be stable) and are core items of Sprint 2.

**`[OPTIONAL++]`** (bonus whenever there is slack, never a dependency): construction contracts (rebuild after demolition), debris-cleanup contracts (shovel, skip/dumpster), extra tools (mini-excavator, charges with different profiles).

### 3.3 Out of scope

Multiplayer `[LOCKED]`; realistic structural simulation; voxel terrain (the Minecraft-style option from `CLAUDE.md` is dropped entirely); a real economy with market dynamics; voice acting; procedural city generation; open world.

---

## 4. Core loop

### 4.1 Macro loop `[LOCKED]`

```
 Hub (warehouse office)
   │  read 3 offers on the PC
   ▼
 Pick a contract ──► walk to the big door ──► truck
   │
   ▼
 Mission map: one building on dirt, inside the taped plot
   │  demolish according to the contract type and conditions
   ▼
 Client report: completeness, collateral damage, salvage, safety
   │  payment (always paid) + reputation change (B3)
   ▼
 Back to the hub ──► offers refresh ──► repeat
```

Branch (B1 bonus tier, not blocking): once enough money is saved → **buy a parcel → build your own house** with new furniture or salvaged pieces, and extend it (garden, workshop).

### 4.2 Micro loop (inside a mission)

1. **Read** the building (and the contract brief) — what is load-bearing, what is salvage, what must stay.
2. **Prepare** — clear salvageable furniture out of the way (carry it to the client drop-zone) *before* the demolition starts.
3. **Plan** — choose tool and attack point; place a charge if appropriate.
4. **Strike** — apply damage; watch damage states and warnings.
5. **React** — the structure shifts; step back; avoid being underneath.
6. **Finish** — check objectives; leave through the truck trigger and confirm.

### 4.3 Session shape

A mission is **5–10 minutes** in the normal range, **20–25 minutes** at most for very large sites. A full session (3 or 4 contracts) should feel like 30–40 minutes. The player's time is spent ~90% in missions, ~10% in the hub.

---

## 5. Game flow and levels

| Level | Purpose | Block |
|---|---|---|
| `Lvl_Hub` | The warehouse. Office with the mission PC, big door with the truck, and the access point to the player's own plot. | B1: navigable v1 (bonus tier; the PC + truck-trigger blockout is the fallback). B2: dressed warehouse. |
| `Lvl_Mission` | Single generic mission level. The house is generated at load from the contract's seed. | B1 |
| `Lvl_Home` | The player's own parcel. Evolves from today's `Lvl_Construction`. Locked until a parcel is purchased. | B1 bonus tier (simple map switch, persistent); polish and extensions B3 |
| `Lvl_DemoTest` | Dev-only sandbox: hand-built structures and every tool, for tuning and for the oral defence. | B1 |

**Transitions.** Level changes use `OpenLevel`; state that must survive (money, reputation, owned tools, the active contract and its seed) lives in a `UGameInstanceSubsystem` and is also serialised to a `USaveGame`. The generated mission house is **never saved**: it is rebuilt from the seed. The **player's home is saved** (pieces as class + grid cell + level + orientation, props as class + transform, plus the hotbar inventory) and restored through the same placement function the generator uses.

**Game states** (driven by the game mode / subsystem): `InHub` → `ContractSelected` → `OnSite_Prep` → `OnSite_Demolition` → `Debrief` → `InHub`.

---

## 6. Contracts

### 6.1 What a contract is

A contract is a **small data object** generated by the board:

| Field | Description |
|---|---|
| `Client` | Name/portrait ref (B3 gives it a personality; B1 uses a placeholder). |
| `Type` | `TotalDemolition`, `PartialDemolition`, `Renovation` (see §6.2). |
| `HouseArchetype` + `Seed` | What gets generated and how. Deterministic. |
| `AreaM2` | Floor area, shown on the board. |
| `EstimatedMinutes` | Derived from area and type, shown on the board. |
| `Conditions[]` | Zero or more conditions (§6.3). |
| `BasePayout` | Money for a perfect job. |
| `ReputationRequired` | Gates harder/larger offers (B3). |

### 6.2 Contract types `[LOCKED]`

| Type | Meaning | Target set (what must be destroyed) |
|---|---|---|
| **Total demolition** | The whole property goes. | All structural pieces + all furniture/props not explicitly salvaged. |
| **Partial demolition** | Only part of it: for instance one storey, one wing. | The pieces inside a designated section (a set of levels and/or a footprint region). Everything else is "must stay". |
| **Renovation** | Strip the interior; the shell stays. | Non-load-bearing pieces (interior partitions, fixtures, some furniture). Load-bearing walls, slabs and roof stay. |

### 6.3 Conditions

| Condition | Description | Status |
|---|---|---|
| **Salvage furniture** | The client wants specific items (e.g. "the sofa and the piano"); the rest is irrelevant. Items must be carried out to the client drop-zone *intact* before the house falls. Naturally forces a "clean the house first" phase. | `[LOCKED]` |
| **Protected zone** | A volume that must not be damaged (a neighbour's shed, a tree, a fence, a garage). Used heavily in very hard contracts. | `[PROPOSAL]`, matches the "high-rise inside a residence" idea. |
| **Keep standing** | A named part must remain (a façade, a load-bearing wall, the ground floor). | `[PROPOSAL]` — B1 bonus tier |
| **Forbidden tool** | e.g. no jackhammer (noise ordinance) or no explosives. | `[PROPOSAL]` — B1 bonus tier |
| ~~Time limit~~ | Explicitly doubtful. **Not in scope**; no time pressure in scoring. | dropped for now |

### 6.4 Offers `[LOCKED]`

- **Three offers at a time**, refreshed each time the player returns from a mission.
- Each offer shows: **area in m²** (with the estimated duration), **type**, **conditions**, **payment**.
- Hard offers (large, many conditions, protected zones) appear as reputation grows (B3). In B1 the offers are drawn from a weighted pool with no gating, or gated by a single "tier" integer.

**Estimated duration (starting formula).** `Minutes ≈ 0.06 × AreaM2 × TypeFactor`, with `TypeFactor`: total 1.0, partial 0.6, renovation 0.5. A 100 m² total demolition shows ~6 min; a 400 m² building ~24 min. To be validated by playtests.

### 6.5 Scoring and payment `[LOCKED: structure]`, `[TBD: coefficients]`

**Principle:** the client's report determines the payment, and **the payment is always made**. There is no hard "fail" state. A sloppy player still gets paid something; the real cost of bad play is *delay* (you reach the parcel later). This is a deliberate design stance: it keeps the loop moving and avoids punishing exploration.

Starting formula:

```
Completion   = Σ weight(destroyed pieces ∩ TargetSet) / Σ weight(TargetSet)
Collateral   = Σ weight(destroyed pieces ∉ TargetSet) / Σ weight(all non-target pieces)
FurnitureLoss= Σ value lost on client-keep items / Σ value of client-keep items
              (destroyed or left inside the building = lost; damaged = reduced value, §10.3)
Safety       = min(SafetyCap, Σ penalty per crush/explosion event on the player)

Penalty      = min(PenaltyCap, kCollateral·Collateral + kFurniture·FurnitureLoss + Safety)
Payout       = BasePayout × Completion × (1 − Penalty)
```

| Parameter | Start value | Comment |
|---|---|---|
| `weight` of a piece or prop | the cost of its `DT_Shop` row (`FShopItemRow::Cost`), found by actor class | One source for costs. The `Cost` field on the piece Blueprints is unused (left at 0). A class with no shop row weighs its `Value` (props) or 1, and is counted in the mission summary. |
| `kCollateral` | 1.5 | Collateral is weighed heavier than its raw share. |
| `kFurniture` | 1.0 | Value-weighted. |
| `PenaltyCap` | 0.8 | Never below 20% of the earned amount. |
| `SafetyCap` | 0.25 | |
| Crush event | −0.10 (stackable up to the cap) | See §7.7. |
| Explosion on player | −0.10 | Same family of penalty. |

**Debrief breakdown.** The report screen lists each term (completion %, collateral, furniture, safety) with its effect on the money. The same breakdown is printed in the debug log, which makes the evaluator easy to test and to defend orally.

**End of mission.** The player ends the job by interacting with the truck. If completion is below 100% or conditions are unresolved, the game asks for confirmation. No automatic fail.

**Debris** left on the ground carries **no penalty** in B1 (it only matters in the `[OPTIONAL++]` cleanup contracts).

---

## 7. Demolition

### 7.1 Overview

Every destructible thing (a `ABuildablePiece` or an `APhysicsProp` flagged as breakable) carries **durability**: hit points and a material. Tools deal damage; a **tool × material matrix** decides how effective each tool is on each material. When hit points reach zero the piece is **destroyed**: it is removed from the structural graph, which may cause **other pieces to become unstable and fall**, in a **staggered cascade**. Destroyed or collapsing pieces **fracture** into a few short-lived fragments.

### 7.2 Materials `[PROPOSAL, 6 materials as agreed]`

`EDemolitionMaterial`: **Wood**, **Plaster** (partitions/drywall), **Brick**, **Concrete**, **Glass**, **Metal**.

Furniture uses the same enum (wood furniture, metal appliances, glass tables...). A piece's material is data on its Blueprint, **separate from the mesh** — the same philosophy as sockets (§18). That makes "premium material" variants in B2 a data and shader change, not a code change.

### 7.3 Hit points

`HP = MaterialBaseHP × PieceClassFactor`

| Material | Base HP |
|---|---|
| Plaster | 120 |
| Wood | 250 |
| Brick | 500 |
| Concrete | 800 |
| Glass | 60 |
| Metal | 600 |

| Piece class | Factor |
|---|---|
| Foundation | 1.5 |
| Floor slab | 0.9 |
| Exterior wall (plain / door / window) | 1.0 |
| Interior wall | 0.8 |
| Corner post | 0.8 |
| Stairs | 0.9 |
| Roof tile (slope/ridge/hip/valley/flat/gable) | 0.6 |
| Door leaf | 0.3 (always wood, free to break) |
| Window pane | 1.0 (always glass, so 60 HP) |

Resulting examples: concrete foundation 1 200 HP; brick wall 500 HP; wood wall 250 HP; plaster interior wall ≈ 96 HP; wood roof tile 150 HP.

Furniture HP is set per prop (indicatively 20–150).

### 7.4 Tools `[LOCKED: 4 tools; PROPOSAL: stats]`

| Tool | Role | Use | Damage | Rate | Range | Footprint | Character |
|---|---|---|---|---|---|---|---|
| **Explosive charge** | First tool built (easy to debug and optimise); future root of explosives | Placed by hand; **manual detonation** (RMB) | 400 flat | one-shot | radius 4 m | Whole radius | Constant force, no falloff, no occlusion in v1 → trivial to test. Consumable. |
| **Sledgehammer** | Big, slow, effective on walls | LMB, with a wind-up | 80 / hit | ~0.8 hits/s | 2.5 m | Small radius (~0.5 m): can hit several adjacent pieces | Strong but imprecise → collateral risk. |
| **Jackhammer** | Continuous, precise on concrete | Hold LMB | 12 / tick | 10 ticks/s | 2.0 m | Single target | The answer to foundations and slabs. Maintenance cost (§11). |
| **Crowbar** | Fast and clean on wood and furniture | LMB | 25 / hit | ~2 hits/s | 2.0 m | Single target | The salvage tool: precise, low collateral. |

**The explosive charge is built first** (author's instructor's advice, adopted): constant radius and force make the damage pipeline, the cascade and the profiling easy to verify. Later it can grow into several explosive types.

**Charge details.**
- It is a physics actor (`AExplosiveCharge`): throwable/placeable with the existing hands tool. Today it rests where it lands; in the B1 bonus tier it can also be **placed on the aimed surface** (a camera trace within reach, oriented on the impact normal, attached to the hit component so it follows it, and falling if that piece is destroyed first).
- Detonation is **manual**, not timed (author decision). The player triggers it with the secondary action; a short audible/visible beep precedes the blast.
- On detonation: an overlap sphere gathers every `IDamageable` in the radius → each takes the flat damage (modulated only by the charge's column in the matrix, default 1.0) → a radial impulse (no falloff) is applied to nearby simulating bodies (debris, props).
- It **hurts the player** if inside the radius (same penalty family as crush). A placement preview shows the radius in a debug-friendly translucent sphere; this doubles as the visual debug tool.

### 7.5 Tool × material matrix `[PROPOSAL]`

Multipliers applied to a tool's base damage. Stored in `DA_DemolitionRules`.

| Tool \ Material | Wood | Plaster | Brick | Concrete | Glass | Metal |
|---|---|---|---|---|---|---|
| Crowbar | 1.5 | 1.5 | 0.2 | 0.05 | 1.0 | 0.3 |
| Sledgehammer | 1.0 | 1.5 | 1.0 | 0.3 | 2.0 | 0.4 |
| Jackhammer | 0.6 | 0.8 | 1.2 | 1.5 | 0.5 | 0.8 |
| Charge | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 | 1.0 |

**Sanity check against the design intent** (the matrix must make the *intended* tool the best choice):

| Situation | Calculation | Outcome |
|---|---|---|
| Brick wall (500 HP) with sledgehammer | 80 × 1.0 = 80 → 7 hits ≈ 9 s | Reasonable, a good tool for walls ✔ |
| Brick wall with crowbar | 25 × 0.2 = 5 → 100 hits ≈ 50 s | Painfully slow ✔ |
| Concrete foundation (1 200 HP) with sledgehammer | 80 × 0.3 = 24 → 50 hits ≈ 62 s | "Struggles" ✔ |
| Concrete foundation with jackhammer | 12 × 1.5 × 10 = 180 /s → ≈ 7 s | The right tool ✔ |
| Wood wall (250 HP) with crowbar | 25 × 1.5 = 37.5 × 2/s = 75 /s ≈ 3.3 s | Best on wood ✔ |
| Plaster partition (≈ 96 HP) with sledgehammer | 80 × 1.5 = 120 → one hit | Fast but wide footprint, collateral risk ✔ |
| Brick wall with one charge | 400 → 100 HP left (critical) | Weakens a lot, finishing is a second charge or a hit ✔ |

The matrix is a Data Asset so these numbers can be tuned live and even mid-defence.

### 7.6 Damage states and warnings `[PROPOSAL]`

Readability is the key requirement (P2). Each piece moves through states by remaining-HP ratio `h`:

| State | Condition | Feedback (B1 basic → B2 final) |
|---|---|---|
| **Intact** | h ≥ 0.75 | — |
| **Damaged** | 0.35 ≤ h < 0.75 | Material parameter `Damage` drives a crack/tint; small dust puffs on hit. |
| **Critical** | h < 0.35 | Heavier cracks, creaking sound, dust trickle. For load-bearing pieces: **Compromised** (below). |
| **Destroyed** | h = 0 | Removed from the graph, replaced by fragments. |

**Stability coupling.** A damaged piece transmits less stability to the pieces resting on it: `transmitted = stability × c(h)`, with `c = 1` for h ≥ 0.75, ~0.6 at h = 0.35, ~0.3 at h = 0. This makes an attacked load-bearing wall progressively *weaker for everything above it*, which is what makes the player "feel they are thinking".

**Compromised.** A load-bearing piece in the Critical state **that other pieces depend on** enters **Compromised**: after a random delay (0.5–3 s) it fails by itself, with a clear warning (creak, dust, shake) beforehand. This is the **bounded unpredictability** the design asks for: the collapse may come on its own, but never with zero warning. It is the main punishment for hammering the base of a structure.

*"Depend on" means sole support* `[LOCKED 2026-10-31]`. A Critical load-bearing piece is Compromised only if it is a **true bottleneck**: if it were removed, at least one other piece would fall below the collapse threshold (`0.1`) or lose its chain to an anchor. A piece that merely lies on the best path of a neighbour, while another path would still hold that neighbour, stays Critical (cracks, creaks) but is not Compromised. The test is a **dry run of the cascade computation**: the same evaluation that returns the set `U` of §7.7, with the piece removed from the graph (not with its damaged transmission value: `c(h)` is already applied separately), and nothing destroyed. It runs once when the piece enters Critical and again after each batch of destructions (the batching of §18.6); during a mission nothing is built, so redundancy only decreases, and a piece that stops being a bottleneck because its dependants are gone has its timer cancelled. A looser reading ("any piece whose best path passes through it") was rejected: it depends on tie-breaking between equal paths, cannot be tested, and would put almost every load-bearing wall of a house into Compromised.

### 7.7 Collapse cascade `[PROPOSAL, with fallbacks]`

**What happens when a piece is destroyed (or fails):**

1. The piece is removed from the graph and its slot freed.
2. The graph is recomputed (**once per event batch**, §18.6) and returns the set `U` of newly unstable pieces (stability below the collapse threshold `0.1` or no chain to an anchor).
3. Each piece in `U` receives a **release time**: `t = Base + hops × HopDelay + jitter`, where `hops` is the graph distance from the destroyed piece. Defaults: `HopDelay` 0.12 s, jitter ±0.08 s, with the total spread capped (~3 s) by compressing delays on big cascades.
4. At its release time a piece **detaches**: it becomes a simulating rigid body (**Falling** state) and gets a small impulse pointing away from the remaining stable structure (§below).
5. On first significant impact (or after a short max fall time) the falling piece **fractures** into fragments (§7.8). The falling phase is also the window in which it can crush the player (§7.9).
6. Pieces freed from support by step 4 will show up in a **later** recompute (coalesced every ~0.25 s), so the wave naturally continues.

**Sideways collapse.** The brief asks for collapses "to the sides when there is too much weight". Two layers, shippable in order:

| Layer | Idea | Status |
|---|---|---|
| **A. Direction bias** | Each released piece gets a horizontal impulse away from the centroid of its still-stable neighbours → walls fall outward, slabs slide off, instead of dropping straight down. Cheap, deterministic. | `[PROPOSAL]` — B1 target |
| **B. Overload** | Propagation also accumulates the **mass supported through each load-bearing piece**; if supported mass exceeds a capacity (itself reduced by `c(h)`), the piece becomes Compromised even if its chain to the anchor is intact. | `[PROPOSAL]` — B1 stretch / B2; fallback is A only |

**Why graph-driven rather than letting Chaos decide.** The portance graph is cheap, predictable, explainable in one sentence and visible on screen. Chaos only animates the consequence. This is the direct application of pillar P2, and it sidesteps the known Week 2 limitation (the Anchored bottom of `GC_wall` not surviving a cluster break), because the new pipeline **does not rely on clustered Geometry Collections holding together**: each piece is its own object and is replaced by fragments only when it is destroyed.

### 7.8 Fracture and debris `[LOCKED: simple, short lifetime]`

- On destruction a piece is swapped for a **pre-fractured Geometry Collection per *piece mesh***, authored once and reused for all instances (so no per-instance fracturing cost). Fragment count is set per mesh rather than one fixed number: **8–10** for a larger mesh (3–6 looked too chunky in testing), down to **3–6** for a small one (e.g. a corner post). The 3–6 range stays as-is for the generic per-class fallback used by any mesh without an authored override.
- Fragments have a **short life** — a few seconds (start at 6 s, matching today's behaviour of the falling chunk), then shrink/fade and despawn.
- A **`UDebrisSubsystem`** enforces a global cap on active fragments (start ~300), removes the oldest first, and puts slow fragments to sleep to save physics cost.
- A dedicated **Debris** collision channel/object type: fragments collide with the world and each other only as needed, never block the player's movement, and are ignored by build-mode obstruction checks.
- Large leftovers do not accumulate: **clean-up of rubble is not a requirement**. Rubble handling (shovel, skip) is `[OPTIONAL++]` and is designed so it can be added later (fragments are tagged, debris counts can be queried).
- B2 revisits this with Niagara dust and a profiling pass; the logic here must not depend on it.

### 7.9 Crush rule `[LOCKED: rule of thumb]`

> "One wall falling on me is fine; three walls and the roof, no."

Implemented as an **accumulated crush energy**, not a single threshold:

- A `UCrushDetectionComponent` on the player listens for hits from **Falling** pieces.
- Each hit adds `mass × relative speed` (or a scaled value) to an accumulator that **decays over a window** (~2 s).
- Below `CrushThreshold` a hit only staggers/hurts the feel (screen shake, small stagger). Crossing the threshold triggers a **crush event**: a **payment penalty** (§6.5), a **short stun** (~2 s), and a strong audio/visual cue.
- No death, no game over.

Single wall: under threshold. Three walls plus roof within the window: over it.

### 7.10 Attacking "from the bottom"

Structures should **resist** a naive bottom-up approach, as the author wants: the brick/concrete base is hard to break with the wrong tool (§7.5), the damage coupling (§7.6) weakens everything above it, and Compromised pieces collapse unpredictably within a window. Good play attacks supports in an order that lets the structure come down away from the player (or removes upper parts first), prepares an escape route, or uses charges from a distance.

---

## 8. Structural system — what changes

The existing portance graph (`UStructuralSupportComponent`, `DA_StructuralRules`) stays the heart. It currently supports placement and removal; this GDD requires it to support **gradual damage and staged failure**.

| Need | Change |
|---|---|
| Damage matters to stability | `c(h)` coupling applied to a piece's transmission (§7.6). |
| Cascades must be staggered | Replace "collapse everything unstable at once" by a **collapse queue with release times** (§7.7). |
| Warnings | `Compromised` state with timer. |
| Performance | **Batching:** destroying N pieces in a cascade must not recompute the graph N times (§18.6). |
| Direction | Neighbour-centroid impulse; optional overload accumulation. |
| Generated houses | The generator registers through the **same API** as build mode, so generated structures are valid by construction. |
| Debugging | Heat-map view of stability; wave visualisation; HP overlay (§18.7). |
| Stairwell walls | **Direct wall link.** Walls bordering a stairwell hole have no slab beneath or above them at any level, so they have nothing to pass stability through, and a slab cannot be placed in a hole that is empty on purpose. A direct `WallTop` ↔ `WallBottom` link between a wall and the wall stacked on it on the same edge (vertical class, same loss as the other vertical links) lets stability climb through the stack. It only passes a value along, it creates none: a wall stack still needs a chain back to an anchor. This is the original bug; the sub-anchor below does *not* fix it. Tests in Appendix B (T9), passed 2026-11-01. `[LOCKED: D28]` |
| Tall buildings | **Sub-anchor slab.** With `VerticalLoss` 0.1 and a collapse threshold of 0.1, a plain stack of walls and slabs reaches the threshold around the fifth storey, so a tall building cannot exist even at full health. A reinforced slab variant (same footprint and same 20 cm height as the normal slab, so storeys stay 300 cm and nothing is stacked) resets the stability to 1.0 like the foundation's Anchor, *provided the chain below it is valid*. The tall archetypes place one every `SubAnchorInterval` slabs (data); the player can place one in build mode. Tests in Appendix B (T8), passed 2026-11-01. `[LOCKED: D28]` |
| Roofs | Under every roof there is a **ceiling slab, always a reinforcement slab** (sub-anchor, above) whatever the storey interval, and a roof tile can rest on a slab edge (`RoofLow` on `FloorEdge`). A wide roof on a tall house therefore starts from a stability reset. A roof without a slab (high ceilings) stays a backlog idea. `[LOCKED: D32]` |
| Visual overlaps | Exterior walls back to 300 cm (visual only); parity bias at wall corners; slabs and stairs retreat near exterior walls. Scheduled Oct 29, Oct 31 and Nov 5 (§19.1). No change to sockets or the graph. |

No new sockets are required. The author's note holds: structurally the kit is varied enough; the glass window will be a Blueprint like the door (a frame piece plus a glass sub-piece, two assets, one Blueprint; scheduled Nov 3, bonus tier), and a different staircase is possible but optional.

---

## 9. Mission houses: hybrid generation

### 9.1 Approach `[LOCKED: hybrid]`

Hand-authored **archetype templates** with **procedural variation**, rather than a fully procedural layout generator. This is the risk-conscious choice for a 3-week B1 window. The "very technical" part of the project, as the author notes, is generating houses; the hybrid limits the unknowns.

### 9.2 Pipeline

```
Contract (archetype, seed, area target, type)
   ▼
1. Archetype template        — footprint rules, number of storeys, room slots, roof type
   ▼
2. Parameter rolling (seed)  — dimensions (in grid cells), openings, roof variant, material set
   ▼
3. Shell emission            — foundation → walls → slabs → roof, via the same grid placement API
   ▼
4. Interior partitioning     — rooms assigned to slots, interior walls, doors
   ▼
5. Structural validation     — all pieces connected; WouldBeStable for each; reject and re-roll on failure
   ▼
6. Furnishing                — props per room type and quality tier, from rule tables
   ▼
7. Contract binding          — target set, must-stay set, keep list, protected zones
```

### 9.3 Archetypes

| Archetype | Description | Block |
|---|---|---|
| Bungalow | Single storey, simple roof | B1 |
| Two-storey house | Ground + upper floor, gable roof | B1 |
| Small apartment block | 3+ storeys (up to 5–6 with sub-anchor slabs, §8), repeated floor plans | B1 bonus tier (first thing cut), otherwise B2 |
| Residence building | A high-rise inside a residential area with **neighbours that must not be touched** (ultra-hard contracts, protected zones) | B3 / `[OPTIONAL++]` |

B1 needs **2–3 of the first three**; two are enough if the generator is solid.

**Roof shape `[LOCKED: D31]`.** The roof (flat, two-sided, four-sided) is a parameter rolled by the seed, from weights on the archetype tilted by the footprint: a square leans towards four sides, a long house towards two. The number of storeys is not rolled: it is a data choice, one archetype per count.

### 9.4 Rooms and furniture `[LOCKED: five room types]`, `[PROPOSAL: counts]`

Room types: **bathroom, kitchen, living room, bedroom, office**.

Props come from the **Kenney Furniture Kit** (existing assets, imported and tagged). The author's "about thirty" is a ballpark: use as many usable props as fit across the five room types (fewer than 30 is fine, rarity does not matter), with **quality variation** on kitchens, beds and bathrooms where the kit allows it. An indicative breakdown:

| Room | Props (indicative) | Quality variants |
|---|---|---|
| Kitchen | counter/cabinets, fridge, stove, table, chairs | 3 tiers |
| Bathroom | bath/shower, toilet, sink, mirror, cabinet | 3 tiers |
| Bedroom | bed, wardrobe, nightstand, desk, lamp | 3 tiers (bed especially) |
| Living room | sofa, armchair, table, TV unit, shelves | 2 tiers |
| Office | desk, chair, shelves, filing cabinet | 2 tiers |

**Standing `[LOCKED: D30]`.** Each generated house rolls a standing (1 to 3) from the seed. It tilts the quality tier of the furniture in every room: a furniture row's weight is multiplied by a falloff for each tier it is away from the standing, so a house is coherently modest or well off instead of random room by room. A group with a single row is furnished at any standing. The standing can later feed the contract's tier.

**Standing per storey `[LOCKED: D33]`.** A plan that repeats its typical storeys (the apartment block) also gives each typical storey a standing of its own, 1 to 3. It decides how often that storey is stacked at each height (high standings go up) and moves the furniture of its rooms one tier up or down from the house's own standing, so a block can mix modest and well-off storeys. A storey with no standing of its own follows the house's.

Each prop is a Blueprint (`BP_Prop_*`) with tags: **room type**, **quality tier**, **material**, **HP**, **value**, **placement rule** (against wall, in corner, centred, near plumbing…). Rules live in a `DT_FurnishingRules` Data Table; adding a variant is a new row, not new code. The variant count is "filling", as the author says — the pipeline is the work.

### 9.5 Determinism and validation

- `Seed → identical house`, always.
- Every generated structure passes `WouldBeStable` before being accepted; failures re-roll the seed (bounded attempts, then fall back to a known-good seed).
- A debug console command spawns any archetype with any seed (§18.8) — essential for testing and for the defence.

---

## 10. Furniture, salvage and drop zones

### 10.1 Carrying

Furniture is carried with the **grab** (the Week 1 hands tool, simplified), **not** through the inventory (author decision). Carrying is physical: a heavy item slows you (B1 bonus tier) or can be dropped; it can be damaged by being thrown or hit.

### 10.2 Drop zones `[LOCKED]`

Per level, three overlap-based **drop zones** (reusing `ADetectionZone`):

| Zone | Effect | Availability |
|---|---|---|
| **Client** | Marks the item as handed to the client (counts as saved for the report). | B1 |
| **Sell** | Item is sold for its current value. | B1 stretch / B2 |
| **Keep (for me)** | Item goes to the player's storage/home. | **Dropped for now** (author, 2026-10-29: not needed; buying the parcel no longer unlocks it) |

Anything not placed in a zone is destroyed with the house (or lost if the client wanted it).

A limited **warehouse storage** with a soft warning ("you are hoarding: use it or sell it") is **dropped for now** (author, 2026-10-29).

### 10.3 Value of damaged furniture `[PROPOSAL, curve agreed]`

Value drops **by half fast**, then declines slowly: a `UCurveFloat` over the HP ratio:

| HP ratio `h` | Value multiplier |
|---|---|
| 1.00 | 1.00 |
| 0.80 | 0.50 |
| 0.00 | 0.05 |

Used for client-keep items (report), sell zone, and keep zone. It is data (a curve asset), tuned later.

---

## 11. Economy and progression

### 11.1 Principles `[LOCKED]`

- Money buys **tools**, **the parcel**, and **items** (props and structure).
- **Purchases are made where they matter** (on site, in the build context), not in a hub shop. Tools **and the parcel** are bought at the **PC terminal** in the hub `[LOCKED 2026-10-29]` (no new hub place needed; the real purchase flow is B1 bonus tier, a stub is the fallback).
- Demolition is cheap on purpose: you need only a sledgehammer to start and you never buy furniture inside a demolition contract, so **the risk of spending more than you earn is low** — in contrast with a construction game.
- Some **starting money** is given.
- Larger tools and vehicles have **maintenance costs** (details `[TBD]`).
- **Reputation** accompanies money (B3).

### 11.2 Starting values (placeholders)

| Item | Value | Note |
|---|---|---|
| Start money | 500 | Enough to buy one consumable or a cheap tool. |
| Sledgehammer | owned | The starter tool (the author's "just a hammer"). |
| Crowbar | 300 | |
| Jackhammer | 2 500 | Maintenance per use `[TBD]`. |
| Explosive charge | 150 each | Consumable. |
| Parcel | 40 000 | Roughly 15 average contracts. Confirmed as the default (2026-10-29); infinite-money mode and `pb.Money` make it testable, and it can be lowered if the instructor wants to play the full progression. |
| Base payout | ~12 / m² (total demolition) | Partial ×0.6, renovation ×0.5 (aligned to duration). |

All of this is for tuning; the *relations* matter (tools cheap early, parcel a real goal).

### 11.3 Reputation (B3) `[LOCKED: concept]`

- Rises with good scores; falls with accidents.
- **Asymmetric and cushioned** so one mistake does not undo progress: gains larger than losses early, a per-mission loss cap, and a soft floor within each tier (author request: "just tune the drop rate so an error doesn't make you fall too fast").
- Unlocks: bigger houses, more complex conditions, new client types, premium clients (B3).

### 11.4 Optional construction contracts `[OPTIONAL++]`

If rebuilding is added, here are three options for the money model (the author asked for a list):

| Option | How it works | Pros | Cons |
|---|---|---|---|
| **A. Fixed advance** | Client advances a budget; you keep what's left. | Simple, readable. | Little tension with premium choices. |
| **B. Advance + premium upgrades** | Budget covers standard materials; the client pays *more* if you choose premium materials, you pay for the extra cost up-front. | Matches the "premium clients" arc; ties to material quality. | Needs per-material pricing in the report. |
| **C. Quote then negotiate** | Reuses the dialogue/negotiation system (B3). | Strong narrative fit. | Depends on B3 work. |

Default if this is ever built: **B**.

---

## 12. The player's home and construction mode

- **Purpose:** pleasure, expression, progression showcase (author: "it's just for fun"). Not a second economy.
- **Unlock:** after saving enough money, **buy a parcel at the PC terminal** (40 000 by default); the access point in the hub is a **simple map switch** to `Lvl_Home`.
- **Persistence** `[LOCKED 2026-10-29]`: the home must persist across map switches (hub → mission → home). It is saved as pieces (class + grid cell + level + orientation), props (class + transform) and the hotbar inventory, and restored at level load through the shared placement function (the one the generator uses). No layout export tool is needed for this.
- **Mechanics:** exactly the Week 2 build mode (grid, sockets, portance, hotbar/inventory, shop). It stays valid and unchanged; it is simply *no longer the core loop*.
- **Extensions:** garden, workshop, etc. (author: extensible).
- **Furniture:** new from the shop (the "Keep" zone is dropped for now).
- **Paint tool** (B1 bonus tier, 2026-10-31; reworked 2026-11-08 `[LOCKED: D34]`): the **brush** is a slot of the tool belt (the last one), available wherever building is allowed. Colours are **paint buckets** sold in the shop (category "Paint"), one item per colour of a palette data asset; the brush paints a placed piece with the bucket selected in the hotbar. A bucket is infinite for now and can be made finite later. A `PaintIndex` per piece is saved with the home, and the paint stays on the placed piece: a piece taken back and placed again is neutral. Painting belongs to the building side, so a renovation contract that allows building (break partitions, re-place them, repaint) gets it with no extra work; that contract needs build mode inside missions, so it stays beyond B1.
- **Priority:** B1 bonus tier, shipped or cut as one block (parcel purchase + `Lvl_Home` + access point + persistence). Nothing in the must-have loop depends on it.

**Existing systems affected.** In mission levels the build mode, hotbar and shop are **disabled** (the mission uses a separate tool belt, §18.2). They are active only in `Lvl_Home` and the test map.

**What a map allows `[LOCKED: D34]`.** Three independent switches, set per map: the **belt lock** (the player holds only the Hands; the hub), **building** (build mode, hotbar, shop and the paint brush; off in the hub and in missions), and **destruction** (the demolition tools; off in the home). Any mix is valid, so a renovation contract can allow building and demolition together.

---

## 13. Environments

### 13.1 Hub — the warehouse `[LOCKED]`

| Element | Purpose |
|---|---|
| **Warehouse hall** with many **racks** | Atmosphere only: decorative resources, not usable. |
| **Small cosy office** on a mezzanine ("en surplomb") | Contains the **PC** that opens the mission board. Warm light, small desk, lamp. |
| **Big door + truck** | Where the player goes to start a mission. |
| **Build access point** | Entry to the player's own parcel. |

B1: a **navigable v1** (bonus tier): hall, mezzanine office with the PC reached by the kit's stairs, big door with the truck trigger, home access-point trigger; boxes, or a warehouse asset pack if one is found. A one-room blockout (PC + truck trigger) is the fallback. B2: dressed with a warehouse asset pack, lighting, dust. Cosy office vs. cold, vast warehouse is the contrast to nail.

### 13.2 Mission map `[LOCKED: simple]`

- **One building, alone on the map.** The simplest option, by choice.
- Ground: **sand/dirt**.
- **Caution tape** marking the **plot** (`APlot` repurposed as the mission zone). The player is only supposed to demolish within it.
- **Protected zones** (shed, fence, neighbour wall, tree) as volumes when a contract requires.
- Some **foliage and greenery**.
- **Background that feels alive** but is not simulated: either a **city skyline** or **countryside** (open question on the exact choice: backdrops, a distant card/mesh ring, or a light matte; see §17).
- Variation between maps comes from the generated house, the backdrop, time of day and weather (B2/B3), not from new geometry.

### 13.3 Plot changes

`APlot` is currently "bounds + ground reference + owns the structural graph". It becomes the **mission zone**: bounds, caution-tape perimeter, protected-zone registry, the three drop zones, and the contract binding (target / must-stay sets). **The "buy/sell with value" idea in the old `CLAUDE.md` is retired.**

---

## 14. UI and UX

| Screen | Content | Block |
|---|---|---|
| **Mission board** (PC) | 3 offers: m² and estimated time, type, conditions, payment. Pick one. | B1 |
| **Mission HUD** | Tool belt and equipped tool; objective checklist (conditions with status); optional coarse "target demolished %"; warnings/toasts (e.g. "protected zone damaged"). | B1 |
| **Debrief** | Itemised report (§6.5): completion, collateral, furniture, safety, payment, reputation. | B1 |
| **Dialogue / negotiation** | See §16. | B3 |
| **Home build UI** | The existing temporary UMG (HUD, inventory, shop). | existing |

**Feedback policy `[PROPOSAL]`.** Live feedback during the mission is about *state and warnings* (damage states, compromised creaks, a red outline when touching a protected zone). The exact monetary effect of each penalty is revealed in the debrief — it keeps the mission tense and the debrief meaningful. This is easy to flip if testers need more information.

**Controls `[PROPOSAL]`.** The mission uses a tool belt, with the former Push/Pull/Launch collapsed into one "hands" tool:

| Action | Input |
|---|---|
| Move / Look / Jump | ZQSD / Mouse / Space (unchanged) |
| Equip tool | Left / Right arrow keys (slot 0 = Hands); **tool wheel** (B1 bonus tier): hold a key, move the mouse towards a slot, release to equip |
| Primary use | Left Click |
| Secondary use | Right Click (charge: place → detonate; Hands: throw) |
| Hands: grab / carry | Left Click on a prop with Hands equipped |
| Brush: paint | Left Click on a placed piece with the brush equipped (last belt slot, where building is allowed); paints with the bucket selected in the hotbar |
| Interact | E (truck, PC, doors) |
| Controls overlay | H |

Build-mode keys (Tab, B, I, A/E rotation) stay for `Lvl_Home` and `Lvl_DemoTest`. Final key mapping is `[TBD]`; the Enhanced Input mapping contexts (`IMC_Mission`, `IMC_Build`) make it a data change.

---

## 15. Game feel, audio and VFX

### 15.1 Game feel targets `[LOCKED: intent]`

- Demolition is **pleasant**: technically (not too long; realistic enough that attacking from the bottom is hard) and **visually** (staggered cascades, collapse to the sides when overloaded).
- Every hit has weight: **hit pause / camera shake / FOV kick**, per tool.
- Clear **audio language** for state: impact per material, creak for Compromised, thud and rubble for collapse, beep for charges; a dedicated **drill loop** for the jackhammer while the button is held (B1 bonus tier, otherwise it reuses the per-material impact sound).

### 15.2 B1 minimum vs. later

| Feedback | B1 | B2 |
|---|---|---|
| Damage visual | Dynamic-material parameter, simple tint | Crack/dust shader, masks, wetness |
| Dust | Placeholder particles or none | Niagara dust, occlusion, GPU particles |
| Debris | Fracture + lifetime | Optimised pooling, LODs, culling |
| Sound | Basic cues (even placeholders) | Layered per-material sound design |
| Camera | Shake only | Staged camera work in B3 |

### 15.3 Performance posture

60 FPS on the author's development machine is the working target `[TBD: hardware]`. Chaos cost is bounded by: fragment cap, sleeping, short lifetime, per-mesh GC reuse, staggered release, and graph batching.

---

## 16. Narrative and dialogue (B3) `[LOCKED: intent]`

- **Real dialogues** between the client and the player, with **choices**, and **price negotiation** in the Witcher style (a persuasion/slider with a risk of refusal if you push too far).
- A **handful of recurring clients** with fixed names and portraits, short exchanges, no voice acting.
- **Clients evolve** with the player's level: increasingly premium.
- **Environmental storytelling** through the house itself: furniture, belongings, wear and room arrangements tell who lived there (an obvious benefit of the furnishing pipeline).
- Narrative stays **light and systemic**: it supports the sandbox, it does not gate it.

Data sketch `[PROPOSAL]`: `UClientProfile` (name, portrait, temperament, tier range), `UDialogueGraph` (nodes, conditions, price modifiers), a negotiation resolver (offered % vs. client tolerance). Built on the contract data object already in B1, so B3 is an extension, not a rewrite.

---

## 17. Visual direction and rendering plan (B2)

### 17.1 Direction `[LOCKED: grain; PROPOSAL: rest]`

- **Slightly sandy/dusty grain** over the image (author request: "because it's demolition").
- **Semi-realistic, readable** style: clean shapes, **good PBR materials**, light dust. Feasible solo, uses marketplace/Fab-type assets, and sits naturally on the existing greybox pieces.
- Mission map: warm, harsh daylight, dusty air, caution tape colours as the visual anchor.
- Hub: warm cosy office vs. cool vast warehouse.

### 17.2 B2 work packages (prioritised)

1. **Materials/shaders:** per-material PBR; **damage shader** (crack/dirt masks driven by the `Damage` parameter); quality variants (the author notes this is easy B2 work).
2. **Niagara:** dust on hits and collapses, debris puffs, sparks (jackhammer/metal), explosion. Interactive: driven by tool and damage events.
3. **Lighting and post-process:** day lighting, interior lighting in the office; grain/dust post-process; evaluate the engine's global-illumination and virtual-geometry options against the generated-pieces use case (instanced pieces, many fragments) **before committing**.
4. **Profiling and GPU optimisation:** Unreal Insights, `stat GPU`, `stat Chaos`, `stat Game`; instancing for generated pieces; LODs; fragment/draw-call budgets; optional compute-shader exploration (e.g. dust/grain), only if the profile justifies it.
5. **Hub dressing:** warehouse asset pack, racks, lighting.

### 17.3 Open visual question

Background of the mission map: city vs. countryside backdrop (§22).

---

## 18. Technical architecture

### 18.1 Principles (inherited, `[LOCKED]`)

- C++ for the skeleton; Blueprint for data and variants; **never new code for a variant**.
- Single-responsibility components; interfaces over direct references; **no object-specific logic in `APlayerCharacter`**.
- Everything must be explainable orally and in the tech doc.
- AI-assisted code must be understood, tested, explainable, and listed in the AI statement of intent.

Naming follows existing conventions: `A`/`U`/`I` prefixes, short role-descriptive names with **no project prefix**, except framework singletons that clash with engine names (`APlotBuilderGameMode`, `APlotBuilderPlayerController`).

### 18.2 Class map

**Existing (kept)**

| Class | Role going forward |
|---|---|
| `APlayerCharacter` | Movement, camera, Enhanced Input. |
| `UInteractionComponent` | Focus trace → `IInteractable` (PC, truck, doors, pickup). |
| `UPhysicsManipulationComponent` | Becomes the **Hands tool** (grab / carry / throw); push/pull simplification as already planned. |
| `UHotbarComponent`, `UBuildModeComponent` | Home/test-map only. |
| `ABuildablePiece` | Gains a durability component and a material. |
| `APhysicsProp` | Same (furniture). |
| `APlot` | Becomes the mission zone. |
| `UStructuralSupportComponent` | Gains damage coupling, collapse queue, batching. |
| `DA_StructuralRules` | Unchanged; a few new tunables if needed. |
| `ADetectionZone` | Base for drop zones and protected zones. |

**New**

| Class | C++/BP | Role |
|---|---|---|
| `IDamageable` | C++ interface | `ApplyDamage(FDamageInfo)`; implemented by pieces and breakable props. Tools never know concrete classes. |
| `UDurabilityComponent` | C++ | HP, material, damage state, events (`OnDamaged`, `OnStateChanged`, `OnDestroyed`). Drives the visual `Damage` parameter. |
| `UToolbeltComponent` | C++ | Owned/equipped tool, switching, input routing. Separate from the inventory (single responsibility). |
| `ADemolitionTool` | C++ base + `BP_Sledgehammer`, `BP_Jackhammer`, `BP_Crowbar` | One tool = one class; stats from a `UDemolitionToolData` data asset. Traces and calls `IDamageable`. |
| `AExplosiveCharge` | C++ + BP | Placeable physics actor; manual detonation; radial damage + impulse. |
| `UDemolitionToolData` / `DA_DemolitionRules` | Data assets | Tool stats; tool × material matrix; HP table; state thresholds; `c(h)` curve. |
| `UCrushDetectionComponent` | C++ | Impact accumulation on the player, crush events. |
| `UPaintBrushComponent`, `UPaintPalette`, `APaintBucket` | C++ + data | The brush (last belt slot), the plot's palette of colours, a colour as an inventory item (D34). |
| `UDebrisSubsystem` | C++ world subsystem | Fragment cap, oldest-first eviction, sleeping. |
| `UFractureLibrary` (or data table) | C++ + assets | Piece mesh → Geometry Collection mapping. |
| `UContractSubsystem` | C++ game instance subsystem | Offers, active contract, evaluation, payment, progression hooks. |
| `FContractDefinition`, `UContractEvaluator` | C++ struct + pure functions | Evaluator takes the mission result and returns the breakdown — **unit-testable without a level**. |
| `UProgressSave` | `USaveGame` | Money, reputation, owned tools, parcel flag, and the player's home snapshot (pieces as class + cell + level + orientation, props as class + transform, hotbar inventory). |
| `AHouseGenerator` | C++ + BP | Runs the pipeline of §9 at level load. |
| `UHouseArchetype` | Data asset | Template data. |
| `DT_FurnishingRules` | Data table | Props per room, tier, placement rule. |
| `ADropZone` | C++ (from `ADetectionZone`) | Client/Sell/Keep zones with item accounting. |
| `AProtectedZone` | C++ | Volume that records damage for the evaluator. |
| `WBP_MissionBoard`, `WBP_MissionHUD`, `WBP_Debrief` | UMG | UI. |
| `APlotBuilderGameMode`, `APlotBuilderPlayerController` | C++ | Game state flow; HUD creation. |

### 18.3 Data-driven design

The following live in Data Assets/Tables and are tunable without recompiling: tool stats, the tool × material matrix, HP bases and class factors, damage-state thresholds, stability curve `c(h)`, collapse timings, crush thresholds, scoring coefficients, penalty caps, economy numbers, furnishing rules, archetype templates, offer weights, the sub-anchor interval, the paint palette.

### 18.4 Collision

Existing: `GameTraceChannel2` "Interactable". Added `[PROPOSAL]`: **Debris** object type/profile (ignores player movement, ignores build-mode obstruction, collides with world); the tool traces use a **Damageable** query or reuse the Visibility/Interactable channel — decide at implementation time and note it in the tech doc.

### 18.5 Events and coupling

`UDurabilityComponent` broadcasts; `UStructuralSupportComponent`, the evaluator, the HUD and the audio/VFX layer subscribe. The tool does not know the structural graph; the graph does not know the tools; the evaluator only reads events (pieces destroyed, zones hit, items delivered, crush events).

### 18.6 Performance considerations

- **Graph recompute cost:** today it is recomputed on every placement/removal. A cascade of dozens of removals would recompute dozens of times. Solution: a **dirty flag + coalesced recompute** (once per frame or per ~0.25 s).
- **Piece count budget:** target a few hundred pieces per house, with instancing considered in B2.
- **Chaos:** fragment cap, sleep, short lifetime, per-mesh GC reuse, staggered release.
- **Generation:** done at load, off the critical interaction path; the seed makes reload cheap.
- **Profiling plan (B1):** at least one documented capture of a worst-case collapse (`stat Chaos`, `stat Game`, Unreal Insights), with the numbers in the tech doc.

### 18.7 Debug tools (assessment requires at least one visual tool; this offers several)

| Tool | Shows |
|---|---|
| Stability heat-map | Each piece coloured by stability (green→red). Extends `BP_SocketDebugger`. |
| HP overlay | Piece HP, state, material on focus. |
| Damage log | Last N damage events with tool, material, multiplier, result. |
| Collapse wave view | Release time of each piece in the current cascade (numbers/lines). |
| Charge radius | Translucent sphere on placement. |
| Crush accumulator | Bar/number while testing. |
| Evaluator breakdown | Printed on screen at debrief (the same numbers as the report). |
| Fragment counter | Active fragments vs. cap. |

### 18.8 Console commands (proposal)

`pb.SpawnHouse <archetype> <seed>`, `pb.Money <n>`, `pb.GiveTool <name>`, `pb.DamageAll <n>`, `pb.CollapseTest`, `pb.ShowStability 0|1`, `pb.ShowHP 0|1`, `pb.EndMission`, `pb.SetSeed <n>`. They make tuning and the oral defence fast and reproducible.

---

## 19. Production plan

### 19.1 Block 1 — Oct 24 → Nov 13 (~150 h)

Priority order follows **risk**: the collapse feel (pillar) first, then generation, then the contract loop (wide but low-risk).

| Phase | Dates | Hours | Deliverables |
|---|---|---|---|
| **0. Foundations** | Oct 24–25 | ~14 | GDD validated; `IDamageable`, `UDurabilityComponent`, `EDemolitionMaterial`, `DA_DemolitionRules`; durability + material assigned to every existing piece Blueprint; damage visual parameter; HP overlay; `Lvl_DemoTest`. |
| **1. Tools** | Oct 26–29 | ~30 | Toolbelt; explosive charge first; sledgehammer; crowbar; jackhammer; matrix tested against §7.5; hit feedback; **exterior walls back to 300 cm (Oct 29)**. |
| **2. Collapse (core)** | Oct 30 – Nov 2 | ~48.5 core + 4.5 bonus | Damage → stability coupling; collapse queue with staggered release; Compromised state + warning; direction bias; fracture-on-destroy; `UDebrisSubsystem`; crush detection and penalty; graph batching; tuning pass until a demolition feels good; **corner parity fix (Oct 31)**; **stairwell wall link and sub-anchor slab (Nov 1)**. |
| **3. Houses** | Nov 3–8 | ~36 core + 25.5 bonus + 3 buffer | Generator pipeline; 2–3 archetypes; furnishing; validation; mission level with plot, tape, protected zone, drop zones; **slab/stairs visual fix near exterior walls (Nov 5)**. Nov 7–8 also carry the pieces pulled forward from Phase 4 because they do not depend on it: hub blockout and v1, `UProgressSave` skeleton, `Lvl_Home`, home persistence; Nov 8 ends with a 3 h explicit buffer. |
| **4. Contracts and loop** | Nov 9–11 | ~22.5 core + 13.5 bonus | Contract data, board UI, conditions, evaluator, debrief, payment, level travel (wiring the hub and the save built in Phase 3), parcel purchase. |
| **5. Freeze and defence** | Nov 12 (+13) | ~11.5 | Feature freeze after Nov 11; bug fixing; profiling capture; tech doc update; AI statement; packaged build; defence rehearsal. **Nov 13: delivery and oral.** |

These total ~162 h for the core plan plus a 3 h explicit buffer on Nov 8; beyond that, the cut line (§3.2) is the buffer.

**Bonus tier (added 2026-10-29).** The bonus items of §3.2 add ~43.5 h in the plan's conservative units (Phase 2 +4.5, Phase 3 +25.5, Phase 4 +13.5; the 2026-10-31 backlog items account for 9.5 of them), listed with the **[+]** tag in the sprint plans. From Oct 31 (Sprint 1 finished two days early, so Sprint 2 now runs Oct 31 – Nov 8 and Sprint 3 Nov 9–13) the plan needs about 144 conservative hours (97.5 core, 43.5 bonus, 3 buffer) against a capacity of about 150 (14 days × ~7.5 h ÷ the author's observed 0.7× pace): it fits on paper with very little slack, which is why each sprint plan ends with a cut order that removes the bonus items first. Decision gates are judged on the core loop only; the freeze rule (Nov 11 evening) applies to bonus items too.

**Decision gates**

| Gate | Condition | Fallback |
|---|---|---|
| **Oct 31, first thing** | Staggered collapse reads clearly on the test structure. | Drop direction bias and overload; keep hop-delay + warning only. Never block Phase 3. |
| **Nov 4 evening** | The generator produces **one** archetype with valid, furnished, stable houses. | Switch to **2–3 hand-built houses** (existing build mode) with random furnishing and variants; generator becomes B2 work. |
| **Nov 10 evening** | The full loop plays end to end. | Cut conditions to "salvage furniture" and "protected zone"; drop sell zone. |

**Definition of "done" for the week:** playable loop + one documented worst-case profile + one-page-plus AI statement + the updated technical documentation.

### 19.2 Block 2 — after Nov 13 → Feb 19 (capacity `[TBD]`)

In priority order (cut from the bottom): damage shader and PBR materials → Niagara dust/debris/explosion → hub dressing (warehouse) → lighting and post-process → profiling and optimisation pass (incl. optional compute-shader experiment) → quality variants as visual work → third archetype (apartment block) if not already done.

### 19.3 Block 3 — Feb 19 → May 7

Dialogue and negotiation → client profiles and evolution → reputation → Sequencer (intro, truck arrival, debrief) → camera work → sound design and pacing → environmental storytelling in the houses → home extensions (garden, workshop) and home polish; the basic home, parcel purchase and persistence are B1 bonus tier → `[OPTIONAL++]` construction/cleanup contracts and the residence-building contracts.

---

## 20. Risks

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | Collapse does not read clearly or feels random. | Medium | **High** (core pillar) | Graph decides, Chaos decorates; warnings; gate Oct 31; debug wave view. |
| R2 | House generation takes too long. | **High** | High | Hybrid design; one archetype first; gate Nov 4; hand-built fallback. |
| R3 | Graph recomputation spikes during cascades. | Medium | Medium | Batching; profile early in Phase 2. |
| R4 | Chaos fragment cost on large demolitions. | Medium | Medium | Cap, sleep, short life, per-mesh GC; worst-case capture. |
| R5 | Scope creep (dialogue, reputation, home) pulled into B1. | Medium | High | Cut line §3.2; moved-to-later list. |
| R6 | Balance feels bad (tools, payout). | High | Low | Everything in Data Assets; tune last; defence does not require perfect balance. |
| R7 | Existing Week 1–2 systems regress during integration. | Medium | Medium | Keep build mode intact; a regression checklist before each phase end. |
| R8 | Time lost to AI-generated code that is not understood. | Medium | High | Review everything, small steps, log usage for the AI statement. |
| R9 | Reduced availability in B2/B3. | Medium | Medium | Backend is done in B1; B2/B3 plans are prioritised lists. |
| R10 | The bonus tier overbooks the last ten days of B1. | High | Medium | Every sprint plan lists a cut order that removes [+] items first; freeze rule applies to them; gates and the documentation day never take bonus work. |

---

## 21. Quality bar, assessment mapping and AI use

### 21.1 Definition of done for B1

Testable acceptance checks:

- [ ] Each of the four tools damages pieces according to the matrix (spot-checked against §7.5).
- [ ] Removing a ground-floor load-bearing wall of a two-storey house produces a **multi-wave** staggered collapse with a visible warning beforehand; only true bottlenecks go Compromised (T10).
- [ ] Attacking the base of a structure with the wrong tool is clearly slower than the right tool.
- [ ] A single wall falling on the player does **not** trigger a crush event; a multi-piece collapse does.
- [ ] A seed generates the same house every time, and every generated house is stable at spawn.
- [ ] Walls around a stairwell hole stand and fall correctly (T9); a tall stack of storeys with sub-anchor slabs is stable and still collapses correctly (T8).
- [ ] A contract with *salvage furniture* and a *protected zone* can be completed, failed partially, and the debrief itemises exactly why.
- [ ] The loop hub → mission → debrief → hub runs without errors three times in a row, with money and tools persisted.
- [ ] A worst-case collapse is profiled and the numbers are documented.
- [ ] The Week 1–2 build mode still works in `Lvl_Home`/`Lvl_DemoTest`.

Bonus tier (checked only if shipped):

- [ ] *Keep standing* and *forbidden tool* conditions work and are itemised in the debrief.
- [ ] Tools can be bought at the PC terminal and stay owned across levels.
- [ ] A parcel bought at the PC opens `Lvl_Home` from the hub access point; a house built there is still there after hub → mission → home.
- [ ] The hub is navigable (hall, mezzanine office with the PC, door with the truck trigger, access-point trigger).
- [ ] Sticky charge sticks to an aimed surface and falls if its piece is destroyed; the jackhammer drill loop plays only while held; the paint brush, with a bucket selected, colours placed pieces (and the colour survives the home's persistence); the tool wheel equips by direction.

### 21.2 Mapping to the CDC criteria (`CDC_GTech4_Fil_Rouge`)

| CDC criterion | Where this project answers it |
|---|---|
| Functionality | §21.1 checklist. |
| Architecture | §18: single-responsibility components, interfaces, data-driven rules, no logic in the character. |
| Technical mastery | Graph-driven collapse, Chaos replacement strategy, deterministic generation; each can be explained in a sentence. |
| Robustness | Validation and fallbacks (generation, collapse), regression checks. |
| Performance | §18.6 and the profiling capture. |
| Readability | Naming conventions, tech doc, debug tools. |
| Iteration | The decision gates and the documented tuning cycle. |
| Experience | Pillars P1–P3, §14–15. |
| Professional practice | This GDD, the plan, risks, gates. |
| AI | §21.3. |

### 21.3 AI statement of intent (mandatory when AI is used)

The submission must state **where, why and how** AI was used, justify its value and name its limitations (minimum one page for a project). Keep a running log from day 1:

| Date | Area | What AI did | What I verified/changed | Limitation observed |
|---|---|---|---|---|
| 2026-10-24 | Design | Structured the Q&A and drafted this GDD from my decisions. | Every decision in §23 is mine; numbers are placeholders I will tune. | Proposals (tagged) are not decisions. |
| 2026-10-29 | Planning | Audited the GDD against the three sprint plans and re-planned Sprints 2–3 with a bonus tier and cut orders. | I chose what to add, what to drop, and the cut priorities; estimates stay conservative and I track real times. | The audit only sees the documents, not the code. |

Add one row per significant use (generated code, debugging help, doc drafts). Anything generated must be understood well enough to explain at the oral.

### 21.4 Language

The course states that **two projects during the year must be completed entirely in English**. If this capstone is one of them, the technical documentation, AI statement and oral defence must also be in English (§22).

---

## 22. Open questions and assumptions to validate

| # | Item | Default used in this GDD |
|---|---|---|
| Q1 | Block ↔ deadline mapping | Block 2 = Feb 19, Block 3 = May 7. |
| Q2 | What "about thirty pieces" means | Resolved 2026-10-29: a ballpark. Source is the Kenney Furniture Kit; use as many usable props as fit, fewer than 30 is fine, rarity does not matter. |
| Q3 | Where tools are bought | Resolved 2026-10-29: at the PC terminal in the hub, together with the parcel (the "purchases on site" rule applies to building items). |
| Q4 | Live vs. debrief-only penalty feedback | Warnings live; money effect in the debrief (§14). |
| Q5 | Is this capstone one of the two "entirely in English" projects? | Assumed possibly; at least the GDD is in English. |
| Q6 | Mission background | Undecided: city skyline or countryside, a pure backdrop either way. |
| Q7 | Heavy-tool maintenance cost model | Per-use fee `[TBD]`, decided at economy tuning. |
| Q8 | Hardware target for the performance budget | "Author's dev machine, 60 FPS". |
| Q9 | Working title | Undecided. |
| Q10 | Charge occlusion (do walls shield?) | No occlusion in v1 (flat, simplest to debug). |
| Q11 | Overload (mass-based) collapse | B1 stretch; direction bias is the baseline. |
| Q12 | Where the sell zone sits vs. the economy | Pays current value instantly. |
| Q13 | Exact key bindings | §14 proposal. |

---

## 23. Decision log and impact on existing documents

### 23.1 Locked decisions (2026-10-24)

| # | Decision |
|---|---|
| D1 | Game pivots to **demolition contracts**; construction becomes secondary. |
| D2 | Macro loop: offers → pick → site → demolish → client report → hub. |
| D3 | Contract types: total / partial / renovation, plus conditions. Always paid; no hard fail; no time limit. |
| D4 | Three offers at a time, refreshed after each mission; m², type, conditions, payment shown. |
| D5 | Missions 5–10 min, up to 20–25 for very large. |
| D6 | Tools: explosive charge (first), sledgehammer, jackhammer, crowbar; damage until a breaking point per material; manual detonation. |
| D7 | Load-bearing resists; unpredictable collapse and crush = heavy penalty, no death. |
| D8 | Fracture simple, short lifetime; clean-up is optional++. |
| D9 | Legibility over realism; the graph decides, Chaos decorates. |
| D10 | Hybrid generation; five room types; ~30 props with quality variants. |
| D11 | Six materials; matrix as a Data Asset. |
| D12 | Money (tools, parcel, items) + reputation; heavy tools carry maintenance. |
| D13 | Home is for fun, extensible; three drop zones per level; storage limit later. |
| D14 | Furniture is carried with grab, not the inventory. |
| D15 | Crush: one wall fine, many walls plus roof is not. |
| D16 | Damaged furniture loses half its value quickly, then slowly. |
| D17 | Reputation cushioned against fast drops. |
| D18 | Cut line for Nov 13 (§3.2); dialogues, reputation, warehouse, quality visuals are B2/B3. |
| D19 | The CDC per-week documents are not to be relied upon; follow the fil rouge. |
| D20 | Exterior walls go back to 300 cm (visual only) to hide the slab edge between storeys; roof `VerticalOffset` revisited. Scheduled Oct 29 (decided 2026-10-27). |
| D21 | Visual overlap fixes: parity bias at wall corners (Oct 31) and slab/stairs retreat near exterior walls (Nov 5). A material depth offset is only a last resort, to be evaluated in B2 (decided 2026-10-27). |
| D22 | Bonus tier added to Sprints 2–3 (2026-10-29): console commands, window glass piece, damage log and crush display, extended Kenney props, third archetype, heavy-carry slowdown, *keep standing* and *forbidden tool* conditions, furniture value curve and left-inside check, `Tier` offer filter, game-state flow, tool purchase and jackhammer maintenance, parcel purchase, simple home with persistence, hub access point, navigable hub v1. Cut first, in the order given in each sprint plan. |
| D23 | Dropped for now (2026-10-29): the "keep for myself" drop zone, the warehouse storage limit, hub dressing beyond a navigable v1, the mission background. |
| D24 | The player's home persists across map switches; it is saved as pieces (class, cell, level, orientation), props (class, transform) and hotbar inventory, and restored through the shared placement function. No layout export tool is built for this. |
| D25 | Tools and the parcel are bought at the PC terminal; the parcel costs 40 000 by default and infinite-money mode makes the demo possible. |
| D26 | Props come from the Kenney Furniture Kit; "about thirty" is a ballpark. |
| D27 | Sprint estimates stay in the original conservative units (the author's observed pace is ≈ 0.7×); the bonus tier is cut first if time is short. |
| D28 | 2026-10-31: five `Ideas_Backlog.md` items promoted, plus one fix the backlog pointed at but did not schedule. The **stairwell wall link** (direct `WallTop`/`WallBottom` match between stacked walls) is core (Sprint 2, Nov 1): it fixes the original bug of walls bordering a stairwell hole, which the sub-anchor cannot. The **sub-anchor slab** is core (Sprint 2, Nov 1): a reinforced slab variant of the same 20 cm height that resets stability to 1.0 if the chain below is valid. **Sticky charge, jackhammer drill-loop sound, paint tool and tool wheel** are bonus tier, cut before every other bonus item in this order: tool wheel, paint tool, jackhammer sound, sticky charge. The author's own priority order is sub-anchor, sticky charge, jackhammer sound, paint, tool wheel. |
| D29 | 2026-10-31: Compromised means sole support. A Critical load-bearing piece is Compromised only if removing it would make another piece fall below the collapse threshold or lose its anchor chain (dry run of the cascade computation, no destruction), re-evaluated per destruction batch (§7.6). |
| D30 | 2026-11-05: Each generated house rolls a **standing** (1 to 3) from the seed. It tilts the quality tier of the furniture in every room: a furniture row's weight is multiplied by a falloff for each tier it is away from the standing, so a house is coherently modest or well off instead of random room by room. A group with a single row is furnished at any standing. The standing can later feed the contract's tier. |
| D31 | 2026-11-05: The **roof shape** (flat, two-sided, four-sided) is a parameter rolled by the seed, from weights on the archetype tilted by the footprint (a square leans towards four sides, a long house towards two). The number of storeys is not rolled: it is a data choice, one archetype per count. |
| D32 | 2026-11-05: Under every roof there is a **ceiling slab, always a reinforcement slab** (sub-anchor, §8) whatever the storey interval, and a roof tile can rest on a slab edge, so a wide roof on a tall house starts from a stability reset. A roof without a slab (high ceilings) stays a backlog idea. |
| D33 | 2026-11-08: The standing also applies **per storey** for a plan that repeats its typical storeys (the apartment block): each typical storey has its own standing (1 to 3) that decides how often it is stacked at each height and moves its rooms' furniture one tier from the house's own standing (§9.4). |
| D34 | 2026-11-08: Two decisions by the author. **Paint rework:** the brush is the last slot of the tool belt, not a build mode function; colours are paint buckets sold in the shop (category Paint), one item per colour of the plot's palette, infinite for now; the brush paints with the bucket selected in the hotbar. A backpack of paints opened from the bucket stays possible if the colours outgrow the hotbar. **What a map allows:** three independent switches per map, the belt lock, building (painting included) and destruction, so a renovation contract can allow building and demolition together; `Lvl_Home` no longer locks the belt and turns destruction off instead (§12). |

### 23.2 Changes to existing documents

| Document | Change |
|---|---|
| `CLAUDE.md` | Rewritten around this GDD (delivered alongside). The `APlot` buy/sell valuation, the voxel option and the "mission = build" assumption are retired. Its cut-line section still needs aligning with the bonus tier (D22). |
| `README.md` | "Next" paragraph to be updated after Phase 2: demolition replaces "first demolition pass"; Push/Pull/Launch becomes the Hands tool. |
| `Docs/TECHNICAL.md` | Add: durability, tools, collapse queue, debris subsystem, generator, contracts, profiling results; update "Known limitations" (the Anchored-bottom limitation is bypassed by design, not fixed; the stairs-in-wall and visible slab-edge limitations are scheduled for fixes, see `TECHNICAL_limitations.md` for the replacement text). |

---

## 24. Glossary

| Term | Meaning |
|---|---|
| **Portance** | Structural support: the propagated stability value (0–1) of each piece. |
| **Portance graph** | Nodes = placed pieces, links = matching sockets (`UStructuralSupportComponent`). |
| **Socket** | Data declared on a piece Blueprint (type, grid offset, `RoofStep`); not a mesh socket. |
| **Anchor** | Foundation socket giving stability 1.0. |
| **Load-bearing** | Flag on a piece: it transmits stability to what rests on it. |
| **Compromised** | A critical load-bearing piece that is a true bottleneck (removing it would drop another piece below the threshold or cut its anchor chain), about to fail within a random delay, after a warning. |
| **Falling** | State of a released piece simulating as a rigid body before fracturing. |
| **Crush event** | Accumulated impact energy on the player over a threshold → payment penalty and stun. |
| **Target set** | Pieces that must be destroyed for the contract. |
| **Must-stay set** | Pieces that must not be destroyed. |
| **Drop zone** | Overlap volume where carried items are handed to client / sold / kept. |
| **Geometry Collection (GC)** | Chaos asset for fractured meshes. |
| **Archetype** | A hand-authored house template with procedural variation. |
| **Debrief** | The client's report at the end of a mission. |

---

## Appendix A — Data schema sketches (proposals)

```text
FDamageInfo
  float   Amount
  FName   ToolId              // Crowbar, Sledgehammer, Jackhammer, Charge
  FVector HitLocation
  FVector Impulse
  AActor* Instigator

UDemolitionToolData (DataAsset)
  FName   ToolId
  float   BaseDamage
  float   Rate               // hits per second or ticks per second
  float   Range
  float   HitRadius          // 0 = single target
  bool    bContinuous
  float   MaintenanceCost

DA_DemolitionRules (DataAsset)
  TMap<EDemolitionMaterial, float>  MaterialBaseHP
  TMap<EPieceClass, float>          PieceClassFactor
  TMap<FName, FMaterialRow>         ToolMaterialMultiplier   // tool x material
  float   DamagedThreshold         // 0.75
  float   CriticalThreshold        // 0.35
  UCurveFloat* StabilityTransmission   // c(h)
  UCurveFloat* FurnitureValue          // value(h)
  float   HopDelay, Jitter, MaxCascadeSpread
  float   CompromisedDelayMin, CompromisedDelayMax
  float   CrushThreshold, CrushWindow

FContractDefinition
  FName   ClientId
  EContractType Type
  FName   ArchetypeId
  int32   Seed
  float   AreaM2
  TArray<FContractCondition> Conditions
  int32   BasePayout
  int32   ReputationRequired

FContractCondition
  EConditionType Type          // SalvageFurniture, ProtectedZone, KeepStanding, ForbiddenTool
  TArray<FName>  Targets
  FName          Param

FDebriefResult
  float   Completion, Collateral, FurnitureLoss, Safety, Penalty
  int32   Payout
  int32   ReputationDelta
  TArray<FDebriefLine> Lines   // for the UI and the debug print
```

## Appendix B — Test scenarios for `Lvl_DemoTest`

| ID | Scenario | Expected |
|---|---|---|
| T1 | Hit a brick wall with each tool. | Times roughly match §7.5. |
| T2 | Charge next to a plaster partition, a brick wall, a foundation. | Partition and glass gone, brick critical, foundation damaged. |
| T3 | Destroy the load-bearing ground wall of a two-storey test house. | Warning, then multi-wave collapse, upper floor falls to the side, fragments fade. |
| T4 | Stand under a collapsing floor with one wall vs. a full storey. | One wall: no crush; full storey: crush event. |
| T5 | 100 pieces destroyed in one cascade. | One batched graph update, no frame spike beyond budget, fragment cap respected. |
| T6 | Seed 1234 twice. | Identical house, identical furniture. |
| T7 | Contract with salvage + protected zone, partially failed. | Debrief itemises each penalty, money matches the formula. |
| T8 | Single-column tower of 8 storeys, without and with a sub-anchor slab every third slab; then remove a wall under a sub-anchor, and destroy a sub-anchor with the storeys below intact. | Without: the upper storeys fall below the threshold. With: stable. Wall removed: everything above falls in waves. Sub-anchor destroyed: only the part above falls. A sub-anchor with no valid chain below does not reset. |
| T9 | Walls around a stairwell hole with no slab at any level. | Before the fix: yellow or falling. After: stable when the stack below is; the stack above falls when its bottom wall is removed; a wall stacked on an unsupported wall does not hold. |
| T10 | A Critical load-bearing wall that has a second support path, then the same wall with no alternative path. | With the alternative: stays Critical, no Compromised warning. Without: warning, then failure within 0.5–3 s, and the dependants fall in waves. |

## Appendix C — One-paragraph version (for the defence)

*PlotBuilder is a demolition-contract sandbox built on a structural-support graph. Each piece has materials and hit points; tools damage them according to a tool × material matrix; the graph decides what becomes unstable and when, and Chaos handles the visual consequence with short-lived fragments. A hybrid generator builds the houses with the same placement API as the build mode, so they are valid by construction. The client report turns the player's choices into payment, and everything — tools, materials, scoring, economy — is data-driven and debuggable.*
