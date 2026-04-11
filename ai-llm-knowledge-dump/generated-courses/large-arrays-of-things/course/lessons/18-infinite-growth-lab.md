# Lesson 18 --- Infinite Growth Lab (Capstone)

> **Capstone Module.** This lesson requires the `--infinite-growth-lab` build flag. All code is conditionally compiled under `#ifdef ENABLE_INFINITE_GROWTH_LAB`. This is the final lesson in the course --- it integrates every concept from lessons 02--17.

## What we're building

A laboratory that spawns entities forever --- unbounded, relentless, ~50 per second --- and then gives you ten different strategies for surviving the onslaught. You press `Tab` to cycle through them. Each mode is a real engine technique, and each one tells a different story about what "surviving infinite pressure" means.

The ten modes, in order:

| # | Mode | Core idea |
|---|---|---|
| 0 | malloc/realloc unbounded | The wrong way: call the OS every spawn |
| 1 | Arena pre-allocation | Pre-allocate 4 MB, then O(1) bump allocation |
| 2 | Pool recycling with free list | Zero allocations in the game loop |
| 3 | Lifetime cleanup | Entities expire after a TTL |
| 4 | Budget cap | Spawner throttles based on frame-time pressure |
| 5 | Spatial streaming | Only simulate entities near a focal point |
| 6 | Simulation LOD | Near = full update, far = reduced update |
| 7 | Amortized work | Stagger entity updates across frames |
| 8 | Threaded update | Parallel entity processing with pthreads |
| 9 | Production combo | All strategies combined |

When you run `./build-dev.sh --backend=raylib --infinite-growth-lab -r` and press `I`, the screen fills with entities. The HUD shows: entity count, pool occupancy (percentage of 1024 slots used), active/idle counts, spawn rate, frame time, memory used, and the current mode. In mode 0, you watch malloc overhead climb and frame time degrade. In mode 1, the arena pre-allocates and the frame time flatlines. In mode 9, every strategy works together and the system runs for hours without breaking a sweat.

This is the capstone. Everything from this course --- flat arrays, nil sentinels, free lists, generational IDs, pool iteration, spatial awareness, frame budgets, lifetime management, arena allocation --- is running under infinite load. If the system survives ten minutes of continuous spawning in every mode, you have built engine infrastructure, not a tutorial toy.

## What you'll learn

- **Why demo code dies and engine code survives** --- the gap between "works for 30 seconds" and "works for 3 hours"
- **Why malloc degrades under sustained allocation** --- OS calls, lock contention, heap fragmentation, realloc copying
- **Arena allocation** --- O(1) bump pointer, zero OS calls, zero fragmentation
- **Pool recycling** --- the thing_pool free list pattern from lessons 07--08 applied at production scale
- **Lifetime cleanup** --- entities with a TTL (time-to-live) that expire and free their slots automatically
- **Budget caps** --- the spawner monitors frame time and throttles itself to stay within budget
- **Spatial streaming** --- only entities near a focal point simulate; distant entities are idle
- **Simulation LOD** --- near entities get full-fidelity updates, far entities get reduced updates
- **Amortized work** --- entity updates staggered across frames so no single frame pays the full cost
- **Threaded updates** --- splitting independent entity work across CPU cores with pthreads
- **Production combination** --- integrating multiple strategies into a single cohesive system
- **Performance observability** --- watching pool occupancy, spawn success rate, memory usage, and frame timing under sustained pressure

## Prerequisites

- Completed lessons 01--13 (the full things pool system and game demo)
- Completed lesson 14 (Particle Laboratory) --- you have seen the pool under load at 1000 entities
- Completed lesson 15 (Data Layout Lab) --- you have seen how data organization affects performance
- Completed lesson 16 (Spatial Partition Lab) --- you have seen how query strategy affects scalability
- Completed lesson 17 (Memory Arena Lab) --- you have seen how allocation strategy affects frame stability
- The game demo running on at least one backend (Raylib recommended for HUD overlay)

---

## Building and running (quick start)

Get the lab running so you can watch each survival strategy in action:

```bash
cd course/
./build-dev.sh --backend=raylib --infinite-growth-lab -r
```

Once the window opens, press `I` to enter the Infinite Growth Lab. The screen fills with entities spawning continuously. The HUD shows entity count, pool occupancy, spawn rate, frame time, memory used, and the current mode number (0--9). Press `Tab` to cycle through all 10 modes:

| Press Tab to reach | Mode | What to watch |
|---|---|---|
| Start | Mode 0: malloc unbounded | Frame time climbs. Memory grows. System degrades. |
| 1st Tab | Mode 1: Arena | Frame time flatlines. Memory grows from arena (not heap). |
| 2nd Tab | Mode 2: Pool recycling | Zero malloc. Slots recycle. Memory constant. |
| 3rd Tab | Mode 3: Lifetime cleanup | Entities die and respawn. Pool oscillates ~60%. |
| 4th Tab | Mode 4: Budget cap | Spawn rate auto-throttles when frame time rises. |
| 5th Tab | Mode 5: Streaming | Green entities near center (active), gray far away (idle). |
| 6th Tab | Mode 6: LOD | Three concentric color zones: full/half/quarter update rate. |
| 7th Tab | Mode 7: Amortized | 25% of entities flash cyan each frame (staggered updates). |
| 8th Tab | Mode 8: Threaded | Pink entities, HUD shows per-thread update counts. |
| 9th Tab | Mode 9: Production combo | All strategies combined. White/gray. Indefinitely stable. |

Run each mode for at least 30 seconds. The contrast between Mode 0 (degradation) and Mode 9 (rock-solid) is the emotional payoff of this entire course.

If you are on X11:

```bash
./build-dev.sh --backend=x11 --infinite-growth-lab -r
```

> **JS bridge:** This is like stress-testing a React app by rendering 10,000 components and watching whether `requestAnimationFrame` stays at 60fps. Mode 0 is like creating a new DOM node for every component instance (slow). Mode 9 is like using a virtualized list with object pooling and `React.memo` (fast). The difference is that in JavaScript, framework authors made these decisions for you. In C, you make them yourself --- and this lab teaches you every technique the framework authors used.

---

## Step 1 --- Demo code dies, engine code survives

Every lab in this course so far has tested one specific dimension: the Particle Lab tested entity count, the Data Layout Lab tested memory organization, the Spatial Partition Lab tested query scalability, and the Memory Arena Lab tested allocation stability. Each one proved a specific technique works. But they all share one property: they reach a steady state. The Particle Lab spawns 1000 particles and stops. The Data Layout Lab creates 2000 entities at init and leaves them running. The Memory Arena Lab churns entities but within a bounded population.

Real engines do not reach steady states. Players explore. Enemies spawn. Projectiles fire. Particles emit. The world keeps growing, and the entity system must keep working. Not for 20 seconds. Not for 5 minutes. For hours.

Here is what happens to demo code when you run it for an hour:

```
Demo code timeline:
  0:00 ──── Spawn entities. Everything is fast.
  0:05 ──── 500 entities. Still fast. Malloc overhead invisible.
  0:20 ──── 1024 entities (pool-based) or 10K+ (malloc-based).
             malloc version: heap is fragmented, frame time climbing.
             pool version: pool is full, thing_pool_add returns nil.
             ┌──────────────────────────────────────────────┐
             │  Either way: the system has hit a wall.      │
             │  malloc: death by a thousand cuts.           │
             │  pool: clean failure, but frozen world.      │
             └──────────────────────────────────────────────┘
  1:00 ──── Scene is stagnant. Same entities forever. Nothing new happens.
```

Engine code looks different:

```
Engine code timeline:
  0:00 ──── Spawn entities. Arena pre-allocated. Zero malloc calls.
  0:05 ──── Pool is 50% full. Old entities start expiring.
  0:20 ──── Pool reaches ~75% steady state. Births ≈ deaths.
  1:00 ──── Still at ~75%. Frame time is stable. Memory is stable.
  1:00:00 ── Three hours later. Still at ~75%. Still stable.
             The world has churned through 540,000 entities
             using the same 1024 slots and the same 4 MB arena.
```

The difference is not the pool. The pool is the same `thing_pool` you built in lessons 02--10. The difference is what you do *around* the pool: how you allocate memory, how you manage lifetimes, how you throttle growth, how you distribute work. The pool is the foundation. These ten strategies are the walls and roof that make it livable.

