# Lesson 59: Slot Indexing, Generations, and Reuse Counters

## Frontmatter

- Module: `12-pool-reuse-lab`
- Lesson: `59`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `pool_lab_slot_index`
  - `pool_lab_slot_is_active`
  - `pool_lab_spawn`
  - `pool_alloc`
  - `slot_generations`
  - `slot_reuses`
  - `reuse_hits`
  - `hottest_slot`

## Why This Lesson Exists

Lesson 58 established the Pool scene contract.

Now the learner needs the exact mechanism that turns one free-list pointer into a visible slot identity with real historical meaning.

This is where the pool stops being abstract. After this lesson, you should be able to explain why a label such as `s3 g2` is not decorative UI text. It is a compressed summary of actual allocator history derived from real addresses and real reuse events.

## Observable Outcome

By the end of this lesson, you should be able to walk through `pool_lab_spawn(...)` in order and explain how one returned pointer becomes a stable slot index, a generation number, a possible immediate reuse hit, and eventually a contributor to `hottest_slot`.

## First Reading Strategy

Do not start in the middle of `pool_lab_spawn(...)`.

Read in this order:

1. `pool_alloc(...)` to understand what allocation means in a free-list allocator
2. `pool_lab_slot_index(...)` to understand how slot identity is derived from memory layout
3. the top half of `pool_lab_spawn(...)` to see where the proof metrics are updated
4. only then read the world-state initialization on the new probe

If you reverse that order, colors and velocities look more important than allocator history, which is exactly backwards for this module.

## Step 0: Keep the Slot Board in Mind Before It Is Drawn

Lesson 61 will render a slot board.

That board only matters if the runtime can honestly say:

```text
this square corresponds to a real slot in the backing storage
```

The Pool Lab avoids fake pedagogy by deriving slot identity from the backing memory itself. The render path is visualizing real allocator structure, not inventing a parallel teaching fiction.

## Step 1: Read `pool_alloc(...)` as the Simplest Possible Free-List Claim

In `src/utils/arena.h`, allocation is intentionally tiny:

```c
if (!pool || !pool->free_list)
  return NULL;
node = pool->free_list;
pool->free_list = node->next;
--pool->free_count;
used = pool->slot_count - pool->free_count;
if (used > pool->high_watermark)
  pool->high_watermark = used;
```

That is the pool story in one screenful.

- take the current free-list head
- advance the head to the next free slot
- decrement remaining free capacity
- update peak occupancy if needed

There is no search and no new memory request. Allocation means claiming one previously free slot from a bounded region.

## Step 2: Read `pool_lab_slot_index(...)` as the Truth Bridge

After allocation, the scene needs a stable slot number. It computes one from the returned pointer:

```c
offset = (const uint8_t *)probe - (const uint8_t *)scene->pool.memory;
return (int)((size_t)offset / scene->pool.slot_size);
```

That means slot identity comes from pointer position inside the fixed backing storage.

This is the most important design decision in the lesson.

```text
real pointer
  -> byte offset from pool base
  -> slot-sized division
  -> stable slot index
```

Because the index is derived from memory layout, the later slot board can claim to be a real allocator map instead of a teaching sketch.

## Worked Example: Why Pointer Arithmetic Is the Right Proof Source

Suppose `scene->pool.memory` points at the first slot and `slot_size` is one aligned `PoolProbe` wide.

If `pool_alloc(...)` returns the third slot in the region, the scene computes:

1. returned pointer minus pool base pointer
2. byte offset inside the backing region
3. offset divided by `slot_size`
4. stable slot index `2`

That is why the runtime can later draw `s2 g1` and mean it literally.

## Step 3: Follow the Top of `pool_lab_spawn(...)` Carefully

The top of the spawn path is the allocator truth core:

```c
PoolProbe *probe = (PoolProbe *)pool_alloc(&scene->pool);
int slot_index;
if (!probe)
  break;
if (probe == scene->last_freed)
  scene->reuse_hits++;
slot_index = pool_lab_slot_index(scene, probe);
probe->slot_index = slot_index;
```

That block already performs four different proof jobs.

- it claims a slot from the free list
- it checks whether the exact pointer that just came back is the most recently freed slot
- it converts the pointer into a stable slot index
- it stores that slot identity on the live probe itself

Notice the order. `last_freed` is checked before the probe is repopulated with new motion and color state. That keeps the immediate-reuse comparison focused on pointer identity rather than on the values about to be written into the new occupant.

## Step 4: Separate Generation, Reuse, and Current Occupant Identity

