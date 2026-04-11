# Large Arrays of Things — State Management Without Modern C++

## Personalized Course Plan

Build a hands-on, build-along course that teaches the **"Large Arrays of Things" (LOATs)** state management architecture demonstrated by **Anton Mikhailov** (Media Molecule, _Dreams_ PS4) in his live coding stream. The course takes the student from zero to a complete, working entity pool system with intrusive lists, nil sentinels, slot maps, generational IDs, and a live game demo — all in C, no modern C++ features, no STL, no dynamic allocation in hot paths.

---

## Source Material

- **Primary:** YouTube transcript `@_ignore/Avoiding Modern C++  Anton Mikhailov.txt` — ~2.5 hour live coding session. Anton builds a game entity system from scratch using flat arrays, integer indices, intrusive linked lists, nil sentinels, and slot maps. Demonstrates it with a sprite-based game (SDL2 harness, sprite sheet, player + trolls/werewolves).
- **Student profile:** `@_ignore/about-me.md` / `@_ignore/about-me-0.md`
- **Instruction manual:** `@ai-llm-knowledge-dump/prompt/course-builder.md`
- **Student background profile:** `@ai-llm-knowledge-dump/llm.txt`

### What the transcript covers (in order of presentation):

1. **The OOP inheritance problem** (00:00–08:00) — Entity/Player/Monster class hierarchies, why they fail
2. **Fat struct with enum kind** (08:00–10:00) — Single struct, type tag, Ryan Fleury's trait flags
3. **Pointers break on reallocation** (10:00–16:00) — std::vector inside struct, cache thrashing, pointer invalidation
4. **Indices replace pointers** (16:00–22:00) — `ThingIdx` as integer index, survives reallocation, serializable
5. **Intrusive linked lists** (22:00–40:00) — first_child/next_sibling embedded in the struct, no external containers
6. **Intrusive trees** (36:00–44:00) — parent/first_child/next_sibling/prev_sibling for scene graphs
7. **Circular doubly-linked lists** (44:00–55:00) — Eliminate edge cases, last_child = first_child.prev_sibling
8. **Nil sentinel at index 0** (55:00–70:00) — Zero-initialized entry, never crashes, chain-dereference safety
9. **Back-of-napkin memory math** (64:00–68:00) — 10K things x 100 properties = 15MB, don't over-engineer
10. **Slot map: add/remove/get** (70:00–82:00) — Static array, used[] flags, next_empty_slot, nil on failure
11. **Zero initialization as design principle** (82:00–86:00) — memset reset, no constructors/destructors
12. **Operator overload for bool check** (84:00–86:00) — `operator bool` returns `kind != NIL`
13. **Nil-safe chain dereferencing** (86:00–95:00) — `thing.first_child.next_sibling.health` never crashes
14. **Free list for slot reuse** (96:00–98:00) — Intrusive free list inside unused slots
15. **Generational IDs** (mentioned, not fully coded) — Prevent use-after-free by tracking slot generations
16. **Iterator for the pool** (105:00–116:00) — Skip unused slots, range-for via begin/end/operator++
17. **Live game demo** (98:00–140:00) — SDL2 harness, sprite sheet, player movement, troll spawning
18. **SoA vs AoS discussion** (125:00–127:00) — Ripping struct into per-property arrays for cache
19. **Philosophy: "log cabin development"** (02:40–03:10, 122:00–140:00) — Solo/small team, build what you need, don't use STL building blocks

---

## Output Directory

`ai-llm-knowledge-dump/generated-courses/large-arrays-of-things/`

All output goes here. Nothing outside this directory.

---

## Context

**Who is this for:** Mazen (DreamEcho100) — a Cairo-based full-stack web developer (React, Next.js, Node.js, TypeScript) deliberately transitioning into systems programming and game engine development. Currently paused at day 32 of Handmade Hero, building classic game courses in C to develop mechanical intuition in collision, state machines, entity systems, and small game architecture.

**Why this course now:** Mazen's game courses (Snake, Tetris, Asteroids) use fixed arrays and `compact_pool` swap-with-last removal. This course formalizes and EXTENDS those patterns into a complete entity management system that will become the foundation for:

- **DE100 Engine** entity system (when Handmade Hero resumes)
- **Game Programming Patterns** — Object Pool, Data Locality, Component patterns
- **Game Engine Architecture** — entity systems (Ch 15-16)
- All future game courses that need entities beyond simple fixed pools

**Strategic relationship:**

- **Platform Foundation Course** = "how to open a window, handle input, render a backbuffer" (infrastructure)
- **Game courses (Snake, Tetris, Asteroids)** = "how to use fixed arrays and compact_pool for entities" (basic pools)
- **THIS COURSE** = "how to build a production-quality entity pool with intrusive lists, nil sentinels, generational IDs, and safe references" (advanced state management)
- **Game Programming Patterns** = "named patterns for game systems" (Object Pool, Data Locality, Component) — this course provides the concrete implementation those patterns describe abstractly
- **Game Engine Architecture** = "what engine subsystems exist" — this course builds a real entity subsystem
- **Handmade Hero (days 33+)** = Casey builds toward similar entity handling. This course gives Mazen the vocabulary and implementation skills before he encounters them in HH.

**Key difference from other courses:** This is NOT a game course or a book translation. It's a **systems programming course** that builds a reusable data structure (the "things pool") from first principles, justified at every step by concrete problems (pointer invalidation, cache misses, null crashes, loot duplication bugs). The game demo at the end is the VERIFICATION, not the goal.

---

## Key Design Decisions

1. **All code in C11.** No C++ features except where Anton specifically uses them (operator overloading for `operator bool` — translated to an explicit `thing_is_valid()` function in C). No STL, no classes, no constructors/destructors, no templates.

2. **Dual-backend: X11/GLX + Raylib.** Same dual-backend pattern as all other DE100 courses. The entity pool is platform-independent; backends only handle window/input/rendering. Build with `build-dev.sh --backend=x11|raylib`.

3. **Incremental build-along.** The "things pool" grows lesson by lesson: flat array → add/get → nil sentinel → remove with used flags → free list → intrusive linked lists → generational IDs → iterator → game demo. Each lesson compiles and runs.

4. **Standalone + integrated.** The pool system is first built as a standalone `.h`/`.c` library with a test harness (`main()` that exercises add/remove/get). Then integrated into a game demo in the final lessons. This matches the GPP pattern of "minimal version first, integrated version second."

5. **No `malloc` in the pool.** The entire pool is a static array of fixed size. Memory math is shown (back-of-napkin) to justify why this is sufficient. Matches Handmade Hero and Anton's approach.

6. **Zero initialization as architecture.** All structs designed so `memset(0)` produces a valid nil/empty state. No constructors. Lesson on WHY this matters and what it enables.

7. **Cross-references to existing courses.** Each concept maps back to where the student has already seen simpler versions: compact_pool (Asteroids), fixed arrays (all games), state machines (Tetris/Asteroids phases), delta-time loops (all games).

8. **C coding standards:** ALL code MUST follow:
   - `@ai-llm-knowledge-dump/c-pitfalls-for-web-devs.md`
   - `@ai-llm-knowledge-dump/modern-c-programming-safe,-performant,-and-practical-practices.md`

---

## Engineering Principle Priority Order

When rules in this prompt conflict with each other, resolve in this order (highest priority first):

1. **Conceptual clarity** — the student must understand the idea
2. **Correct mental model** — the student's mental picture must match reality
3. **Demonstrable behavior** — the student can build it, run it, verify it
4. **Practical engineering usefulness** — what's taught is actually used in production
5. **Consistency with Anton/Casey/Handmade Hero philosophy** — stay in the "log cabin" mindset
6. **Style guide compliance** — naming, formatting, terminology
7. **Depth targets** — approximate word counts