> **Anton says:** "We have 1024 things. If you run out, you return zero. No crash."

That is the baseline: the nil sentinel at index 0 means running out of space is safe. You do not crash. You do not corrupt memory. You just stop growing. The question this lesson answers is: how do you stop needing to stop?

> **Handmade Hero connection:** Casey's entity system in Handmade Hero uses a fixed-size pool with similar overflow handling. When the pool is full, new entity requests fail gracefully. The entity simulation code never assumes allocation succeeds --- it always checks. The strategies in this lesson are the same techniques Casey applies when the world outgrows its initial capacity: arena-based memory, lifetime management, spatial streaming (only simulate entities near the camera), and amortized updates across frames. Casey pre-allocates permanent and transient arenas at startup. The game loop never calls malloc.

---

## Step 2 --- Mode 0: malloc/realloc unbounded (the wrong way)

We start with the failure mode. This is deliberate. You need to see the system break before you can appreciate what fixes it.

Mode 0 does not use the thing_pool at all. Instead, it maintains a dynamically growing array of entities via `malloc` and `realloc`. Every spawn calls `realloc` to grow the array by one element. Nothing is ever deleted. The array grows without bound.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 0: malloc/realloc unbounded                */
/* ══════════════════════════════════════════════════════ */

typedef struct malloc_entity {
    float x, y, dx, dy;
    float size;
    uint32_t color;
    float age;
} malloc_entity;

static malloc_entity *malloc_entities = NULL;
static int malloc_count    = 0;
static int malloc_capacity = 0;
static size_t malloc_bytes_used = 0;
```

The spawner calls `realloc` on every batch:

```c
static void mode0_spawn_batch(float dt)
{
    for (int i = 0; i < SPAWNS_PER_FRAME; i++) {
        total_spawn_attempts++;

        /* Grow the array by one. realloc copies the ENTIRE array. */
        malloc_count++;
        if (malloc_count > malloc_capacity) {
            malloc_capacity = malloc_count;
            malloc_entity *new_buf = (malloc_entity *)realloc(
                malloc_entities,
                (size_t)malloc_capacity * sizeof(malloc_entity)
            );
            if (!new_buf) {
                malloc_count--;
                total_spawn_failures++;
                continue;  /* out of memory */
            }
            malloc_entities = new_buf;
            malloc_bytes_used = (size_t)malloc_capacity * sizeof(malloc_entity);
        }

        total_spawn_successes++;
        malloc_entity *e = &malloc_entities[malloc_count - 1];
        e->x    = (float)(rand() % 780 + 10);
        e->y    = (float)(rand() % 580 + 10);
        e->dx   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        e->dy   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        e->size = 4.0f;
        e->color = 0xFFFF4444; /* red = malloc-managed */
        e->age  = 0.0f;
    }
}
```

Now let us talk about WHY this degrades. Every `realloc` does the following:

1. **Calls the OS allocator.** `realloc` enters the C runtime, acquires a lock on the heap, and searches for a block large enough to hold the new size. On most implementations, if the block cannot be extended in place, `realloc` calls `malloc` for the new size, `memcpy`s the old data, and `free`s the old block.

2. **Copies the entire array.** If the allocator cannot extend the existing block (which happens increasingly often as the array grows and the heap fragments), it copies every byte of the existing array to a new location. At 10,000 entities, that is:

```
Copy cost per realloc that cannot extend in place:

  10,000 entities x 48 bytes/entity = 480,000 bytes = 480 KB copied

  At ~10 GB/s memory bandwidth: 480 KB / 10 GB/s ≈ 48 microseconds per copy
  At ~50 realloc-copies per second: 48 μs × 50 = 2,400 μs = 2.4 ms
  That is 14% of your 16.67 ms frame budget. Gone. Just copying.
```

3. **Fragments the heap.** Every realloc that moves the block leaves a hole in the heap where the old block was. After hundreds of reallocs, the heap is a Swiss cheese of allocated and freed regions. Future allocations take longer because the allocator has to search through more free list entries.

4. **Takes locks.** On multi-threaded systems, every `malloc`, `realloc`, and `free` acquires a mutex. Even on a single-threaded game, the C runtime's allocator may use locks because it does not know you are single-threaded.

The HUD in mode 0 shows three things getting worse over time:

```
Mode 0 degradation timeline:

  Time    Entities   Memory      Frame time    Notes
  ────    ────────   ──────      ──────────    ─────
  0:00    0          0 KB        0.5 ms        Fast. Nothing happening.
  0:10    500        24 KB       1.2 ms        Still fine.
  0:30    1,500      72 KB       2.8 ms        Starting to feel it.
  1:00    3,000      144 KB      5.1 ms        realloc copying 144 KB
  2:00    6,000      288 KB      9.3 ms        Heap fragmented.
  3:00    9,000      432 KB      14.1 ms       Approaching frame budget.
  5:00    15,000     720 KB      22+ ms        BELOW 60 FPS. Stuttering.
```

The memory column keeps climbing because nothing is ever freed. The frame time column keeps climbing because realloc copies more data and the heap gets more fragmented. This is not a theoretical concern. This is what actually happens when you let a game allocate from the heap on every spawn.

> **Why?** In JavaScript, `array.push()` hides the reallocation behind the engine's internal growth strategy (typically doubling). The cost is amortized but the GC pause when the old buffer is collected produces the exact same kind of frame spike. In C, `realloc` makes the copy explicit. You can see it, measure it, and --- most importantly --- eliminate it.

> **JS bridge: memory exhaustion.** You have seen this: a Chrome tab eating 2 GB of RAM until the browser kills it. That is the JavaScript equivalent of Mode 0. The tab kept creating objects, V8 kept growing the heap, the GC kept running longer and longer pauses trying to find dead objects, and eventually the system gave up. In C, `realloc` makes this same progression visible and measurable: each reallocation copies more data, fragments the heap more, and takes more time. The fix is the same in both languages: stop allocating new memory in the hot loop. Pre-allocate what you need. Reuse what you have. That is what Modes 1--9 teach.

> **Common mistake:** "But I can use a doubling strategy with realloc instead of growing by one!" That helps with the copy frequency (O(log N) reallocs instead of O(N)), but the fundamental problems remain: you are still calling the OS allocator, still fragmenting the heap, still taking locks, and the array still grows without bound. Doubling is the right optimization IF you must use malloc. The point of this lesson is that you do not have to.

---

## Step 3 --- Mode 1: Arena pre-allocation (the first fix)

The first fix is the simplest and most impactful: stop calling the OS allocator in the game loop entirely. Pre-allocate a single large block at initialization. Then hand out pieces of it by bumping a pointer.

This is the linear arena from lesson 17. One `malloc` call at startup. Zero `malloc` calls afterward. Ever.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 1: Arena pre-allocation                    */
/* ══════════════════════════════════════════════════════ */

#define ARENA_CAPACITY (4 * 1024 * 1024)  /* 4 MB */

typedef struct growth_arena {
    uint8_t *base;
    size_t   capacity;
    size_t   offset;
} growth_arena;

static uint8_t arena_memory[ARENA_CAPACITY];
static growth_arena arena;
```

Initialization is one line of real work:

```c
static void arena_init(growth_arena *a, uint8_t *memory, size_t cap)
{
    a->base     = memory;
    a->capacity = cap;
    a->offset   = 0;
}
```

Allocation is three operations --- add alignment, check bounds, bump pointer:

```c
static void *arena_alloc(growth_arena *a, size_t bytes)
{
    size_t aligned = (bytes + 7) & ~(size_t)7;  /* 8-byte alignment */
    if (a->offset + aligned > a->capacity) return NULL;

    void *ptr = a->base + a->offset;
    a->offset += aligned;
    return ptr;
}
```

That is it. No locks. No free-list search. No block splitting. No coalescing. No OS calls. The entire allocator is three lines of arithmetic.

```
Arena layout:

  base                                                    base + capacity
  │                                                               │
  ▼                                                               ▼
  ┌────────┬────────┬────────┬────────┬───────────────────────────┐
  │Entity 0│Entity 1│Entity 2│Entity 3│     (unused space)        │
  │ 48 B   │ 48 B   │ 48 B   │ 48 B   │                           │
  └────────┴────────┴────────┴────────┴───────────────────────────┘
                                      ▲                           ▲
                                      │                           │
                                    offset                    capacity

  Arena: [████████████░░░░░░░░░░░░░] 40% used
          ^--- offset                ^--- capacity

  4 MB arena = 87,381 entities @ 48 bytes each
  At 50 spawns/sec: 29 minutes before the arena is full
```