The next block updates historical state:

```c
scene->slot_generations[slot_index]++;
probe->generation = scene->slot_generations[slot_index];
if (probe->generation > 1)
  scene->slot_reuses[slot_index]++;
if (scene->hottest_slot < 0 ||
    scene->slot_reuses[slot_index] >
        scene->slot_reuses[scene->hottest_slot]) {
  scene->hottest_slot = slot_index;
}
```

These values are related, but they do not answer the same question.

### `slot_generations[slot_index]`

How many times this slot has ever been allocated.

### `probe->generation`

Which numbered life the current occupant represents.

### `slot_reuses[slot_index]`

How many times this slot has been reused after its first life.

### `hottest_slot`

Which slot currently leads the long-term reuse race.

The first allocation of a slot should therefore produce:

- generation `1`
- reuse count `0`

That distinction is essential. First use is not yet reuse.

## Step 5: Read `reuse_hits` and `hottest_slot` as Two Timescales of Proof

`reuse_hits` increments only when the newly allocated pointer equals `last_freed`.

That is immediate local proof that the newest returned slot came right back out.

`hottest_slot` is slower and broader. It tracks which slot has accumulated the most reuse over time, so the render path can spotlight concentration instead of only the latest event.

So the scene proves reuse at two timescales:

- immediate: did the newest returned pointer come back right away?
- cumulative: which slot has been reused the most over the life of the scene?

That is why those metrics must stay separate.

## Step 6: See Why `pool_lab_slot_is_active(...)` Exists

The lesson is mainly about spawn, but `pool_lab_slot_is_active(...)` matters because it gives the render path a way to ask a clean question:

```text
is this slot currently occupied right now?
```

That lets the runtime separate three different truths about one slot:

- whether it exists in the backing region
- how much historical reuse it has accumulated
- whether it is active in the current frame

The slot board later depends on that separation.

## Step 7: Notice the Spawn Path Also Creates a New Visible Life

After the allocator metrics are updated, the spawn function writes world-facing state such as:

- position
- velocity
- TTL
- size
- color

That is not filler. The scene deliberately couples allocator history to moving visuals. The learner can watch a new probe appear while still being able to track which slot it came from and which numbered life it represents.

## Common Mistake: Treating All Reuse Metrics as Equivalent

They are not.

- `slot_generations` answers: how many lives has this slot had?
- `slot_reuses` answers: how many times has it been reused after the first life?
- `reuse_hits` answers: did the most recently freed pointer come back immediately?
- `hottest_slot` answers: which slot has attracted the most reuse historically?

If you flatten those into one generic "reuse counter," the later proof surfaces become much less informative.

## JS-to-C Bridge

This is like pulling a worker object from a bounded pool and keeping two kinds of statistics: whether you immediately got back the one you just returned, and which worker slot has been recycled the most over the whole lifetime of the system.

## Try Now

Open `src/utils/arena.h` and `src/game/main.c`, then do these checks:

1. Read `pool_alloc(...)` and explain why free-list allocation is constant-time.
2. Read `pool_lab_slot_index(...)` and explain how it derives slot identity from pointer arithmetic.
3. Read the first half of `pool_lab_spawn(...)` and explain why `last_freed` is checked before the probe is repopulated.
4. Explain the difference between `slot_generations`, `slot_reuses`, `reuse_hits`, and `hottest_slot`.
5. Find `pool_lab_slot_is_active(...)` and explain why current occupancy should stay separate from historical reuse.

## Exercises

1. Explain why slot identity should come from the backing memory address instead of a synthetic ID.
2. Explain why the first allocation of a slot should increase generation but not reuse count.
3. Explain why immediate reuse and long-term reuse concentration need different metrics.
4. Describe the spawn path in the exact order the code performs it.

## Runtime Proof Checkpoint

At this point, you should be able to explain how one pool allocation becomes readable evidence: the allocator pops the free-list head, pointer arithmetic maps the returned address to a stable slot index, and the scene records generation and reuse history before giving the slot a new visible life.

## Recap

This lesson explained how new live probes are born from free slots:

- `pool_alloc(...)` claims the free-list head from a bounded region
- `pool_lab_slot_index(...)` turns a real address into a stable slot label
- generation and reuse counters preserve slot history across occupants
- `reuse_hits` and `hottest_slot` expose immediate and cumulative reuse separately
- current activity is tracked separately from lifetime history

Next, we follow the update loop so the learner can see how collisions, TTL, and exits return slots to the free list and make those metrics meaningful.
