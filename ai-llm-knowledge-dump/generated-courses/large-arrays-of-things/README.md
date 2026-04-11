# Large Arrays of Things — State Management Without Modern C++

Build a production-quality entity pool system from scratch in C, using the techniques demonstrated by Anton Mikhailov (Media Molecule, *Dreams* PS4). No classes, no STL, no dynamic allocation, no crashes.

---

## Prerequisites

- Completed the **Platform Foundation Course** (window/input/backbuffer infrastructure)
- Completed **Asteroids** or **Tetris** course (familiarity with fixed arrays, `compact_pool`, `GameState`, state machines)
- Comfortable with C pointers, structs, enums, and `memset`

---

## How to build and run

**Per-lesson standalone binaries** (lessons 02-10 — each is self-contained, no platform dependency):

```bash
cd course/
./build-dev.sh --lesson=04 -r     # Build + run lesson 04 (singly-linked lists)
./build-dev.sh --lesson=05 -r     # Build + run lesson 05 (circular lists)
./build-dev.sh --lesson=07 -r     # Build + run lesson 07 (kind-check vs used[])
```

**Test harness** (exercises the final things.h/things.c library):

```bash
./build-dev.sh --test -r
```

**Game demo with 9-scene selector** (press 1-9 to switch scenes):

```bash
./build-dev.sh --backend=raylib -r   # Raylib (recommended)
./build-dev.sh --backend=x11 -r     # X11/GLX
```

### Scene selector (in game demo)

| Key | Scene | What it demonstrates |
|---|---|---|
| 1 | Singly-linked inventory | O(n) child removal — walk the list to find prev |
| 2 | Circular doubly-linked inventory | O(1) removal — prev_sibling gives direct access |
| 3 | Kind-check vs used[] liveness | Both methods compared per-frame |
| 4 | Fat struct info | Struct size comparison (fat vs union vs char buffer) |
| 5 | Free list ON vs OFF | Slot utilization with/without free list recycling |
| 6 | Generational IDs demo | Stale reference detection: with gen IDs (safe) vs without (bug) |
| 7 | Prepend vs append order | Iteration order difference when adding children |
| 8 | Default movement demo | Player + troll spawning + collision (default on startup) |
| 9 | Separate pools | Two pools (player + trolls) with cross-pool collision |
| 0 | Particle Laboratory | 1000 particles, 6 archetypes, prediction, mutation, perf metrics (requires `--particle-lab`) |
| G | Spatial Partition Lab | O(N²) vs Grid vs Hash vs Quadtree — Tab to toggle, debug overlay shows partitions |
| M | Memory Arena Lab | malloc/free vs arena vs pool vs scratch — Tab to toggle allocator |
| I | Infinite Growth Lab | Unbounded spawn → 7 stabilization strategies — Tab to toggle |

### Particle Laboratory

The Particle Laboratory (Scene 10, key `0`) is a **learning laboratory**, not gameplay. It spawns 1000 particles with 6 movement archetypes (Bounce, Wander, Seek, Predictive Seek, Orbit, Drift Noise) and runs them through the same pool system you built in lessons 02-13. Its purpose is to transform theory into observable system behavior: you can watch the pool manage 1000 entities, see behavior dispatched via data (archetype IDs, not inheritance), see mutation in action (collisions rewrite archetype fields), and verify performance under real load via the HUD metrics overlay.

**Why it exists:** Lessons 11-12 exercise the pool with ~20 entities. That proves correctness but not scalability. The Particle Lab proves the flat-array approach works at 1000 entities, 60 FPS, with zero heap allocation. It is the stress test that validates every design decision from the course.

**How to build:**

```bash
./build-dev.sh --backend=raylib --particle-lab -r    # Raylib with Particle Lab enabled
./build-dev.sh --backend=x11 --particle-lab -r       # X11 with Particle Lab enabled
```

The `--particle-lab` flag defines `ENABLE_PARTICLE_LAB` at compile time. Without it, zero particle lab code is included in the binary.

### Data Layout Toggle Laboratory

The Data Layout Toggle Lab (Scene 11, key `L`) demonstrates that **same logic + different data layout = different performance**. It spawns 2000 entities and runs them through three memory layouts — Array of Structs (AoS), Struct of Arrays (SoA), and Hybrid (hot/cold separation) — with identical update logic in each. The only thing that changes is how the data is organized in memory. A performance HUD shows real timing data so you can directly observe the difference.

**Why it exists:** Lesson 13 discusses SoA conceptually. The Particle Lab (lesson 14) proves the pool works at scale. The Data Layout Lab takes the next step: it proves that *how you organize the same data* changes how fast the same code runs. This is the bridge from LOATs to cache locality, data-oriented design, and the intuition behind ECS architectures. When an engine team decides to "rip the struct apart into arrays," this lab shows you exactly why.