Compare the per-allocation cost:

```
malloc:
  1. Acquire lock           ~10-100 ns
  2. Search free list       ~10-500 ns
  3. Split/coalesce         ~5-50 ns
  4. Update bookkeeping     ~5-20 ns
  Total: 30-670 ns (VARIABLE)

arena_alloc:
  1. Add alignment          ~1 ns
  2. Check bounds           ~1 ns
  3. Bump offset            ~1 ns
  Total: ~3 ns (FIXED)
```

The mode 1 spawner is identical to mode 0 except it calls `arena_alloc` instead of `realloc`:

```c
static void mode1_spawn_batch(float dt)
{
    for (int i = 0; i < SPAWNS_PER_FRAME; i++) {
        total_spawn_attempts++;

        malloc_entity *e = (malloc_entity *)arena_alloc(
            &arena, sizeof(malloc_entity)
        );
        if (!e) {
            total_spawn_failures++;
            continue;  /* arena exhausted */
        }

        total_spawn_successes++;
        e->x    = (float)(rand() % 780 + 10);
        e->y    = (float)(rand() % 580 + 10);
        e->dx   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        e->dy   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        e->size = 4.0f;
        e->color = 0xFF00FF00; /* green = arena-managed */
        e->age  = 0.0f;
    }
}
```

When you switch from mode 0 to mode 1, the HUD tells the story:

- **Memory used** still climbs (we are not deleting anything yet --- that comes in later modes). But it climbs from a pre-allocated pool, not from the OS heap.
- **Frame time** flatlines. No realloc copies. No heap fragmentation. No lock contention. The per-spawn cost dropped from hundreds of nanoseconds to three nanoseconds.
- **Allocations per frame** drops to zero in the OS-call sense. The arena bump is so cheap it is essentially free.

The arena has one limitation in this mode: it does not reclaim memory from dead entities. When the arena is full, spawning stops --- same wall as mode 0, just reached later (29 minutes instead of immediately) and without the frame-time degradation along the way. Modes 2--9 address the reclamation problem.

> **Cross-reference:** The `arena.h` in the Platform Foundation course is the production implementation of this pattern. Lesson 17 (Memory Arena Lab) proved arenas eliminate frame-time variance. This mode proves they eliminate allocation-induced degradation under sustained growth.

> **Handmade Hero connection:** Casey pre-allocates permanent and transient arenas at startup. The game loop never calls malloc. This is exactly what mode 1 does. The difference between Casey's approach and mode 0 is not a small optimization --- it is a fundamental architectural decision that determines whether the game can ship.

---

## Step 4 --- Mode 2: Pool recycling with free list (zero alloc in game loop)

The arena from mode 1 eliminated OS allocation overhead, but it never reclaims memory. Mode 2 switches to the `thing_pool` you built in lessons 02--10 and demonstrates the pattern that solves both problems: fixed-size slots with an intrusive free list. Zero malloc calls. Zero memory growth. Instant slot reuse.

This is EXACTLY lessons 07-08 (the thing_pool pattern) applied at scale. Dead entities mark their slot as unused. The free list links the dead slot. New entities take the first free slot. The pool never grows. The pool never shrinks. It cycles.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 2: Pool recycling with free list           */
/* ══════════════════════════════════════════════════════ */

/* Uses the thing_pool directly. No separate allocation. */

static void mode2_spawn_batch(thing_pool *pool, float dt)
{
    for (int i = 0; i < SPAWNS_PER_FRAME; i++) {
        total_spawn_attempts++;

        thing_ref ref = thing_pool_add(pool, THING_KIND_PARTICLE);
        thing *t = thing_pool_get_ref(pool, ref);

        if (!thing_is_valid(t)) {
            /* Pool full. thing_pool_add returned nil (index 0). */
            /* This is the nil sentinel from lesson 06 doing its job. */
            total_spawn_failures++;
            continue;
        }

        total_spawn_successes++;
        t->x    = (float)(rand() % 780 + 10);
        t->y    = (float)(rand() % 580 + 10);
        t->dx   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        t->dy   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        t->size = 4.0f;
        t->color = 0xFF44AAFF; /* blue = pool-managed */

        thing_idx idx = ref.index;
        entity_ttl[idx]         = 3.0f + (float)(rand() % 40) * 0.1f;
        entity_age[idx]         = 0.0f;
        entity_spawn_frame[idx] = current_frame;
        entity_update_group[idx] = idx % 4;
    }
}
```

When a pool entity dies (see mode 3 for the lifetime logic), `thing_pool_remove` puts its slot onto the intrusive free list. The next `thing_pool_add` call finds it there in O(1). No scanning. No searching. The slot goes from alive to dead to alive again in constant time.

```
Pool slot recycling visualization:

  Frame 100:
  Slot:  [0]   [1]    [2]    [3]    [4]    [5]    [6]    [7]
         NIL   ALIVE  ALIVE  ALIVE  ALIVE  ALIVE  dead→  dead→
         ^                                        free   free
         sentinel                                 list   list(-1)

  Free list: 6 → 7 → -1

  Frame 101: entity in slot 3 dies, new entity spawns
  Step 1: thing_pool_remove(pool, 3)
    Slot 3 → dead. next_free = 6. free list: 3 → 6 → 7 → -1
    Generation[3] increments (stale refs invalidated).

  Step 2: thing_pool_add(pool, PARTICLE)
    Takes slot 3 (head of free list). free list: 6 → 7 → -1
    Slot 3 → ALIVE with new entity. Same memory. Zero allocation.
```

The parallel arrays for growth-lab-specific per-entity data use the same index:

```c
/* Parallel arrays --- indexed by thing_idx, same as lesson 14 */
static float    entity_ttl[MAX_THINGS];          /* time to live (seconds)       */
static float    entity_age[MAX_THINGS];          /* time alive (seconds)         */
static uint32_t entity_spawn_frame[MAX_THINGS];  /* frame number when spawned    */
static float    entity_distance_sq[MAX_THINGS];  /* squared distance from focal  */
static int      entity_update_group[MAX_THINGS]; /* amortized update bucket      */
```

> **Why parallel arrays?** These fields do not belong in the `thing` struct. They are specific to this lab. The parallel array pattern (lesson 14) keeps `things.h` clean while extending entities with lab-specific data. The index is the link --- `entity_ttl[idx]` is the TTL for `things[idx]`.

In mode 2, pool entities also get a simple lifetime so the pool does not fill up and stay full (that would be the same wall as mode 0). Entities that expire are removed, their slots go to the free list, and new entities reuse them. The steady state looks like this:

```
Pool state in mode 2 (steady state ~60-70%):

  Alive: ~650/1024    Free list: ~374 slots
  Spawn success rate: ~100%
  Malloc calls in game loop: 0
  Memory growth: 0 bytes (pool is fixed-size)
  Frame time: rock-solid
```

The key insight: **the pool never allocates and never frees in the OS sense.** All 1024 slots exist from the moment `thing_pool_init` runs. The free list is an accounting system that tracks which slots are in use. Adding an entity is O(1). Removing an entity is O(1). No system calls. No locks. No fragmentation. This is the pattern that makes everything else in this lesson possible.

> **Cross-reference:** Game Programming Patterns Ch 19 (Object Pool) describes this exact technique. The intrusive free list from lesson 08 is the implementation detail that makes it O(1). The generational ID from lesson 09 is what makes recycling safe --- stale references to the old entity in that slot will fail the generation check.

---

## Step 5 --- Mode 3: Lifetime cleanup (entities expire)

With the pool recycling foundation from mode 2, we add the simplest stabilization strategy: give every entity a time-to-live (TTL). When the TTL expires, remove the entity. The pool reclaims the slot. New entities spawn into it.

This is the same concept as a garbage collector's reachability analysis, but simpler: instead of tracing references, each entity carries its own death clock.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 3: Lifetime cleanup                        */
/* ══════════════════════════════════════════════════════ */

static void mode3_update(thing_pool *pool, float dt)
{
    /* ---- Expire old entities ---- */
    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;

        entity_age[i] += dt;

        if (entity_age[i] >= entity_ttl[i]) {
            /* TTL expired. Remove from pool. */
            thing_pool_remove(pool, i);
            /* Slot is now on the free list (lesson 08). */
            /* Next thing_pool_add will reuse it (lesson 08). */
            /* Any stale thing_ref to this slot will fail
               the generation check (lesson 09). */
        }
    }

    /* ---- Update living entities ---- */
    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        thing *t = &pool->things[i];
        t->x += t->dx * dt;
        t->y += t->dy * dt;
        if (t->x < 0 || t->x > 800) t->dx = -t->dx;
        if (t->y < 0 || t->y > 600) t->dy = -t->dy;
        t->color = 0xFF00FF00; /* green = lifetime-managed */
    }

    /* ---- Spawn new entities ---- */
    mode2_spawn_batch(pool, dt);
}
```

