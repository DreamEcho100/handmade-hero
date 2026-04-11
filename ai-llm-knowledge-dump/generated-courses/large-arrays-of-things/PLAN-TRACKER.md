# Large Arrays of Things — PLAN-TRACKER.md

## Infrastructure Files

| File | Status |
|---|---|
| Prompt (`build-large-arrays-of-things-course.md`) | [x] Done — includes Alternative Approaches rule, variant tables, 20+ transcript concepts, per-lesson binaries, scene selector |
| `PLAN.md` | [x] Done — lesson table with variants column, updated file tree |
| `README.md` | [x] Done — scene selector table, 3 build modes documented |
| `GLOBAL_STYLE_GUIDE.md` | [x] Done — variant exploration rules, 7 variant progressions, architectural rules for lesson binaries |
| `PLAN-TRACKER.md` | [x] Done |
| `COURSE-BUILDER-IMPROVEMENTS.md` | [x] Done |
| Summaries (block-01 through block-04) | [x] Done |

## Build Verification (all `-Wall -Wextra -Werror -std=c11`)

| Target | Status |
|---|---|
| `./build-dev.sh --test` | [x] 6/6 tests pass |
| `./build-dev.sh --backend=raylib` | [x] Compiles + runs (9-scene selector) |
| `./build-dev.sh --backend=x11` | [x] Compiles + runs (9-scene selector) |
| `./build-dev.sh --lesson=02` | [x] Compiles + runs |
| `./build-dev.sh --lesson=03` | [x] Compiles + runs |
| `./build-dev.sh --lesson=04` | [x] Compiles + runs |
| `./build-dev.sh --lesson=05` | [x] Compiles + runs |
| `./build-dev.sh --lesson=06` | [x] Compiles + runs |
| `./build-dev.sh --lesson=07` | [x] Compiles + runs |
| `./build-dev.sh --lesson=08` | [x] Compiles + runs |
| `./build-dev.sh --lesson=09` | [x] Compiles + runs |
| `./build-dev.sh --lesson=10` | [x] Compiles + runs |
| **Total: 12/12 targets** | **All pass** |

## Source Files — Core Library

| File | Status | Contents |
|---|---|---|
| `things/things.h` | [x] | Pool API + 6 variant functions (singly-linked, kind-check, no-freelist, no-gen, append) |
| `things/things.c` | [x] | All implementations |
| `test_harness.c` | [x] | 6 test suites |

## Source Files — Per-Lesson Standalone Binaries

| File | Compiles | Runs | Key variants demonstrated |
|---|---|---|---|
| `lessons/lesson_02.c` | [x] | [x] | Fat struct, kind enum, napkin math |
| `lessons/lesson_03.c` | [x] | [x] | 3 typedef variants (raw int / typedef / struct wrapper), serialization, realloc demo |
| `lessons/lesson_04.c` | [x] | [x] | Singly-linked ONLY (no prev_sibling), O(n) remove, domain-named lists |
| `lessons/lesson_05.c` | [x] | [x] | BOTH singly + circular side-by-side, prepend vs append, last_child trick |
| `lessons/lesson_06.c` | [x] | [x] | Nil sentinel, 5+ crash scenarios, chain-dereference, debug assert |
| `lessons/lesson_07.c` | [x] | [x] | BOTH kind-check (V2-A) and used[] (V2-B) liveness |
| `lessons/lesson_08.c` | [x] | [x] | Free list ON vs OFF comparison, slot waste demo |
| `lessons/lesson_09.c` | [x] | [x] | WITH vs WITHOUT gen IDs, stale ref detection comparison |
| `lessons/lesson_10.c` | [x] | [x] | Iterator, complete pool API summary |

## Source Files — Game Demo (9-Scene Selector)

| File | Status | Contents |
|---|---|---|
| `game/game.h` | [x] | 9 scenes, scene_key input, per-scene state |
| `game/game.c` | [x] | 9 scene implementations + dispatch + game_hud |
| `platforms/raylib/main.c` | [x] | Number key input, HUD rendering |
| `platforms/x11/main.c` | [x] | Number key input, terminal HUD |
| `platform.h` | [x] | Minimal contract |

## Lesson Markdown Files (13 lessons, ~65,900 words)

