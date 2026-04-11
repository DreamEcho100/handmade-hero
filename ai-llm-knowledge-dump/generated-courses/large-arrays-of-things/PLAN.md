# Large Arrays of Things — PLAN.md

Source material: Anton Mikhailov live coding stream "Avoiding Modern C++" (~2.5 hours)
Course output: `ai-llm-knowledge-dump/generated-courses/large-arrays-of-things/`

---

## What the final system does

A complete, reusable **entity pool** ("things pool") that stores up to 1024 game entities in a single flat array with zero heap allocation. Entities are referenced by integer indices (not pointers), organized into parent-child hierarchies via intrusive linked lists, protected from stale references by generational IDs, and made crash-proof by a nil sentinel at index 0. The pool supports O(1) add, O(1) remove (with free list reuse), O(1) get (with generation check), and linear iteration over living entities. A game demo integrates the pool into a dual-backend (X11/Raylib) application with player movement, enemy spawning, collision removal, and visible slot reuse.

**What makes this course new vs Snake/Tetris/Asteroids:**

| Technique | Where first seen | Why it's new |
|---|---|---|
| Fat struct with kind enum | Lesson 02 | One struct for ALL entity types — no inheritance, no separate types |
| Integer indices (ThingIdx) | Lesson 03 | References survive reallocation, trivially serialize |
| Intrusive singly-linked list | Lesson 04 | Parent-child hierarchies without external containers or heap allocation |
| Intrusive doubly-linked circular list | Lesson 05 | Eliminates boundary edge cases, O(1) last-child access |
| Nil sentinel at index 0 | Lesson 06 | Chain-dereference without null checks — never crashes |
| Slot map (add/remove/get) | Lesson 07 | Full pool management with nil-safe access |
| Intrusive free list | Lesson 08 | Reuse removed slots without scanning the array |
| Generational IDs (ThingRef) | Lesson 09 | Detect stale references to recycled slots |
| Pool iterator | Lesson 10 | Clean iteration skipping dead slots |

---

## Platform configuration

- coord_mode: N/A (lessons 01-10 use a standalone test harness with printf; lessons 11-12 use the platform layer with explicit pixel coordinates for simplicity)
- This is a systems programming course, not a game course — the platform layer only appears in the final demo lessons

---

## Lesson sequence