Three lessons are working together in those few lines of the expire loop:

1. **Lesson 08 (Free List):** `thing_pool_remove` puts the slot onto the intrusive free list. The next `thing_pool_add` will find it there and reuse it instead of bumping `next_empty_slot`.

2. **Lesson 09 (Generational IDs):** When the slot is removed, its generation counter increments. Any `thing_ref` held by other code (a chasing AI, a parent link, a targeting system) will fail the generation check on the next `thing_pool_get_ref` call and return nil safely.

3. **Lesson 06 (Nil Sentinel):** If anything still tries to use the removed entity through a stale index, `thing_pool_get` returns `&things[0]` --- the nil sentinel. No crash.

The steady state emerges naturally:

```
Pool state with lifetime cleanup (steady state ~60%):

  Slot:  [0]   [1]    [2]    [3]    [4]    [5]    [6]   ...
         NIL   ALIVE  dead → ALIVE  dead → dead → ALIVE ...
         ^            free          free   free
         sentinel     list          list   list

  Deaths per second: ~50 (matches spawn rate)
  Pool occupancy: ~600/1024 (~60%)
  Spawn success rate: ~100%
```

Entities spawn at ~50/sec. Entities die at ~50/sec (because their TTLs average 5 seconds and there are ~250 alive at any time with 5-second lives = 50 deaths/sec). The pool oscillates around 60% occupancy. It could run forever.

When you watch this mode, you see entities appear, drift around for a few seconds, then vanish. The pool occupancy in the HUD oscillates but never approaches 100%. The spawn success rate stays at or near 100%. This is the first mode that achieves true indefinite operation.

> **Why?** In JavaScript, you might use `setTimeout(() => entity.destroy(), 5000)`. In C, you do not have a runtime managing deferred calls. You track the TTL explicitly and check it every frame. This is the "ticking clock" pattern --- the same pattern used for projectile lifetimes, particle decay, and buff durations in every game engine.

---

## Step 6 --- Mode 4: Budget cap (hard limits, guaranteed frame time)

Lifetime cleanup keeps the pool from overflowing, but it does not care about frame time. If the update loop for 600 entities takes 12ms and your budget is 16.67ms, you have only 4.67ms left for rendering. What if you need less?

A budget system works in the opposite direction: instead of the entities managing their own death, the *spawner* manages how many entities it creates based on how much frame time is available.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 4: Budget cap                              */
/* ══════════════════════════════════════════════════════ */

#define FRAME_BUDGET_MS 12.0f  /* max ms the growth system may consume */

static float last_update_ms = 0.0f;

static void mode4_update(thing_pool *pool, float dt)
{
    /* ---- Phase 1: measure how much time the update took last frame ---- */
    float pressure = last_update_ms / FRAME_BUDGET_MS;

    /* ---- Phase 2: adjust spawn rate based on pressure ---- */
    int spawns_this_frame = SPAWNS_PER_FRAME;
    if (pressure > 0.9f) {
        spawns_this_frame = 0;          /* over budget: stop spawning entirely */
    } else if (pressure > 0.7f) {
        spawns_this_frame = 1;          /* near budget: spawn conservatively   */
    }
    /* else: under budget, spawn normally */

    /* ---- Phase 3: spawn with throttled rate ---- */
    for (int i = 0; i < spawns_this_frame; i++) {
        total_spawn_attempts++;
        thing_ref ref = thing_pool_add(pool, THING_KIND_PARTICLE);
        thing *t = thing_pool_get_ref(pool, ref);
        if (!thing_is_valid(t)) {
            total_spawn_failures++;
            continue;
        }
        total_spawn_successes++;
        t->x    = (float)(rand() % 780 + 10);
        t->y    = (float)(rand() % 580 + 10);
        t->dx   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        t->dy   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        t->size = 4.0f;
        t->color = 0xFFFF8800; /* orange = budget-managed */
        entity_ttl[ref.index] = 5.0f;
        entity_age[ref.index] = 0.0f;
    }

    /* ---- Phase 4: expire + update (lifetime still applies) ---- */
    double start = platform_get_time_ms();
    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        entity_age[i] += dt;
        if (entity_age[i] >= entity_ttl[i]) {
            thing_pool_remove(pool, i);
            continue;
        }
        thing *t = &pool->things[i];
        t->x += t->dx * dt;
        t->y += t->dy * dt;
        if (t->x < 0 || t->x > 800) t->dx = -t->dx;
        if (t->y < 0 || t->y > 600) t->dy = -t->dy;
    }
    last_update_ms = (float)(platform_get_time_ms() - start);
}
```

The budget system is a feedback loop:

```
Frame N:
  update takes 11ms → pressure = 11/12 = 0.92 → OVER THRESHOLD
  ↓
Frame N+1:
  spawns_this_frame = 0 → entity count drops slightly (deaths > births)
  update takes 10ms → pressure = 10/12 = 0.83 → still high
  ↓
Frame N+2:
  spawns_this_frame = 1 → count stabilizes
  update takes 9ms → pressure = 9/12 = 0.75 → near threshold
  ↓
Frame N+3:
  spawns_this_frame = 1 → slow growth resumes
  update takes 9.5ms → steady state achieved
```

The HUD in this mode shows the pressure value and current spawn rate. When pressure exceeds 0.9, the spawn rate text turns red and shows "THROTTLED." When it drops below 0.7, it turns green and shows the normal rate. The entity count self-regulates to whatever the machine can handle.

> **JS bridge: budget cap is like `useDeferredValue` in React.** React 18's `useDeferredValue` delays non-urgent re-renders when the main thread is busy. The budget cap here does the same thing for entity spawning: if the update loop is taking too long (high pressure), the spawner defers its work (spawns fewer or zero entities) to keep frame time under budget. It is a feedback loop: measure how much time you spent last frame, then decide how much work to do this frame. The same concept appears in `requestIdleCallback` --- only do non-essential work when the browser has spare time. The budget cap is `requestIdleCallback` for game entity spawning.

This is a real technique. Every open-world game has a spawn budget that throttles NPC creation based on system load. You do not spawn 200 enemies if the CPU is already struggling to update 100. The spawner adapts to the machine it is running on --- a fast machine gets more entities, a slow machine gets fewer, and both maintain 60 FPS.

> **Alternative approach:** Instead of throttling the spawner, you could dynamically remove entities when over budget. Some engines kill the least-important entities first (particles before NPCs, distant before near). Budget-on-spawn is simpler. Budget-on-kill gives more responsive adaptation but requires a priority system.

---

## Step 7 --- Mode 5: Spatial streaming (simulate only nearby)

In an open-world game, the world is bigger than what fits on screen. You might have 10,000 entities in the game state, but only 200 are near the player. Do you simulate all 10,000? No. You simulate the 200 that are nearby and put the rest to sleep.

Mode 5 simulates this by defining a focal point (the center of the window) and a streaming radius. Entities inside the radius get full updates. Entities outside it are idle --- they exist in the pool, they occupy a slot, but they do not move, do not collide, do not consume frame time.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 5: Spatial streaming                       */
/* ══════════════════════════════════════════════════════ */

#define STREAM_RADIUS    200.0f
#define STREAM_RADIUS_SQ (STREAM_RADIUS * STREAM_RADIUS)

static float focal_x = 400.0f;  /* center of 800x600 window */
static float focal_y = 300.0f;

static int active_count = 0;
static int idle_count   = 0;

static void mode5_update(thing_pool *pool, float dt)
{
    active_count = 0;
    idle_count   = 0;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;

        thing *t = &pool->things[i];
        float dx_f = t->x - focal_x;
        float dy_f = t->y - focal_y;
        entity_distance_sq[i] = dx_f * dx_f + dy_f * dy_f;

        if (entity_distance_sq[i] <= STREAM_RADIUS_SQ) {
            /* ACTIVE: full simulation */
            active_count++;
            t->x += t->dx * dt;
            t->y += t->dy * dt;
            if (t->x < 0 || t->x > 800) t->dx = -t->dx;
            if (t->y < 0 || t->y > 600) t->dy = -t->dy;
            t->color = 0xFF00FF00; /* green = active */
        } else {
            /* IDLE: entity exists but does not simulate */
            idle_count++;
            t->color = 0xFF444444; /* dark gray = idle */
        }

        /* Lifetime still ticks for all entities (active or idle) */
        entity_age[i] += dt;
        if (entity_age[i] >= entity_ttl[i]) {
            thing_pool_remove(pool, i);
        }
    }

    mode2_spawn_batch(pool, dt);
}
```

