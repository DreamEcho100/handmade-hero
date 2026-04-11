# Lesson 17 --- Memory Arena Lab

## What we're building

A laboratory that runs an entity churn workload --- hundreds of spawns and destroys per second --- through four different memory allocation strategies: raw malloc/free, linear arena (bump pointer), pool allocator (fixed-size free list), and scratch arena (per-frame reset). You press `Tab` to cycle between allocator modes while the simulation runs. A performance HUD shows allocations per frame, bytes used, peak memory, and frame-time variance. A memory visualization overlay renders arena fill level as a progress bar and pool slots as a grid of occupied/free cells. The entity behavior --- positions, colors, movement --- is identical across all four modes. The only thing that changes is how memory is allocated and freed, and how stable your frame times are.

When you run `./build-dev.sh --backend=raylib --memory-arena-lab -r` and press `M`, the screen fills with entities spawning and dying in a continuous churn. In malloc mode, the HUD shows hundreds of allocations per frame and the frame-time graph jitters visibly. Press `Tab` to switch to Arena mode. The allocation count drops to zero (arena resets are not counted as individual allocations). The frame-time graph flatlines. Press `Tab` for Pool mode --- similar stability, but the overlay shows individual slots being reused. One more `Tab` for Scratch mode --- the arena resets every frame, all temporary allocations vanish instantly. Same entities. Same behavior. Radically different allocation cost and frame-time stability.

This is the lab that proves: production engines do not call malloc in the hot loop, and now you know why.

## What you'll learn

- **Why malloc/free kills frame-time stability** --- lock contention, fragmentation, unpredictable latency spikes
- **Linear arena allocation** --- bump a pointer, never free, reset the whole arena at once
- **Pool allocator** --- fixed-size slots with O(1) alloc and O(1) free via intrusive free list
- **Scratch arena** --- the "frame allocator" that resets every frame, zero cleanup required
- **Lifetime categories** --- permanent, level, frame, scratch: the four lifetimes every engine needs
- **Memory visualization** --- rendering arena fill levels and pool slot occupancy to see the invisible
- **Performance observability** --- measuring allocs/frame, bytes used, peak memory, and frame-time variance

## Prerequisites

- Completed lessons 01--13 (the full things pool system and game demo)
- Completed lesson 14 (Particle Laboratory) recommended --- you have seen the pool under load
- Completed lesson 15 (Data Layout Lab) recommended --- you have seen how data organization affects performance
- Completed lesson 16 (Spatial Partition Lab) recommended --- you have seen how query strategy affects scalability
- The game demo running on at least one backend (Raylib recommended for HUD and memory visualization overlay)

---

## Building and running (quick start)

Get the lab running so you can see malloc's frame-time jitter compared to arena stability:

```bash
cd course/
./build-dev.sh --backend=raylib --memory-arena-lab -r
```

Once the window opens, press `M` to enter the Memory Arena Lab. You will see entities spawning and dying in a continuous churn. The HUD shows the current allocator mode, allocations per frame, bytes in use, frame time, and frame-time variance. In malloc mode, watch the frame-time graph jitter. Press `Tab` to cycle through Arena, Pool, and Scratch modes --- watch the frame-time graph flatten. That is the entire lesson in one keypress.

If you are on X11:

```bash
./build-dev.sh --backend=x11 --memory-arena-lab -r
```

> **JS bridge:** This is like comparing `new Object()` (which eventually triggers a garbage collection pause) versus reusing objects from a pre-allocated pool (which never triggers GC). In JavaScript, you have felt GC pauses as frame drops in canvas games or animation jank in React apps. In C, `malloc` is the equivalent --- except you get to see the cost on EVERY call instead of it being hidden and then appearing as a surprise GC spike. This lab shows you both the problem and four different solutions.

---

## Step 1 --- Why malloc/free kills frame stability

Here is a question that does not get asked often enough in game programming tutorials: what actually happens when you call `malloc`?

The answer is: a lot more than you think. `malloc` is not a simple operation. On most platforms, it does ALL of the following:

1. **Acquires a lock.** The heap is a shared resource. On a multi-threaded system, `malloc` must lock a mutex to prevent two threads from corrupting the free list simultaneously. Even on a single-threaded game, the C runtime's allocator may still use a lock because it does not know you are single-threaded.

2. **Searches for a free block.** The allocator maintains a data structure (free list, red-black tree, segregated size classes --- it varies by implementation) tracking available memory regions. It must find a block large enough for your request. This search is not O(1). It depends on fragmentation, allocation history, and the allocator's internal bookkeeping.

3. **Splits or coalesces blocks.** If the found block is larger than requested, the allocator may split it, creating a new free block from the remainder. If adjacent blocks are both free, it may coalesce them. Both operations modify the allocator's internal data structures.

4. **Updates bookkeeping.** Every allocated block has a header (typically 8--16 bytes) storing the block size and allocation metadata. This is overhead you pay on every allocation.

5. **Possibly calls the OS.** If the heap is exhausted, `malloc` calls `sbrk`, `mmap`, or `VirtualAlloc` to request more memory from the operating system. This is a system call --- orders of magnitude slower than a function call.

Now consider what happens when you call `free`:

