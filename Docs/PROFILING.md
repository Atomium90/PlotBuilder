# PlotBuilder: profiling log

Measured numbers, how they were taken, and what they mean. One dated entry per session; new entries go at the end of section 3. [`TECHNICAL.md`](TECHNICAL.md) links here instead of repeating the numbers. Source of the targets: GDD §18.6 (performance considerations) and Appendix B, test T5.

## 1. Method

**Tools.** Unreal Insights (`Trace.Start` / `Trace.Stop` in the console), `stat unit`, `stat game`, `stat chaos`, and the project's own readouts: `DebugShowFragmentCount 1` (active fragments against the cap), `pb.ShowCollapseQueue 1`.

**Named CPU scopes added for this work** (they show in the Timers panel):
- `Structural.Tick` and `Structural.Recompute` in `UStructuralSupportComponent` (the coalesced graph recompute runs at most once per tick).
- `pb.CollapseAll` around the console command, plus a trace bookmark (`pb.CollapseAll fall` or `break`) at the moment it fires.

**Scenarios.** House `ThreeStorey`, seed 1, built with `pb.SpawnHouse threestorey 1` before each trace (233 pieces).

| Scenario | How | What it stresses |
|---|---|---|
| `fall` | `pb.CollapseAll fall`: every piece released under physics in one frame, no stagger | Chaos, rigid body creation |
| `break` | `pb.CollapseAll break`: every piece and prop takes lethal damage and fractures in place | Fragment spawning, the fragment cap |
| `charge` | A charge at the foot of the ground-floor load-bearing walls (the real path); the cascade then runs wave by wave | The graph recompute, the collapse queue |

`fall` and `break` are stress tests: the graph has almost nothing to recompute when everything leaves at once. Only `charge` exercises the graph.

**Procedure.** Warm-up pass without a trace (the first fracture loads assets and compiles shaders). Then, per scenario: build the house, wait a few seconds, `Trace.Start`, wait 3 s, trigger, wait 15 s (fragment life is 6 s plus the fade), `Trace.Stop`.

**Reading a trace** (Timing Insights).
- A range is selected by dragging on the time ruler at the top of the Timing view, or by clicking one frame in the Frames track and Shift-clicking another. Dragging in the track area only pans.
- The Timers panel then lists the scopes of the selection. Filter on `Structural`, `Chaos` or `UpdateAllPrimitiveSceneInfos`. Right-click a timer, "Find Max Instance", jumps to its longest call.
- Times in the Timers panel are in seconds and add up every thread, so they are not wall time. Frame durations come from the Frames track.

## 2. Machine

Intel Core Ultra 7 155H, 32 GB RAM, NVIDIA GeForce RTX 4070 Laptop GPU (8 GB), driving the display through D3D12 adapter 0 (the log line `Chosen D3D12 Adapter Id = 0`; the Intel Arc iGPU is present and not used). Unreal Engine 5.8, Development Editor. Lumen, Nanite and virtual shadow maps on. All traces in Play In Editor (selected viewport) except where noted.

## 3. Entries

### 2026-11-06: T5, `ThreeStorey` seed 1, 233 pieces

**Frame times** (game frame, from the Frames track):

| Scenario | Worst frames | Back to 60 FPS |
|---|---|---|
| `break`, editor | 340.8 ms (render 335.9 ms), then 57.7 ms (render 55.6 ms) about 3 s later | the frame after each spike |
| `break`, Standalone | 232 ms twice (4.3 FPS), then 50 ms and 52 ms | not recorded |
| `fall`, editor | 189, 47, 185 and 46 ms (5.3, 21.3, 5.4, 21.6 FPS), a small spike of 58 and 56 ms later | frame 1362, with 30 FPS or more in between |
| `charge`, editor | 64 ms at the worst (15.7 FPS); other peaks at 50, 39 and 51 ms; dips of 1 to 5 frames around each peak | between peaks |

On `charge`, about 800 frames were selected: 27.07 s for 1596 frame scopes, so about 17 ms per frame, roughly 59 FPS on average. The game is smooth in play, the dips are not felt.

**Graph recompute** (`Structural.Recompute`):
- `fall` and `break`: one call, 2.4 to 2.9 µs. All the pieces leave in one frame and the batch is one recompute.
- `charge`: 64 calls over about 800 frames for 233 pieces, 331 ms in total, 5.2 ms on average, **17.64 ms at most** (game frame 1368, which ran at 36 FPS right after a two-frame dip to 20 FPS).
- Worst `charge` frame (64 ms, game frame 1649): no recompute in it, and `UpdateAllPrimitiveSceneInfos` takes 1.7 ms on the CPU. Neither explains the frame. **Cause not identified yet.**

**Chaos.** Small everywhere. `break`: 383 `AddActorsToScene` (2.3 ms) and 161 contact events (1.4 ms). `fall`: 150 actors (0.9 ms) and 118 contact events (0.9 ms). `charge`: 0.1 ms on the frame looked at.

**What the `break` and `fall` spikes are.** The longest instance sits inside `RenderGraphExecute`, in `UpdateAllPrimitiveSceneInfos`: one call of about 320 ms. That is the renderer applying every primitive added, removed or moved in the frame (233 pieces gone, hundreds of fragments created). It is not game code and not physics. Standalone is about a third cheaper than the editor (232 ms against 341 ms) but the spike is still there.

**Fragment cap.** The readout reaches 300 of 300 in `break` and in `charge`, every time, and never above. `FragmentGlobalCap` is 300.

**Against the T5 criteria**

| Criterion | Result |
|---|---|
| One batched graph update | Met: at most one recompute per frame, 64 for 233 pieces on `charge`, one on the stress cases. The largest single call, 17.64 ms, is the figure to watch as houses grow. |
| Fragment cap respected | Met. |
| No frame spike beyond the budget | Not met on the stress cases (189 to 341 ms) nor on the real cascade (64 ms at worst, against a working limit of 33 ms for one frame, a threshold proposed here, the GDD gives only 60 FPS on the dev machine, Q8). |

**Likely causes, from the code** (not yet confirmed by a change):
- `UDebrisSubsystem::RegisterDebrisActor` registers a fragment after it was spawned and then evicts the oldest ones above the cap. A burst larger than the cap therefore creates primitives and destroys them in the same frame.
- `ABuildablePiece::Fracture` spawns 3 to 6 fragments per piece, so 233 pieces ask for far more than the cap.

**Candidate mitigations** (none tried, all Block 2 work):
1. Spawn no fragment past the cap, or give the spawn a per-frame budget.
2. Stagger the fracture in bulk cases. The collapse queue already does it in a real cascade.
3. Test fragments without Nanite, without Lumen mesh cards, without dynamic shadows.
4. Bring the recompute below a frame budget on large houses: recompute only the connected part that changed.

**Open items**
- Find what makes the 64 ms frame (game frame 1649): list its largest scopes, and capture the GPU channel (`Trace.Start default,gpu`).
- Repeat `charge` in Standalone and in a packaged build.
- Cost of the room lights at rest (backlog of 2026-11-04): not measured yet.

## 4. Adding an entry

Date and what was measured, the scenario and its house (archetype, seed, piece count), the machine if it changed, then the same tables as above: worst frames and recovery, recompute count and maximum, fragment peak, and the criteria. Say what was not looked at. Keep old entries as they are; a later entry says what changed since.