When you see this running, you get a circle of green entities in the center actively bouncing around, surrounded by a haze of dark gray entities that sit perfectly still:

```
Window visualization:

  ┌─────────────────────────────────────┐
  │  . . .     . . .     . . .         │
  │     . .         . .       .         │  <-- dark gray (idle)
  │        ╔═══════════════╗            │
  │   .    ║ * * * * * * * ║    .       │
  │  .     ║  *  * * *  *  ║     .     │  <-- green (active)
  │        ║ *  * *  * * * ║            │
  │   .    ╚═══════════════╝    .       │
  │     . .         . .       .         │
  │  . . .     . . .     . . .         │
  └─────────────────────────────────────┘
         streaming radius ───┘
```

The HUD shows "Active: 200, Idle: 600." You are simulating 200 entities but the pool holds 800. The frame time reflects the active count, not the total count. If you could move the focal point (a simple extension: tie it to WASD input), the active/idle sets would shift dynamically --- exactly like streaming in an open world.

This is exactly how every open-world engine works. Bethesda's Creation Engine has "loaded" and "unloaded" cells. Rockstar's RAGE engine has streaming zones. Unity has scene loading boundaries. The principle is identical: simulate what matters, sleep the rest.

> **Handmade Hero connection:** Casey implements a similar concept with "simulation regions" --- entities near the camera run their full simulation, while distant entities are either dormant or run simplified updates. The pool structure does not change. Only the iteration logic decides what gets processed.

---

## Step 8 --- Mode 6: Simulation LOD (near = full, far = reduced)

Streaming has a hard boundary: inside the radius you get full simulation, outside you get nothing. LOD (Level of Detail) adds a gradient. Near entities get full updates every frame. Medium entities get updates every other frame. Far entities get updates every fourth frame. The visual result is smooth motion near the camera and slightly choppy motion in the distance --- exactly like geometric LOD in rendering, but for simulation.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 6: Simulation LOD                          */
/* ══════════════════════════════════════════════════════ */

typedef enum {
    SIM_LOD_FULL    = 1,   /* update every frame     */
    SIM_LOD_HALF    = 2,   /* update every 2nd frame */
    SIM_LOD_QUARTER = 4    /* update every 4th frame */
} sim_lod;

#define LOD_NEAR_SQ   (150.0f * 150.0f)
#define LOD_MID_SQ    (300.0f * 300.0f)

static sim_lod get_entity_lod(int idx)
{
    if (entity_distance_sq[idx] <= LOD_NEAR_SQ)  return SIM_LOD_FULL;
    if (entity_distance_sq[idx] <= LOD_MID_SQ)   return SIM_LOD_HALF;
    return SIM_LOD_QUARTER;
}

static void mode6_update(thing_pool *pool, float dt)
{
    int full_count = 0, half_count = 0, quarter_count = 0;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;

        thing *t = &pool->things[i];

        /* Compute distance to focal point */
        float dx_f = t->x - focal_x;
        float dy_f = t->y - focal_y;
        entity_distance_sq[i] = dx_f * dx_f + dy_f * dy_f;

        sim_lod lod = get_entity_lod(i);

        /* Only update if this frame aligns with the entity's LOD rate */
        if ((current_frame % lod) == 0) {
            /* Scale dt by LOD to compensate for skipped frames */
            float effective_dt = dt * (float)lod;
            t->x += t->dx * effective_dt;
            t->y += t->dy * effective_dt;
            if (t->x < 0 || t->x > 800) t->dx = -t->dx;
            if (t->y < 0 || t->y > 600) t->dy = -t->dy;
        }

        /* Color indicates LOD tier */
        switch (lod) {
            case SIM_LOD_FULL:    t->color = 0xFF00FF00; full_count++;    break;
            case SIM_LOD_HALF:    t->color = 0xFFFFFF00; half_count++;    break;
            case SIM_LOD_QUARTER: t->color = 0xFFFF4400; quarter_count++; break;
        }

        /* Lifetime always ticks at full rate */
        entity_age[i] += dt;
        if (entity_age[i] >= entity_ttl[i]) {
            thing_pool_remove(pool, i);
        }
    }

    mode2_spawn_batch(pool, dt);
}
```

The key detail is `effective_dt = dt * lod`. When an entity only updates every 4th frame, it multiplies dt by 4 to cover the distance it would have traveled in those skipped frames. The motion looks slightly choppy at SIM_LOD_QUARTER but covers the same ground. Position is conserved. Physics stays approximately correct.

```
LOD tiers visualized:

  Frame 0:  FULL updates + HALF updates + QUARTER updates  (all 3)
  Frame 1:  FULL updates                                   (only full)
  Frame 2:  FULL updates + HALF updates                    (full + half)
  Frame 3:  FULL updates                                   (only full)
  Frame 4:  FULL updates + HALF updates + QUARTER updates  (all 3 again)

  Work distribution across frames:
  ┌────────┬──────┬──────┬──────┬──────┬──────┐
  │ Frame  │  0   │  1   │  2   │  3   │  4   │
  ├────────┼──────┼──────┼──────┼──────┼──────┤
  │ Full   │  *   │  *   │  *   │  *   │  *   │
  │ Half   │  *   │      │  *   │      │  *   │
  │ Quarter│  *   │      │      │      │  *   │
  ├────────┼──────┼──────┼──────┼──────┼──────┤
  │ Total  │ 800  │ 300  │ 550  │ 300  │ 800  │
  └────────┴──────┴──────┴──────┴──────┴──────┘
```

Frame 0 and 4 are the expensive frames. Frames 1 and 3 are cheap. The average work per frame is significantly lower than updating all 800 entities every frame. The HUD shows this as fluctuating frame times with a lower average.

On screen, you see three concentric zones of color: green (near, full rate), yellow (mid, half rate), and orange (far, quarter rate). The green entities move smoothly. The yellow entities have a slight stutter. The orange entities visibly jump every 4 frames. The player's eye is on the center of the screen, so the choppiness at the edges is acceptable --- exactly like how you do not notice low-LOD geometry at the edges of your field of view in a 3D game.

> **Why?** In JavaScript, you might throttle animations with `requestIdleCallback` or skip rendering frames. LOD for simulation is the same idea applied to game logic: spend your CPU budget where the player is looking, save it where they are not.

---

## Step 9 --- Mode 7: Amortized work (stagger across frames)

Every mode so far updates all living entities (or some subset) on every frame. What if you split all entities into groups and only update one group per frame?

Amortized work divides entities into N buckets based on their slot index. Each frame, only one bucket gets updated. Over N frames, every entity has been updated exactly once. The per-frame cost drops to ~(total_entities / N), at the cost of each entity's state being slightly stale --- it reflects a position from up to N frames ago.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 7: Amortized work                          */
/* ══════════════════════════════════════════════════════ */

#define AMORTIZE_BUCKETS 4

static void mode7_update(thing_pool *pool, float dt)
{
    int active_bucket = current_frame % AMORTIZE_BUCKETS;
    int updated_count = 0;
    int skipped_count = 0;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;

        thing *t = &pool->things[i];

        /* Lifetime always ticks (never amortized --- death must be punctual) */
        entity_age[i] += dt;
        if (entity_age[i] >= entity_ttl[i]) {
            thing_pool_remove(pool, i);
            continue;
        }

        if (entity_update_group[i] == active_bucket) {
            /* This entity's bucket is active --- full update with 4x dt */
            float effective_dt = dt * AMORTIZE_BUCKETS;
            t->x += t->dx * effective_dt;
            t->y += t->dy * effective_dt;
            if (t->x < 0 || t->x > 800) t->dx = -t->dx;
            if (t->y < 0 || t->y > 600) t->dy = -t->dy;
            t->color = 0xFF00FFFF; /* cyan = updated this frame */
            updated_count++;
        } else {
            t->color = 0xFF006666; /* dark cyan = waiting for bucket */
            skipped_count++;
        }
    }

    mode2_spawn_batch(pool, dt);
}
```

