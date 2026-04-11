# Lesson 21 -- Slab Allocator

> **What you'll build:** A slab allocator that manages multiple pages of fixed-size objects with automatic growth, demonstrated with an entity spawn/despawn simulation.

## Observable outcome

The program allocates and frees Entity objects from a slab, shows freed slots being reused, allocates 25 objects across 4 automatically-created pages, runs a 20-frame entity simulation with spawn/despawn churn, and prints a comparison table contrasting slab, pool, and malloc.

## New concepts

- Slab allocator: a pool that grows automatically by allocating new pages on demand
- Per-page free lists: each slab page maintains its own free list of available slots
- Page growth: when all current pages are full, a new page is allocated via `malloc`
- Slab vs pool vs malloc: trade-offs between fixed capacity, automatic growth, and general allocation

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_21.c` | New | Slab usage, page growth demo, entity simulation, comparison table, benchmark |
| `src/lib/slab.h` | Dependency | Slab API: `slab_init`, `slab_alloc`, `slab_free`, `slab_destroy` |
| `src/lib/slab.c` | Dependency | Multi-page slab implementation with per-page free lists |

---

## Background -- from web pools to kernel slabs

If you have used a connection pool in Node.js (database connections, HTTP agents), you already understand the core idea: pre-allocate a set of identical resources so you can hand them out and take them back without the cost of creating/destroying each one. A slab allocator is the systems-programming equivalent for raw memory.

### The problem the pool left unsolved

In Lesson 18 we built a pool allocator with a fixed capacity. If the pool has 64 slots and you need 65, you are out of luck. In a game loop where the number of entities fluctuates wildly (bullets, particles, enemies), a fixed pool either wastes memory (too large) or runs out (too small).

### How a slab solves it

The slab organizes objects into **pages** (also called "slabs"). Each page is a contiguous block of N same-sized slots with its own embedded free list. Pages are linked together. When no existing page has a free slot, the slab allocates a new page via `malloc` and adds it to the linked list.

```
JS analogy:

  // Pool (fixed)
  const pool = new Array(64).fill(null);  // That's it. No more.

  // Slab (grows)
  const pages = [];
  function slabAlloc() {
    for (const page of pages) {
      if (page.hasFreeSlot()) return page.pop();
    }
    const newPage = new Page(64);   // <-- auto-growth
    pages.push(newPage);
    return newPage.pop();
  }
