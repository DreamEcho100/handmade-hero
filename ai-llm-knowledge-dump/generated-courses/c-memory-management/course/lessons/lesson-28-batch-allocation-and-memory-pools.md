# Lesson 28 -- Batch Allocation and Memory Pools

> **What you'll build:** A pool allocator with O(1) alloc and free for fixed-size game entities, benchmarked against per-object malloc and batch-malloc strategies in a spawn/despawn workload.

## Observable outcome

The pool allocator demo shows 3 entities allocated, one freed, and its slot immediately reused by a new entity (pointer equality confirms reuse). The benchmark shows the pool allocator outperforming per-object malloc by a large margin, with batch-malloc in between.

## New concepts

- Free-list pool allocator: threading a linked list through unused blocks
- O(1) allocation (pop from free list) and O(1) deallocation (push onto free list)
- Zero fragmentation for same-sized objects
- Batch allocation: one malloc for N objects with manual index tracking
- The swap-remove pattern for O(1) unordered array removal

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_28.c` | New | `Pool` struct with init/alloc/free/destroy, Entity demo, three-way benchmark |
| `src/common/bench.h` | Dependency | Benchmark timing infrastructure |

---

## Background -- the entity spawn/despawn problem

In a game loop, entities are constantly created and destroyed. Bullets live for a few frames. Particles live for a fraction of a second. Enemies spawn in waves and die individually. This creates a very specific allocation pattern: many same-sized objects with unpredictable lifetimes and high churn.

### Three strategies

```
  Strategy 1: malloc/free each entity
  +---------+
  | malloc  |---> OS searches free lists, maybe calls sbrk/mmap
  | free    |---> OS returns block to free lists
  +---------+
  Cost: system call overhead, heap fragmentation over time

  Strategy 2: Batch malloc (one allocation for N entities)
  +------------------------------------------+
  | slot[0] | slot[1] | slot[2] | ... | [N-1]|
  +------------------------------------------+
  One malloc. Track which slots are free with an index stack.
  Cost: manual index management, fixed capacity

  Strategy 3: Pool allocator (free list through the blocks)
  +--------+--------+--------+--------+--------+
  | block0 | block1 | block2 | block3 | block4 |
  +---+----+---+----+--------+--------+--------+
      |        |
      v        v
    [next]->[next]->NULL       (free list threaded through blocks)
  Cost: ~2 pointer operations per alloc/free. Zero fragmentation.
```

### JS analogy

```js
  // In JS, you might pre-allocate a TypedArray and manage indices:
  const MAX_BULLETS = 10000;
  const bulletX = new Float32Array(MAX_BULLETS);
  const bulletY = new Float32Array(MAX_BULLETS);
  const freeSlots = [];
  for (let i = MAX_BULLETS - 1; i >= 0; i--) freeSlots.push(i);

  function spawnBullet(x, y) {
    const idx = freeSlots.pop();    // O(1)
    bulletX[idx] = x;
    bulletY[idx] = y;
    return idx;
  }

  function despawnBullet(idx) {
    freeSlots.push(idx);            // O(1)
  }
```

The C pool allocator is the same idea, but instead of a separate index array, the free list is threaded **through the blocks themselves** -- the first bytes of each free block store a pointer to the next free block.

---

## Code walkthrough

### Pool structure

```c
typedef struct Pool {
  void   *memory;        /* backing allocation (one big malloc) */
  void   *free_list;     /* head of free list */
  size_t  block_size;    /* size of each block (>= sizeof(void*)) */
  size_t  capacity;      /* total blocks */
  size_t  active;        /* currently allocated blocks */
} Pool;
```

### pool_init -- threading the free list

```c
static int pool_init(Pool *pool, size_t block_size, size_t capacity) {
  if (block_size < sizeof(void *))
    block_size = sizeof(void *);     /* must fit a pointer! */

  pool->memory = malloc(block_size * capacity);
  if (!pool->memory) return 0;

  /* Thread the free list through all blocks */
  pool->free_list = pool->memory;
  uint8_t *p = (uint8_t *)pool->memory;
  for (size_t i = 0; i < capacity - 1; i++) {
    void *next = p + block_size;
    memcpy(p, &next, sizeof(void *));   /* store 'next' pointer in block */
    p += block_size;
  }
  /* Last block points to NULL */
  void *null_ptr = NULL;
  memcpy(p, &null_ptr, sizeof(void *));

  return 1;
}
```

Why `memcpy` instead of a pointer cast? Strict aliasing. The C standard says you cannot cast `uint8_t *` to `void **` and dereference it -- the compiler is allowed to assume pointers of different types do not alias. `memcpy` is always safe and optimizes to the same code.

After init, the free list looks like this:

```
  free_list
      |
      v
  +--------+    +--------+    +--------+    +--------+
  | next---+--->| next---+--->| next---+--->| NULL   |
  | (free) |    | (free) |    | (free) |    | (free) |
  +--------+    +--------+    +--------+    +--------+
  block 0       block 1       block 2       block 3