1. **Acquires the same lock.**
2. **Validates the pointer** (in debug builds).
3. **Returns the block to the free list.**
4. **May coalesce with adjacent free blocks.**
5. **May return memory to the OS** (via `munmap` or `VirtualFree`).

Every one of these steps has variable latency. The lock might be uncontended (fast) or contended (slow). The search might find a block immediately (fast) or scan hundreds of free blocks (slow). The OS call might not happen (fast) or might trigger a page fault (catastrophically slow).

This is the fundamental problem: **malloc's cost is unpredictable.** You cannot budget a fixed time for it. Some frames it takes 0.1 milliseconds. Some frames it takes 2 milliseconds. At 60 FPS you have 16.67 milliseconds per frame. A 2ms malloc spike eats 12% of your frame budget. Multiply by hundreds of allocations per frame and you are in trouble.

```
Frame time budget at 60 FPS:

  16.67 ms total
  ├── Game logic:     ~4 ms
  ├── Physics:        ~3 ms
  ├── Rendering:      ~6 ms
  ├── Audio:          ~1 ms
  └── malloc/free:    ??? ms   <── THIS IS THE PROBLEM

  malloc/free latency per call (typical ranges):
  ┌─────────────────────────────────────────┐
  │ Best case:    0.05 μs  (uncontended)    │
  │ Typical:      0.5  μs  (some search)    │
  │ Worst case:  50    μs  (lock + coalesce) │
  │ Pathological: 500  μs  (OS call)        │
  └─────────────────────────────────────────┘

  At 200 allocs/frame (entity churn scenario):
  Best case:   200 × 0.05 μs =    10 μs  (fine)
  Typical:     200 × 0.5  μs =   100 μs  (fine)
  Worst case:  200 × 50   μs = 10000 μs = 10 ms  (FRAME SPIKE)
```

That worst case is not theoretical. Under sustained allocation churn --- allocating and freeing hundreds of small objects every frame --- fragmentation accumulates, the allocator's free list grows, coalescing becomes more frequent, and those 50-microsecond spikes start happening regularly.

> **Why?** In JavaScript, `new Object()` is invisible because the garbage collector amortizes the cost across many frames (until a GC pause hits and you get the exact same kind of frame spike). In C, `malloc` makes the cost explicit on every call. The advantage is that you can SEE the problem. The disadvantage is that you must SOLVE it yourself.