This prevents optimizing for compliance over insight. A lesson that nails concepts 1-4 but is 1,000 words short is DONE. A lesson that hits the depth target but leaves the student confused is NOT done.

---

## Emergent Insight Rule

If during a lesson a deeper engineering insight naturally appears (e.g., cache locality implications, ownership models, ECS comparison, debugging advantages, serialization benefits), the instructor may temporarily deviate from the lesson plan to explore it **provided**:

1. It strengthens the mental model.
2. It relates directly to the current system.
3. The lesson returns to the planned trajectory afterward.

---

## Pedagogical Approach (MANDATORY)

This section defines NON-NEGOTIABLE rules for how content is written. Every lesson file must follow these rules.

**The student learns by BUILDING, not by reading.** Mazen explicitly prefers "hands-on building over passive watching" and wants "Casey Muratori-style: direct, no-gap explanations, engineering reasoning for every choice, teach me to fish." Each lesson is a build-along where understanding solidifies by typing code, compiling, running tests, and seeing the system grow.

### 0. Instructor Voice Lock

The course is written in the voice of a **senior engine programmer doing a live whiteboard session** — not a textbook, not a generic tutorial. Characteristics:

- **Assumes intelligence, not ignorance.** The student is a professional developer switching domains — explain new concepts fully, but don't condescend about things they already know (loops, functions, basic data structures).
- **Thinks out loud like a code review.** "We could do X here, but that breaks when Y happens. So instead we do Z — here's why." Show the reasoning process, not just the conclusion.
- **Talks about trade-offs like production decisions.** "You lose random access of children. In 10 years of writing scene graphs, I've never needed random child access — it's always 'iterate my kids.'" (Direct Anton paraphrase.)
- **Uses casual, direct language.** "This is garbage. Don't do this." / "Now we're cooking." / "This is your happy little cabin." Match Anton and Casey's conversational engineering tone.
- **Never drifts into textbook energy.** No "In this section, we will explore..." — instead "Here's the problem. Here's how we fix it. Let's build it."

If the tone starts sounding like a Wikipedia article or a university lecture, it's wrong. Re-read Anton's transcript and channel that energy: direct, opinionated, practical, occasionally funny, always engineering-reasoned.

### 1. ASCII Diagrams for Every Data Structure Concept

Not optional. EVERY memory layout, list operation, tree traversal, and pool state gets a diagram:

```
Memory layout of things[] array:
┌───────┬───────┬───────┬───────┬───────┬───────┐
│ [0]   │ [1]   │ [2]   │ [3]   │ [4]   │ [5]   │
│ NIL   │Player │ Troll │(empty)│ Axe   │Potion │
│kind=0 │kind=1 │kind=2 │used=0 │kind=3 │kind=3 │
└───────┴───────┴───────┴───────┴───────┴───────┘
  ^sentinel        ^removed ─── on free list
```

Types of diagrams needed:

- Memory layout (array slots, what's in each slot, which are used/free)
- Intrusive list linkage (first_child → next_sibling → next_sibling → 0)
- Tree structure (parent pointers up, child/sibling pointers across/down)
- Circular list (ring showing prev/next wrapping around)
- Before/after operations (add, remove, reparent — show array state before and after)
- Free list chain (which free slots point to which)
- Generational ID validation (ref.generation vs slot.generation)
- Cache line visualization (AoS vs SoA memory access patterns)

### 2. Mental Model BEFORE Implementation

For every data structure concept: explain WHAT it is, WHY it exists, and what PROBLEM it solves BEFORE showing any code. Build the mental model first.

**WRONG order:**

```
typedef int ThingIdx;
struct Thing { ThingIdx parent; ThingIdx first_child; ThingIdx next_sibling; };
```

**RIGHT order:**

```
When a player picks up an axe, the axe needs to know "who owns me?"
and the player needs to know "what am I carrying?" With raw pointers,
if the array reallocates, every pointer breaks. With std::vector<Item>
inside the entity, you get heap allocations scattered across memory.

Solution: store INTEGER INDICES instead of pointers. An index into
a flat array survives reallocation (the index doesn't change even if
the array's base address moves). And embed the linked list INTO the
struct itself — no separate container needed.

┌─── Player (index 1) ──────────────┐
│ parent: 0 (root)                   │
│ first_child: 4 (Axe)    ─────────────┐
│ next_sibling: 2 (Troll)             │ │
└──────────────────────────────────────┘ │
┌─── Axe (index 4) ──────────────┐◄────┘
│ parent: 1 (Player)              │
│ first_child: 0 (none)           │
│ next_sibling: 5 (Potion) ──────────┐
└─────────────────────────────────────┘ │
┌─── Potion (index 5) ─────────────┐◄──┘
│ parent: 1 (Player)                │
│ first_child: 0 (none)             │
│ next_sibling: 0 (end of list)     │
└───────────────────────────────────┘

Here's what that looks like in C:
typedef int ThingIdx;
struct Thing { ThingIdx parent; ThingIdx first_child; ThingIdx next_sibling; };
```

### 3. Web Dev Bridges (MANDATORY per concept)

Map each concept to web development equivalents the student already knows:

| LOATs Concept                    | Web Dev Equivalent                                                                                         |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| Fat struct with kind enum        | TypeScript discriminated union: `type Entity = { kind: 'player', ... } \| { kind: 'monster', ... }`        |
| Integer index instead of pointer | Array index `entities[id]` instead of object reference — React uses numeric keys in lists                  |
| Intrusive linked list            | DOM `nextSibling` / `parentNode` — the node IS the list element, no separate container                     |
| Nil sentinel at index 0          | JavaScript's `undefined` / nullish coalescing: `entity?.health ?? 0` — you get a safe default, not a crash |
| Slot map add/remove              | `Map<number, Entity>` with `set()` / `delete()` — but our version is a flat array with O(1) everything     |
| Free list                        | Connection pool in databases — reuse freed connections instead of creating new ones                        |
| Generational ID                  | UUID or auto-increment ID in a database — ensures you don't confuse recycled row 5 (v1) with row 5 (v2)    |
| Zero initialization              | `Object.create(null)` / `{}` — start with nothing, add properties explicitly                               |
| compact_pool swap-with-last      | `array.splice(i, 1)` is O(n); swap-with-last is O(1) — same result, 1000x faster                           |
| SoA (struct of arrays)           | Column-oriented database vs row-oriented: PostgreSQL column stores for analytics vs row stores for OLTP    |

### 4. "Why" Before "How" — The Problem-Driven Approach

Every concept must answer WHY before showing WHAT. Structure each concept as:

1. **The problem** — show the broken/slow/unsafe version first
2. **Why it breaks** — concrete failure scenario (pointer dangling, cache miss, null crash, loot duplication)
3. **The fix** — the LOATs technique that solves it
4. **The trade-off** — what you give up (random access of children, slightly more complex insert code)

### 5. Common Misconceptions

Address these explicitly when relevant:

- "Linked lists are slow" — Intrusive lists in a flat array are fast because the nodes ARE the array elements. No pointer chasing to random heap locations.
- "You need smart pointers for safety" — No. Nil sentinels + generational IDs give you safety without reference counting overhead.
- "Flat arrays waste memory for unused properties" — Do the napkin math. 10K entities x 100 properties x 16 bytes = 15MB. Your GPU has 8GB.
- "This only works for games" — No. UI frameworks, note-taking apps, 3D modelers — anything with "lots of things" benefits (Anton mentions Blender, Obsidian).
- "ECS is the answer" — ECS is ONE formalization. LOATs is simpler, gives you 80% of the benefit with 20% of the complexity.

### 6. Alternative Approaches at Every Design Point (CRITICAL)

Anton's transcript deliberately presents a **spectrum of choices** at every stage — not just the final answer. The course MUST do the same. At every design decision point, show:

1. **The simpler version first** — let the student build it, compile it, test it
2. **The problem with the simpler version** — concrete scenario where it fails or is insufficient
3. **The upgrade** — the next variant that fixes it
4. **The trade-off** — what you gain AND what you lose

Use `> **Alternative approach:**` callouts to present each variant. The student should be able to choose which variant to use in their own project based on their needs.

**Key variant progressions that MUST appear in lessons:**

| Decision point | Variants (simplest → most advanced) | Lesson |
|---|---|---|
| List linking | A. Singly-linked → B. +last_child → C. +prev_sibling → D. Circular | 04, 05 |
| Slot liveness | A. Check `kind != NIL` → B. Separate `used[]` array | 07 |
| Type safety | A. Raw `int` → B. `typedef int` → C. `struct { int val; }` wrapper | 03 |
| Data storage | A. Fat struct → B. +union → C. +char buffer → D. Separate pools | 02, 13 |
| Child finding | A. Store first_child → B. Derive from parent pointers dynamically | 05 |
| Insert strategy | A. Prepend (O(1)) → B. Append (needs last_child or circular) | 04, 05 |
| List naming | A. Generic (first_child/next_sibling) → B. Domain-named (first_inventory/next_inventory) → C. Multiple simultaneous lists | 04 |

> **Anton says:** "You don't got to do it. You can have a last child and have one more member. It could be your happy cabin." — Every variant is valid. Show them all, let the student choose.

### 7. Depth Targets (Not Word Counts)

| Type                                | Depth target                       | Examples                                                  |
| ----------------------------------- | ---------------------------------- | --------------------------------------------------------- |
| Core concept lesson (new technique) | ~8,000 words of expert explanation | Intrusive lists, nil sentinel, slot map, generational IDs |
| Foundation lesson (setup, review)   | ~5,000 words of expert explanation | OOP problems, fat struct, indices vs pointers             |
| Lab lesson (advanced module)        | ~6,000 words of expert explanation | Particle lab, data layout lab, spatial partition lab       |
| Capstone lesson                     | ~10,000 words of expert explanation | Infinite growth lab                                      |

**Prefer depth over length.** These are depth targets, not token quotas. A 7,000-word lesson that nails every concept with precision beats a 10,000-word lesson padded with repetition. The ASCII diagrams, before/after operations, and worked examples naturally produce the required depth — don't inflate beyond what the concept demands.

> **Note:** The original targets (6K/4K/5K/3K) were too low. During the course rewrite, lessons at those lengths consistently lacked sufficient JS bridges, ASCII diagrams, and step-by-step explanations for a web developer transitioning to systems programming. The updated targets above reflect what was actually needed.

### 7. Build-Along Methodology

Each lesson is a build-along, NOT a code dump. The student builds incrementally:

1. **Start from the previous lesson's code** — "Open `things.h` from the previous lesson..."
2. **Add code in chunks of 5-20 lines** — never dump the whole file at once
3. **After EACH chunk:**
   - Explain every line — what it does, why, what the memory state looks like now
   - ASCII diagram showing the pool state after this operation
   - "Build and run" — compile + test command + expected output
4. **100% code coverage** — every line of the final source code must appear in some lesson
5. **Test checkpoints** — at least 3 "run and verify" moments per lesson (add a thing, remove a thing, check nil behavior, etc.)

### 8. Lesson Quality Standards (MANDATORY — learned from the course rewrite)

Every lesson MUST have ALL of the following. No exceptions:

1. **`## What we're building`** — one paragraph, what the student will have at the end
2. **`## What you'll learn`** — bullet list of concepts
3. **`## Prerequisites`** — which lessons must be completed first
4. **Numbered `## Step N — [descriptive name]` sections** — the build-along progression
5. **5-20 lines of code per step** followed by line-by-line explanation
6. **"Build and run" compile checkpoint** after every step that adds code
7. **Complete compilable file at the end** — student can copy-paste and compile
8. **`> **Why?**` callout for EVERY C concept** with JavaScript analogy
9. **ASCII diagram for every data structure operation** (before + after state)
10. **`## Common mistakes` section** with wrong code and explanation
11. **`## Exercises` section** with 3-5 hands-on tasks
12. **`## Connection to your work` section** mapping to GPP/GEA/HH
13. **`## What's next` section** previewing the next lesson
14. **Platform/backend code NOT deeply explained** — say "copy from Platform Foundation Course"

**Minimum depth targets:**
- Core concept lessons: ~8,000 words
- Foundation lessons: ~5,000 words
- Lab lessons: ~6,000 words
- Capstone lesson: ~10,000 words

These targets were validated during the course rewrite. The original targets (6K/4K/5K/3K) were too low — lessons at those lengths lacked sufficient JS bridges and ASCII diagrams for a true beginner.

### 9. On-Screen HUD Text (MANDATORY for all scenes)

Every scene (lesson demos + labs) MUST draw informative real-time text on screen showing what's happening. This text is rendered into the CPU pixel buffer using a `FONT_8X8[128][8]` bitmap font (same pattern as the Platform Foundation Course's `draw-text.c`).

**Implementation:** Add `FONT_8X8` array and `draw_text_buf()` function directly in `game.c`. The HUD is drawn AFTER scene rendering, before GPU upload. Both backends (Raylib + X11) show identical output because the text is in the pixel buffer.

**HUD content per scene must include:**
- Scene name and key hint (how to switch scenes)
- Entity/particle count and pool utilization
- Mode name (for labs with toggleable modes)
- Real-time metrics (frame time, collision checks, memory used, etc.)
- Mode-specific stats (archetype counts, layout name, partition type, etc.)

This ensures the student always sees what the pool system is doing — not just the visual output.

---

## Scene Selector System

The game demo contains 14 scenes accessible via keyboard:

| Key | Scene | Type |
|---|---|---|
| **1-9** | Lesson demonstration scenes | Core demos |
| **P** | Particle Laboratory (Scene 10) | Lab |
| **L** | Data Layout Toggle Lab (Scene 11) | Lab |
| **G** | Spatial Partition Lab (Scene 12) | Lab |
| **M** | Memory Arena Lab (Scene 13) | Lab |
| **I** | Infinite Growth Lab — Capstone (Scene 14) | Lab |
| **Tab** | Cycle modes within active lab | Control |
| **R** | Reset current scene | Control |

**Implementation:** `game_input.scene_key` receives the scene number. `game_update` dispatches to `sceneN_init` on switch. Each scene has init/update/render/hud functions. Labs use `Tab` (via `game_input.tab_pressed`) to cycle their internal modes.

**Labs are always compiled in.** No conditional compilation flags. The student runs `./build-dev.sh --backend=raylib -r` and ALL scenes are available. This was changed from the original design (which used `#ifdef ENABLE_*_LAB` guards) because requiring build flags to access labs was bad UX — the student should just press a key.

---

## Variant Functions in things.h/things.c

The pool library provides ALTERNATIVE implementations of key operations for side-by-side comparison in scenes and lessons:

| Function | What it demonstrates | Used in |
|---|---|---|
| `thing_add_child_singly()` | Singly-linked prepend (no prev_sibling) | Scene 1, lesson_04.c |
| `thing_remove_child_singly()` | O(n) removal via list walk | Scene 1, lesson_04.c |
| `thing_add_child_append()` | Append to end of circular list | Scene 7, lesson_05.c |
| `thing_pool_get_by_kind()` | Liveness check via kind != NIL (no used[]) | Scene 3, lesson_07.c |
| `thing_pool_add_no_freelist()` | Add without free list (shows slot waste) | Scene 5, lesson_08.c |
| `thing_pool_get_no_gen()` | Get without generation check (shows stale ref bug) | Scene 6, lesson_09.c |