```

### Memory layout

```
  Slab
  +------------------+
  | obj_size: 32     |     Page 1 (malloc'd)
  | objs_per_slab: 4 |     +---------+------+------+------+------+
  | pages  ----------+---->| header  | [e0] | [e1] | [e2] | [e3] |
  | total_allocs: 7  |     | free=.. | used | used | FREE | used |
  +------------------+     +---------+------+------+------+------+
                                |
                                v  Page 2 (malloc'd when page 1 filled)
                           +---------+------+------+------+------+
                           | header  | [e4] | [e5] | [e6] | [e7] |
                           | free=.. | used | used | used | FREE |
                           +---------+------+------+------+------+
                                |
                                v  NULL (end of page list)
```

Each `SlabPage` header stores three things:
- A pointer to the next page (`next`)
- The head of the per-page free list (`free_head`)
- A count of used slots (`used`)

The per-page free list is an intrusive linked list. Each free slot stores a `SlabFreeNode *next` pointer in the slot's own memory -- exactly like the pool allocator from Lesson 18, but one free list per page instead of one global free list.

### How it compares to the Linux kernel

This design is inspired by the Linux kernel's SLAB/SLUB allocator. The kernel uses slab allocators for frequently-allocated kernel objects like `task_struct`, `inode`, and `dentry`. The kernel version adds per-CPU caches, coloring (to reduce cache conflicts), and constructor/destructor callbacks, but the core page-based free-list concept is identical.

---

## Code walkthrough

### Data structures (lib/slab.h)

The header defines four types:

```c
typedef union SlabFreeNode {
  union SlabFreeNode *next;   /* Each free slot stores a 'next' pointer */
} SlabFreeNode;

typedef struct SlabPage {
  struct SlabPage *next;       /* linked list of all pages              */
  SlabFreeNode    *free_head;  /* per-page free list                    */
  size_t           used;       /* number of allocated objects this page */
} SlabPage;

typedef struct Slab {
  SlabPage *pages;          /* linked list of all slab pages            */
  size_t    obj_size;       /* size of each object (aligned)            */
  size_t    objs_per_slab;  /* number of objects per slab page          */
  size_t    total_allocs;   /* total outstanding allocations            */
} Slab;
```

In JS terms, `SlabFreeNode` is like a linked-list node that lives *inside* the free slot itself -- no separate allocation needed. The `SlabPage` is like one "chunk" of a chunked array, and the `Slab` struct is the manager that owns all pages.

### Section 1: basic slab usage

```c
Slab slab;
slab_init(&slab, sizeof(Entity), 4);  /* 4 objects per page (tiny for demo) */

Entity *e1 = (Entity *)slab_alloc(&slab);
e1->id = 1; e1->x = 10.0f; e1->health = 100;

Entity *e2 = (Entity *)slab_alloc(&slab);
e2->id = 2; e2->x = 20.0f; e2->health = 80;
```

`slab_init` records the object size (aligned to at least pointer size) and objects-per-page count. It does not allocate any pages yet -- the first `slab_alloc` triggers the first page creation.

After freeing `e1` and allocating `e3`, the demo confirms slot reuse: `e3 == e1` because the freed slot was pushed onto the page's free list, then immediately popped by the next allocation.

### Section 2: automatic page growth

```c
Slab slab;
slab_init(&slab, sizeof(Entity), 8);  /* 8 objects per page */

Entity *entities[25];
for (int i = 0; i < 25; i++) {
  entities[i] = (Entity *)slab_alloc(&slab);
  /* Detect new page by checking if slab.pages changed */
}
```

With 8 objects per page and 25 allocations, the slab creates `ceil(25/8) = 4` pages. The demo detects new page creation by monitoring `slab.pages`. Each time the head of the page list changes, a new page was created transparently.

```
  Page creation timeline:
  alloc[ 0]: NEW PAGE #1 created      (slots 0-7)
  alloc[ 8]: NEW PAGE #2 created      (slots 8-15)
  alloc[16]: NEW PAGE #3 created      (slots 16-23)
  alloc[24]: NEW PAGE #4 created      (slot 24, slots 25-31 free)
```

### Section 3: entity simulation

The most realistic part of the lesson -- a 20-frame game loop:

```c
for (int frame = 0; frame < 20; frame++) {
  /* Spawn 10-30 entities per frame */
  int to_spawn = 10 + (int)(rng % 20);
  for (int i = 0; i < to_spawn && num_active < MAX_ENTITIES; i++) {
    Entity *e = (Entity *)slab_alloc(&slab);
    active[num_active++] = e;
  }

  /* Despawn ~30% of active entities */
  for (int i = num_active - 1; i >= 0; i--) {
    if ((rng % 100) < 30) {
      slab_free(&slab, active[i]);
      active[i] = active[--num_active];   /* swap-remove */
    }
  }
}
```

The swap-remove pattern (`active[i] = active[--num_active]`) is a C idiom for O(1) removal from an unordered array -- equivalent to swapping the last element into the gap and decrementing the length.

After all frames, every entity is freed and `slab.total_allocs` returns to zero. `slab_destroy` frees all pages back to the OS.

### Section 4: comparison table

```
  +-------------------+---------------+---------------+---------------+
  | Feature           | Slab          | Pool          | malloc        |
  +-------------------+---------------+---------------+---------------+
  | Object size       | Fixed         | Fixed         | Any           |
  | Grows             | Yes (pages)   | No (fixed)    | Yes           |
  | Alloc speed       | O(1)*         | O(1)          | O(1) amortiz. |
  | Free speed        | O(1)          | O(1)          | O(1) amortiz. |
  | Fragmentation     | None internal | None          | Yes           |
  | Overhead/object   | 0 bytes       | 0 bytes       | 16-32 bytes   |
  +-------------------+---------------+---------------+---------------+
  * O(n) when searching for a page with free space (rare path).
```

The slab sits between pool (simple, fixed) and malloc (flexible, expensive). Use it when you know the object size but not the count.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Forgetting `slab_destroy` | Slab pages are malloc'd and must be explicitly freed | Always call `slab_destroy` when done; unlike arena pools, slabs own their memory |
| Freeing a pointer not from this slab | The slab cannot validate ownership | Track which slab owns each object, or use debug assertions in development |
| Very small `objs_per_slab` (like 1 or 2) | Every allocation triggers a new page malloc | Set `objs_per_slab` to at least 64 for real workloads |
| Use-after-free on slab objects | Freed slot is immediately reusable; old pointer now points to the free-list node | NULL your pointers after `slab_free`, or use a generation counter pattern |

---

## Build and run

```bash
./build-dev.sh --lesson=21 -r              # run the demo
./build-dev.sh --lesson=21 --bench -r      # includes slab vs malloc benchmark
```

---

## Key takeaways

1. A slab allocator is a pool that grows. It keeps the O(1) alloc/free of a pool while removing the fixed-capacity limitation.
2. Each page has its own free list. This makes it possible to track per-page usage and eventually return empty pages to the OS.
3. The slab pattern is the foundation of the Linux kernel's object allocator. Understanding it connects you to how real operating systems manage memory for frequently-created objects.
4. In JS terms, a slab is like a chunked `FreeList` where each chunk is pre-allocated. The key difference from JS is that *you* control when chunks are created and destroyed -- the garbage collector is you.