| # | Title | What gets built | Variants & alternatives shown | Transcript ref |
|---|---|---|---|---|
| 01 | The OOP Problem | Conceptual only — inheritance vs fat struct vs traits | V4-A (fat struct) intro; Ryan Fleury trait-based approach as look-ahead | 00:00–10:00 |
| 02 | Fat Struct and Kind Enum | `thing` struct, `thing_kind` enum, static array, `pool_add()` | V4: fat struct (A) + mention union (B) and char buffer (C) for later; napkin math | 08:00–10:00, 64:00–68:00 |
| 03 | Indices, Not Pointers | `thing_idx` typedef, parent field, reallocation-safe demo | V3: raw int (A) → typedef (B) → struct wrapper (C); serialization benefit; hot reload mention | 16:00–22:00, 28:27–28:46, 101:01–103:02 |
| 04 | Intrusive Linked Lists | `first_child`, `next_sibling`, add/iterate/remove | V1-A: singly-linked only (no prev); V6: prepend vs append; V7: generic vs domain-named lists; Diablo bug; Linux kernel list_head | 22:00–42:56 |
| 05 | Intrusive Trees and Circular Lists | `prev_sibling` added, circular trick, tree traversal | V1: A→B→C→D progression; V5: stored vs derived first_child; "if you can remove an if"; "circular is optional — your cabin" | 42:00–62:18 |
| 06 | The Nil Sentinel and Zero Initialization | nil sentinel, chain-dereference, debug assert | Erlang/Docker crash-recovery analogy; "game reset = just zero"; "don't add std::optional to your array" | 55:00–70:00, 74:35–75:07, 82:00–95:00 |
| 07 | Slot Map: Add, Remove, Get | `thing_pool` struct, full add/remove/get cycle | V2: kind-check (A) shown first, then used[] (B); "deactivated objects are gaps" framing | 70:00–82:00 |
| 08 | Free List for Slot Reuse | `first_free`, intrusive free list in dead slots | How next_sibling is reused in dead things; gap problem motivation | 96:00–98:00 |
| 09 | Generational IDs | `thing_ref`, `generations[]`, `pool_get_ref()` | Our Machinery blog reference; slot map naming | mentioned, 117:02–119:48 |
| 10 | Pool Iterator | `thing_iter`, begin/next/valid/get | "static things things" file-global pattern as alternative to parameter passing | 105:00–116:00 |
| 11 | Game Demo: Window, Sprites, Pool | Platform integration, game_init, game_render | Live recompile benefit of no-pointers | 98:00–110:00 |
| 12 | Game Demo: Movement, Spawning, Removal | WASD, troll spawning, collision, slot reuse visible | All pool features exercised visually | 110:00–140:00 |
| 13 | Architecture Review and What's Next | No new code — full review | V4-B/C/D (union, char buffer, separate pools); SoA; "valley of sadness"; STL composition vs owning; when NOT to use this; performance as adoption inroad | 122:00–140:00 |
| 14 | Particle Laboratory | 1000 particles, 6 archetypes, prediction, mutation, perf metrics | Window: swarm of colored particles, collision changes colors | Advanced Visualization Module (requires `--particle-lab` build flag) |
| 15 | Data Layout Toggle Lab | AoS vs SoA vs Hybrid, runtime toggle, perf metrics | Window: 2000 entities, Tab toggles layout, HUD shows timing differences | Advanced Visualization Module (requires `--data-layout-lab` build flag) |
| 16 | Spatial Partition Lab | Naive O(N²) vs Grid vs Hash vs Quadtree, visual debug overlay, live metrics | Window: 2000 entities, Tab toggles partition mode, grid lines visible | Advanced Visualization Module |
| 17 | Memory Arena Lab | malloc/free vs arena vs pool vs scratch, frame-time stability, memory visualization | Window: entity churn, Tab toggles allocator, HUD shows allocs/frame and memory used | Advanced Visualization Module |
| 18 | Infinite Growth Lab (Capstone) | Unbounded spawn → lifetime/budget/streaming/LOD/recycle/amortized modes | Window: entities spawn forever, Tab toggles stabilization strategy | Capstone Module — integrates all prior labs |

---

## Final file structure

