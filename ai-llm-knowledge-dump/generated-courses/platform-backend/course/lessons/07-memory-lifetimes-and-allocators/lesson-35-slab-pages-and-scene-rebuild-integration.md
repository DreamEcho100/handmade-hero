# Lesson 35: Slab Pages and Scene Rebuild Integration

## Frontmatter

- Module: `07-memory-lifetimes-and-allocators`
- Lesson: `35`
- Status: Required
- Target files:
  - `src/utils/arena.h`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
  - `src/platform.h`
- Target symbols:
  - `SlabAllocator`
  - `SlabPage`
  - `slab_alloc_page`
  - `slab_init_in_arena`
  - `slab_alloc`
  - `slab_free`
  - `slab_reset`
  - `game_fill_slab_audio`
  - `game_rebuild_scene`

## Observable Outcome

By the end of this lesson, you should be able to explain how the slab allocator grows page by page, how freed slab objects return to a page-local free list, and why scene rebuild makes a natural lifetime boundary for slab-backed content created inside the level arena.

## Prerequisite Bridge

Lesson 34 covered one fixed pool with one backing array.

The next problem is slightly different:

```text
what if we still want fixed-size slots,
but we do not want to commit all capacity up front?
```

The course answers that with slabs.

## Step 1: Read `SlabAllocator` as a Growable Chain of Pool-Like Pages

In `src/utils/arena.h`:

```c
typedef struct SlabPage {
  struct SlabPage *next;
  void *memory;
  PoolFreeNode *free_list;
  size_t slot_count;
  size_t free_count;
} SlabPage;

typedef struct SlabAllocator {
  SlabPage *pages;
  size_t slot_size;
  size_t slots_per_page;
  size_t page_count;
  size_t total_slots;
  size_t free_slots;
  size_t high_watermark;
  Arena *backing_arena;
  int owns_pages;
  const char *debug_name;
} SlabAllocator;
```

This is the key design idea:

```text
a slab allocator is not one giant slot array
it is a linked list of slot arrays called pages
```

Each page has its own free list.

## Step 2: `slab_alloc_page(...)` Creates One More Capacity Chunk

When the allocator needs a new page, it allocates:

- one `SlabPage` header
- one contiguous slot-memory region for that page

If the slab is arena-backed, both come from `arena_push_zero(...)` on the backing arena.

Then the page wires its internal free list exactly like a small pool would.

Finally the page is pushed onto the allocator's page list and the global counts are updated.

So page growth is explicit, incremental capacity growth.

## Visual: Pool vs Slab

```text
pool:
  one backing region
  one free list

slab:
  page 2 -> page 1 -> page 0
   |         |         |
   free      free      free
   list      list      list
```

Both use fixed-size slots.

The slab just grows by adding more pool-like pages.

## Step 3: `slab_init_in_arena(...)` Makes Page Growth a Scene-Lifetime Decision

The scene path uses:

```c
slab_init_in_arena(&scene->slab, level, sizeof(SlabEvent),
                   GAME_SLAB_SLOTS_PER_PAGE,
                   "slab_audio_lab_events")
```

This tells you two things at once.

### First

Each slab object is a `SlabEvent`-sized fixed slot.

### Second

All slab pages belong to the level arena's lifetime.

So page growth can happen during the scene, but all of it disappears cleanly when the scene rebuilds.

## Step 4: Read `slab_alloc(...)` as "Find a Page With a Free Slot"

The allocation path loops over the page list:

```c
for (page = allocator->pages; page; page = page->next) {
  if (page->free_list)
    break;
}
if (!page) {
  page = slab_alloc_page(allocator);
  if (!page)
    return NULL;
}
```

Then it pops one node from that page's free list and updates page-local and allocator-wide counts.

So the slab allocation rule is:

```text
reuse an existing page if possible
grow by one new page only when all existing pages are full
```

## Step 5: Read `slab_free(...)` as a Page Search Plus Free-List Pushback

To free a pointer, the allocator scans pages until it finds the page that owns that slot:

```c
if (slab_page_contains(allocator, page, ptr)) {
  slab_link_free_slot(page, ptr);
  ++allocator->free_slots;
  ...
  return;
}
```

That is the price slabs pay for growable pages: freeing must locate the owning page first.

Once found, the actual reinsertion is still just free-list pushback.

## Step 6: The Heap-Backed and Arena-Backed Behaviors Differ on Empty Pages

`slab_free_empty_pages(...)` only releases totally empty pages when the slab is not arena-backed.

That is a useful nuance.

Why?

Because if the slab lives inside an arena, its pages are part of that arena's lifetime policy. They are not individually destroyed on free.