These exist so the student can COMPARE approaches in the same binary. The "correct" versions (`thing_add_child`, `thing_unlink_child`, `thing_pool_get`, `thing_pool_get_ref`) are the primary API.

---

## Anti-Drift Systems

### 1. `GLOBAL_STYLE_GUIDE.md` (NON-OPTIONAL)

Every source file, lesson file, and snippet must comply. Defines:

**Terminology (STRICT):**

- "Thing" = the generic entity in the pool (Anton's term — not "entity" to avoid ECS connotations)
- "Things pool" / "Pool" = the flat array + management data (used[], free list, generation counters)
- "ThingIdx" = integer index into the things array (NOT a pointer)
- "ThingRef" = index + generation pair for safe references
- "Nil" = the zeroth array element, always zero-initialized, never a valid thing (NOT NULL — nil is a value, null is a crash)
- "Intrusive list" = linked list where the next/prev pointers are embedded in the data struct itself
- "Sentinel" = a reserved element that represents "nothing" but can be safely accessed
- "Kind" = the type tag for a thing (enum: NIL, PLAYER, TROLL, GOBLIN, etc.)
- "Slot" = one position in the things array
- "Free list" = intrusive linked list of unused slots available for reuse
- "Generation" = monotonic counter incremented each time a slot is recycled

**C Style Rules:**

```c
// lowercase_snake_case for types and functions
typedef struct thing thing;
struct thing { ... };

typedef int thing_idx;

typedef struct thing_ref thing_ref;
struct thing_ref { thing_idx index; uint32_t generation; };

// Prefixed function names (namespace substitute)
thing_pool_add();
thing_pool_remove();
thing_pool_get();
thing_is_valid();
intrusive_list_push_front();
intrusive_list_remove();

// Section headers
/* ══════════════════════════════════════════════════════ */
/*                    Thing Pool                          */
/* ══════════════════════════════════════════════════════ */
```

**Memory Rules:**

- No `malloc` in the pool or game loop — ever
- Static arrays: `thing things[MAX_THINGS]`
- Zero initialization: `memset(&pool, 0, sizeof(pool))` resets everything
- No constructors, no destructors
- All structs designed so all-zeros = valid nil/empty state

### 2. `QUALITY_BENCHMARKS.md`

3 **golden reference lessons** written to ideal quality. Every review cycle compares new lessons against these:

1. **Lesson 04: Intrusive Linked Lists** — Tests: ASCII diagrams (list linkage, before/after insert/remove), mental model (DOM sibling analogy), code build-along quality, worked numerical example
2. **Lesson 06: Nil Sentinel** — Tests: problem-driven approach (5 different crash scenarios that nil prevents), chain-dereference example, zero-initialization philosophy depth
3. **Lesson 09: Game Demo — Spawning Entities** — Tests: integration quality (pool + platform + rendering), build-along from empty game_init to working sprites, visual checkpoints

**Quality dimensions measured per review:**

| Dimension          | Ideal (benchmark level)                                      | Warning Signs                             |
| ------------------ | ------------------------------------------------------------ | ----------------------------------------- |
| **Mental Model**   | ASCII diagram + plain-language explanation BEFORE any code   | Jumps straight into struct definition     |
| **Problem First**  | Concrete broken example BEFORE the fix                       | Solution presented without motivation     |
| **Build-Along**    | 3-5 incremental code chunks per concept, compile checkpoints | Whole file dumped at once                 |
| **Web Dev Bridge** | Concrete mapping to JS/TS/React/DOM that illuminates         | Missing, or forced analogy                |
| **ASCII Diagrams** | Pool state shown before AND after every operation            | Text-only explanation of spatial concepts |
| **Failure Modes**  | Concrete bugs with "here's what happens" scenarios           | Vague "be careful" warnings               |
| **Cross-refs**     | 1-3 links to existing courses per lesson                     | >5 (spam) or 0 (isolated)                 |
| **Depth**          | Covers Anton's full explanation + adds personalized value    | "See the video for details"               |

### 3. Rolling Summaries (`summaries/`)

Every 3 lessons, produce a summary:

```
summaries/
  block-01-foundations.md    (lessons 01-03)
  block-02-lists-and-nil.md  (lessons 04-06)
  block-03-pool-system.md    (lessons 07-09)
  block-04-game-demo.md      (lessons 10-12)
```

Each contains:

- Concepts introduced so far (cumulative)
- Pool system state (which features exist, what compiles)
- Data structure state (what `thing` struct looks like at this point)
- Key code patterns established
- Open questions

### 4. Review Protocol

**Every 3 lessons** — local review:

1. Guideline alignment (GLOBAL_STYLE_GUIDE.md compliant?)
2. ASCII diagrams present for EVERY data structure operation?
3. Problem-before-solution ordering respected?
4. Web dev bridges present and helpful?
5. C translation correct (no C++ leaking)?
6. Code compiles and tests pass on both backends?
7. Quality benchmark comparison

**After each phase** — full system audit:

1. Concepts in correct dependency order?
2. Any contradictions across lessons?
3. Thing struct accumulates correctly? (no missing fields)
4. Terminology consistent?
5. Rolling summary up to date?

---

## Example Output Structure

```
ai-llm-knowledge-dump/generated-courses/large-arrays-of-things/
├── README.md
├── PLAN.md
├── PLAN-TRACKER.md
├── GLOBAL_STYLE_GUIDE.md
├── QUALITY_BENCHMARKS.md
├── COURSE-BUILDER-IMPROVEMENTS.md    (Phase 4 retrospective)
├── summaries/
│   ├── block-01-foundations.md
│   ├── block-02-lists-and-nil.md
│   ├── block-03-pool-system.md
│   └── block-04-game-demo.md
├── course/
│   ├── build-dev.sh                  (--test, --backend=x11|raylib, --lesson=NN, -r)
│   ├── src/
│   │   ├── things/                   (the reusable pool library)
│   │   │   ├── things.h              (thing struct, pool API + variant functions)
│   │   │   └── things.c              (all pool ops + variant implementations)
│   │   ├── test_harness.c            (6 test suites exercising full API)
│   │   ├── lessons/                  (per-lesson standalone binaries)
│   │   │   ├── lesson_02.c           (fat struct, napkin math)
│   │   │   ├── lesson_03.c           (indices, 3 typedef variants, serialization)
│   │   │   ├── lesson_04.c           (singly-linked ONLY, O(n) remove, domain-named)
│   │   │   ├── lesson_05.c           (BOTH singly + circular, prepend vs append)
│   │   │   ├── lesson_06.c           (nil sentinel, 5+ crash scenarios)
│   │   │   ├── lesson_07.c           (BOTH kind-check and used[] get)
│   │   │   ├── lesson_08.c           (free list ON vs OFF comparison)
│   │   │   ├── lesson_09.c           (WITH vs WITHOUT gen IDs)
│   │   │   ├── lesson_10.c           (iterator, full system)
│   │   │   ├── lesson_14.c           (particle laboratory)
│   │   │   ├── lesson_15.c           (data layout toggle lab)
│   │   │   ├── lesson_16.c           (spatial partition lab)
│   │   │   ├── lesson_17.c           (memory arena lab)
│   │   │   └── lesson_18.c           (infinite growth lab — capstone)
│   │   ├── game/
│   │   │   ├── game.h                (14-scene selector, game_input with scene_key + tab_pressed, all lab state structs)
│   │   │   └── game.c                (~3,400 lines — 14 scene implementations + dispatch + FONT_8X8 bitmap HUD, all scenes always compiled in, no #ifdef guards)
│   │   ├── platform.h                (minimal platform contract)
│   │   └── platforms/
│   │       ├── x11/main.c            (X11/GLX + scene key input)
│   │       └── raylib/main.c         (Raylib + scene key input + HUD)
│   └── lessons/
│       ├── 01-the-oop-problem.md
│       ├── 02-fat-struct-and-kind-enum.md
│       ├── 03-indices-not-pointers.md
│       ├── 04-intrusive-linked-lists.md
│       ├── 05-intrusive-trees.md
│       ├── 06-nil-sentinel-and-zero-init.md
│       ├── 07-slot-map-add-remove-get.md
│       ├── 08-free-list-for-slot-reuse.md
│       ├── 09-generational-ids.md
│       ├── 10-pool-iterator.md
│       ├── 11-game-demo-window-and-sprites.md
│       ├── 12-game-demo-entities-in-action.md
│       ├── 13-architecture-review-and-whats-next.md
│       ├── 14-particle-laboratory.md
│       ├── 15-data-layout-toggle-lab.md
│       ├── 16-spatial-partition-lab.md
│       ├── 17-memory-arena-lab.md
│       └── 18-infinite-growth-lab.md
```

---

## Lesson Progression (required milestones)

Follow this exact ordering. You may split a milestone into 2 lessons if it's getting too long, but do not reorder or skip:

### Phase A — The Problem (Lessons 01-03)

1. **The OOP Problem** — Show the classic inheritance hierarchy (Entity → Player → Monster → Weapon) and WHY it breaks: diamond problem, empty base classes, inflexible hierarchies. Show the "fat struct with kind enum" solution. No code to compile yet — this is conceptual with ASCII diagrams comparing the two approaches. JS bridge: TypeScript discriminated unions vs class hierarchies. Reference: Anton's opening (00:00–10:00). End state: student understands why OOP hierarchies don't work for game entities.

2. **Fat Struct and Kind Enum** — Build the first `Thing` struct with `Kind` enum. Static array `things[MAX_THINGS]`. Simple add function (bump next_empty_slot). Simple loop to print all things. First compilable code (standalone `main()` test harness, no platform layer yet). Show back-of-napkin memory math: how many things at what size costs how much RAM. Reference: Anton's memory calculations (64:00–68:00). End state: student can add things to a flat array and iterate them.

3. **Indices, Not Pointers** — Show WHY raw pointers break (reallocation scenario with ASCII diagram: pointer to slot 3 becomes dangling after array grows). Introduce `ThingIdx` as an integer type alias. Replace any pointer-based references with indices. Show that indices survive reallocation. Show that indices are trivially serializable (just write the int). JS bridge: array index vs object reference. Reference: Anton (16:00–22:00). End state: student uses integer indices for all inter-entity references.

### Phase B — Intrusive Data Structures (Lessons 04-06)

4. **Intrusive Linked Lists** — THE key lesson. Show the problem: `std::vector<Item> inventory` inside an entity breaks contiguous memory, needs constructors, can't memset. Solution: embed `first_child` and `next_sibling` indices directly in the Thing struct. Walk through add-to-list, iterate-list, remove-from-list with ASCII diagrams showing the array state before and after each operation. Show the correctness advantage: an item can only be in ONE list (no loot duplication). Worked example: player inventory (axe → potion → food). JS bridge: DOM `firstChild`/`nextSibling`. Reference: Anton (22:00–40:00). End state: student can build parent-child hierarchies using intrusive lists.

5. **Intrusive Trees and Circular Lists** — Extend to full tree: `parent`, `first_child`, `next_sibling`, `prev_sibling`. Show scene graph example (root → player → items, root → monsters). Introduce circular doubly-linked list trick: `last_child = first_child.prev_sibling` — eliminates the need for a separate `last_child` field. Show how circular lists eliminate boundary conditions (polygon edge-wrapping example from Anton). Show the loop termination pattern: `do { ... } while (node != first_child)`. Reference: Anton (40:00–55:00). End state: student can build and traverse hierarchical tree structures with circular sibling lists.

6. **The Nil Sentinel and Zero Initialization** — THE philosophy lesson. Reserve `things[0]` as the nil sentinel — always zero-initialized, kind = NIL. Show 5 crash scenarios that nil prevents: (a) index out of bounds, (b) entity was removed, (c) ran out of pool space, (d) stale reference, (e) uninitialized index. Show chain-dereference: `thing_get(pool, thing_get(pool, player).first_child).health` — if any link is nil, you bottom out at the nil sentinel, never crash. Show `thing_is_valid()` check (kind != NIL). Show debug-mode assert: if nil's bytes become non-zero, someone wrote to it illegally. Show `memset(&pool, 0, sizeof(pool))` as the reset function — EVERYTHING resets in one call. JS bridge: optional chaining (`entity?.inventory?.first?.name ?? "none"`). Reference: Anton (55:00–70:00, 82:00–95:00). End state: student designs all structs so all-zeros is a valid, safe state.

### Phase C — The Complete Pool System (Lessons 07-10)

7. **Slot Map: Add, Remove, Get** — Build the full `ThingPool` struct: `things[MAX_THINGS]`, `used[MAX_THINGS]`, `next_empty_slot`. Implement `pool_add(kind)` — find slot, zero it, set kind, mark used, return index (or 0 on failure). Implement `pool_get(idx)` — bounds check + used check, return reference or nil. Implement `pool_remove(idx)` — bounds check, mark unused, zero the slot. Show the complete add/get/remove cycle with test harness. Reference: Anton (70:00–82:00). End state: working slot map with add/remove/get and nil-safe access.

8. **Free List for Slot Reuse** — Show the problem: after removing things, next_empty_slot still advances, eventually you run out even though there are empty slots. Solution: intrusive free list. When removing a slot, add it to a `first_free` linked list (using the thing's `next_sibling` field — it's unused since the thing is dead). When adding, check the free list first before bumping next_empty_slot. ASCII diagram: show 3 removes creating a free chain, then 2 adds consuming from it. Reference: Anton (96:00–98:00). End state: pool efficiently reuses removed slots.

9. **Generational IDs — Safe References** — Show the use-after-free problem: you hold ThingIdx 5, thing at slot 5 is removed, a new thing is added at slot 5. Your old index now points to the WRONG thing. Solution: `ThingRef { index, generation }`. Each slot has a generation counter incremented on removal. `pool_get_ref(ref)` checks `ref.generation == pool.generations[ref.index]` — if mismatch, return nil. `pool_add` returns a `ThingRef` (not just an index). This is the "production" version of the reference system. JS bridge: database auto-increment IDs — row 5 with ID 500 is not the same as row 5 with ID 501. Reference: Anton mentions this, Our Machinery blog posts on slot maps. End state: safe entity references that detect stale/recycled slots.

10. **Pool Iterator** — Show the "gross" way to iterate (raw for loop over all slots, check `used[i]`, skip unused). Then build a proper iterator: `ThingIterator` struct with pool pointer and current index. `thing_iter_begin(pool)` starts at index 1, `thing_iter_next(iter)` advances to next used slot, `thing_iter_valid(iter)` checks bounds. In C11, this works as a manual while-loop pattern (no range-for without C++ operator overloading). Show both the internal raw loop and the clean iterator API. Reference: Anton (105:00–116:00). End state: clean iteration over all living things in the pool.

### Phase D — Game Demo (Lessons 11-13)

11. **Game Demo: Window, Sprites, and the Pool** — Integrate the things pool into a real game. Set up the Platform Foundation Course template (backbuffer, input, build-dev.sh). Create `game_init`: reset pool, add a player thing at screen center. Create `game_render`: iterate pool, draw colored rectangles (or simple sprite shapes) based on kind. Get the player visible on screen. Reference: Anton's SDL2 harness + sprite rendering (98:00–110:00). End state: window opens, player rectangle appears at center.

12. **Game Demo: Movement, Spawning, and Removal** — Add keyboard input: move the player thing's position via WASD/arrow keys. Add troll spawning: on a timer or key press, `pool_add(KIND_TROLL)` at random positions. Add collision: if player overlaps a troll, `pool_remove(troll_ref)`. Show that removed trolls' slots get reused by new spawns (free list in action). Show that stale ThingRefs to removed trolls safely return nil (generational IDs in action). Show the pool iterator skipping removed slots. Reference: Anton's live demo (110:00–140:00). End state: playable demo with player movement, enemy spawning, collision removal, and slot reuse.

13. **Architecture Review and What's Next** — No new code. Review the complete system architecture with a full ASCII diagram showing all the pieces. Discuss trade-offs: what you lose (random child access, slightly complex insert), what you gain (safety, performance, serialization, no crashes). Discuss SoA refactoring: when the fat struct gets too large, rip it into per-property arrays (positions[], healths[], kinds[]) indexed by the same ThingIdx. Discuss union for type-specific data. Discuss where this goes next: ECS (the formalized version), Our Machinery articles, Ryan Fleury's blog posts. Map every concept to its Game Programming Patterns equivalent: Object Pool, Data Locality, Component. Map to Handmade Hero days where similar patterns appear. Reference: Anton's closing discussion (122:00–140:00). End state: student has a mental map of the entire LOATs architecture and knows where to go next.

### Advanced Laboratories (Lessons 14-18)

Each lab is a learning laboratory — NOT gameplay. They stress-test and visualize the concepts from lessons 01-13. Each lab has multiple toggleable modes (cycled via Tab) that let the student compare approaches live.

| Lab | Key | Lesson | What it teaches | Modes |
|---|---|---|---|---|
| **Particle Lab** | P | 14 | Pool at scale, behavior as data, prediction | 1000 particles, 6 archetypes |
| **Data Layout Lab** | L | 15 | Cache locality, AoS vs SoA vs Hybrid | 3 layouts, Tab toggles |
| **Spatial Partition Lab** | G | 16 | O(N²) → structured queries | Naive, Grid, Hash, Quadtree |
| **Memory Arena Lab** | M | 17 | malloc overhead, arena allocation, lifetimes | malloc, arena, pool, scratch |
| **Infinite Growth Lab (Capstone)** | I | 18 | Scalable engine systems | 10 modes: malloc→arena→pool→lifetime→budget→streaming→LOD→amortized→threaded→production |

**Lab design principles:**
- Same simulation logic, different underlying strategy — isolates the variable being studied
- Live metrics in HUD (entity count, frame time, checks, memory)
- Visual debug overlays where applicable (grid lines, quadtree boxes, arena fill bars)
- Explicit for-loops (no hidden iterators) — teaching clarity over abstraction

**Why labs exist:** Lessons 01-13 prove correctness with ~20 entities. Labs prove the SAME techniques work at 1000-5000 entities with real performance implications. The student FEELS why flat arrays, arenas, and spatial partitions matter.

**Future lab ideas** (not implemented, but follow the same pattern):
- **ECS Comparison Lab** — compare fat-struct LOATs approach with a simple ECS; show trade-offs
- **Serialization Lab** — write pool to disk, read it back; demonstrate why indices serialize and pointers don't
- **Network Sync Lab** — replicate pool state across two instances; show why indices + generations enable deterministic sync
- **SIMD Lab** — apply SSE/AVX intrinsics to SoA position updates; show 4x-8x speedup on bulk operations
- **Hot Reload Lab** — dlopen/dlclose game code; show why POD data + indices enables live code replacement

Details for each lab are in separate specification files (`1-...particle-laboratory.md` through `5-...infinite-growth-laboratory.md`).

14. **Particle Laboratory (Advanced Module)** — 1000 particles with 6 movement archetypes (Bounce, Wander, Seek, Predictive Seek, Orbit, Drift Noise). Always compiled in — press **P** to access. Demonstrates: parallel arrays extending pool entities without struct changes, behavior as data via switch-on-archetype, data mutation over inheritance, predictive AI with error margins, explicit for-loop iteration (no hidden abstractions), performance observability (entity count, FPS, update time). This is the capstone stress-test that proves the pool system works under real load.

15. **Data Layout Toggle Lab (Advanced Module)** — 2000 entities with three memory layout modes: Array of Structs (AoS), Struct of Arrays (SoA), and Hybrid (hot/cold separation). Always compiled in — press **L** to access. All three layouts execute identical update logic — only the data organization differs. Runtime toggle via Tab key. Performance HUD shows real timing data so the student can directly observe that memory layout affects performance. Demonstrates: cache line behavior, why engines reorganize data into SoA, hot/cold data separation, and the bridge from LOATs to data-oriented design and ECS intuition. Cross-references: GPP Ch 17 (Data Locality), GEA Ch 6 (memory management), Anton's SoA discussion (125:00–127:00), Casey's SoA for SIMD optimization in Handmade Hero.

16. **Spatial Partition Lab (Advanced Module)** — Always compiled in — press **G** to access. 2000 entities with four spatial query strategies: Naive O(N²) all-pairs, Uniform Grid, Spatial Hash, and Quadtree. All four modes use identical entity behavior and interaction logic — only the query structure changes. Runtime toggle via Tab key. Visual debug overlay renders partition structures (grid lines, hash bucket highlights, quadtree subdivision boxes). Performance HUD shows check counts, collision counts, and frame timing. Demonstrates: why O(N²) kills frame rates at scale (2000 × 2000 = 4M checks/frame), how spatial locality reduces search space to ~10K-50K checks, broadphase vs narrowphase thinking, and the bridge from LOATs to physics engine world organization. Cross-references: GPP Ch 20 (Spatial Partition), GEA Ch 12 (collision detection), Casey's spatial partitioning for entity queries in Handmade Hero.

17. **Memory Arena Lab (Advanced Module)** — Always compiled in — press **M** to access. Entity churn workload (hundreds of spawns/destroys per second) run through four allocation strategies: raw malloc/free, linear arena (bump pointer), pool allocator (fixed-size free list), and scratch arena (per-frame reset). Runtime toggle via Tab key. Performance HUD shows allocations per frame, bytes used, peak memory, and frame-time variance. Memory visualization overlay renders arena fill level and pool slot occupancy. Demonstrates: malloc overhead (lock contention, fragmentation, latency spikes), arena O(1) allocation and reset, pool O(1) alloc/free via free list, scratch arena as the "frame allocator," and lifetime categories (permanent, level, frame, scratch). Learning progression: Large Arrays (data structure) -> Particle Lab (scale) -> Data Layout Lab (memory layout) -> Spatial Partition Lab (query algorithms) -> Memory Arena Lab (lifetime management). Cross-references: GEA Ch 6 (Memory Management), GPP Ch 19 (Object Pool), Casey's permanent + transient arena model in Handmade Hero, Anton's "just zero them" philosophy.

18. **Infinite Growth Lab (Capstone Module)** — Always compiled in — press **I** to access. The FINAL CAPSTONE: entities spawn forever at ~50/sec into the real thing_pool (1024 slots). Ten stabilization modes toggled via Tab across five stages:
    - **Stage 1 — Allocation:** malloc (BAD — intentionally shows overhead), Arena (bump allocator), Pool (free list reuse)
    - **Stage 2 — Lifetime:** Lifetime (TTL expiration), Budget (spawner adapts to frame-time constraints)
    - **Stage 3 — Scale:** Streaming (only simulate nearby entities), LOD (near = full update, far = reduced)
    - **Stage 4 — Throughput:** Amortized (stagger updates across frames), Threaded (pthread worker pool)
    - **Stage 5 — Production:** Production combo (all strategies working together)

    The malloc mode fills the pool and shows allocation overhead. Each subsequent mode is a real engine strategy for long-session stability. Performance HUD shows entity count, pool occupancy, active/idle ratio, and frame timing. Demonstrates: the gap between demo code and engine code, graceful degradation under infinite pressure, and that ALL prior techniques (pool system, free list, spatial awareness, frame budgets, lifetime management) must work together in production. Learning progression: Large Arrays (data structure) → Particle Lab (scale) → Data Layout Lab (memory layout) → Spatial Partition Lab (query algorithms) → Memory Arena Lab (lifetime management) → **Infinite Growth Lab (capstone — all of the above under infinite pressure)**. Cross-references: ALL prior lessons (02-17), HH entity system overflow handling, GPP Ch 19 (Object Pool) capacity management.

---

## Gap Analysis: Transcript vs Course

### What the transcript covers well:

- Intuition for why OOP hierarchies fail
- Intrusive lists (singly/doubly linked, circular)
- Nil sentinel philosophy and crash prevention
- Back-of-napkin memory math
- Slot map add/remove/get
- Live coding demo proving it works

### What the transcript mentions but doesn't fully implement:

- Free list (described, not coded in detail)
- Generational IDs (mentioned, references Our Machinery blog)
- Iterator (started but abandoned due to C++ operator syntax)
- SoA refactoring (discussed conceptually, not implemented)
- Serialization (mentioned as a benefit, not demonstrated)

### Transcript concepts the course MUST include (easy to miss):

- **Ryan Fleury trait-based approach** (08:37–10:15) — flag-based traits as alternative to kind enum
- **typedef vs struct wrapper debate** (101:01–103:02) — Q&A about type safety for thing_idx
- **Domain-named intrusive lists** (42:07–42:56) — `first_inventory`/`next_inventory` instead of generic first_child/next_sibling; things can be in MULTIPLE lists via separate named fields
- **Computing first_child dynamically** (61:23–62:18) — Anton's UI system only used parent pointers, built child lists right before rendering
- **"Deactivated objects are gaps"** (70:02–70:13) — Q&A framing that motivates the free list
- **"If you can remove an if, that's a good day"** (59:18–59:20) — Anton's design heuristic
- **"Don't add std::optional to your array"** (81:38–81:56) — C++ feature creep gotcha
- **Erlang/Docker crash-recovery analogy** (89:44–90:17) — nil-safety compared to container restarts ("my safety net is closer to the memory")
- **"Valley of sadness"** (125:50–126:04) — honest admission: transitioning FROM C++ looks worse before it looks better
- **STL composition vs owning the system** (137:28–139:02) — the philosophical split
- **Separate pools per entity type** (136:37–136:45) — monsters_pool, rocks_pool as optimization
- **char buffer for custom data** (69:24–69:32) — `char custom[104]` as the ultimate flexibility option
- **Union inside fat struct** (68:40–69:02) — `union { player_data; monster_data; }` to reduce waste
- **Live recompile benefit** (109:05–109:22) — no-pointers approach enables Visual Studio hot reload
- **Performance as adoption inroad** (131:40–132:14) — how to sell this to teams
- **"game reset = just zero"** (74:35–75:07) — `things = {};` is the entire reset function
- **Circular lists are OPTIONAL** (58:42–59:23) — "Wesh is not convinced, that's all right, it's your cabin"
- **"Kids and sibs" naming** (63:23–63:40) — pro tip for field alignment
- **Linux kernel list_head** (31:38–31:42) — real-world intrusive list usage
- **Our Machinery blog archive** (117:02–119:48) — slot maps and generational IDs (some URLs dead, GitHub archive exists)

### What the course ADDS beyond the transcript:

- Complete free list implementation with tests
- Complete generational ID implementation with tests
- Clean C11 iterator pattern (no C++ operator overloading)
- Full dual-backend game demo (not just SDL2)
- Explicit connection to Game Programming Patterns, GEA, Handmade Hero
- Web dev bridges for every concept
- Step-by-step build-along with compile checkpoints
- Common bugs reference table
- **14-scene game demo** with keyboard scene selector (1-9, P, L, G, M, I)
- **Bitmap font HUD system** (`FONT_8X8[128][8]` + `draw_text_buf()`) rendering real-time metrics into the CPU pixel buffer
- **Variant functions** in things.h/things.c for side-by-side comparison of approaches (singly vs circular lists, with vs without free list, with vs without generation checks)
- **5 advanced laboratories** (lessons 14-18) stress-testing pool concepts at 1000-5000 entity scale with toggleable modes
- **Infinite Growth Lab capstone** with 10 modes across 5 stages (malloc through production combo), including pthread threading

---

## Workflow — follow this order strictly

### Phase 0 — Analyse (no output yet)

1. Read the full transcript `@_ignore/Avoiding Modern C++  Anton Mikhailov.txt`
2. Read `@ai-llm-knowledge-dump/prompt/course-builder.md`
3. Read `@_ignore/about-me.md` / `@_ignore/about-me-0.md`
4. Read `@ai-llm-knowledge-dump/llm.txt`
5. Study existing game courses (Snake, Tetris, Asteroids) for patterns to reference
6. Identify the natural build progression from the transcript's concept order

### Phase 1 — Planning files (create these first, nothing else)

Create `PLAN.md`, `README.md`, `PLAN-TRACKER.md`, `GLOBAL_STYLE_GUIDE.md`. Do **not** create any lesson or source file until these exist.

**`PLAN.md`** must contain:

- One-paragraph description of what the final system does
- Lesson sequence table: `| # | Title | What gets built | What the student runs/sees |`
- Final file structure tree
- JS→C concept mapping table
- Cross-reference table: which existing courses each lesson connects to
- Transcript timestamp mapping: which transcript segments each lesson covers

**`README.md`** must contain:

- Course title and one-line description
- Prerequisites (recommend completing Asteroids or Tetris for fixed-array pool familiarity)
- How to build and run (both backends)
- Lesson list with one-line summary each
- What the student will have achieved by the end

**`PLAN-TRACKER.md`** — checklist mirror of PLAN.md for tracking completion.

**Stop here and wait for confirmation before continuing.**

### Phase 2 — Source files

Create `course/build-dev.sh` and all `course/src/**` files.

Every source file must:

- Exactly reproduce the system described in the transcript
- Have extensive comments explaining **why** each decision was made
- Anchor C concepts to their nearest JavaScript equivalent in a `/* JS: ... | C: ... */` comment the first time they appear
- Have section headers using `/* ══════ Section Name ══════ */` style

### Phase 3 — Lesson files

One `.md` file per lesson in `course/lessons/`. Filename: `NN-kebab-title.md`.

Each lesson follows the template:

```
# Lesson NN: Title

## What we're building
## What you'll learn
## Prerequisites
---
## Step 1 — [The Problem] ...
## Step 2 — [The Solution] ...
## Step 3 — [Implementation] ...
---
## Build and run
## Expected result
## Common mistakes
## Exercises
## Connection to your work
## What's next
```

Additional requirements:

- Every code block must be the **complete file at that lesson stage** (copy-paste compilable)
- Every non-obvious C pattern gets a `> **Why?**` callout with JS analogy
- Every concept gets a `> **Common mistake:**` note with wrong version and why it fails
- Use `> **Handmade Hero connection:**` callouts where relevant
- Use `> **Anton says:**` callouts for direct quotes/paraphrases from the transcript
- Use `> **New technique:**` blocks for each new data structure technique, with:
  - What it does
  - Why we do it this way
  - The JS equivalent
  - One concrete worked example with numbers and ASCII diagram

### Phase 4 — Retrospective

Create `COURSE-BUILDER-IMPROVEMENTS.md` with:

- What patterns from `course-builder.md` worked well and why
- What was missing, unclear, or needed adjustment
- Concrete suggested edits with before/after diffs
- Comparison with existing game courses (how this architectural course differs)

---

## Quality Bars

### Code Quality (applies to every file in `course/src/`)

| Rule                                           | Detail                                                                                   |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------- |
| No `malloc` in the pool or game loop           | `things[MAX_THINGS]` is a static array; no heap allocation ever                          |
| No OOP patterns                                | No vtables, no function-pointer polymorphism for its own sake                            |
| Flat array, not linked list as primary storage | `thing things[MAX_THINGS]` — intrusive lists are secondary indexing, not primary storage |
| `memset` / `= {0}` for init                    | `memset(&pool, 0, sizeof(pool))` resets everything; `thing thing = {0}` zeros a thing    |
| Nil at index 0                                 | `things[0]` is always zero-initialized, kind = NIL, never allocated to a real entity     |
| Generational IDs for safe references           | `ThingRef { index, generation }` — stale refs detected and return nil                    |
| Free list for slot reuse                       | Removed slots join an intrusive free list; new adds check free list first                |
| No constructors/destructors                    | Zero-init is the "constructor"; marking as unused is the "destructor"                    |
| Platform independence                          | `things.h`/`things.c` compile without X11 or Raylib headers                              |
| `platform.h` 4-function contract               | `platform_init`, `platform_get_time`, `platform_get_input`, `platform_shutdown`          |
| Enums over magic integers                      | `THING_KIND_PLAYER` not `kind == 1`; `THING_KIND_NIL` not `kind == 0`                    |

### Lesson Quality

| Rule                               | Detail                                                                              |
| ---------------------------------- | ----------------------------------------------------------------------------------- |
| Each lesson compilable             | Student can build after every lesson                                                |
| One major concept per lesson       | Don't introduce intrusive lists AND nil sentinel AND generational IDs in one lesson |
| Testable milestone each lesson     | Student runs a test harness or game demo that shows something new                   |
| Web dev bridges present            | Every first use of a C concept has a JS/TS/DOM counterpart                          |
| ASCII diagrams for every operation | Add, remove, iterate, reparent — show pool state before and after                   |
| No forward references              | Lesson N doesn't assume knowledge from Lesson N+2                                   |
| Problem before solution            | Every technique motivated by a concrete failure scenario                            |

---

## Engineering Principles to Embed Throughout

These come from Anton Mikhailov's talk and Casey Muratori's Handmade Hero. Reference them by name in callouts:

- **"It's your happy little cabin."** You don't need to follow the STL way. Build what you need, understand it fully, own it completely.
- **"Large arrays of things."** The fundamental unit of state management. Programs exist to operate on many things. Store them in arrays.
- **"Indices, not pointers."** Pointers break on reallocation, don't serialize, and crash on null. Indices survive reallocation, trivially serialize, and point to nil (safe).
- **"Intrusive over external."** Embed data structure linkage into the data itself. No separate containers. One allocation, one memory layout.
- **"Nil, not null."** Null crashes your program. Nil is a safe zero-value you can always access. Design for nil, not against null.
- **"Zero is the default."** If all-zeros is a valid state for every struct, then memset is your universal initializer and reset function. No constructors needed.
- **"Bugs are invalid states."** Reduce the number of possible states your program can be in. Intrusive lists prevent items from being in two places. Generational IDs prevent stale references from accessing wrong entities.
- **"Do the napkin math."** Before adding complexity, calculate the actual memory cost. It's almost always less than you think.
- **"Don't build the skyscraper. Build the log cabin."** Simple, complete, correct. You can get a lot done and keep your sanity.

---

## Cross-References to Existing Courses

| Concept in This Course      | Existing Course           | Where                                                                          |
| --------------------------- | ------------------------- | ------------------------------------------------------------------------------ |
| Fixed entity arrays         | Asteroids                 | `SpaceObject asteroids[MAX_ASTEROIDS]` — same pattern, simpler                 |
| compact_pool swap-with-last | Asteroids                 | Lesson 08 — this course's free list is the UPGRADE                             |
| State machines with enum    | Tetris, Asteroids         | `GAME_PHASE` enum — same pattern, this course's kind enum is similar           |
| Delta-time game loop        | All game courses          | Platform Foundation — same loop, this course focuses on what's INSIDE it       |
| Zero initialization         | All game courses          | `GameState state = {0}` — this course makes it a DESIGN PRINCIPLE              |
| Backbuffer rendering        | Platform Foundation       | Same pipeline — this course adds entity pool as the content source             |
| Data-oriented design        | Game Programming Patterns | Ch 17 (Data Locality), Ch 19 (Object Pool) — this course is the IMPLEMENTATION |
| Entity component systems    | Game Engine Architecture  | Ch 15-16 — LOATs is the stepping stone before full ECS                         |

---

## Common Bugs Reference

| Bug                               | Symptom                                 | Root Cause                                                          | Fix                                                                            |
| --------------------------------- | --------------------------------------- | ------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| Stale reference to removed entity | Player interacts with "ghost" entity    | ThingIdx still valid after removal; slot reused by different entity | Use ThingRef with generational ID                                              |
| Nil sentinel overwritten          | Random data corruption                  | Code writes to `things[0]` by accident                              | Debug assert: check nil bytes are zero after every get()                       |
| Free list corruption              | Pool runs out of slots despite removals | `next_sibling` not properly set when adding to free list            | Verify free list integrity with debug traversal                                |
| Infinite loop in circular list    | Program hangs during iteration          | Forgot termination condition (`while (node != first_child)`)        | Use do-while with explicit termination check                                   |
| Off-by-one in pool (index 0)      | First entity silently doesn't work      | Allocating at index 0 (the nil sentinel)                            | `next_empty_slot` starts at 1, not 0                                           |
| Parent/child mismatch             | Entity appears in wrong inventory       | Set `first_child` but forgot to set child's `parent`                | Always update both sides in add-to-parent function                             |
| Unused slots iterated             | Rendering deleted entities              | Iterator doesn't check `used[i]` flag                               | Iterator skips slots where `used[i] == false`                                  |
| memset wipes generation counters  | Stale refs stop being detected          | `memset(&pool, 0, ...)` zeros `generations[]`                       | Only zero `things[]` and `used[]`; preserve `generations[]` and increment them |

---

## Deviation Policy

The `course-builder.md` is a **strong guideline**, not a rigid spec. Deviate when:

- The LOATs architecture requires a pattern not covered by course-builder.md (e.g., standalone library before game integration)
- A pattern would confuse a JS developer more than help
- A simplification hides something important about how memory and data structures actually work

**When you deviate:**

1. Add `/* COURSE NOTE: ... */` in the source file
2. Add a `> **Course Note:**` callout in the relevant lesson
3. Add an entry to `COURSE-BUILDER-IMPROVEMENTS.md`

Do **not** deviate just to simplify. Complexity that teaches something important should stay.

---

## Verification

After completing the course:

- All 18 lesson files exist with all template sections (13 core + 5 labs)
- All lessons meet minimum depth targets (8,000/5,000/6,000/10,000 per tier — see Lesson Quality Standards)
- All source files compile with `gcc -Wall -Wextra -Werror`
- `./build-dev.sh --backend=x11` exits 0 and game demo runs
- `./build-dev.sh --backend=raylib` exits 0 and game demo runs
- `things.h`/`things.c` compile independently (no platform dependency)
- ASCII diagrams present for EVERY data structure operation across all lessons
- Web dev bridges present in every lesson
- Problem-before-solution ordering in every concept introduction
- Cross-references to existing courses in every lesson
- PLAN-TRACKER.md fully checked off
- GLOBAL_STYLE_GUIDE.md terminology consistent across all files
- Rolling summaries up to date
- QUALITY_BENCHMARKS.md with 3 golden references evaluated
- COURSE-BUILDER-IMPROVEMENTS.md completed
