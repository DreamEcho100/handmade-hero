# Lesson 64: Slab + Audio Scene Contract and Page-Backed Allocation

## Frontmatter

- Module: `13-slab-audio-lab`
- Lesson: `64`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `GAME_SCENE_SLAB_AUDIO_LAB`
  - `SlabAudioScene`
  - `SlabEvent`
  - `SlabAllocator`
  - `SlabPage`
  - `slab_init_in_arena`
  - `game_scene_name`
  - `game_scene_summary`
  - `game_scene_action_hint`

## Why This Lesson Exists

Lesson 63 closed the Pool module by proving one bounded free-list cycle:

```text
fixed slot
  -> free
  -> reuse
```

The Slab + Audio Lab teaches a different allocator story.

Here the important question is not:

```text
which fixed slot got reused?
```

It is:

```text
what happens when same-size events keep arriving,
one page fills up,
and the allocator grows by adding another page?
```

That is why the next module has to introduce slabs.

## Observable Outcome

By the end of this lesson, you should be able to explain the Slab + Audio scene contract in one sentence: one slab allocator manages same-size `SlabEvent` records across one or more pages, and transport/audio state exists to create visible pressure on those pages.

## First Reading Strategy

Read this scene in the same order the runtime teaches it:

1. read the scene labels first
2. read `SlabAllocator` before `SlabAudioScene`
3. treat `SlabEvent` as the payload the allocator is optimized for
4. only then think about how transport state creates pressure

If you start from the audio side, it is easy to misread this scene as a sequencer demo. The audio layer matters, but the allocator story comes first.

## Step 0: Carry Forward the Previous Module Correctly

The Pool lab taught you to watch one fixed backing region recycle slots.

Carry forward the habit of reading storage shape before reading motion. But do not carry forward the wrong assumption that every allocator scene must stay bounded to one block. The slab lesson begins exactly where the pool lesson stops: what if one page is not enough?

## Step 1: Read the Scene Labels as the Contract

The Slab scene advertises itself through three strings:

- `game_scene_name(...)` -> `Slab + Audio State Lab`
- `game_scene_summary(...)` -> `slab pages plus audio-state transitions`
- `game_scene_action_hint(...)` -> `SPACE inject a transport burst and crowd the sequencer`

Those labels tell you the module has two teaching layers at once:

- allocator page growth for same-size events
- audio/transport pressure as the live stress source

This scene is deliberately coupling memory pressure with sound-system state.

## Step 2: Read `SlabAllocator` Before the Scene Struct

In `src/utils/arena.h`, the allocator is:

```c
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

That already tells you the slab model.

Unlike the Pool allocator, a slab is not one fixed backing block.

It is:

- one linked list of pages
- each page holding many same-size slots
- new pages appearing only when existing ones fill up

One subtle but important point follows from that design: growth is coarse-grained. The allocator does not stretch one giant array by a few bytes. It adds another page of same-size slots.

## Step 2.5: Read `slab_alloc_page(...)` as the Real Growth Unit

The page helper in `src/utils/arena.h` makes the slab story concrete.

It allocates:

- one `SlabPage` header
- one zeroed memory region sized as `slot_size * slots_per_page`
- one per-page free list built by walking those slots backward

Then it links the new page into `allocator->pages` and updates:

- `page_count`
- `total_slots`
- `free_slots`

That means growth is not an abstract concept layered on top of the slab. Growth is literally another whole page entering the linked list.

It also means the slab lesson is still built on the arena lesson underneath it. When the slab lives in the level arena, page headers and page memory both come from level-owned storage.

## Visual: Pool Versus Slab

```text
Pool
  one backing block
  many fixed slots
  reuse inside that one block

Slab
  page 0 -> page 1 -> page 2 ...
  each page contains same-size slots
  grow by adding another page