Instead, the whole scene lifetime eventually resets them together.

This is a very intentional layering of allocation rules.

## Step 7: Watch `game_fill_slab_audio(...)` Build a Slab-Backed Scene

In `src/game/main.c`:

```c
if (slab_init_in_arena(&scene->slab, level, sizeof(SlabEvent),
                       GAME_SLAB_SLOTS_PER_PAGE,
                       "slab_audio_lab_events") != 0)
  return;
```

Then the scene primes the slab with startup events:

```c
for (int i = 0; i < 8; ++i)
  slab_audio_push_event(scene, "note", 1.6f + (float)(i % 4) * 1.15f);
```

That means the slab is not only a background allocator concept.

Its page count and reuse behavior become visible in the actual slab-audio lab.

## Step 8: Study `slab_audio_push_event(...)`

The live helper does two important things.

### It drops the oldest event when the logical event queue is full

```c
if (scene->event_count >= GAME_SLAB_MAX_EVENTS && scene->events[0]) {
  slab_free(&scene->slab, scene->events[0]);
  memmove(scene->events, scene->events + 1,
          (size_t)(scene->event_count - 1) * sizeof(scene->events[0]));
  scene->event_count--;
}
```

### It allocates a new event slot from the slab

```c
event = (SlabEvent *)slab_alloc(&scene->slab);
if (!event)
  return NULL;
memset(event, 0, sizeof(*event));
```

So the scene combines:

- logical queue behavior
- slot reuse
- page growth when needed

That is exactly the kind of workload slabs are good at teaching.

## Step 9: Connect the Slab Back to Scene Rebuild

The rebuild path is still:

```c
arena_reset(level);
level_state = ARENA_PUSH_ZERO_STRUCT(level, GameLevelState);
...
game_fill_slab_audio(level_state, level);
```

This is the architectural payoff.

The slab pages themselves are allocated inside `level`, so rebuilding the scene erases:

- the `GameLevelState`
- the slab allocator object
- all slab pages
- all slab events stored in those pages

No special slab teardown loop is required for the scene reset path.

## Step 10: Why a Slab Fits This Scene Better Than a Plain Pool

A plain pool would require committing all event capacity up front.

A slab can start smaller and grow page by page under pressure.

That makes it a good fit for an event stream whose pressure changes over time.

It also makes page growth itself visible as part of the lab's teaching output.

## Practical Exercises

### Exercise 1: Explain Growth

When does `slab_alloc(...)` create a new page?

Expected answer:

```text
when it cannot find any existing page with a non-empty free list
```

### Exercise 2: Explain the Scene Lifetime

Why is `slab_init_in_arena(..., level, ...)` an important design choice?

Expected answer:

```text
because it makes all slab pages part of the current scene lifetime, so a level-arena reset removes the entire slab-backed scene state in one step
```

### Exercise 3: Compare Pool vs Slab

Why is a slab more flexible than a pool for the slab-audio event stream?

Expected answer:

```text
because a slab can grow capacity one page at a time instead of requiring all fixed-slot capacity to be committed up front
```

## Common Mistakes

### Mistake 1: Thinking a slab is variable-size per object

Each slot in one slab still has one fixed size.

### Mistake 2: Assuming arena-backed slab pages should be individually freed on every slot release

That would fight the arena lifetime model.

### Mistake 3: Forgetting the difference between the logical event queue and the allocator's physical storage

The scene may drop old logical entries even while the slab allocator manages reusable physical slots underneath.

## Troubleshooting Your Understanding

### "Why does `slab_free(...)` search pages?"

Because the slab may own multiple page-sized slot arrays, so it must find which page contains the pointer before returning that slot to the correct free list.

### "Why not rebuild the whole slab every time one event dies?"

Because the slab is designed for ongoing reuse during the scene, not for full reset on each object death.

## Recap

You now understand the slab model and its runtime integration:

1. a slab is a chain of fixed-slot pages
2. allocation reuses existing pages before growing a new one
3. free returns a slot to its owning page
4. arena-backed pages participate in the scene lifetime instead of individual page teardown
5. `game_fill_slab_audio(...)` and `game_rebuild_scene(...)` together show the full ownership story

## Module Bridge

Module 07 has now established the runtime's full memory vocabulary:

- session / scene / frame lifetimes through arenas
- guarded initial arena bootstrap
- aligned push allocation and typed macros
- rewindable temp scopes
- fixed-slot pools
- growable slab pages integrated with scene rebuild

That gives the rest of the course a stable answer to the question:

```text
what survives,
what gets reused,
and what is allowed to vanish immediately?
```