The visual effect is distinctive: on any given frame, about 25% of the entities flash cyan (just updated) while 75% are dark cyan (waiting). Over 4 frames, all of them flash once. The frame time drops to roughly 25% of what a full update costs.

```
Amortization schedule (4 buckets):

  Frame 0: update bucket 0 → entities 1,5,9,13,17,...
  Frame 1: update bucket 1 → entities 2,6,10,14,18,...
  Frame 2: update bucket 2 → entities 3,7,11,15,19,...
  Frame 3: update bucket 3 → entities 4,8,12,16,20,...
  Frame 4: update bucket 0 → entities 1,5,9,13,17,...  (cycle repeats)

  Per-frame work: ~200 updates instead of ~800
  Frame time: drops to ~25% of full update
  Visual fidelity: slightly choppy (4-frame latency)
  Frame-time stability: excellent (consistent load per frame)
```

The `effective_dt = dt * AMORTIZE_BUCKETS` compensates for the skipped frames. When an entity only updates every 4th frame, it moves 4x the distance in that one update. Over 4 frames, the total displacement is the same as if it had updated every frame. The motion is jumpier --- each entity teleports a small amount every 4 frames instead of sliding smoothly every frame --- but the position is correct and the frame time is rock-solid.

Notice the lifetime check is NOT amortized. Death must be punctual --- if an entity's TTL expires, it must be removed that frame, not 3 frames later when its bucket comes around. An entity that should be dead but is still alive for 3 extra frames is a visible bug (it might block a doorway, collide with the player, or occupy a pool slot that something else needs). Amortize the cheap stuff (movement, animation). Never amortize the critical stuff (death, collision response, state transitions).

> **Why?** In web development, this is similar to React's time-slicing or `requestIdleCallback` --- spreading work across frames to avoid janking the main thread. The principle is identical: if the total work exceeds a single frame's budget, split it across multiple frames and accept slightly stale state as the trade-off.

> **JS bridge: amortized work is like `requestIdleCallback` --- do work when there is time.** In a JavaScript app, if you need to process 10,000 search results, you do not do it all in one `requestAnimationFrame` callback (that would jank the UI). Instead you process 2,500 per frame across 4 frames. The user sees results appearing incrementally, and the UI stays smooth. Amortized entity updates are the same concept: split 800 entity updates into 4 buckets of 200, process one bucket per frame. Each entity's position is slightly stale (up to 4 frames behind), but the frame time is rock-solid. The `effective_dt = dt * 4` compensation ensures entities move the correct total distance --- like catching up on missed homework in one big session instead of doing a little each day.

---

## Step 10 --- Mode 8: Threaded update (parallel entity processing)

This mode adds the most complex optimization and the one you should reach for LAST: splitting entity updates across multiple CPU cores. The reason it is last is not that it is ineffective --- on a 4-core machine, a perfect split gives you 4x throughput. The reason is that the complexity cost is enormous, the debugging difficulty is extreme, and the diminishing returns are real.

> **Why `pthread`?** `pthread` (POSIX threads) is the C standard for creating threads on Linux and macOS. `pthread_create` spawns a new thread (a separate execution context running in parallel), and `pthread_join` waits for it to finish. This is the C equivalent of spawning a Web Worker in JavaScript: `const worker = new Worker('update.js')`. Both create a separate execution context that runs in parallel with the main thread. The key difference is that Web Workers communicate via `postMessage` (copying data between isolated memory spaces), while pthreads share the same memory space. Shared memory makes pthreads faster (no copying), but also more dangerous --- two threads writing the same memory location at the same time is a **data race**, which produces unpredictable, unreproducible bugs. This is why threading is the LAST optimization, not the first.

Here is why threading works for this specific case: entity updates are **independent.** Each entity reads its own position, applies its own velocity, bounces off walls, and writes its own new position. Entity A's update does not read or write entity B's data. This means you can split the array in half, update each half on a separate thread, and the results are identical to a single-threaded update --- no races, no locks needed.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 8: Threaded update                         */
/* ══════════════════════════════════════════════════════ */

#include <pthread.h>

typedef struct thread_work {
    thing_pool *pool;
    float dt;
    int range_start;   /* first slot index (inclusive) */
    int range_end;     /* last slot index (exclusive)  */
    int updated;       /* output: how many were updated */
} thread_work;

static void *thread_update_range(void *arg)
{
    thread_work *work = (thread_work *)arg;
    thing_pool *pool = work->pool;
    float dt = work->dt;
    int count = 0;

    for (int i = work->range_start; i < work->range_end; i++) {
        if (!pool->used[i]) continue;

        thing *t = &pool->things[i];
        t->x += t->dx * dt;
        t->y += t->dy * dt;
        if (t->x < 0 || t->x > 800) t->dx = -t->dx;
        if (t->y < 0 || t->y > 600) t->dy = -t->dy;
        t->color = 0xFFFF88FF; /* pink = thread-updated */
        count++;
    }

    work->updated = count;
    return NULL;
}
```

The main thread splits the work and joins:

```c
static void mode8_update(thing_pool *pool, float dt)
{
    /* ---- Phase 1: expire entities (single-threaded, modifies free list) ---- */
    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        entity_age[i] += dt;
        if (entity_age[i] >= entity_ttl[i]) {
            thing_pool_remove(pool, i);
        }
    }

    /* ---- Phase 2: split update range in half ---- */
    int midpoint = pool->next_empty / 2;
    if (midpoint < 1) midpoint = 1;

    thread_work work_a = {
        .pool = pool, .dt = dt,
        .range_start = 1,        .range_end = midpoint,
        .updated = 0
    };
    thread_work work_b = {
        .pool = pool, .dt = dt,
        .range_start = midpoint, .range_end = pool->next_empty,
        .updated = 0
    };

    /* ---- Phase 3: launch thread for first half, main does second half ---- */
    pthread_t worker;
    pthread_create(&worker, NULL, thread_update_range, &work_a);

    /* Main thread processes the second half directly */
    thread_update_range(&work_b);

    /* ---- Phase 4: wait for worker thread to finish ---- */
    pthread_join(worker, NULL);

    /* ---- Phase 5: spawn new entities (single-threaded, modifies pool) ---- */
    mode2_spawn_batch(pool, dt);
}
```

Let me be very explicit about why this is safe --- and why most threading in game engines is NOT this simple.

**Why this specific case needs no locks:**

1. **Each thread writes to different array indices.** Thread A updates slots `[1, midpoint)`. Thread B updates slots `[midpoint, next_empty)`. They never write to the same slot. No data race.

2. **Each entity's update only reads its own fields.** `t->x += t->dx * dt` reads `t->dx` and writes `t->x`. It does not read another entity's position. No shared read/write conflict.

3. **The `used[]` array is read-only during the threaded phase.** We do all removals (which modify `used[]` and the free list) in phase 1, before threading starts. We do all spawns (which also modify the pool) in phase 5, after threading ends. The threaded phase only reads `used[]` to skip dead slots.

4. **The pool structure itself is not modified.** No `thing_pool_add`, no `thing_pool_remove`, no free list manipulation happens during the threaded phase.

**Why this is the LAST optimization, not the first:**

1. **Complexity cost.** The single-threaded version is 10 lines. The threaded version is 50 lines plus a struct, a function pointer, pthread include, and careful phase separation. If you get the phase boundaries wrong --- say, you accidentally spawn during the threaded phase --- you corrupt the free list and get silent, non-deterministic bugs that manifest only under specific timing conditions.

2. **Debugging difficulty.** A race condition in a single-threaded program produces the same wrong result every time. A race condition in a multi-threaded program produces different wrong results on different runs, on different machines, on different days. Thread sanitizer helps. But "helps" is not "solves."

3. **Diminishing returns.** On a 4-core machine with a 2-way split, you get at most 2x speedup (Amdahl's law limits you further because phases 1 and 5 are serial). If your single-threaded update takes 4ms, you save 2ms. If amortized work (mode 7) already brought your update from 8ms to 2ms, threading saves you 1ms. That 1ms is real, but is it worth the complexity?

4. **When to use it.** Threading makes sense when you are CPU-bound on entity updates and you have exhausted all single-threaded optimizations (amortized work, LOD, streaming). In practice, this means large-scale simulations: thousands of AI agents, tens of thousands of particles, physics simulations with independent bodies. For 800 bouncing dots, threading is overkill. For 80,000 AI soldiers, it is necessary.

```
Threading timeline per frame:

  Phase 1 (serial):   expire entities         ~0.5 ms
  Phase 2 (parallel): update half A (thread)  ─┐
                       update half B (main)    ─┤ ~1.5 ms (overlapped)
  Phase 3 (serial):   spawn entities           ~0.2 ms
                                              ─────────
                       Total:                   ~2.2 ms

  Compare single-threaded:
  Phase 1: expire     ~0.5 ms
  Phase 2: update ALL ~3.0 ms
  Phase 3: spawn      ~0.2 ms
            Total:     ~3.7 ms

  Speedup: 3.7 / 2.2 = 1.68x (not 2x because of serial phases)