```

That is the new lesson boundary.

## Step 3: Read `SlabEvent` as the Same-Size Payload Unit

The slab scene allocates one repeated record type:

```c
typedef struct {
  float wx;
  float wy;
  float ttl;
  float size;
  float r;
  float g;
  float b;
  int lane;
  int page_index;
  int state_index;
  unsigned int sequence_id;
  char label[32];
} SlabEvent;
```

This struct is important because it is exactly what slab allocation likes:

- one repeated record shape
- many instances over time
- the same size every time

That is why the slab is a better teaching fit here than a general arena push.

`SlabEvent` is structurally repetitive in exactly the way slabs are good at serving. Each event carries different values, but every allocation asks for the same record shape. That is the signal you should train yourself to notice.

## Step 4: Read `SlabAudioScene` as the Full Teaching Surface

The scene payload is:

```c
typedef struct {
  GameCamera camera;
  SlabAllocator slab;
  SlabEvent *events[GAME_SLAB_MAX_EVENTS];
  int event_count;
  float event_accum;
  float state_timer;
  unsigned int generated_count;
  unsigned int state_index;
  unsigned int phase_changes;
  size_t prev_page_count;
  unsigned int page_growth_count;
  unsigned int burst_count;
} SlabAudioScene;
```

This scene mixes:

- the allocator itself (`slab`)
- live payload (`events[]`, `event_count`)
- transport timing (`state_timer`, `state_index`)
- proof counters (`phase_changes`, `page_growth_count`, `burst_count`)

That means the scene is doing more orchestration than the earlier labs.

It also means this lesson has more moving parts than the pool lab did. The allocator is still the core, but the scene needs extra timing and mode data so the learner can see why another page appears instead of only noticing that it appeared.

One field is especially important for the next lesson: `prev_page_count` stores the last page total the scene already accounted for, so the update loop can detect real growth later instead of repeatedly re-counting the same boot page.

## Step 5: Read `slab_init_in_arena(...)` as the Ownership Bridge

The slab is initialized inside the level arena:

```c
slab_init_in_arena(&scene->slab, level, sizeof(SlabEvent),
                   GAME_SLAB_SLOTS_PER_PAGE,
                   "slab_audio_lab_events")
```

That means the ownership stack is:

```text
level arena owns slab pages
slab allocator manages same-size slots inside those pages
scene code allocates SlabEvent records from the slab
```

So again, the new allocator is layered on top of the earlier arena foundation.

That ownership layering stays consistent across the whole course now:

```text
perm owns long-lived app state
level owns current scene memory
slab owns same-size event slots inside level memory
scene code decides when those slots are requested and released
```

That continuity matters. The slab lesson is an expansion of the lifetime vocabulary, not a replacement for it.

## Worked Example: One Page Before Growth

Suppose the scene boots and the slab has one page with `GAME_SLAB_SLOTS_PER_PAGE` slots.

At this moment, the learner should picture:

1. one page already linked into the allocator
2. some of its slots occupied by initial events
3. remaining slots still free on that page
4. no reason yet for a second page to exist

That baseline is what makes the later growth event legible.

## Step 6: Notice the Pedagogical Shift From Pool to Slab

The Pool lab taught:

- one fixed backing store
- immediate slot return and reuse

The Slab lab teaches:

- repeated same-size event records
- page occupancy
- page growth under pressure
- later reuse inside grown pages

That is a meaningful escalation.

The learner is moving from fixed bounded reuse to page-backed growth plus reuse.

That is a real conceptual jump. The question is no longer only "which old slot came back?" It is also "when did the existing footprint stop being enough?"

## Common Mistake: Thinking This Scene Is Mostly About Audio

Audio is the pressure source.

It is not the allocator lesson by itself.

The allocator lesson is:

```text
same-size event traffic can outgrow one page,
force another page,
and later reuse that expanded capacity
```

The audio layer makes that story visible and urgent.

That separation is worth keeping explicit from the first lesson of the module. Audio is the pressure source and part of the proof surface. Slab pages are still the main subject.

## JS-to-C Bridge

This is like storing many queue entries of the same shape in page-sized chunks instead of one big resizable list. Growth happens one page at a time, and later traffic reuses slots inside those existing pages.

## Try Now

Open `src/utils/arena.h` and `src/game/main.c`, then do these checks:

1. Read the Slab scene strings and explain why the scene contract mentions both pages and audio-state transitions.
2. Read `SlabAllocator` and explain how it differs from `PoolAllocator`.
3. Read `SlabEvent` and explain why it is a natural slab payload.
4. Find `slab_init_in_arena(...)` in the fill path and explain the ownership layering.

## Exercises

1. Explain why slabs are a better fit than pools for this scene's payload.
2. Explain the difference between fixed-slot reuse and page-backed growth.
3. Explain why `page_growth_count` belongs in the scene struct.
4. Describe the Slab scene contract in one sentence.

## Runtime Proof Checkpoint

At this point, you should be able to explain the Slab + Audio scene contract: one slab allocator manages same-size `SlabEvent` records across one or more pages, and scene-specific transport/audio pressure is used to make page growth and later reuse visible.

## Recap

This lesson established the Slab + Audio teaching surface:

- the scene is about slab pages plus transport-state pressure
- `SlabAllocator` grows by page rather than by one huge backing block
- `SlabEvent` is the repeated same-size payload unit
- the slab still lives inside the level arena as scene-owned state

Next, we follow the fill and allocation path so the learner can see how the first page starts, how events get stamped with page/state identity, and why same-size allocation is central to the whole scene.
