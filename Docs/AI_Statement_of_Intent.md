# AI Statement of Intent

Project: PlotBuilder - Week 1 (Unreal Gameplay Architecture & Physics)
Student: Thomas Picart

Two distinct AI tools were used at different stages of this project: **claude.ai** (conversational chat) for pre-production design and planning, and **Claude Code** (in-terminal/in-IDE assistant) for implementation. They played different roles and have different limitations, so each is detailed separately below, following the same six points.

## Part 1 - claude.ai (pre-production, design & planning)

### Tool

Claude (claude.ai, Claude Sonnet 5 model), conversational chat interface. No line of code in the project was written or edited directly by this tool - it produced reasoning, documentation, diagrams, and instruction text that was later handed to Claude Code for actual file edits.

### Where it was used

- Scoping the concept (voxel vs. modular pieces, how ambitious the core loop should be, the C++/Blueprint split) against both supplied specification documents.
- Designing the technical architecture (classes, separation of responsibilities, unifying Push/Pull/Launch, a neighbor-graph / structural-support concept for Chaos demolition).
- Day-by-day Week 1 planning, cross-checked against the CDC's requirements.
- Setup guide (UE template choice, folder structure, conventions, version control).
- Drafting `CLAUDE.md`, `plan_semaine_1.md`, `setup_jour1.md`, and `recadrage_semaine_1.md`.
- Architecture diagrams (3 diagrams generated to visualize communication between systems).
- Challenging the scope (see Limitations) and iterating on the acquisition/placement system (inventory vs. physical manipulation).
- One-off technical-culture questions (testing practices in game dev, full-Blueprint workflows).

### Why it was used

To move faster from a vague idea to a concrete, documented architecture, with an outside check on consistency against the specification and the real time budget - rather than jumping straight into implementation with no structure in place.

### How it was used

Iterative: every proposal (architecture, scope, design decisions) was explicitly validated before being treated as settled. Factual claims were checked via web search when relevant (e.g. Unreal Engine's current version at project start, rather than relying on potentially outdated knowledge). The diagrams are visualization/communication aids, not formal UML documentation.

### Benefits obtained

- A coherent architecture settled before the first commit, rather than discovered as we went.
- A scope problem caught before it cost development time rather than after (see Limitations).
- Documentation produced alongside development rather than reconstructed after the fact.
- An argued justification available for every technical choice (useful for the oral defense).

### Limitations encountered

- The first pass at the architecture (Week 1) included a full construction system (modular pieces, snapping) that went beyond what Week 1's CDC actually requires - the AI proposed that extension on its own, without flagging it as out of scope. The issue was only corrected after I explicitly asked for a genuinely honest opinion on the gap between the ambition and the real time budget (35h/week); the correction did not come up spontaneously. Documented in `recadrage_semaine_1.md`.
- The time-budget estimates given are rough orders of magnitude, not measurements - presented as such, to be checked against reality.
- Architecture proposals needed validation at every step (creative choices like the construction style or how ambitious the core loop should be): the AI proposes argued options, the final call stays human.
- No empirical verification in the engine: diagrams and estimates were reasoned through, not tested under real conditions by the AI itself.

## Part 2 - Claude Code (implementation)

### Tool

Claude Code (Anthropic), used as an in-terminal/in-IDE coding assistant throughout the week, across multiple working sessions.

### Where it was used

- Writing and editing all C++ classes: the interaction system (`IInteractable`, `UInteractionComponent`), physics manipulation (`UPhysicsManipulationComponent`), the buildable-piece system (`ABuildablePiece`, `APhysicsProp`, `UHotbarComponent`, `UBuildModeComponent`), the plot system (`APlot`), and the trigger (`ADetectionZone`).
- Debugging specific technical issues: an off-center mesh pivot causing held physics objects to spin continuously; a rotation bug in the Build Mode snapping math that only showed up once a piece was rotated; a false "blocked" state caused by floating-point error at the exact boundary between two flush-placed pieces; Chaos Damage Threshold calibration for the destructible wall.
- Guiding the Unreal Editor side of the work step by step (Input Actions, Materials, Fracture Mode / Geometry Collection setup) - Claude Code cannot open or edit the engine's binary `.uasset` files, so every Blueprint, material, and Chaos fracture was built by hand in the editor, following instructions given in chat.
- A scope/feasibility discussion mid-week (continuing the one started on claude.ai, see Part 1): I asked directly whether the planned construction system was realistic within the week's time budget on top of the mandatory fundamentals. The honest answer was no, which led to deferring construction to Week 2, before partially reversing that call a day later once I decided a minimal real placement path was worth demoing early anyway.

### Why it was used

To move faster on C++ systems whose design I had already decided (the architecture and its locked decisions in `CLAUDE.md` are mine, not AI-generated), and to have a debugging partner for physics/geometry bugs that are easy to get subtly wrong (rotation math, floating-point tolerances, force application points).

### How it was used

Conversational, iterative pair-programming: I described what I wanted or the bug I was seeing, Claude Code proposed an approach and the actual code, and I read and double-checked each proposed implementation myself before accepting it - not a "build it all and hope it works" process, but a review at every step, then compiling and testing the change myself in the editor. Nothing was taken on faith - several fixes only happened after I reported back exactly what I observed in Play. For example, the Chaos Damage Threshold array turned out to be indexed one cluster level off from what was first assumed; that was only caught because I tested each index in isolation and reported the results back. Commit messages were drafted by Claude but I reviewed and edited every one before it was actually committed.

Everything produced in C++ this way was purely a time save, not a capability gap on my side: every class and system here is something I could write by hand myself - using Claude Code got me the same result faster, it did not let me build something otherwise out of reach.

### Benefits obtained

- Faster iteration on the C++ skeleton (components, interfaces) than writing it solo, while keeping every architecture decision mine.
- Caught bugs I would likely have spent much longer isolating on my own: the pivot/center-of-mass bug behind the Pull spring's continuous spin, and a rotation-dependent offset bug in the piece-to-piece snapping that only appeared once a placed piece wasn't at identity rotation.
- The scope pushback (started on claude.ai, continued here) was genuinely useful: I was heading toward over-scoping Week 1 with a full construction system, and having that reflected back clearly helped me prioritize the fundamentals the grading actually weighs, before later deciding - on purpose, not by drifting - to bring some of it back in.

### Limitations encountered

- AI cannot touch Unreal's binary assets at all - every Blueprint, Input Action, Material, and the Chaos fracture itself had to be done by hand in the editor, which means that half of the work is not something the AI could verify beyond the instructions it gave me.
- Some Chaos Destruction behavior (which Damage Threshold index maps to which cluster level, why an Anchored piece stopped staying fixed once its parent cluster broke) could not be predicted correctly from general knowledge alone and needed real, hands-on testing in the editor to pin down - the Anchor issue is still unresolved as of this submission (see `Docs/TECHNICAL.md`).
- A couple of process mistakes on the AI's side (committing a revised commit message without showing it to me first, twice) were only caught because I was checking its output rather than assuming it was following the process we had agreed on.

## Overall

Across both tools, the pattern was the same: AI proposals (design, architecture, code, or scope) were treated as drafts to validate, not conclusions to accept - including at least one case (the Week 1 scope) where the AI's own default direction was wrong and only got corrected because I pushed back and asked for a genuinely honest opinion. Everything in this project that was produced with AI assistance was reviewed by me, and where applicable compiled, run, and tested in the editor. I can explain the role of every class and the reasoning behind every technical and architectural choice described in `README.md` and `Docs/TECHNICAL.md`.
