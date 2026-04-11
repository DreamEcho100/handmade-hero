# Lesson 18 -- Pool Allocator (Fixed-Size)

> **What you'll build:** A pool allocator for fixed-size objects using an embedded free list, demonstrated with a particle system that rapidly spawns and despawns particles with O(1) alloc and free.

## Observable outcome

Run the program and you see a pool of 10 Particle slots with alloc/free/reuse, a visualization of the embedded free list showing how freed slots store next-pointers, a 10-frame particle simulation with spawn/despawn statistics, and (with `--bench`) pool_alloc + pool_free vs malloc + free performance.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| Pool allocator | Like an object pool pattern in games -- pre-allocate N objects, reuse them |
| Embedded free list | No JS equivalent -- freed memory stores its own bookkeeping |
| Union trick (`PoolFreeNode`) | Like a tagged union where the tag determines whether data or link is active |
| O(1) alloc and free | Faster than `new` / GC -- just pop/push from a linked list head |
| Arena-backed pool | Pool memory comes from the arena; arena_free releases everything |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_18.c` | New | Pool usage, free list visualization, particle simulation, benchmark |
| `src/lib/pool.h` | Dependency | Pool API: `pool_init`, `pool_alloc`, `pool_free`, `pool_count_free` |
| `src/lib/pool.c` | Dependency | Pool implementation |
| `src/lib/arena.h` | Dependency | Arena provides backing memory |

---

## Background -- why pools exist

In JavaScript, you create objects with `new` and the garbage collector frees them. For a particle system spawning thousands of particles per frame, this means thousands of allocations and eventual GC pauses.

Game developers solve this with object pooling: pre-allocate a fixed number of objects, hand them out on "spawn", take them back on "despawn". The pool never allocates or frees individual objects at runtime. In JS, you would implement this manually with an array and a free-index list. In C, the pool allocator does this at the memory level.

### The embedded free list trick

The key insight is that a free slot is not being used for anything. So we can store the free list's `next` pointer inside the slot itself. When the slot is allocated and holds live data, the next-pointer is irrelevant -- the data overwrites it. When the slot is freed, we write the next-pointer back.

```
  Pool with 5 slots (3 allocated, 2 free):

  Slot 0: [ Particle data... ]     USED
  Slot 1: [ next ──────────────┐   FREE
  Slot 2: [ Particle data... ] │   USED
  Slot 3: [ next ──> NULL ]  <─┘   FREE
  Slot 4: [ Particle data... ]     USED

  free_head ──> Slot 1 ──> Slot 3 ──> NULL
```

This is the `PoolFreeNode` union:

```c
typedef union PoolFreeNode {
    union PoolFreeNode *next;
} PoolFreeNode;
```

When free, the slot's first `sizeof(void *)` bytes store the `next` pointer. When allocated, those bytes are overwritten by the object data. Zero per-object overhead.

### Pool operations

**Alloc** (O(1)): pop the head of the free list.

```c
void *pool_alloc(Pool *pool) {
    PoolFreeNode *node = pool->free_head;
    pool->free_head = node->next;
    pool->used++;
    memset(node, 0, pool->obj_size);
    return (void *)node;
}
```

**Free** (O(1)): push the slot back onto the free list head.

```c
void pool_free(Pool *pool, void *ptr) {
    PoolFreeNode *node = (PoolFreeNode *)ptr;
    node->next = pool->free_head;
    pool->free_head = node;
    pool->used--;
}
```

No search, no merge, no split. Just pointer manipulation.

---

## Code walkthrough

### Section 1: basic pool usage (lines 34-76)

```c
Arena arena = {0};
arena.min_block_size = 64 * 1024;

Pool pool;
pool_init(&pool, &arena, sizeof(Particle), 10);
```

`pool_init` (pool.c lines 11-43) does several things:

1. Clamps `obj_size` to at least `sizeof(PoolFreeNode)` so every slot can hold a free-list pointer
2. Aligns `obj_size` to `_Alignof(max_align_t)` for safe access to any type
3. Allocates `obj_size * count` bytes from the arena in one contiguous block
4. Threads all slots onto the free list

```c
Particle *p1 = (Particle *)pool_alloc(&pool);
p1->x = 1.0f; p1->y = 2.0f; p1->life = 1.0f;
```

Allocation pops the free list head and zero-initializes the slot. The returned pointer is the same address that was a free-list node a moment ago.

```c
pool_free(&pool, p2);
// ... later ...
Particle *p4 = (Particle *)pool_alloc(&pool);
// p4 == p2 (reused the same slot!)
```

Freeing `p2` pushes it onto the free list head. The next alloc pops it right back (LIFO order). The demo shows `p4` at the same address as `p2`, confirming reuse.

### Section 2: embedded free list visualization (lines 78-113)

```c
Pool pool;
pool_init(&pool, &arena, sizeof(Particle), 5);

Particle *slots[5];
for (int i = 0; i < 5; i++)
    slots[i] = (Particle *)pool_alloc(&pool);

pool_free(&pool, slots[1]);
pool_free(&pool, slots[3]);
```

After allocating all 5 and freeing slots 1 and 3, the program prints:

```
  [0] 0x...  USED (particle data)
  [1] 0x...  FREE (stores next-ptr)
  [2] 0x...  USED (particle data)
  [3] 0x...  FREE (stores next-ptr)
  [4] 0x...  USED (particle data)
```

The free list threads through slots 3 and 1 (most recently freed first). The same memory that held particle data now holds bookkeeping pointers.

### Section 3: particle system simulation (lines 115-176)

A 10-frame loop spawns 50-150 particles per frame and ages them:

```c
for (int frame = 0; frame < 10; frame++) {
    // Spawn particles
    for (int i = 0; i < to_spawn && num_active < MAX_PARTICLES; i++) {
        Particle *p = (Particle *)pool_alloc(&pool);
        if (!p) break;  // pool exhausted
        p->life = 1.0f;
        active[num_active++] = p;
    }

    // Age and despawn
    for (int i = num_active - 1; i >= 0; i--) {
        active[i]->life -= 0.15f;
        if (active[i]->life <= 0.0f) {
            pool_free(&pool, active[i]);
            active[i] = active[--num_active];  // swap-remove
        }
    }
}
```

The pool handles rapid churn without fragmentation. Freed slots are reused immediately by subsequent spawns. After all frames, all particles are returned and the pool reports full capacity.

The swap-remove pattern (`active[i] = active[--num_active]`) is standard for unordered arrays -- it avoids shifting elements. In JS, you would use `splice`, which is O(n).

---

## Build and run

```bash
./build-dev.sh --lesson=18 -r
./build-dev.sh --lesson=18 --bench -r   # with benchmarks
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `pool_alloc` returns NULL | Pool is full (all slots allocated) | Increase count at init, or check return value |
| Corrupted free list | Writing to an object after `pool_free` | Treat freed pointers as invalid; set to NULL after free |
| Assertion in `pool_free` | Pointer not from this pool (out of bounds or wrong alignment) | Debug asserts verify ptr is within pool bounds and slot-aligned |
| Object too small for free list | `sizeof(YourType) < sizeof(void *)` | `pool_init` auto-clamps to `sizeof(PoolFreeNode)` minimum |

---

## Key takeaways

1. A pool allocator pre-divides memory into fixed-size slots. Alloc and free are both O(1) -- just pop/push a linked list.
2. The embedded free list stores next-pointers inside freed slots themselves. Zero per-object overhead.
3. Pool memory comes from the arena. `arena_free` releases everything -- no per-slot cleanup needed.
4. Use pools for same-size objects with rapid create/destroy cycles: particles, entities, network connections, AST nodes.
5. The pool has a fixed capacity set at init time. If you need dynamic growth, combine pools with a slab allocator (a pool of pools).
