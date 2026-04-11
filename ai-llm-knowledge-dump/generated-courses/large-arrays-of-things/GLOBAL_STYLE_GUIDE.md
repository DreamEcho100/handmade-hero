# Large Arrays of Things — GLOBAL_STYLE_GUIDE.md

Every source file, lesson file, and code snippet in this course must comply with this guide.

---

## Terminology (STRICT)

Use these terms consistently. Do not substitute synonyms.

| Term | Definition | NOT this |
|---|---|---|
| **Thing** | The generic entity in the pool. Anton's term. | "Entity" (ECS connotation), "Object" (OOP connotation) |
| **Things pool** / **Pool** | The flat array + management data (used[], free list, generations[]) | "Entity manager", "world", "registry" |
| **thing_idx** | Integer index into the things array | "pointer", "handle" (handle implies more than just an index) |
| **thing_ref** | Index + generation pair for safe references | "smart pointer", "weak reference" |
| **Nil** | The zeroth array element — always zero-initialized, always accessible, never a valid thing | "NULL" (NULL crashes; nil doesn't) |
| **Sentinel** | A reserved element that represents "nothing" but can be safely dereferenced | "guard", "dummy node" |
| **Kind** | The type tag enum for a thing (NIL, PLAYER, TROLL, etc.) | "type" (ambiguous with C types), "class" |
| **Slot** | One position in the things array | "bucket", "cell" |
| **Free list** | Intrusive linked list of unused slots available for reuse | "garbage collector", "memory pool" |
| **Generation** | Monotonic counter incremented each time a slot is recycled | "version", "epoch" |
| **Intrusive list** | Linked list where next/prev indices live INSIDE the data struct itself | "external list", "container" |
| **Fat struct** | A single struct that holds ALL possible fields for all entity types | "god object" (pejorative — fat struct is deliberate) |

---

## C Style Rules

### Naming

```c
/* lowercase_snake_case for everything */
typedef int thing_idx;
typedef struct thing thing;
typedef struct thing_ref thing_ref;
typedef struct thing_pool thing_pool;
typedef enum thing_kind thing_kind;

/* Prefixed function names (namespace substitute) */
thing_pool_init();
thing_pool_add();
thing_pool_remove();
thing_pool_get();
thing_pool_get_ref();
thing_is_valid();

/* Enum values: SCREAMING_SNAKE_CASE with prefix */
enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
};

/* Constants: SCREAMING_SNAKE_CASE */
#define MAX_THINGS 1024
```

### Section Headers

```c
/* ══════════════════════════════════════════════════════ */
/*                    Thing Pool                          */
/* ══════════════════════════════════════════════════════ */
```

### Comments

```c
/* JS: Array.splice(i, 1) is O(n) — swap-with-last is O(1)
 * C:  We swap the dead slot with the last used slot and decrement count.
 *     Order doesn't matter for our iteration. */

/* WHY: Indices survive reallocation. If the array's base address changes
 *      (e.g., from realloc or platform relocation), every pointer breaks.
 *      An index is just a number — it doesn't care where the array lives. */
```

---

## Memory Rules

1. **No `malloc` in the pool or game loop.** The pool is `thing things[MAX_THINGS]` — a static array.
2. **Zero initialization is the constructor.** `memset(&pool, 0, sizeof(pool))` resets everything to a valid nil state. No explicit constructors.
3. **Marking unused is the destructor.** `pool.used[idx] = false` — no explicit destructors, no cleanup functions per entity.
4. **All structs designed so all-zeros = valid nil/empty state.** This is a hard design constraint, not a convenience. It enables: instant reset via memset, nil sentinel at index 0, no uninitialized-value bugs.
5. **No hidden allocations.** Every byte of pool memory is visible in the struct definition.

---

## Struct Design Rules

1. **Fixed-size only.** No pointers to heap memory inside the thing struct. No `char *name` — use `char name[32]` if needed. No `thing **children` — use intrusive lists.
2. **POD (Plain Old Data).** The struct can be `memcpy`'d, `memset`'d, written to disk byte-for-byte. No vtables, no function pointers (in the struct itself — function pointer tables are fine externally).
3. **All-zeros is nil.** `thing_kind` NIL = 0. `thing_idx` 0 = nil. `float` 0.0f is valid default. Every field's zero value must be a safe, meaningful "nothing."

---

## Architectural Rules

1. **`things.h`/`things.c` must compile without platform headers.** The pool is platform-independent. It knows nothing about X11, Raylib, windows, or rendering.
2. **Per-lesson standalone binaries (`src/lessons/lesson_NN.c`) are self-contained.** They define ALL types inline — do NOT #include things.h. Each compiles with a single `gcc` command. They show the struct at THAT LESSON'S stage.
3. **The test harness (`test_harness.c`) exercises the FINAL `things.h`/`things.c` API.** It includes `things.h` and tests the full feature set.
4. **Game code (`game.h`/`game.c`) implements a 9-scene selector.** Each scene demonstrates a different variant combination using the pool's variant functions.
5. **Game code depends on `things.h` but not vice versa.** The pool doesn't know it's being used for a game.
6. **Platform code depends on `game.h` but game code doesn't know the platform.** Standard platform layer separation.
7. **Build modes:** `--test` (test harness), `--lesson=NN` (per-lesson binary), `--backend=x11|raylib` (game demo with scene selector).

---

## Particle Laboratory Rules

These rules apply to the Particle Laboratory (Scene 10) and any future advanced modules:

1. **Behavior expressed as data.** Movement patterns are selected by an `archetype_id` field, not by type hierarchies or virtual dispatch. A `switch` on the archetype inside an explicit `for` loop is the correct pattern.
2. **Mutation preferred over hierarchy.** When an entity's behavior changes (e.g., collision changes archetype), REWRITE the archetype field. Do not create a new entity, do not swap vtables, do not allocate.
3. **Parallel arrays for extension.** Per-entity data that doesn't belong in the core `thing` struct goes in parallel arrays indexed by `thing_idx`. This extends pool entities without changing `things.h`.
4. **Explicit iteration for teaching clarity.** The Particle Lab uses raw `for (int i = 1; i < pool->next_empty; i++)` loops, NOT the thing_iter abstraction. Students must see the iteration mechanics directly.
5. **Avoid abstraction layers.** No "particle system" wrapper class. The scene function directly accesses the pool and parallel arrays. Simplicity over encapsulation.
6. **Conditional compilation.** All Particle Lab code is `#ifdef ENABLE_PARTICLE_LAB` guarded. The default build contains zero particle lab code.

---

## Data Layout Laboratory Rules

These rules apply to the Data Layout Toggle Lab (Scene 11):

1. **Identical logic, different access.** All three layout modes (AoS, SoA, Hybrid) MUST produce identical visual output. Only the memory access pattern differs.
2. **Three explicit loops.** Each layout has its own for-loop with identical body logic but different struct/array access. No abstraction hiding the access pattern — students must see the difference.
3. **Performance differences must be measurable.** The HUD shows real timing data. The difference between AoS and SoA should be observable (even if small at 2000 entities on modern hardware).
4. **No premature abstraction.** Do NOT create a "layout adapter" or "data accessor" interface. The raw access patterns ARE the lesson.
5. **Diagrams encouraged for memory representation.** Show how AoS interleaves fields (x,y,dx,dy,x,y,dx,dy,...) while SoA groups them (x,x,x,...,y,y,y,...,dx,dx,dx,...).

---

## Spatial Partition Laboratory Rules

These rules apply to the Spatial Partition Lab (Scene 12):

1. **Algorithms must remain visible.** Each partition mode (Naive, Grid, Hash, Quadtree) is a separate explicit loop. No hidden dispatch, no strategy pattern — the code IS the algorithm.
2. **Identical interaction logic.** All modes produce the same collision results. Only the query structure changes. This isolates algorithmic impact from logic changes.
3. **Educational clarity over optimization.** The quadtree doesn't need to be optimal — it needs to be understandable. Prefer readability over tricks.
4. **Diagrams encouraged.** ASCII diagrams showing grid cells, hash buckets, quadtree subdivision are strongly recommended in the lesson.
5. **O(N²) baseline is MANDATORY.** Students must see the naive approach fail before they appreciate why partitioning exists. Don't skip it.

---

## Memory Arena Laboratory Rules

These rules apply to the Memory Arena Lab (Scene 13):

1. **Allocation intent must be explicit.** Every allocation call must indicate its lifetime category: permanent, level, frame, or scratch. No "just allocate" — the student must always know WHY the memory lives as long as it does.
2. **Lifetimes prioritized over ownership abstractions.** Do not introduce RAII, smart pointers, or ownership types. The lesson is about WHEN memory dies, not WHO kills it. Arena reset is the mechanism, not destructors.
3. **Real malloc/free calls in baseline mode.** The malloc baseline must call actual `malloc` and `free` every frame to demonstrate actual overhead: lock contention, fragmentation, unpredictable latency. Do not simulate the cost — measure it.
4. **Debugging friendliness required.** Peak tracking, per-frame allocation counters, and bytes-used watermarks are mandatory. If you cannot observe the allocator's behavior, you cannot reason about it.
5. **Memory visualization must show arena growth and pool occupancy.** The debug overlay renders arena fill level as a progress bar and pool slots as a grid of occupied/free cells. Students must SEE the memory, not just read numbers.

---

## Infinite Growth Laboratory Rules

These rules apply to the Infinite Growth Lab (Scene 14, Capstone):

1. **System MUST allow observable failure.** The unbounded mode intentionally breaks — the pool fills to capacity and spawning silently fails (returns nil). Do NOT hide this. The student must see the system hit its limit before seeing how stabilization strategies prevent it.
2. **Scalability discussions required.** Every stabilization mode must address the gap between demo code and engine code. Demo code runs for seconds; engine code runs for hours. The lesson must make clear which strategies survive long sessions.
3. **Stability prioritized over peak performance.** The goal is not maximum throughput but sustainable, predictable frame times. A strategy that runs 10% slower but never stutters beats one that averages faster but spikes every 30 seconds.
4. **Long-running survival mindset.** Frame-time budgets, memory pressure, and entity churn must be evaluated over minutes, not milliseconds. If a strategy only looks good for the first 5 seconds, it is not a solution.
5. **Failure is part of the lesson.** The unbounded baseline exists specifically to fail. The budget system exists to show graceful degradation. The streaming system exists to show that "simulate everything" is an unreasonable default. Do not sanitize the failure modes — they are the teaching material.

---

## Lesson Writing Rules

1. **Problem before solution.** Every technique is motivated by a concrete failure scenario before the fix is shown.
2. **ASCII diagram for every data structure operation.** Show the array state before AND after add, remove, reparent, iterate.
3. **Web dev bridge for every new concept.** Map to JS/TS/DOM/React equivalents.
4. **Build-along, not code dump.** Code presented in 5-20 line chunks with explanation after each.
5. **Complete file at each lesson stage.** Student can copy-paste and compile after every lesson.
6. **No forward references.** Lesson N never assumes knowledge from lesson N+1.

---

## Callout Conventions

Use these consistently in lesson markdown:

```markdown
> **Why?** Explanation of a non-obvious C pattern, with JS analogy.

> **Common mistake:** Show the wrong version and explain why it fails.

> **Anton says:** Direct quote or close paraphrase from the transcript.

> **Handmade Hero connection:** Where this technique appears in Casey's series.

> **New technique:** Name, what it does, why, JS equivalent, worked example.

> **Alternative approach:** A different way to implement the same concept, with
> trade-offs. EVERY design decision point must show at least one alternative.
> Show the simpler version first, explain what it lacks, then show the upgrade.

> **Course Note:** Deviation from course-builder.md, with rationale.
```

---

## Variant Exploration Rules

Anton's transcript deliberately presents a **spectrum of choices** at every design point. The course must do the same.

### How to present variants

1. **Build the simplest version first.** The student compiles and tests it.
2. **Show where it breaks.** Concrete scenario — "try removing a middle child. See how you have to walk the list to find prev? That's O(n)."
3. **Introduce the upgrade.** "Add prev_sibling. Now removal is O(1)."
4. **State the trade-off.** "You gain O(1) removal. You pay 4 extra bytes per thing and must maintain prev on every insert/remove."
5. **Let the student choose.** "For your game with < 10 children per entity, the singly-linked version is fine. For a UI system with hundreds of nodes, the doubly-linked version wins."

### Key variant progressions

| ID | Decision point | Variants (simple → advanced) | Lessons |
|---|---|---|---|
| V1 | List linking | Singly → +last_child → +prev_sibling (doubly) → Circular | 04, 05 |
| V2 | Slot liveness | Check `kind != NIL` → Separate `used[]` array | 07 |
| V3 | Type safety | Raw `int` → `typedef int` → `struct { int val; }` | 03 |
| V4 | Data storage | Fat struct → +union → +char buffer → Separate pools | 02, 13 |
| V5 | Child finding | Store first_child → Derive from parent pointers | 05 |
| V6 | Insert strategy | Prepend (O(1)) → Append (needs circular or last_child) | 04, 05 |
| V7 | List naming | Generic (first_child) → Domain-named (first_inventory) → Multiple simultaneous lists | 04 |

### Additional terminology

- **"Your happy cabin"** — Anton's phrase for "your project, your rules." Used when presenting optional enhancements the student may or may not need.
- **"If you can remove an if, that's a good day"** — Anton's heuristic for evaluating data structure choices. If a variant eliminates a conditional branch, it's probably worth the complexity.
- **"Kids and sibs"** — Anton's naming shorthand (children → kids, siblings → sibs) for field alignment. Optional style choice.