**How to build:**

```bash
./build-dev.sh --backend=raylib --data-layout-lab -r    # Raylib with Data Layout Lab enabled
./build-dev.sh --backend=x11 --data-layout-lab -r       # X11 with Data Layout Lab enabled
```

The `--data-layout-lab` flag defines `ENABLE_DATA_LAYOUT_LAB` at compile time. Without it, zero data layout lab code is included in the binary.

**How to use:** Press `L` to enter the Data Layout Lab scene. Press `Tab` to cycle between AoS, SoA, and Hybrid layouts. Watch the timing numbers change in the HUD. The visual output stays identical — only performance differs.

### Spatial Partition Laboratory

The Spatial Partition Lab (Scene 12, key `G`) demonstrates that **O(N²) brute force collision detection is the real bottleneck** — not your CPU speed. It spawns 2000 entities and runs them through four query strategies: Naive all-pairs, Uniform Grid, Spatial Hash, and Quadtree. The only thing that changes is how the simulation finds nearby entities. A visual debug overlay draws the partition structure (grid lines, hash bucket highlights, quadtree subdivision boxes) so you can see the spatial organization directly. A performance HUD shows real-time check counts, collision counts, and frame timing.

**Why it exists:** The Particle Lab (lesson 14) proves the pool works at 1000 entities. The Data Layout Lab (lesson 15) proves memory organization matters. The Spatial Partition Lab takes the next step: it proves that *how you query your data* determines whether your simulation scales. 2000 entities means 4,000,000 pair checks per frame in naive mode — at 60 FPS that is 240 million checks per second. It WILL stutter. Grid partitioning reduces that to roughly 10K-50K checks by only testing entities in the same and neighboring cells. This is the bridge from LOATs to broadphase collision detection, spatial indexing, and the intuition behind every physics engine's world organization.

**How to build:**

```bash
./build-dev.sh --backend=raylib -r    # Always compiled in (no separate build flag)
```

**How to use:** Press `G` to enter the Spatial Partition Lab scene. Press `Tab` to cycle between Naive, Grid, Hash, and Quadtree modes. Watch the check count and frame timing change dramatically in the HUD. The visual output stays identical — only the query strategy and performance differ.

### Memory Arena Laboratory

The Memory Arena Lab (Scene 13, key `M`) demonstrates that **how you allocate memory determines whether your frame times are stable**. It runs an entity churn workload — spawning and destroying hundreds of entities per second — through four allocation strategies: raw malloc/free, linear arena (bump pointer), pool allocator (fixed-size free list), and scratch arena (per-frame reset). A performance HUD shows allocations per frame, bytes used, peak memory, and frame-time variance. A memory visualization overlay renders arena fill level and pool slot occupancy so you can see the allocator's behavior directly.

**Why it exists:** The Particle Lab (lesson 14) proves the pool works at 1000 entities. The Data Layout Lab (lesson 15) proves memory organization matters. The Spatial Partition Lab (lesson 16) proves query strategy matters. The Memory Arena Lab takes the final step: it proves that *how you manage memory lifetimes* determines whether your frame times are smooth or stuttery. malloc/free in a hot loop introduces lock contention, fragmentation, and unpredictable latency. Arenas eliminate all three by making allocation O(1) (bump a pointer) and deallocation O(1) (reset the offset to zero). This is the bridge from LOATs to production engine memory management — the same model Casey uses in Handmade Hero (permanent arena + transient arena) and the same model shipping in AAA engines.

**How to build:**

```bash
./build-dev.sh --backend=raylib --memory-arena-lab -r    # Raylib with Memory Arena Lab enabled
./build-dev.sh --backend=x11 --memory-arena-lab -r       # X11 with Memory Arena Lab enabled
```

The `--memory-arena-lab` flag defines `ENABLE_MEMORY_ARENA_LAB` at compile time. Without it, zero memory arena lab code is included in the binary.

**How to use:** Press `M` to enter the Memory Arena Lab scene. Press `Tab` to cycle between malloc/free, Linear Arena, Pool, and Scratch Arena modes. Watch the allocations-per-frame counter and frame-time variance change in the HUD. The memory visualization overlay shows arena growth and pool slot occupancy in real time.

### Infinite Growth Laboratory

The Infinite Growth Lab (Scene 14, key `I`) is the **capstone** that integrates every concept from the entire course into a single stress test. It spawns entities forever — unbounded — and lets you toggle between seven stabilization strategies: lifetime cleanup, budget caps, spatial streaming, simulation LOD, pool recycling, and amortized work. Each strategy is a real engine technique for keeping a long-running game session stable under infinite pressure.