```
large-arrays-of-things/
├── PLAN.md
├── README.md
├── PLAN-TRACKER.md
├── GLOBAL_STYLE_GUIDE.md
├── COURSE-BUILDER-IMPROVEMENTS.md
├── summaries/
│   ├── block-01-foundations.md
│   ├── block-02-lists-and-nil.md
│   ├── block-03-pool-system.md
│   └── block-04-game-demo.md
└── course/
    ├── build-dev.sh                  (--test, --backend=x11|raylib, --lesson=NN, --particle-lab, --data-layout-lab, --spatial-partition-lab, --memory-arena-lab, --infinite-growth-lab)
    └── src/
        ├── things/
        │   ├── things.h          ← thing struct, pool API, + variant functions
        │   └── things.c          ← pool ops + variant implementations (singly-linked, kind-check, no-freelist, no-gen, append)
        ├── test_harness.c        ← standalone main() exercising full API (6 test suites)
        ├── lessons/              ← per-lesson standalone binaries (compile with --lesson=NN)
        │   ├── lesson_02.c       ← fat struct, napkin math
        │   ├── lesson_03.c       ← indices, typedef variants, serialization demo
        │   ├── lesson_04.c       ← singly-linked ONLY (no prev_sibling), O(n) remove
        │   ├── lesson_05.c       ← BOTH singly + circular side-by-side, prepend vs append
        │   ├── lesson_06.c       ← nil sentinel, 5+ crash scenarios, chain-dereference
        │   ├── lesson_07.c       ← BOTH kind-check (V2-A) and used[] (V2-B) get
        │   ├── lesson_08.c       ← free list ON vs OFF comparison
        │   ├── lesson_09.c       ← WITH vs WITHOUT gen IDs comparison
        │   ├── lesson_10.c       ← iterator, complete pool system
        │   ├── lesson_14.c       ← particle laboratory (requires --particle-lab)
        │   ├── lesson_15.c       ← data layout toggle lab (requires --data-layout-lab)
        │   ├── lesson_16.c       ← spatial partition lab
        │   ├── lesson_17.c       ← memory arena lab (requires --memory-arena-lab)
        │   └── lesson_18.c       ← infinite growth lab (requires --infinite-growth-lab)
        ├── game/
        │   ├── game.h            ← 14-scene selector, game_input with scene_key
        │   └── game.c            ← 14 scene implementations + dispatch (scene 10 is #ifdef ENABLE_PARTICLE_LAB, scene 11 is #ifdef ENABLE_DATA_LAYOUT_LAB, scene 12 is #ifdef ENABLE_SPATIAL_PARTITION_LAB, scene 13 is #ifdef ENABLE_MEMORY_ARENA_LAB, scene 14 is #ifdef ENABLE_INFINITE_GROWTH_LAB)
        ├── platform.h            ← minimal platform contract
        └── platforms/
            ├── x11/main.c        ← X11/GLX backend with scene key input
            └── raylib/main.c     ← Raylib backend with scene key input + HUD
    └── lessons/                  ← lesson markdown files (13 lessons, ~66K words)
        ├── 01-the-oop-problem.md
        ├── 02-fat-struct-and-kind-enum.md
        ├── 03-indices-not-pointers.md
        ├── 04-intrusive-linked-lists.md
        ├── 05-intrusive-trees.md
        ├── 06-nil-sentinel-and-zero-init.md
        ├── 07-slot-map-add-remove-get.md
        ├── 08-free-list-for-slot-reuse.md
        ├── 09-generational-ids.md
        ├── 10-pool-iterator.md
        ├── 11-game-demo-window-and-sprites.md
        ├── 12-game-demo-entities-in-action.md
        ├── 13-architecture-review-and-whats-next.md
        ├── 14-particle-laboratory.md
        ├── 15-data-layout-toggle-lab.md
        ├── 16-spatial-partition-lab.md
        ├── 17-memory-arena-lab.md
        └── 18-infinite-growth-lab.md
```

---

## JS → C concept mapping

| C concept | JS equivalent | First lesson | Notes |
|---|---|---|---|
| `typedef int thing_idx` | `number` (array index) | 03 | Type alias for documentation — still just an int |
| `enum thing_kind { ... }` | TypeScript `type Kind = 'player' \| 'troll'` | 02 | Discriminated union tag |
| `struct thing { ... }` | `{ kind: 'player', x: 0, y: 0, health: 100 }` | 02 | Fat object with all possible fields |
| `thing things[MAX_THINGS]` | `const entities = new Array(1024)` | 02 | Pre-allocated fixed-size array |
| `memset(&pool, 0, sizeof(pool))` | `entities.fill(null)` / `Object.assign(state, initial)` | 06 | Zero-init resets everything |
| `thing_idx first_child / next_sibling` | DOM `node.firstChild` / `node.nextSibling` | 04 | Intrusive tree navigation |
| `things[0]` nil sentinel | `undefined` / `null` with `?.` optional chaining | 06 | Safe default instead of crash |
| `pool_get(idx)` returns `&things[0]` on invalid | `entities.get(id) ?? defaultEntity` | 06 | Nil-safe access |
| `thing_ref { index, generation }` | Database row ID (auto-increment prevents confusion on reuse) | 09 | Versioned handle |
| `bool used[MAX_THINGS]` | `Set<number>` tracking which indices are alive | 07 | Liveness flag per slot |
| `first_free` intrusive free list | Connection pool — reuse freed connections | 08 | No scanning required |
| Circular doubly-linked list | N/A (no direct JS equivalent — closest: circular buffer) | 05 | Eliminates boundary edge cases |