```

### pool_alloc -- pop from free list (O(1))

```c
static void *pool_alloc(Pool *pool) {
  if (!pool->free_list) return NULL;   /* pool exhausted */

  void *block = pool->free_list;
  memcpy(&pool->free_list, block, sizeof(void *));  /* pop: read next */
  pool->active++;
  return block;
}
```

Two operations: read the `next` pointer from the block, update `free_list` to point to `next`. The returned block is now available for the caller to use -- the `next` pointer that was stored there will be overwritten by the caller's data.

### pool_free -- push onto free list (O(1))

```c
static void pool_free(Pool *pool, void *block) {
  if (!block) return;
  memcpy(block, &pool->free_list, sizeof(void *));  /* push: write old head */
  pool->free_list = block;
  pool->active--;
}
```

Write the current `free_list` head into the block's first bytes, then update `free_list` to point to this block. The block is now back on the free list.

### Demo: slot reuse confirmed

```c
Entity *e1 = pool_alloc(&pool);
Entity *e2 = pool_alloc(&pool);
Entity *e3 = pool_alloc(&pool);

pool_free(&pool, e2);              /* free e2's slot */

Entity *e4 = pool_alloc(&pool);    /* reuses e2's slot */
printf("e4 == e2? %s\n", (e4 == e2) ? "yes" : "no");
// Output: "yes (slot reused!)"
```

Since `pool_free` pushes e2's slot onto the free list head, the next `pool_alloc` pops it immediately. This is the core pool pattern -- slots cycle between active use and the free list without any OS allocator involvement.

### Benchmark: three strategies compared

The benchmark spawns 100,000 entities, despawns every other one, then respawns into the gaps. This simulates realistic game-loop churn.

```c
/* Strategy 1: malloc/free each */
for (int i = 0; i < BENCH_N; i++) {
  entities[i] = malloc(sizeof(Entity));
  entities[i]->x = (float)i;
}
for (int i = 0; i < BENCH_N; i += 2) free(entities[i]);
for (int i = 0; i < BENCH_N; i += 2) {
  entities[i] = malloc(sizeof(Entity));
}

/* Strategy 2: batch (one malloc, index-based reuse) */
Entity *batch = malloc(BENCH_N * sizeof(Entity));
int *free_stack = malloc(BENCH_N * sizeof(int));
/* ... manage free_stack manually ... */

/* Strategy 3: pool allocator */
Pool pool;
pool_init(&pool, sizeof(Entity), BENCH_N);
for (int i = 0; i < BENCH_N; i++)
  entities[i] = pool_alloc(&pool);
```

The pool wins because every alloc and free is just a pointer update. No free list search, no system calls, no fragmentation.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Pool returns NULL immediately | Forgot to call `pool_init` or capacity is 0 | Initialize with capacity matching your expected max entity count |
| Corruption after pool_alloc | `block_size < sizeof(void *)` | The pool requires each block to fit a pointer; `pool_init` enforces this |
| Entity data contains garbage after alloc | Pool blocks are NOT zeroed | `memset` the returned pointer, or add a zeroing wrapper |
| Pool runs out for particle effects | Fixed capacity is too small for burst spawns | Use a slab allocator (Lesson 21) which grows automatically |
| Freeing a pointer not from this pool | Pool cannot validate ownership | In debug builds, assert that the pointer falls within `pool->memory` range |

---

## Build and run

```bash
./build-dev.sh --lesson=28 -r              # pool demo with slot reuse
./build-dev.sh --lesson=28 --bench -r      # full three-way benchmark
```

---

## Key takeaways

1. A pool allocator provides O(1) alloc and O(1) free for fixed-size objects by threading a free list through the unused blocks themselves. This is the fastest possible allocator for same-sized objects.
2. The free list is **intrusive** -- it uses the block's own memory to store the `next` pointer. When the block is allocated, the caller's data overwrites the pointer. When it is freed, the pointer is restored.
3. Pools have zero external fragmentation because all blocks are the same size. The trade-off is that you need one pool per object type/size.
4. The batch strategy (one malloc + index tracking) is a middle ground: better than per-object malloc, but slower than a pool because index management has overhead.
5. In game development, pools are the standard pattern for bullets, particles, projectiles, and any entity with high spawn/despawn frequency. The slab allocator (Lesson 21) extends this pattern with automatic growth when the pool capacity is exceeded.