```

The HUD in mode 8 shows two extra metrics: "Thread A updated: 400" and "Thread B updated: 400." If they are roughly equal, the split is balanced. If one is much larger (because more entities are clustered in one half of the index range), the split is unbalanced and you are leaving performance on the table. A production implementation would use work-stealing or adaptive splitting, but for this lab, a simple midpoint split demonstrates the principle.

> **Cross-reference:** Game Engine Architecture Ch 7 (The Game Loop and Real-Time Simulation) discusses task-based parallelism and fork-join patterns. The approach here is a minimal fork-join: fork one worker, join before the next serial phase.

---

## Step 11 --- Mode 9: Production combo (all strategies combined)

No production engine uses just one of these techniques. They use the right combination. Mode 9 integrates five strategies into a single cohesive system:

1. **Arena allocation** --- all entity memory comes from a pre-allocated arena. Zero OS calls.
2. **Pool recycling** --- the `thing_pool` free list handles slot reuse. Zero memory growth.
3. **Lifetime cleanup** --- entities expire after their TTL. The pool never fills up.
4. **Budget cap** --- the spawner throttles based on frame-time pressure. Frame time stays under budget.
5. **Amortized work** --- entity movement updates are staggered across 4 frames. Per-frame cost is smoothed.

```c
/* ══════════════════════════════════════════════════════ */
/*        Mode 9: Production combo                        */
/* ══════════════════════════════════════════════════════ */

static void mode9_update(thing_pool *pool, float dt)
{
    /* ---- Budget check: throttle spawner if under pressure ---- */
    float pressure = last_update_ms / FRAME_BUDGET_MS;
    int spawns_this_frame = SPAWNS_PER_FRAME;
    if (pressure > 0.9f)      spawns_this_frame = 0;
    else if (pressure > 0.7f) spawns_this_frame = 1;

    /* ---- Spawn (budget-throttled) ---- */
    for (int i = 0; i < spawns_this_frame; i++) {
        total_spawn_attempts++;
        thing_ref ref = thing_pool_add(pool, THING_KIND_PARTICLE);
        thing *t = thing_pool_get_ref(pool, ref);
        if (!thing_is_valid(t)) {
            total_spawn_failures++;
            continue;
        }
        total_spawn_successes++;
        t->x    = (float)(rand() % 780 + 10);
        t->y    = (float)(rand() % 580 + 10);
        t->dx   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        t->dy   = ((float)(rand() % 200) - 100.0f) * 0.5f;
        t->size = 4.0f;
        t->color = 0xFFFFFFFF; /* white = production-managed */
        entity_ttl[ref.index]         = 3.0f + (float)(rand() % 40) * 0.1f;
        entity_age[ref.index]         = 0.0f;
        entity_spawn_frame[ref.index] = current_frame;
        entity_update_group[ref.index] = ref.index % AMORTIZE_BUCKETS;
    }

    /* ---- Update: amortized + lifetime ---- */
    int active_bucket = current_frame % AMORTIZE_BUCKETS;
    double start = platform_get_time_ms();

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;

        /* Lifetime always ticks at full rate */
        entity_age[i] += dt;
        if (entity_age[i] >= entity_ttl[i]) {
            thing_pool_remove(pool, i);
            continue;
        }

        /* Movement is amortized: only update this entity's bucket */
        if (entity_update_group[i] == active_bucket) {
            thing *t = &pool->things[i];
            float effective_dt = dt * AMORTIZE_BUCKETS;
            t->x += t->dx * effective_dt;
            t->y += t->dy * effective_dt;
            if (t->x < 0 || t->x > 800) t->dx = -t->dx;
            if (t->y < 0 || t->y > 600) t->dy = -t->dy;
            t->color = 0xFFFFFFFF; /* white = just updated */
        } else {
            pool->things[i].color = 0xFFAAAAAA; /* light gray = waiting */
        }
    }

    last_update_ms = (float)(platform_get_time_ms() - start);
}
```

When you watch mode 9, you see white and light-gray entities (the amortization pattern from mode 7), a stable entity count (the lifetime pattern from mode 3), no spawn-rate surges (the budget pattern from mode 4), and rock-solid frame times (the combination of all three). The pool oscillates around 50--70% occupancy. The memory usage is constant. The system can run indefinitely.

This is what a production engine's entity system looks like. Not one technique. Not a silver bullet. The right combination of:

- **Arena/pool allocation** to eliminate OS calls
- **Lifetime management** to keep the pool from filling up
- **Budget throttling** to keep frame time under control
- **Amortized work** to smooth per-frame cost

The HUD in mode 9 shows all metrics: entity count, pool occupancy, spawn rate (with throttle indicator), frame time, amortization bucket, and memory usage. Everything is green. Everything is stable. That is the goal.

---

## Step 12 --- The real-engine progression

Here is the order you should apply these techniques in a real project. This is not arbitrary --- it is ordered by impact-per-complexity:

```
The real-engine progression:

  Step   Technique                What it solves              Complexity
  ────   ─────────                ──────────────              ──────────
  1.     Fixed pools + arenas     90% of allocation issues    Low
  2.     Lifetime + budget        Scaling + frame stability   Low
  3.     Amortized work           Frame-time spikes           Medium
  4.     Spatial streaming + LOD  Open-world scalability      Medium
  5.     Threading                CPU saturation              High (LAST)
```

Step 1 alone --- replacing malloc with a fixed pool and an arena --- solves the vast majority of performance problems in game entity systems. If you do nothing else from this lesson, do step 1. It costs almost nothing in complexity and eliminates an entire category of bugs (leaks, fragmentation, frame spikes from allocation).

Step 2 adds lifetime and budget management. These are simple counters and threshold checks. A few `if` statements per frame. The payoff is that your system self-regulates instead of hitting a wall.

Step 3 adds amortization. This requires buckets and effective-dt compensation. Slightly more complexity, but it turns unpredictable frame-time spikes into smooth, consistent load.

Step 4 adds spatial awareness. This requires distance calculations and a streaming radius. Medium complexity, but essential for any game with a world larger than the screen.

Step 5 adds threading. This requires thread creation, join, phase separation, and careful reasoning about shared state. High complexity. Only do this when you have profiling data proving that the entity update is the bottleneck AND you have already applied steps 1--4.

```
Impact of each technique on a system with 1000+ entities:

  Technique        | Frame time | Memory | Spawn success | Complexity
  ─────────        | ────────── | ────── | ───────────── | ──────────
  malloc baseline  | 15+ ms ↑   | ↑↑↑    | 100% → 0%    | None
  Arena (mode 1)   | 3 ms flat  | flat   | 100% → 0%    | Very low
  Pool (mode 2)    | 3 ms flat  | flat   | ~100%         | Low
  Lifetime (3)     | 3 ms flat  | flat   | ~100%         | Low
  Budget (4)       | <12 ms     | flat   | throttled     | Low
  Streaming (5)    | 1-2 ms     | flat   | ~100%         | Medium
  LOD (6)          | 1-3 ms avg | flat   | ~100%         | Medium
  Amortized (7)    | ~0.8 ms    | flat   | ~100%         | Medium
  Threaded (8)     | ~0.5 ms    | flat   | ~100%         | HIGH
  Production (9)   | ~0.8 ms    | flat   | ~100%         | Medium (combo)
