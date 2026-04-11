# Lesson 34: `PoolAllocator`, Free Lists, and Fixed-Size Reuse

## Frontmatter

- Module: `07-memory-lifetimes-and-allocators`
- Lesson: `34`
- Status: Required
- Target files:
  - `src/utils/arena.h`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
- Target symbols:
  - `PoolAllocator`
  - `pool_init_in_arena`
  - `pool_build_free_list`
  - `pool_alloc`
  - `pool_free`
  - `pool_reset`
  - `pool_contains`

## Observable Outcome

By the end of this lesson, you should be able to explain why a pool allocator is a better fit than an arena for frequently recycled fixed-size objects, and how the runtime uses a free list to hand slots back out without fragmenting the underlying storage.

## Prerequisite Bridge

Lessons 30 through 33 focused on lifetime-grouped push allocation.

That model is great when data dies together, but it is not ideal for objects that:

- all have the same size
- are allocated and freed individually
- should be reused immediately

That is where the pool allocator fits.

## Step 1: Read `PoolAllocator` as a Fixed-Slot Manager

In `src/utils/arena.h`:

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

This is not a variable-size allocator.

It manages one preallocated array of same-sized slots.

That is the central rule:

```text
every allocation from one pool has the same slot size
```

## Step 2: Why Pool Allocators Exist Beside Arenas

An arena is excellent when many allocations share one lifetime boundary.

A pool is better when you want:

- fast allocate
- fast free
- immediate reuse of freed slots
- no per-allocation heap churn

So you should think of the pool as solving a different problem, not as an arena competitor in the abstract.

## Step 3: Slot Size Is Aligned Before the Pool Is Built

The helper:

```c
static inline size_t pool_slot_size_aligned(size_t slot_size)
```

ensures every slot is at least large enough for a `PoolFreeNode` and then rounds it up to the default alignment.

That matters because freed slots are temporarily reinterpreted as free-list nodes.

If a slot were too small for `PoolFreeNode`, the free-list bookkeeping would not fit safely.

## Step 4: Read `pool_build_free_list(...)` as a Reverse Wiring Pass

The build helper iterates over all slots and pushes them onto the free list:

```c
for (index = pool->slot_count; index > 0; --index) {
  uint8_t *slot = (uint8_t *)pool->memory + ((index - 1) * pool->slot_size);
  PoolFreeNode *node = (PoolFreeNode *)slot;
  node->next = pool->free_list;
  pool->free_list = node;
}
```

The important idea is that the free list lives inside the free slots themselves.

No extra side array is needed.

That is a classic fixed-pool trick.

## Visual: Free Slots Become the Linked List

```text
pool memory:
  [slot0][slot1][slot2][slot3]

when free:
  slot0.next -> slot1
  slot1.next -> slot2
  slot2.next -> slot3
  slot3.next -> NULL
```

When allocated, the slot stops acting like a list node and starts acting like user data again.

## Step 5: `pool_init_in_arena(...)` Combines Two Policies

The runtime often builds a pool inside the level arena:

```c
memory = arena_push_zero(arena, aligned * slot_count,
                         arena_default_slot_alignment());
...
pool->backing_arena = arena;
```

That means the design is layered:

```text
scene lifetime from the level arena
plus individual fixed-slot reuse inside that scene lifetime
```

This is a very useful composition pattern.

## Step 6: `pool_alloc(...)` Pops One Free Slot

The allocation path is tiny:

```c
node = pool->free_list;
pool->free_list = node->next;
--pool->free_count;
```

Then it updates the high-watermark stats.

So `pool_alloc(...)` is basically:

```text
take the head of the free list
return it as the next object slot
```

That is why it is O(1).

## Step 7: `pool_free(...)` Pushes the Slot Back

The free path is the mirror image:

```c
node = (PoolFreeNode *)ptr;
node->next = pool->free_list;
pool->free_list = node;
++pool->free_count;
```

Before that, the function asserts:

- the pool exists
- the pointer belongs to the pool via `pool_contains(...)`
- `free_count` was not already saturated

Those checks matter. They catch freeing alien pointers and many double-free mistakes early.

## Step 8: See Pool Reuse in the Live Runtime

In `src/game/main.c`, the pool lab initializes scene storage like this:

```c
if (pool_init_in_arena(&scene->pool, level, sizeof(PoolProbe),
                       GAME_POOL_CAPACITY, "pool_lab_pool") != 0)
  return;
```

Then spawning pulls probes from the pool:

```c
PoolProbe *probe = (PoolProbe *)pool_alloc(&scene->pool);
```

The scene also tracks whether a newly allocated probe equals `scene->last_freed` and increments `reuse_hits` when that happens.

That is a great teaching detail because it makes reuse visible rather than theoretical.

## Step 9: `pool_reset(...)` Means "Make Every Slot Free Again"

The reset helper simply rebuilds the free list from the full memory region.

This is useful when the whole pool's object set should be discarded at once, but the backing storage itself should remain.

So the pool supports both styles:

- individual `pool_free(...)`
- whole-pool `pool_reset(...)`

## Step 10: Why Pool Fits the Pool Lab Better Than a Plain Arena

The pool lab repeatedly spawns, recycles, and reuses `PoolProbe` objects.

If that were built with ordinary arena pushes only, individually dead probes would not become reusable until the whole arena reset.

That would be the wrong behavior.

The pool is correct here because each probe slot can leave and re-enter circulation independently.

## Practical Exercises

### Exercise 1: Explain the Free List

Why can the free list live inside the pool slots themselves?

Expected answer:

```text
because free slots are not currently holding live user objects, so their bytes can temporarily store the next-pointer bookkeeping for the allocator
```

### Exercise 2: Explain the Arena Composition

Why is `pool_init_in_arena(...)` a useful combination instead of a contradiction?

Expected answer:

```text
because the arena provides the scene lifetime for the pool's backing storage, while the pool provides per-object fixed-slot reuse within that larger lifetime
```

### Exercise 3: Explain the Pool Lab

Why is a pool a better fit for `PoolProbe` objects than plain arena pushes?

Expected answer:

```text
because probes are recycled individually during the scene, so freed slots need to become immediately available again without waiting for a full scene reset
```

## Common Mistakes

### Mistake 1: Treating a pool like a variable-size allocator

One pool manages one aligned slot size.

### Mistake 2: Freeing a pointer that did not come from the pool

That violates the pool's ownership model and is exactly what `pool_contains(...)` is trying to catch.

### Mistake 3: Expecting pool allocation order to preserve previous object identity

Freed slots return through the free list, not through a stable object registry.

## Troubleshooting Your Understanding

### "Why not allocate each `PoolProbe` with `malloc(...)`?"

Because that would reintroduce per-object heap churn and make reuse less predictable and less teachable.

### "Why does the runtime still need the level arena if it already has a pool?"

Because the pool's own backing memory still needs a lifetime root, and the current scene lifetime is the right one.

## Recap

You now understand the pool allocator model:

1. preallocate fixed-size aligned slots
2. thread free slots into an internal free list
3. pop on `pool_alloc(...)`
4. push back on `pool_free(...)`
5. optionally rebuild all free slots with `pool_reset(...)`

## Next Lesson

Lesson 35 generalizes the idea from one fixed pool to a slab allocator that can grow page by page, then ties that allocator back into `game_fill_slab_audio(...)` and the scene rebuild flow.