| Lesson | Words | Gaps filled | Code alignment |
|---|---|---|---|
| 01 | 4,397 | [x] Ryan Fleury traits | N/A (conceptual) |
| 02 | 3,790 | [x] Union/char buffer/pools preview | [x] Matches lesson_02.c |
| 03 | 4,834 | [x] typedef variants; serialization; hot reload | [x] Matches lesson_03.c |
| 04 | 5,559 | [x] Domain-named lists; kernel; Diablo bug | [x] Matches lesson_04.c |
| 05 | ~6,300 | [x] A→B→C→D; derived first_child; rename callout; kids/sibs | [x] Matches lesson_05.c |
| 06 | 8,017 | [x] Erlang/Docker; std::optional; game reset | [x] Matches lesson_06.c |
| 07 | 4,937 | [x] Kind-check (V2-A); deactivated gaps | [x] Matches lesson_07.c |
| 08 | 4,283 | [x] next_sibling reuse thorough | [x] Matches lesson_08.c |
| 09 | 4,099 | [x] Our Machinery blog archive | [x] Matches lesson_09.c |
| 10 | 3,329 | [x] File-static global alternative | [x] Matches lesson_10.c |
| 11 | 6,057 | [x] Live recompile benefit | [x] Matches game demo |
| 12 | 4,319 | [x] Complete | [x] Matches game demo |
| 13 | 5,878 | [x] Union/buffer/pools/valley/STL/inroad | N/A (review) |

## Particle Laboratory (Advanced Module)

| Milestone | Status |
|---|---|
| Particle data model (parallel arrays in game.h) | [ ] |
| Archetype system (6 archetypes in game.c) | [ ] |
| Prediction system (seek + predictive seek) | [ ] |
| Mutation system (collision → data rewrite) | [ ] |
| Performance overlay (HUD metrics) | [ ] |
| Build flag (--particle-lab in build-dev.sh) | [ ] |
| Platform input (KEY_ZERO/XK_0 ifdef guarded) | [ ] |
| Lesson 14 written | [ ] |
| Final integration test | [ ] |

## Data Layout Toggle Laboratory (Advanced Module)

| Milestone | Status |
|---|---|
| AoS layout implementation | [ ] |
| SoA layout implementation | [ ] |
| Hybrid (hot/cold) layout implementation | [ ] |
| Runtime layout switching (Tab key) | [ ] |
| Sync between layouts on switch | [ ] |
| Performance overlay (timing metrics) | [ ] |
| Build flag (--data-layout-lab) | [ ] |
| Platform input (KEY_L/XK_l + Tab) | [ ] |
| Lesson 15 written | [ ] |
| Final integration test | [ ] |

## Spatial Partition Laboratory (Advanced Module)

| Milestone | Status |
|---|---|
| Naive O(N²) collision baseline | [ ] |
| Uniform grid implementation | [ ] |
| Spatial hash implementation | [ ] |
| Quadtree implementation | [ ] |
| Runtime mode toggle (Tab key) | [ ] |
| Visual debug overlay (grid lines, quadtree boxes) | [ ] |
| Performance overlay (checks, collisions, timing) | [ ] |
| Lesson 16 written | [ ] |
| Final integration test | [ ] |

## Memory Arena Laboratory (Advanced Module)

| Milestone | Status |
|---|---|
| malloc/free baseline implementation | [ ] |
| Linear arena allocator (bump pointer) | [ ] |
| Pool allocator (fixed-size free list) | [ ] |
| Scratch arena (per-frame reset) | [ ] |
| Memory visualization overlay (arena growth, pool occupancy) | [ ] |
| Runtime allocator toggle (Tab key) | [ ] |
| Performance HUD (allocs/frame, bytes used, peak tracking) | [ ] |
| Build flag (--memory-arena-lab) | [ ] |
| Platform input (KEY_M/XK_m + Tab) | [ ] |
| Lesson 17 written | [ ] |
| Final integration test | [ ] |

## Infinite Growth Laboratory (Capstone Module)

| Milestone | Status |
|---|---|
| Unbounded baseline (spawn forever, never delete) | [ ] |
| Lifetime cleanup (entities expire after TTL) | [ ] |
| Budget system (spawner adapts to frame-time constraints) | [ ] |
| Spatial streaming (only simulate nearby entities) | [ ] |
| Simulation LOD (near = full update, far = reduced) | [ ] |
| Pool recycling (free list slot reuse under pressure) | [ ] |
| Amortized work (stagger updates across frames) | [ ] |
| Performance observability overlay | [ ] |
| Build flag (--infinite-growth-lab) | [ ] |
| Platform input (KEY_I/XK_i + Tab) | [ ] |
| Lesson 18 written | [ ] |
| Final integration test | [ ] |

## Transcript Concept Coverage (42 concepts)

All 42 concepts from Anton's transcript are now covered across lessons, lesson binaries, and/or the scene selector. The "kids and sibs" naming tip (the last gap) was added to lesson 05. Full coverage audit passed.