**Why it exists:** Demo code runs for seconds. Engine code runs for hours. Every prior lab in this course proves a specific technique works at scale — the Particle Lab proves the pool handles 1000 entities, the Data Layout Lab proves memory organization matters, the Spatial Partition Lab proves query strategy matters, the Memory Arena Lab proves lifetime management matters. The Infinite Growth Lab asks the final question: what happens when entities never stop spawning? The answer is that you need ALL of these techniques working together. This is the difference between a tutorial toy and engine infrastructure.

**How to build:**

```bash
./build-dev.sh --backend=raylib --infinite-growth-lab -r    # Raylib with Infinite Growth Lab enabled
./build-dev.sh --backend=x11 --infinite-growth-lab -r       # X11 with Infinite Growth Lab enabled
```

The `--infinite-growth-lab` flag defines `ENABLE_INFINITE_GROWTH_LAB` at compile time. Without it, zero infinite growth lab code is included in the binary.

**How to use:** Press `I` to enter the Infinite Growth Lab scene. Press `Tab` to cycle between Unbounded (baseline), Lifetime, Budget, Streaming, LOD, Recycling, and Amortized modes. Watch the entity count, pool occupancy, and frame timing in the HUD. The unbounded mode will fill the pool and silently stop spawning — that is intentional. The stabilization modes each demonstrate a different strategy for surviving infinite pressure.

**Learning progression:** Large Arrays (data structure) → Particle Lab (scale) → Data Layout Lab (memory layout) → Spatial Partition Lab (query algorithms) → Memory Arena Lab (lifetime management) → **Infinite Growth Lab (capstone — all of the above under infinite pressure)**.

---

## Lesson index

**Design philosophy:** Each lesson presents **multiple approaches** so you understand trade-offs and can choose what fits your project. Anton's mantra: "It could be your happy cabin."

| # | Title | What you learn |
|---|---|---|
| 01 | The OOP Problem | Why inheritance hierarchies fail; fat struct alternative; Ryan Fleury's trait-based approach |
| 02 | Fat Struct and Kind Enum | Your first thing struct, kind enum, static array; napkin math; union/char buffer previewed |
| 03 | Indices, Not Pointers | Why pointers break on reallocation; typedef vs struct wrapper debate; serialization benefit |
| 04 | Intrusive Linked Lists | Singly-linked lists embedded in structs; generic vs domain-named lists; prepend vs append |
| 05 | Intrusive Trees and Circular Lists | Progressive upgrade: singly → doubly → circular; derived last_child trick; "remove an if" |
| 06 | The Nil Sentinel and Zero Initialization | Reserve index 0 as safe "nothing"; chain-dereference; Erlang crash-recovery analogy |
| 07 | Slot Map: Add, Remove, Get | Two liveness approaches: kind-check vs used[] array; full add/remove/get cycle |
| 08 | Free List for Slot Reuse | Intrusive free list recycling removed slots via dead things' next_sibling field |
| 09 | Generational IDs | Detect stale references to recycled slots; Our Machinery blog context |
| 10 | Pool Iterator | Clean iteration skipping dead slots; file-static global pattern as alternative |
| 11 | Game Demo: Window, Sprites, Pool | Integrate pool into a real game; live recompile benefit of no-pointers |
| 12 | Game Demo: Movement, Spawning, Removal | Player movement, enemy spawning, collision, visible slot reuse and gen ID safety |
| 13 | Architecture Review and What's Next | Union/char buffer/separate pools; SoA; "valley of sadness"; when NOT to use this; STL composition vs owning |
| 14 | Particle Laboratory | Parallel arrays, behavior as data, mutation, prediction, performance observability |
| 15 | Data Layout Toggle Lab | AoS vs SoA vs Hybrid memory layouts, runtime toggle, cache locality, performance measurement |
| 16 | Spatial Partition Lab | O(N²) vs Grid vs Hash vs Quadtree spatial queries, visual debug overlay, live performance metrics |
| 17 | Memory Arena Lab | malloc/free vs arena vs pool vs scratch allocation, frame-time stability, memory visualization |
| 18 | Infinite Growth Lab (Capstone) | CAPSTONE — integrates all prior labs: unbounded spawn → 7 stabilization strategies (lifetime, budget, streaming, LOD, recycling, amortized) |

---

## What you'll have by the end

- A **reusable `things.h`/`things.c` library** implementing: flat array pool, nil sentinel, intrusive linked lists, free list, generational IDs, and a clean iterator
- A **working game demo** (dual-backend: X11 + Raylib) that exercises every pool feature
- **Deep understanding** of why Anton/Casey/Ryan Fleury avoid modern C++ for state management, and the concrete techniques they use instead
- **Vocabulary** for concepts you'll encounter in Handmade Hero (days 33+), Game Programming Patterns (Object Pool, Data Locality), and Game Engine Architecture (entity systems)
- **Confidence** that this approach ships AAA games (Dreams PS4) — it's not a hobby technique