> **JS bridge: malloc is like `new ArrayBuffer()`.** Every time you call `malloc(48)` in C, it is like calling `new ArrayBuffer(48)` in JavaScript --- you are asking the runtime (in C's case, the OS) for a chunk of raw memory. The difference is that in JavaScript, the garbage collector eventually reclaims `ArrayBuffer`s you are no longer using. In C, YOU must call `free()` to return the memory, or it leaks forever. This is why C programmers obsess over memory management --- there is no safety net. Forget to `free` once and you have a memory leak. Call `free` twice on the same pointer and you corrupt the heap. Call `free` then use the pointer and you read garbage data. Arenas eliminate ALL of these problems by changing the question from "when do I free this specific allocation?" to "when does this entire CATEGORY of allocations die?"

> **Anton says:** "I don't want to construct my things. I'm just going to zero them." This is not laziness. It is the recognition that construction --- allocation, initialization, field assignment --- is overhead that can be eliminated by designing your data to be valid at zero.

> **Handmade Hero connection:** Casey never calls malloc in the game loop. His memory model uses two arenas: a permanent arena (lives for the entire program) and a transient arena (reset every frame). Every allocation in the game comes from one of these two arenas. Zero malloc calls. Zero free calls. Predictable frame times.

---

## Step 2 --- The malloc baseline: real allocation overhead

Before we build the alternatives, we need to see the problem. Not theoretically --- actually. We are going to allocate and free entities using real `malloc` and `free` calls every frame, and measure the cost.

The baseline scenario: every frame, we spawn some entities and destroy some entities. Each spawn calls `malloc(sizeof(thing))`. Each destroy calls `free(ptr)`. The HUD shows how many allocations happened, how much memory is in use, and how stable the frame time is.

```c
/* ================================================================ */
/*  malloc_baseline.h --- entity allocation via raw malloc/free     */
/* ================================================================ */

typedef struct malloc_entity {
    float x, y, dx, dy;
    float size;
    uint32_t color;
    bool alive;
} malloc_entity;

typedef struct malloc_state {
    malloc_entity **entities;    /* array of POINTERS to heap-allocated entities */
    int capacity;
    int count;
    int allocs_this_frame;
    int frees_this_frame;
    size_t bytes_allocated;
    size_t peak_bytes;
} malloc_state;
```

Notice the double pointer: `malloc_entity **entities`. This is an array of pointers, each pointing to a separately heap-allocated entity. This is exactly the pattern you see in naive C++ code: `std::vector<Entity*>` where each entity is `new Entity()`.

```c
malloc_entity *malloc_spawn(malloc_state *state, float x, float y) {
    malloc_entity *e = (malloc_entity *)malloc(sizeof(malloc_entity));
    if (!e) return NULL;

    state->allocs_this_frame++;
    state->bytes_allocated += sizeof(malloc_entity);
    if (state->bytes_allocated > state->peak_bytes) {
        state->peak_bytes = state->bytes_allocated;
    }

    e->x = x;
    e->y = y;
    e->dx = ((float)(rand() % 200) - 100.0f);
    e->dy = ((float)(rand() % 200) - 100.0f);
    e->size = 4.0f;
    e->color = 0xFF44AAFF;
    e->alive = true;

    /* Add to tracking array */
    if (state->count < state->capacity) {
        state->entities[state->count++] = e;
    } else {
        free(e);  /* pool full --- waste the allocation */
        state->frees_this_frame++;
        state->bytes_allocated -= sizeof(malloc_entity);
        return NULL;
    }
    return e;
}

void malloc_destroy(malloc_state *state, int index) {
    if (index < 0 || index >= state->count) return;

    free(state->entities[index]);
    state->frees_this_frame++;
    state->bytes_allocated -= sizeof(malloc_entity);

    /* Swap with last to avoid holes */
    state->entities[index] = state->entities[state->count - 1];
    state->count--;
}
```

Every `malloc_spawn` call enters the C runtime, acquires a lock, searches for a block, updates bookkeeping, and returns. Every `malloc_destroy` call enters the C runtime again, acquires the lock again, returns the block, possibly coalesces. Under churn (100+ spawns and 100+ destroys per frame), you are doing 200+ lock acquisitions per frame just for entity memory.

> **Common mistake:** "But I'm only allocating 32 bytes per entity, that's nothing!" The size is not the problem. The overhead is per-call, not per-byte. A 32-byte allocation has the same lock acquisition and free-list search cost as a 32-kilobyte allocation. Small frequent allocations are the worst case for malloc.

---

## Step 3 --- Linear arena: bump a pointer, never free

Here is the first alternative. Instead of calling malloc for each entity, we grab a big chunk of memory at startup and hand out pieces of it by bumping a pointer. That is the entire algorithm.

```c
/* ================================================================ */
/*  arena.h --- linear arena allocator                              */
/* ================================================================ */

typedef struct arena {
    uint8_t *base;       /* start of the memory block */
    size_t   size;       /* total capacity in bytes */
    size_t   offset;     /* current allocation point */
    size_t   peak;       /* high-water mark */
    int      alloc_count; /* total allocations (for stats) */
} arena;
```

Five fields. That is the entire allocator state. Compare this to the hundreds of fields inside a production malloc implementation (glibc's `malloc_state` struct is over 200 lines).

```c
void arena_init(arena *a, void *memory, size_t size) {
    a->base = (uint8_t *)memory;
    a->size = size;
    a->offset = 0;
    a->peak = 0;
    a->alloc_count = 0;
}

void *arena_alloc(arena *a, size_t bytes) {
    /* Align to 8 bytes (natural alignment for most types) */
    size_t aligned = (bytes + 7) & ~(size_t)7;

    if (a->offset + aligned > a->size) {
        return NULL;  /* arena exhausted */
    }

    void *ptr = a->base + a->offset;
    a->offset += aligned;
    a->alloc_count++;

    if (a->offset > a->peak) {
        a->peak = a->offset;
    }

    return ptr;
}

void arena_reset(arena *a) {
    a->offset = 0;
    /* a->peak stays --- it's a high-water mark */
}
```

That is it. The entire linear arena allocator. `arena_alloc` bumps the offset and returns a pointer. `arena_reset` sets the offset back to zero. There is no `arena_free` for individual allocations --- you cannot free a single allocation from a linear arena. You free everything at once by resetting.

Let me show you why this is so much better than malloc:

```
malloc vs arena --- per-allocation cost:

malloc:
  1. Acquire lock         ~10-100 ns
  2. Search free list     ~10-500 ns
  3. Split/coalesce       ~5-50 ns
  4. Update bookkeeping   ~5-20 ns
  5. Return pointer
  Total: 30-670 ns (VARIABLE)

arena_alloc:
  1. Add alignment        ~1 ns
  2. Check bounds         ~1 ns
  3. Bump offset          ~1 ns
  4. Return pointer
  Total: ~3 ns (FIXED)
```

```
Arena memory layout:

base                                              base + size
│                                                         │
▼                                                         ▼
┌────────┬────────┬────────┬────────┬─────────────────────┐
│ Entity │ Entity │ Entity │ Entity │    (unused space)    │
│   #1   │   #2   │   #3   │   #4   │                     │
│ 32 B   │ 32 B   │ 32 B   │ 32 B   │                     │
└────────┴────────┴────────┴────────┴─────────────────────┘
                                    ▲
                                    │
                                  offset = 128

After arena_reset():
┌─────────────────────────────────────────────────────────┐
│                   (all free)                             │
└─────────────────────────────────────────────────────────┘
▲
│
offset = 0
```

No fragmentation. Ever. The arena allocates sequentially, so there are no gaps between allocations. When you reset, the entire arena becomes available again in one operation. The memory is contiguous, cache-friendly, and trivially serializable (just write the arena's bytes from base to base+offset).

> **Why?** This is the same mental model as a stack frame. When a function returns, all its local variables vanish at once --- the stack pointer resets. An arena is a user-controlled stack: you decide when to "return" by calling `arena_reset`.

> **JS bridge: an arena is a pre-allocated `ArrayBuffer` you slice from.** Imagine this JavaScript: `const arena = new ArrayBuffer(1024 * 1024); let offset = 0; function alloc(bytes) { const ptr = offset; offset += bytes; return new Float32Array(arena, ptr, bytes / 4); }`. That is a linear arena. You grab one big buffer up front, then hand out slices by bumping `offset`. When you want to "free" everything, you set `offset = 0`. No garbage collection needed because you never created separate objects --- you just moved an integer. The C version is identical, except instead of `Float32Array` views you get raw `void *` pointers. The arena pattern is how you get the performance of stack allocation with the flexibility of heap allocation.

> **Handmade Hero connection:** Casey's permanent arena works exactly like this. Game-lifetime data (entity pool, asset storage, font data) is allocated from the permanent arena at startup and never freed. The arena never resets. Its offset only grows. The transient arena is similar but resets every frame (we will get to that in Step 5).

---

## Step 4 --- Pool allocator: fixed slots, O(1) reuse

The linear arena has one limitation: you cannot free individual allocations. If you spawn 100 entities into an arena and 50 die, the dead entities' memory sits there until the entire arena resets. For entity churn --- constant spawning and destroying --- you want to reuse individual slots immediately.

This is exactly the problem we already solved in lessons 07-08. The pool allocator is the free list pattern applied to memory allocation.

```c
/* ================================================================ */
/*  pool_alloc.h --- fixed-size pool allocator                      */
/* ================================================================ */

typedef struct pool_slot {
    union {
        uint8_t data[64];       /* the actual entity storage */
        int next_free;          /* free list link (when slot is unused) */
    };
    bool used;
} pool_slot;

typedef struct pool_allocator {
    pool_slot *slots;
    int capacity;
    int first_free;
    int used_count;
    int alloc_count;    /* stats: total allocations this frame */
    int free_count;     /* stats: total frees this frame */
    size_t peak_used;
} pool_allocator;
```

Look familiar? The `union` trick is the same one from lesson 08: when a slot is alive, it holds entity data. When a slot is dead, it holds a `next_free` index pointing to the next dead slot. The free list is intrusive --- it lives inside the unused slots themselves.

```c
void *pool_alloc_slot(pool_allocator *p) {
    if (p->first_free < 0) return NULL;  /* pool exhausted */

    int slot = p->first_free;
    p->first_free = p->slots[slot].next_free;
    p->slots[slot].used = true;
    memset(p->slots[slot].data, 0, sizeof(p->slots[slot].data));

    p->used_count++;
    p->alloc_count++;
    if ((size_t)p->used_count > p->peak_used) {
        p->peak_used = (size_t)p->used_count;
    }

    return p->slots[slot].data;
}

void pool_free_slot(pool_allocator *p, void *ptr) {
    /* Find slot index from pointer */
    int slot = (int)((pool_slot *)((uint8_t *)ptr - offsetof(pool_slot, data)) - p->slots);
    if (slot < 0 || slot >= p->capacity) return;

    p->slots[slot].used = false;
    p->slots[slot].next_free = p->first_free;
    p->first_free = slot;
    p->used_count--;
    p->free_count++;
}
```

The cost analysis:

```
Pool allocator --- per-allocation cost:

pool_alloc_slot:
  1. Read first_free      ~1 ns
  2. Advance free list    ~1 ns
  3. Set used flag        ~1 ns
  4. memset slot          ~5 ns  (64 bytes)
  Total: ~8 ns (FIXED)

pool_free_slot:
  1. Compute slot index   ~2 ns
  2. Clear used flag      ~1 ns
  3. Push to free list    ~1 ns
  Total: ~4 ns (FIXED)
```

Both alloc and free are O(1), fixed-cost, no locks, no fragmentation. The trade-off: all allocations must be the same size. You cannot allocate 32 bytes from a pool designed for 64-byte slots (well, you can, but you waste 32 bytes per slot). This is fine for entities because all entities in the things pool are the same size. That is the whole point of the fat struct.

```
Pool allocator slot occupancy:

┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ [0] │ [1] │ [2] │ [3] │ [4] │ [5] │ [6] │ [7] │
│USED │USED │free │USED │free │USED │free │USED │
│ ent │ ent │  ──►│ ent │  ──►│ ent │  ──►│ ent │
│  A  │  B  │  4  │  C  │  6  │  D  │ -1  │  E  │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
              ▲
              │
          first_free = 2

Free list chain: 2 → 4 → 6 → -1 (end)

Allocate one entity:
  - Take slot 2 (first_free)
  - first_free = 4
  - Slot 2 becomes USED

Free entity at slot 5:
  - Slot 5 becomes free
  - slot 5.next_free = first_free (4)
  - first_free = 5
  - Free list: 5 → 4 → 6 → -1
```

> **Cross-reference:** This is the same free list from lesson 08, applied to raw memory allocation instead of pool management. In lesson 08 you reused dead things' `next_sibling` field as the free chain. Here you reuse the slot's data bytes themselves. Same pattern, different context. Game Programming Patterns Ch 19 (Object Pool) describes this exact technique.

> **JS bridge: a pool is like a database connection pool.** In Node.js, you do not call `pg.connect()` for every database query --- that is too slow (TCP handshake, authentication, socket setup). Instead you create a connection pool at startup (`new Pool({ max: 20 })`) and check out/return connections as needed. The pool allocator is the same idea applied to memory: instead of calling `malloc` for every entity (expensive OS call), you pre-allocate a fixed number of slots and check out/return them as entities spawn and die. `pool_alloc_slot` = `pool.acquire()`. `pool_free_slot` = `pool.release()`. Both are O(1). The slot itself never moves in memory --- just like a database connection object stays at the same address in the pool.

---

## Step 5 --- Scratch arena: reset every frame

The scratch arena is the simplest and most powerful pattern for frame-temporary data. It is a linear arena that resets at the start (or end) of every frame.

```c
/* ================================================================ */
/*  scratch.h --- per-frame scratch arena                           */
/* ================================================================ */

/* A scratch arena IS a linear arena --- same struct, same alloc.
 * The only difference is WHEN you reset it: every frame. */

typedef arena scratch_arena;  /* same struct */

/* Usage pattern:
 *
 *  void game_frame(scratch_arena *scratch) {
 *      arena_reset(scratch);  // everything from last frame is gone
 *
 *      // allocate temporary data for this frame
 *      float *distances = arena_alloc(scratch, entity_count * sizeof(float));
 *      int *sorted_ids  = arena_alloc(scratch, entity_count * sizeof(int));
 *      collision_pair *pairs = arena_alloc(scratch, max_pairs * sizeof(collision_pair));
 *
 *      // use the data...
 *      compute_distances(distances, entities, entity_count);
 *      sort_by_distance(sorted_ids, distances, entity_count);
 *      find_collisions(pairs, entities, sorted_ids, entity_count);
 *
 *      // frame ends --- no need to free ANYTHING
 *      // next frame's arena_reset() reclaims all of it
 *  }
 */
```

No `free`. No cleanup. No destructors. No bookkeeping per allocation. You allocate into the scratch arena during the frame, and at the start of the next frame, one `arena_reset` call reclaims everything. The cost of "freeing" 500 frame-temporary allocations is exactly the same as freeing 1: set `offset = 0`.

This is where the power of the arena model really shines. Think about all the temporary data a game frame produces:

- Collision pair lists
- Sorted entity arrays for rendering
- String formatting buffers for HUD text
- Temporary computation buffers (distances, dot products, visibility flags)
- Debug visualization geometry
- Spatial partition buckets (rebuilt every frame)

In a malloc world, every one of these needs a matching `free` call. Miss one and you leak memory. Free one too early and you corrupt data. Free one twice and you crash.

In a scratch arena, the question of "when to free" has exactly one answer: **next frame.** You cannot leak because the arena resets. You cannot double-free because there is no free. You cannot use-after-free because all scratch pointers are invalid after reset (and you know this because you designed the system).

```
Scratch arena lifecycle:

Frame N:
  arena_reset()          offset = 0
  alloc distances[500]   offset = 2000
  alloc sorted_ids[500]  offset = 4000
  alloc pairs[200]       offset = 5600
  ... use all the data ...
  frame ends

Frame N+1:
  arena_reset()          offset = 0    ← everything from frame N is gone
  alloc distances[500]   offset = 2000
  alloc pairs[180]       offset = 3440
  ... use all the data ...
  frame ends

Frame N+2:
  arena_reset()          offset = 0    ← everything from frame N+1 is gone
  ...
```

> **Why?** In JavaScript, you create temporary arrays all the time (`array.map(...)`, `array.filter(...)`, `Object.keys(...)`) and the garbage collector handles cleanup. The scratch arena is the C equivalent: "I need temporary memory, I'll get it from the scratch space, and it disappears when the frame ends." The difference is that the arena reset is deterministic (happens at exactly the frame boundary) while GC is nondeterministic (happens whenever the runtime decides).

> **JS bridge: a scratch arena is like `requestAnimationFrame` clearing temporary state each frame.** In a JavaScript game loop, you might write `function frame() { const particles = []; /* fill with frame-local data */ render(particles); requestAnimationFrame(frame); }`. The `particles` array is garbage-collected after the frame because nothing holds a reference. The scratch arena does the same thing, except instead of relying on the GC to eventually notice the dead array, you explicitly call `arena_reset()` at the frame boundary. The cost of "GC-ing" 500 temporary allocations from a scratch arena is exactly one integer write: `offset = 0`. That is deterministic, instantaneous, and happens at the exact moment you choose --- not whenever V8 decides it is time for a collection cycle.

---

## Step 6 --- Lifetime categories: permanent, level, frame, scratch

Now that you have seen all four allocation strategies, let me organize them into a framework. Every allocation in a game engine falls into one of four lifetime categories:

```
Lifetime categories:

┌──────────────────────────────────────────────────────────────┐
│  PERMANENT (lives for entire program)                        │
│  Arena: permanent_arena                                      │
│  Reset: never                                                │
│  Examples: entity pool, asset storage, font data, audio      │
│            buffers, configuration tables                     │
├──────────────────────────────────────────────────────────────┤
│  LEVEL (lives for one level/map/scene)                       │
│  Arena: level_arena                                          │
│  Reset: on level load                                        │
│  Examples: level geometry, navmesh, enemy spawn tables,      │
│            level-specific textures, trigger volumes           │
├──────────────────────────────────────────────────────────────┤
│  FRAME (lives for one frame)                                 │
│  Arena: scratch_arena / frame_arena                          │
│  Reset: every frame                                          │
│  Examples: collision pairs, render command buffers, sorted    │
│            entity lists, temporary computation buffers        │
├──────────────────────────────────────────────────────────────┤
│  SCRATCH (lives for one function call or scope)              │
│  Arena: scratch sub-arena (save/restore offset)              │
│  Reset: on scope exit                                        │
│  Examples: string formatting, temporary sort buffers,        │
│            local computation space                           │
└──────────────────────────────────────────────────────────────┘
```

> **Why do arenas exist? (Garbage collection vs manual memory)** JavaScript has a garbage collector (GC) that automatically frees memory you are no longer using. C does not have a GC. In C, every byte you allocate with `malloc` stays allocated until you explicitly call `free`. If you forget, the memory leaks. If you `free` too early, other code reads garbage. If you `free` twice, the heap corrupts. Arenas exist because they eliminate ALL three of these problems without needing a garbage collector. You cannot leak (the arena resets). You cannot free too early (there is no individual free). You cannot double-free (there is no free at all). Arenas give you the safety of garbage collection with the performance of manual memory management --- the best of both worlds. The trade-off is that you must think about memory LIFETIMES (when does this whole category of data die?) instead of individual object ownership.

The key insight: **you choose the lifetime at allocation time, and the arena enforces it automatically.** There is no garbage collector deciding when things die. There is no reference counting. There is no destructor chain. You, the programmer, know that "this collision pair list only matters for this frame," so you allocate it from the frame arena. When the frame ends, the arena resets and the memory is reclaimed.

This is the opposite of the ownership model in Rust or C++ smart pointers. Those systems ask: "WHO owns this memory?" Arena allocation asks: "WHEN does this memory die?" The "when" question is almost always easier to answer in game code:

- "When does the entity pool die?" Never (permanent).
- "When does the level geometry die?" When the level unloads (level).
- "When does the collision pair list die?" At the end of this frame (frame).
- "When does this formatted string die?" When this function returns (scratch).

> **Handmade Hero connection:** Casey's memory model is exactly this hierarchy. He uses two arenas: permanent and transient (which combines our level + frame categories). Every allocation in the game specifies which arena it comes from. There is no malloc. There is no free. There is no question about memory lifetime because the arena's reset policy IS the lifetime.

In practice, most engines use 2-3 arenas:

```c
typedef struct game_memory {
    arena permanent;     /* never reset --- pool, assets, config */
    arena level;         /* reset on level load */
    arena frame;         /* reset every frame --- scratch data */
} game_memory;

void game_init(game_memory *mem) {
    /* Permanent allocations */
    thing_pool *pool = arena_alloc(&mem->permanent, sizeof(thing_pool));
    font_data  *font = arena_alloc(&mem->permanent, sizeof(font_data));
    /* These live forever */
}

void game_load_level(game_memory *mem, int level_id) {
    arena_reset(&mem->level);  /* wipe previous level */
    level_geo *geo = arena_alloc(&mem->level, sizeof(level_geo));
    navmesh   *nav = arena_alloc(&mem->level, sizeof(navmesh));
    /* These live until next level load */
}

void game_frame(game_memory *mem) {
    arena_reset(&mem->frame);  /* wipe previous frame's temporaries */
    float *distances = arena_alloc(&mem->frame, count * sizeof(float));
    int   *sorted    = arena_alloc(&mem->frame, count * sizeof(int));
    /* These live until next frame */
}
```

No malloc. No free. No leaks. No fragmentation. No unpredictable latency. Every allocation's lifetime is encoded in which arena it comes from.

---

## Step 7 --- Memory visualization: seeing the invisible

Memory allocation is invisible by default. You call malloc, you get a pointer, you use it, you free it. At no point do you SEE where that memory lives, how much is used, or how fragmented the heap is. This invisibility is why memory bugs are so hard to find --- you cannot debug what you cannot see.

The memory visualization overlay makes allocation visible. For each allocator mode, it renders a different view:

### malloc visualization

```
┌─── Heap Fragmentation Map (256 buckets) ─────────────────┐
│ ████░░██░░░░████░██░░░████░░░░██░░░░░██████░░████░░░░██ │
│ ████░░██░░░░████░██░░░████░░░░██░░░░░██████░░████░░░░██ │
└──────────────────────────────────────────────────────────────┘
  ████ = allocated    ░░░░ = free (fragmentation gap)

  Allocs this frame: 187    Frees this frame: 142
  Bytes in use:      23,104  Peak:  28,672
  Fragmentation:     34.2%   (free bytes between allocated blocks)
```

We cannot easily visualize the real heap layout (the allocator's internals are opaque), so we approximate: each time we allocate, we record the allocation in a tracking array. The visualization draws allocated vs. free regions based on our tracking data. It is not a perfect picture of the heap, but it shows the essential pattern: gaps between allocations, growing fragmentation over time.

### Arena visualization

```
┌─── Arena Fill Level ──────────────────────────────────────┐
│ ████████████████████████████████████░░░░░░░░░░░░░░░░░░░░ │
│ ╠══════════════════════════════════╬═══════════════════╣  │
│ 0 KB              offset: 18.4 KB          capacity: 32 KB
│                                                           │
│ Peak: 22.1 KB     Allocs this frame: 0 (arena is pre-set) │
└──────────────────────────────────────────────────────────────┘
```

The arena visualization is simple and honest: a single progress bar from 0 to capacity, filled to the current offset. No fragmentation to show because there is none. The peak marker shows the high-water mark across all frames.

### Pool visualization

```
┌─── Pool Slot Occupancy (512 slots) ───────────────────────┐
│ ██░░██████░░██░░░░████░░██████████░░░░██░░████████░░████ │
│ ██░░██████░░██░░░░████░░██████████░░░░██░░████████░░████ │
│ ████████░░░░██████░░████░░██████████░░████░░████░░██████ │
│ ████████░░░░██████░░████░░██████████░░████░░████░░██████ │
└──────────────────────────────────────────────────────────────┘
  ████ = occupied slot    ░░░░ = free slot

  Used: 312 / 512 (60.9%)    Peak: 387 (75.6%)
  Allocs this frame: 47      Frees this frame: 39
  Free list length: 200
```

The pool visualization shows every slot as a small colored block: green for occupied, dark for free. You can watch slots flicker between occupied and free as entities spawn and die. The free list length tells you how many reusable slots are available without growing the pool.

### Scratch visualization

```
┌─── Scratch Arena (per-frame) ─────────────────────────────┐
│ ████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│ ╠══════════════════╬══════════════════════════════════╣   │
│ 0 KB          frame peak: 12.8 KB         capacity: 64 KB │
│                                                           │
│ Reset every frame   Allocs this frame: 23                 │
│ Lifetime: one frame (all pointers invalid after reset)    │
└──────────────────────────────────────────────────────────────┘
```

The scratch arena looks like the linear arena but resets to zero every frame. The peak marker drifts up and down depending on how much temporary data each frame needs. If the peak is consistently near capacity, you need a bigger scratch arena. If it is consistently near zero, you over-allocated.

The visualization code itself is straightforward --- it maps allocator state to colored rectangles in the backbuffer:

```c
void draw_memory_bar(uint32_t *pixels, int width, int height,
                     int bar_x, int bar_y, int bar_w, int bar_h,
                     float fill_ratio, uint32_t fill_color, uint32_t empty_color) {
    int fill_w = (int)(bar_w * fill_ratio);
    if (fill_w > bar_w) fill_w = bar_w;

    for (int y = bar_y; y < bar_y + bar_h && y < height; y++) {
        for (int x = bar_x; x < bar_x + bar_w && x < width; x++) {
            if (x < bar_x + fill_w) {
                pixels[y * width + x] = fill_color;
            } else {
                pixels[y * width + x] = empty_color;
            }
        }
    }
}
```

This is the same `draw_rect` pattern from lesson 11 --- fill pixels in a region of the backbuffer. The memory visualization is not a separate rendering system; it is just rectangles drawn into the same pixel buffer as the entities.

---

## Step 8 --- Performance observability

The HUD displays five metrics that tell you everything you need to know about your allocator's behavior:

```
┌──────────────────────────────────────────────────────────────┐
│ Mode: ARENA   Allocs/frame: 0   Frees/frame: 0              │
│ Bytes used: 18,432 / 32,768 (56.3%)   Peak: 22,144          │
│ Frame time: 2.1 ms   Variance: 0.08 ms   (stable)           │
└──────────────────────────────────────────────────────────────┘
```

**Allocs/frame and Frees/frame** --- In malloc mode, this shows hundreds of allocations per frame. In arena mode, this drops to zero (the arena was pre-allocated; bumping the offset is not counted as a "new allocation" in the HUD because no system call occurs). In pool mode, this shows the churn rate (allocs roughly equal frees). This counter directly answers: "how much allocator work is happening per frame?"

**Bytes used and peak** --- Current memory in use and the high-water mark. In malloc mode, this fluctuates as entities are created and destroyed. In arena mode, it grows monotonically until reset. In pool mode, it stays bounded by pool capacity. The peak value tells you whether your arena/pool is sized correctly: if peak equals capacity, you are running out of space.

**Frame time and variance** --- The critical metric. Frame time is how long the entire frame took (update + render + allocator overhead). Variance is the standard deviation of frame times over the last 60 frames. In malloc mode, variance is high (frame times jitter). In arena and pool modes, variance is low (frame times are consistent). This is the metric that proves the point of this entire lesson: **stable allocation = stable frame times.**

```c
typedef struct perf_stats {
    float frame_times[60];   /* ring buffer of last 60 frame times */
    int frame_index;
    float avg_frame_time;
    float frame_variance;
    int allocs_per_frame;
    int frees_per_frame;
    size_t bytes_used;
    size_t peak_bytes;
} perf_stats;

void perf_update(perf_stats *stats, float dt) {
    stats->frame_times[stats->frame_index % 60] = dt;
    stats->frame_index++;

    /* Compute average and variance over last 60 frames */
    float sum = 0.0f;
    int count = stats->frame_index < 60 ? stats->frame_index : 60;
    for (int i = 0; i < count; i++) {
        sum += stats->frame_times[i];
    }
    stats->avg_frame_time = sum / (float)count;

    float var_sum = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = stats->frame_times[i] - stats->avg_frame_time;
        var_sum += diff * diff;
    }
    stats->frame_variance = sqrtf(var_sum / (float)count);
}
```

The frame time ring buffer is a fixed 60-element array. No dynamic allocation. The variance computation is a simple standard-deviation calculation. This is the kind of lightweight telemetry that belongs in every game engine --- not just during development, but in the shipped product. If your frame variance exceeds a threshold, you know something is wrong with your allocator (or your update logic, or your renderer --- but the allocator is always the first suspect).

---

## Step 9 --- Building and running

### Build command

```bash
./build-dev.sh --backend=raylib --memory-arena-lab -r
```

The `--memory-arena-lab` flag defines `ENABLE_MEMORY_ARENA_LAB` at compile time. The lab code is guarded by `#ifdef ENABLE_MEMORY_ARENA_LAB` so it adds zero code to the default build.

### What you should see

1. Press `M` to enter the Memory Arena Lab scene.
2. The screen fills with entities spawning and dying in continuous churn.
3. The HUD shows allocator mode, allocs/frame, bytes used, frame time, and variance.
4. The memory visualization overlay shows the allocator's state.
5. Press `Tab` to cycle through modes:

| Mode | Expected behavior |
|---|---|
| **malloc** | High allocs/frame (200+), visible frame-time jitter, fragmented memory visualization |
| **Arena** | Zero allocs/frame after init, flat frame-time graph, clean fill bar with no gaps |
| **Pool** | Moderate allocs/frame (matching churn rate), stable frame time, slot grid flickering as entities reuse slots |
| **Scratch** | Zero allocs/frame (reset handles everything), flat frame-time graph, fill bar rises during frame then resets to zero |

### What to watch for

- **Frame-time variance in malloc mode.** This is the entire point. If your hardware is fast enough that malloc does not show variance, increase the entity count or decrease the entity lifetime to increase churn.
- **Arena peak vs capacity.** If the peak approaches capacity, the arena is too small. In a real engine, this would be a hard assertion failure, not a silent out-of-memory.
- **Pool slot reuse.** Watch the pool visualization. Dead slots get reused within 1-2 frames. The free list is doing its job.
- **Scratch arena reset behavior.** Watch the fill bar climb during the frame and then snap back to zero at the frame boundary. This is the frame allocator pattern in action.

### Compile flags

The lesson binary at `src/lessons/lesson_17.c` compiles standalone:

```bash
gcc -std=c11 -Wall -Wextra -Werror -o lesson_17 src/lessons/lesson_17.c -lm
./lesson_17
```

The standalone binary runs a text-mode comparison: 10,000 allocations under each strategy, printing timing and allocation counts. It does not require a window or platform backend.

---

## Connection to your work

This lesson completes the memory management story that started in lesson 02 with "just put everything in a flat array."

The progression:

1. **Lessons 02-10:** Build the entity pool --- a fixed flat array with no dynamic allocation. You proved that 1024 entities fit in 53 KB and zero malloc calls are needed.

2. **Lesson 14 (Particle Lab):** Stress-test the pool at 1000 entities, 60 FPS. Still zero malloc. The flat array scales.

3. **Lesson 15 (Data Layout Lab):** Prove that how you organize data in memory affects performance. The transition from AoS to SoA is a memory layout decision.

4. **Lesson 16 (Spatial Partition Lab):** Prove that how you query data determines scalability. O(N^2) does not scale; spatial partitioning does.

5. **This lesson (Memory Arena Lab):** Prove that how you manage memory lifetimes determines frame-time stability. malloc/free in the hot loop is the enemy. Arenas and pools are the fix.

Together, these five perspectives --- data structure, scale, layout, query strategy, lifetime management --- form a complete picture of how production engines think about memory. You are not just learning allocator techniques. You are learning the mindset that shipped Dreams, that powers Handmade Hero, and that every serious engine team uses.

> **Cross-reference to Game Engine Architecture Ch 6:** Gregory's chapter on Memory Management covers stack allocators (our linear arena), pool allocators (our pool), double-buffered allocators (a variation of our scratch arena), and memory alignment. Everything in this lesson maps directly to that chapter.

> **Cross-reference to Game Programming Patterns Ch 19:** The Object Pool pattern describes the pool allocator. But Nystrom's version focuses on the object lifecycle (create/destroy). This lesson focuses on the memory lifecycle (allocate/free), which is the lower-level perspective that makes the Object Pool pattern possible.

When you return to Handmade Hero (day 33+), Casey's permanent and transient arenas will be immediately recognizable. You have already built both of them. You know why they exist, how they work, and what happens when you replace them with malloc.

> **Look ahead:** In Lesson 18 (Infinite Growth Lab), you'll see arenas in action under extreme pressure --- thousands of entities spawning continuously. The malloc mode shows FPS degradation. The arena mode shows rock-solid stability. That's the payoff for understanding lifetime-based allocation.

## What's next

You have now completed all four advanced laboratories:

- **Particle Lab** --- pool under scale
- **Data Layout Lab** --- memory organization
- **Spatial Partition Lab** --- query algorithms
- **Memory Arena Lab** --- lifetime management

Each lab proved a different dimension of the same thesis: data-oriented design is not just about flat arrays. It is about understanding how your data is organized, queried, and allocated --- and making conscious, measurable decisions about all three.

The things pool you built in lessons 02-10 is the foundation. The labs are the proof that the foundation holds under real load. The next step is to take this foundation into a real engine: integrate it with a physics system, a rendering pipeline, an asset loader, and a scripting layer. The entity pool does not change. The arenas do not change. The patterns do not change. The scale changes. And you are ready for it.