---

## Cross-reference table

| Lesson | Connects to |
|---|---|
| 01 — OOP Problem | GPP Ch 14 (Component) — why composition beats inheritance |
| 02 — Fat Struct | Asteroids `SpaceObject` — same fat struct pattern, simpler |
| 03 — Indices | HH entity system — Casey uses handles not pointers |
| 04 — Intrusive Lists | Linux kernel `list_head` — same pattern; GPP Ch 19 (Object Pool) |
| 05 — Trees | GEA Ch 16 (Scene Graph) — parent/child hierarchies |
| 06 — Nil Sentinel | All game courses `= {0}` init — this lesson makes it a PRINCIPLE |
| 07 — Slot Map | Asteroids `compact_pool` — this is the UPGRADE |
| 08 — Free List | GPP Ch 19 (Object Pool) — free list is the standard pool technique |
| 09 — Generational IDs | Our Machinery blog (slot maps); GEA Ch 15 (Entity handles) |
| 10 — Iterator | Platform Foundation — iteration over game entities |
| 11 — Game Demo | Platform Foundation Course — window/input/backbuffer template |
| 12 — Game Demo | Asteroids collision + compact_pool — now with gen IDs and free list |
| 13 — Review | GPP Ch 17 (Data Locality), GEA Ch 15-16, HH entity system |
| 14 — Particle Lab | HH entity system (flat arrays + type tags), GPP Ch 19 (Object Pool) stress test, GEA Ch 15 (entity handles at scale) |
| 15 — Data Layout Lab | GPP Ch 17 (Data Locality), GEA Ch 6 (memory management), HH SoA for SIMD optimization, Anton SoA discussion (125:00–127:00) |
| 16 — Spatial Partition Lab | GPP Ch 20 (Spatial Partition), GEA Ch 12 (collision detection), HH spatial partitioning for entity queries |
| 17 — Memory Arena Lab | GEA Ch 6 (Memory Management), GPP Ch 19 (Object Pool) lifetime extension, HH permanent + transient arena model, Anton "just zero them" philosophy |
| 18 — Infinite Growth Lab (Capstone) | ALL prior lessons — integrates pool system (02-10), game demo (11-12), architecture (13), and all four labs (14-17) into a single stress test; HH entity system overflow handling, GPP Ch 19 (Object Pool) capacity management |

---

## Thing struct evolution across lessons

The struct grows incrementally. Each lesson adds fields:

| Lesson | Fields added | Struct size (approx) |
|---|---|---|
| 02 | `thing_kind kind; float x, y; float health;` | 16 bytes |
| 03 | `thing_idx parent;` | 20 bytes |
| 04 | `thing_idx first_child; thing_idx next_sibling;` | 28 bytes |
| 05 | `thing_idx prev_sibling;` | 32 bytes |
| 06 | (no new fields — nil sentinel is about the POOL, not the struct) | 32 bytes |
| 07 | (no new fields — pool management is external: `used[]`, `next_empty_slot`) | 32 bytes |
| 08 | (no new fields — free list reuses `next_sibling` in dead things) | 32 bytes |
| 09 | (no new fields — `generations[]` is external to the struct) | 32 bytes |
| 11-12 | `float dx, dy; float size; uint32_t color;` (game-specific) | 48 bytes |

---

## Back-of-napkin memory math

(Shown in Lesson 02, referenced throughout)

```
thing struct:        48 bytes (final, with game fields)
MAX_THINGS:          1024
Total pool memory:   1024 * 48 = 49,152 bytes = ~48 KB
used[] array:        1024 * 1  = 1,024 bytes  = ~1 KB
generations[] array: 1024 * 4  = 4,096 bytes  = ~4 KB
                                 ─────────────────────
Total:                           ~53 KB

Even at 10,000 things with 100 properties (float4 each):
10000 * 100 * 16 = 16,000,000 bytes = ~15 MB

Conclusion: don't over-engineer. Just put everything in the struct.
```