```

> **Cross-reference:** GEA Ch 6 (Memory Management) covers arena and pool allocators as foundational engine infrastructure. GPP Ch 19 (Object Pool) covers the pool recycling pattern. Both books position these as "do this first, always."

> **Anton says:** "I don't want to construct my things. I'm just going to zero them." This is not laziness. It is the recognition that when you control the allocator (arena) and the data layout (flat array), zeroing is all you need. No constructors, no destructors, no RAII, no reference counting. `memset(&thing, 0, sizeof(thing))` is the constructor. `pool->used[idx] = 0` is the destructor.

> **Handmade Hero connection:** Casey pre-allocates permanent and transient arenas at startup. The game loop never calls malloc. The entity system uses generational handles and a fixed pool. Simulation regions control what gets updated. This is steps 1 + 2 + 4 of the progression --- the same subset that solves 95% of real-world entity system problems.

---

## Step 13 --- Building and running

The build flag follows the same pattern as every other lab:

```bash
# Build and run with Raylib
./build-dev.sh --backend=raylib --infinite-growth-lab -r

# Build and run with X11
./build-dev.sh --backend=x11 --infinite-growth-lab -r
```

The `--infinite-growth-lab` flag defines `ENABLE_INFINITE_GROWTH_LAB` at compile time. In `game.c`, the infinite growth lab scene is wrapped in `#ifdef ENABLE_INFINITE_GROWTH_LAB`. Without the flag, zero infinite growth lab code is compiled into the binary.

Press `I` to enter the Infinite Growth Lab scene. Press `Tab` to cycle through all 10 modes:

| Mode | Key | Color | What to watch |
|---|---|---|---|
| 0: malloc unbounded | Tab | Red | Memory climbs. Frame time degrades. realloc copies visible. |
| 1: Arena | Tab | Green | Frame time flatlines. Memory climbs but from arena, not heap. |
| 2: Pool + free list | Tab | Blue | Zero malloc. Slots recycle. Memory constant. |
| 3: Lifetime | Tab | Green | Entities die and respawn. Pool oscillates ~60%. Indefinite. |
| 4: Budget | Tab | Orange | Spawn rate drops when frame time rises. Self-regulating. |
| 5: Streaming | Tab | Green/gray | Green = active (near center). Gray = idle. Frame time tracks active count only. |
| 6: LOD | Tab | Green/yellow/orange | Three tiers visible. Near = smooth. Far = choppy. Frame time varies by frame. |
| 7: Amortized | Tab | Cyan/dark cyan | 25% flash each frame. Motion slightly choppy. Frame time rock-solid. |
| 8: Threaded | Tab | Pink | Same as single-threaded but faster. HUD shows per-thread counts. |
| 9: Production | Tab | White/gray | All strategies combined. Everything stable. Indefinite. |

Run each mode for at least 30 seconds. Watch the HUD. Compare the spawn success rates. Compare the frame times. Compare the memory usage. The story each mode tells:

- **Mode 0** shows WHY you need the other nine modes. The system degrades until it is unusable.
- **Modes 1--2** show how to eliminate allocation overhead. Arena kills OS calls. Pool kills memory growth.
- **Modes 3--4** show how to survive indefinitely. Lifetime prevents overflow. Budget prevents frame spikes.
- **Modes 5--6** show how to scale beyond the screen. Streaming and LOD reduce work proportional to distance.
- **Mode 7** shows how to smooth frame time. Amortized work trades latency for consistency.
- **Mode 8** shows the last resort. Threading is powerful but complex.
- **Mode 9** shows the production answer. Not one technique --- the right combination.

---

## Connection to your work

This capstone integrates every major concept from the course:

| Strategy | Pool feature it exercises | Lesson |
|---|---|---|
| malloc baseline (mode 0) | Contrast with pool approach | 02 (flat array), 17 (arena) |
| Arena allocation (mode 1) | Pre-allocated memory, bump pointer | 17 (Memory Arena Lab) |
| Pool recycling (mode 2) | `thing_pool_add/remove` → free list reuse | 07, 08 |
| Lifetime cleanup (mode 3) | Slot removal triggers free list + generation bump | 06, 08, 09 |
| Budget caps (mode 4) | Spawn throttling uses pool count | 07 |
| Spatial streaming (mode 5) | Iteration over `used[]` with spatial filter | 10 |
| Simulation LOD (mode 6) | Frame-modulo update with per-entity data | 14 (parallel arrays) |
| Amortized work (mode 7) | Bucketed iteration over pool | 10 |
| Threaded update (mode 8) | Range-split iteration, phase separation | 10 |
| Production combo (mode 9) | Arena + pool + lifetime + budget + amortized | ALL |
| All modes | Generational IDs protect stale refs during churn | 09 |
| All modes | Nil sentinel prevents crashes on pool overflow | 06 |

Every line of pool code you wrote in lessons 02--10 is load-bearing in this lab. The nil sentinel prevents crashes when the pool overflows. The free list ensures removed slots are reused instantly. Generational IDs prevent stale references when entities are recycled. The iterator skips dead slots efficiently. The flat array ensures cache-friendly iteration even at 1024 entities.

The strategies you built here appear in every production game engine:

- **Unreal Engine** has actor lifetimes, net relevancy (streaming), simulation LOD for AI, and task-based threading.
- **Unity** has object pooling, culling (streaming), LOD groups, and the Job System for threaded entity processing.
- **Handmade Hero** uses permanent/transient memory arenas (lifetime), simulation regions (streaming), and entity update budgets.
- **Dreams (PS4)** --- Anton's own engine --- uses the flat-array approach with these exact strategies to manage hundreds of thousands of sculptable elements.

> **Handmade Hero connection:** Casey's entity system handles overflow exactly as mode 2 does --- `thing_pool_add` returns nil, the caller checks, life goes on. The stabilization strategies in this lesson are what Casey builds on top of that foundation to make the world sustainable for hour-long play sessions.

---

## Course complete

You started with a flat array and an enum:

```c
enum thing_kind { THING_KIND_NIL = 0, THING_KIND_PLAYER, THING_KIND_TROLL };
struct thing { thing_kind kind; float x, y; };
thing things[1024];
```

Seventeen lessons later, that flat array has become a production entity pool with intrusive linked lists, nil sentinels, generational IDs, free list recycling, clean iteration, arena allocation, and a suite of ten stabilization strategies --- from naive malloc all the way to multi-threaded production combinations --- that can survive infinite pressure for hours.

Here is what you built, in order:

| Lesson | What you added | Why it matters |
|---|---|---|
| 01 | The problem | OOP hierarchies do not work for game entities |
| 02 | Fat struct + kind enum | One struct for all entity types --- simple, fast, memcpy-safe |
| 03 | Integer indices | References survive reallocation and serialize trivially |
| 04 | Intrusive singly-linked lists | Parent-child hierarchies without heap allocation |
| 05 | Doubly-linked + circular | O(1) removal, no boundary edge cases |
| 06 | Nil sentinel | Overflow and stale access are safe, not catastrophic |
| 07 | Slot map (add/remove/get) | Complete pool management |
| 08 | Free list | Removed slots reuse without scanning |
| 09 | Generational IDs | Stale references detected and rejected |
| 10 | Pool iterator | Clean iteration over living entities |
| 11 | Game demo: window + pool | The pool runs inside a real application |
| 12 | Game demo: movement + collision | Every pool feature exercised visually |
| 13 | Architecture review | Trade-offs understood, alternatives mapped |
| 14 | Particle Lab | 1000 entities prove scalability |
| 15 | Data Layout Lab | Memory organization affects performance |
| 16 | Spatial Partition Lab | Query strategy determines scalability |
| 17 | Memory Arena Lab | Allocation strategy determines frame stability |
| 18 | Infinite Growth Lab | ALL of the above survives infinite pressure --- 10 modes from naive malloc to production threading |

That is the difference between a tutorial toy and engine infrastructure. The tutorial toy works for the demo. Engine infrastructure works for the ship date --- and every hour of play time after it.

> **Anton says:** "That's it. That's the whole thing. A big array, some indices, a free list, a generation counter. No templates. No smart pointers. No STL. Just data."

He is right. And now you own every line.
