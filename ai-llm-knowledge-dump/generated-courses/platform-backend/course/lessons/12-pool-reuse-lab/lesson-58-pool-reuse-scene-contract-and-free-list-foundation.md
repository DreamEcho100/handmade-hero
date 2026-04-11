# Lesson 58: Pool Reuse Scene Contract and Free-List Foundation

## Frontmatter

- Module: `12-pool-reuse-lab`
- Lesson: `58`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `GAME_SCENE_POOL_LAB`
  - `PoolLabScene`
  - `PoolProbe`
  - `PoolBarrier`
  - `PoolAllocator`
  - `pool_init_in_arena`
  - `game_scene_name`
  - `game_scene_summary`
  - `game_scene_action_hint`

## Why This Lesson Exists

Lesson 57 ended with a rebuild rule:

```text
one owner can discard one whole payload and rebuild it from scratch
```

The Pool Reuse Lab teaches a different rule:

```text
one owner keeps one bounded backing store alive,
returns individual fixed-size slots to a free list,
and later reuses those exact slots without moving the storage region
```

That shift matters because the learner is no longer asking:

```text
what gets rebuilt together?
```

The learner is now asking:

```text
how does one same-size slot leave circulation and come back later?
```

## Observable Outcome

By the end of this lesson, you should be able to explain the Pool scene contract in one sentence: the level arena owns one fixed memory block, the pool allocator manages same-size probe slots inside it, and the scene is engineered to create enough churn that slot reuse becomes visible and measurable.

## First Reading Strategy

Read the scene in the same order the runtime teaches it:

1. start with the scene labels
2. read `PoolAllocator` before reading probe motion
3. read `PoolLabScene` as a proof surface, not only as storage
4. treat barriers as churn generators that exist for allocator pedagogy

If you start with moving probes, the allocator story feels like background noise. If you start with contract and ownership, the rest of the module lands much more cleanly.

## Step 0: Carry Forward the Previous Module Correctly

The Level Reload Lab taught you to separate rebuild from reuse-in-place.

Carry that habit forward here.

The Pool scene is not a smaller rebuild lesson. It is a different ownership pattern entirely. Nothing in this scene says "tear down the whole payload and make it again." The core question is whether a returned slot can come back as a later occupant while the underlying backing storage stays put.

## Step 1: Read the Scene Labels as the Public Runtime Contract

The Pool scene advertises itself through three functions in `src/game/main.c`:

- `game_scene_name(...)` -> `Pool Reuse Lab`
- `game_scene_summary(...)` -> `fixed-size reuse with a free-list pool`
- `game_scene_action_hint(...)` -> `SPACE fire a collision wave and saturate the voice pool`

Those labels are not filler. They tell you the runtime contract before you open the allocator code.

- the name says the goal is reuse, not mere occupancy
- the summary says the storage model is fixed-size and free-list-based
- the action hint says manual input creates churn pressure, not rebuild work

That is already a complete conceptual handoff from Module 11.

## Step 2: Read `PoolAllocator` as the Minimal Contract for Reuse

In `src/utils/arena.h`, the allocator is:

```c
typedef struct PoolAllocator {
  void *memory;
  PoolFreeNode *free_list;
  size_t slot_size;
  size_t slot_count;
  size_t free_count;
  size_t high_watermark;
  int owns_memory;
  Arena *backing_arena;
  const char *debug_name;
} PoolAllocator;
```

That struct already tells the whole story.

- `memory` is one stable backing region
- `free_list` points at the currently available slots
- `slot_size` and `slot_count` define the bounded capacity
- `free_count` exposes current remaining capacity
- `high_watermark` records peak occupancy pressure
- `backing_arena` says the pool is layered on top of arena-owned memory rather than replacing it

This allocator is intentionally narrow. It is not trying to grow, compact, or move things around. It is trying to make same-size reuse cheap and obvious.

## Step 3: Notice the Pool Prepares Its Shape Before Any Probe Moves

The important setup functions in `arena.h` are:

- `pool_slot_size_aligned(...)`
- `pool_init_in_arena(...)`
- `pool_init_from_memory(...)`
- `pool_build_free_list(...)`

Read that pipeline as one idea.

1. the slot size is aligned so every slot is large enough and correctly aligned for both a `PoolProbe` and the free-list node overlay
2. the level arena provides one contiguous memory block
3. the pool records that block as its backing store
4. the pool walks that block and links every slot into the initial free list

So when the scene starts, the pool is not empty because it has no storage. It is empty because all of its storage exists and is currently free.

## Visual: What the Pool Owns

```text
level arena owns one block

[slot 0][slot 1][slot 2][slot 3]...[slot N-1]

pool allocator overlays a free list on top of those slots
free_list points at the next available slot
```

That is a very different picture from the rebuild lesson. The backing block persists. Individual slots rotate between active and free.

## Step 4: Read `PoolLabScene` as the Teaching Surface

The scene payload is:

```c
typedef struct {
  GameCamera camera;
  PoolAllocator pool;
  PoolProbe *active[GAME_POOL_CAPACITY];
  int active_count;
  PoolProbe *last_freed;
  PoolBarrier barriers[GAME_POOL_BARRIER_COUNT];
  unsigned int slot_generations[GAME_POOL_CAPACITY];
  unsigned int slot_reuses[GAME_POOL_CAPACITY];
  unsigned int reuse_hits;
  unsigned int collision_count;
  unsigned int escaped_count;
  unsigned int burst_count;
  int hottest_slot;
  float spawn_accum;
} PoolLabScene;
```

Do not read that as one undifferentiated field list. Read it in groups.

### Ownership and allocator state

- `pool`
- `last_freed`

### Live occupancy

- `active[]`
- `active_count`

### Reuse history

- `slot_generations[]`
- `slot_reuses[]`
- `reuse_hits`
- `hottest_slot`

### Churn pressure

- `collision_count`
- `escaped_count`
- `burst_count`
- `spawn_accum`

This scene is dense on purpose. It is not only simulating moving probes. It is storing enough evidence to prove allocator behavior at several timescales.

## Step 5: Understand Why the Scene Needs `PoolProbe` and `PoolBarrier`

`PoolProbe` is the fixed-size occupant that lives inside one slot. Its fields are mostly world-facing:

- position and velocity
- TTL
- size and color
- slot index and generation

That mix is important. The probe is both a moving object and a labelable proof carrier.

`PoolBarrier` is not allocator state at all. It exists because the scene needs churn. Barriers create collisions, shorten lifetimes, and light up on impact so the learner can see where return pressure comes from.

Without barriers, the scene would mostly prove startup allocation. With barriers, it can prove the full round trip from active slot to returned slot to reused slot.

## Step 6: Read `pool_init_in_arena(...)` as the Ownership Bridge From Module 11

The Pool scene is initialized through:

```c
pool_init_in_arena(&scene->pool, level, sizeof(PoolProbe),
                   GAME_POOL_CAPACITY, "pool_lab_pool")
```

That one line carries the ownership bridge from the Level Reload module:

```text
level arena owns the raw memory block
pool allocator manages fixed slots inside that block
```

So the Pool Lab does not replace the arena model. It specializes it. The arena still owns the bytes. The pool only changes how same-size pieces inside that owned region are handed out and returned.

## Step 7: Follow the Initial Scene Fill

`game_fill_pool_lab(...)` makes the contract concrete:

- reset active occupancy and history counters
- initialize the pool inside the level arena
- place three barriers
- call `pool_lab_spawn(scene, 6)` to create the initial occupants

That means the learner does not start with an abstract empty allocator. They start with a live scene that already has:

- a fixed-capacity backing store
- some occupied slots
- some still-free slots
- visible churn obstacles waiting to create returns

## Worked Example: What Exists Right After Scene Boot

Suppose the scene has just entered the Pool Lab.

What is true already?

1. the level arena contains one backing region reserved for pool slots
2. the pool has linked those slots into a free list
3. six slots have already been claimed by `pool_lab_spawn(scene, 6)`
4. the remaining slots are still free and waiting behind the free-list head
5. the barriers are already placed so churn can begin immediately

That is the actual scene contract the rest of Module 12 will elaborate.

## Common Mistake: Thinking the Pool Lab Is About Dynamic Growth

It is not.

The interesting event is not:

```text
ask the OS for more memory
```

The interesting event is:

```text
reuse one of the slots you already own
```

If you keep that distinction clear, the later slot board, trace evidence, and warning logic all read naturally.

## JS-to-C Bridge

This is like keeping a bounded worker pool instead of constructing a new worker object for every task. The key property is stable backing storage plus predictable slot reuse, not flexibility or unbounded growth.

## Try Now

Open `src/utils/arena.h` and `src/game/main.c`, then do these checks:

1. Read `PoolAllocator` and explain what `memory`, `free_list`, `slot_count`, and `free_count` mean together.
2. Read `pool_init_in_arena(...)` and explain how it layers the pool on top of the level arena.
3. Read `PoolLabScene` and identify which fields describe current occupancy versus historical reuse.
4. Find the Pool scene strings and explain why `SPACE` is described as churn pressure rather than rebuild.
5. Read `game_fill_pool_lab(...)` and explain why the scene starts with both barriers and six live probes already present.

## Exercises

1. Explain why this lesson has to follow the Level Reload Lab instead of coming earlier.
2. Explain why barriers are part of the teaching design even though they are not allocator internals.
3. Explain the ownership relationship between the level arena and the pool allocator in one paragraph.
4. Describe the main proof question of the Pool Reuse Lab in one sentence.

## Runtime Proof Checkpoint

At this point, you should be able to explain the Pool scene contract without mentioning rendering details yet: one bounded backing store exists, the pool manages same-size slots inside it, and the scene is deliberately engineered to create visible slot turnover rather than whole-scene rebuilds.

## Recap

This lesson established the Pool Reuse teaching surface:

- the scene is about fixed-size free-list reuse, not rebuild or growth
- `PoolAllocator` describes one stable backing region plus a free-list head
- the allocator is layered on top of level-arena-owned memory
- `PoolLabScene` stores both live occupancy and reuse history
- barriers and startup probes exist so reuse becomes observable quickly

Next, we follow the spawn path so the learner can see how one pool pointer becomes a stable slot identity with generation and reuse history attached to it.
