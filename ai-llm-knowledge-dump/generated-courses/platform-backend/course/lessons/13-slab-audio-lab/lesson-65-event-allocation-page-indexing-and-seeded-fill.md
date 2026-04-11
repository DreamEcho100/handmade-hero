# Lesson 65: Event Allocation, Page Indexing, and Seeded Fill

## Frontmatter

- Module: `13-slab-audio-lab`
- Lesson: `65`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `game_fill_slab_audio`
  - `slab_audio_push_event`
  - `slab_alloc`
  - `slab_audio_page_index_for_ptr`
  - `slab_audio_state_name`
  - `generated_count`
  - `sequence_id`
  - `page_index`

## Why This Lesson Exists

Lesson 64 defined the slab scene contract.

Now the learner needs to see the first page come alive.

This scene only teaches slabs well if the learner understands how one event allocation gets stamped with:

- page identity
- state identity
- sequence identity

That is what turns page growth from abstract allocator theory into visible scene evidence.

## Observable Outcome

By the end of this lesson, you should be able to walk through `game_fill_slab_audio(...)` and `slab_audio_push_event(...)` in order and explain how the scene boots with one live page, how events are stamped with page and transport identity, and why the visible `events[]` list is not the allocator itself.

## First Reading Strategy

Read the slab boot path in layers:

1. scene reset fields in `game_fill_slab_audio(...)`
2. `slab_init_in_arena(...)` and `slab_alloc(...)`
3. the oldest-event eviction rule in `slab_audio_push_event(...)`
4. page, lane, state, and sequence stamping

If you jump directly to `page_index`, you will miss why the scene starts with seeded occupancy and why that matters for the rest of the module.

## Step 0: Keep Two Capacities Separate

This scene has two limits that are easy to confuse.

```text
scene-visible event list capacity
slab allocator capacity across one or more pages
```

The scene can trim the visible pointer list while the slab allocator still keeps page-backed storage underneath. That distinction will matter later when growth and reuse coexist.

## Step 1: Read `game_fill_slab_audio(...)` as Scene Boot

The fill path starts by clearing scene-local counters:

- `event_count = 0`
- `event_accum = 0.0f`
- `state_timer = 0.0f`
- `generated_count = 0`
- `state_index = 0`
- `phase_changes = 0`
- `page_growth_count = 0`
- `burst_count = 0`

Then it initializes the slab and seeds the scene with eight starting events.

That is a very important design choice.

It also sets `prev_page_count` twice: once before seeding and once again after the eight startup pushes finish. That detail matters because the scene wants the later growth proof to mean "another page beyond the seeded baseline," not "the initial page exists."

The learner begins with visible same-size traffic already occupying the first page.

That is a strong teaching choice. The scene does not waste the learner's attention on waiting for the first useful event. It starts with enough occupancy to make the page board meaningful immediately.

## Step 2: Read `slab_init_in_arena(...)` and `slab_alloc(...)` Together

`slab_init_in_arena(...)` creates the first page.

Then each later `slab_alloc(...)` call searches existing pages for a free slot and allocates a new page only if no page has space left.

That means the main allocator rule is:

```text
reuse free slots on existing pages first
grow only when all pages are full
```

That is the heart of slab behavior.

One subtle but important implication is that growth only happens after reuse opportunities inside existing pages have been exhausted. The allocator is conservative by design.

## Visual: Slab Allocation Strategy

```text
page 0 has free slots?
  yes -> allocate there
  no  -> look at next page

no page has free slots?
  -> allocate a new page
```

This is not random growth.

It is page-by-page reuse and expansion.

That exact rhythm is what later makes `page_growth_count` meaningful. A new page is evidence that every older page was already out of free slots at allocation time.

## Step 3: Follow `slab_audio_push_event(...)`

The event push helper is the most important function in the module.

It does four big jobs:

1. if the scene-level event pointer array is full, free the oldest visible event
2. allocate one new `SlabEvent` from the slab
3. stamp allocator and scene identity fields onto the event
4. append the event to the visible event list

That means the scene is coordinating two capacities at once:

- scene-visible event list capacity
- slab slot capacity across pages

That is one of the reasons the function is pedagogically rich. It is not only allocating memory. It is translating allocator decisions into scene-visible evidence.

## Step 4: Notice the Oldest-Event Eviction Rule

At the top of `slab_audio_push_event(...)`:

```c
if (scene->event_count >= GAME_SLAB_MAX_EVENTS && scene->events[0]) {
  slab_free(&scene->slab, scene->events[0]);
  memmove(scene->events, scene->events + 1,
          (size_t)(scene->event_count - 1) * sizeof(scene->events[0]));
  scene->event_count--;
}
```

That means the scene will not let its visible event list grow forever.

Older events are freed back to the slab and removed from the front of the pointer list.

This is good pedagogy because it lets the learner watch both:

- page growth under pressure
- later reuse of already-grown pages

That eviction rule also prevents the visible scene from becoming unreadable. The learner can still inspect page behavior without drowning in an ever-growing list of old event labels.

## Step 5: Read the Page Index Stamp as the Proof Bridge

After allocation, the helper does:

```c
event->page_index = slab_audio_page_index_for_ptr(&scene->slab, event);
```

This is the slab equivalent of the Pool scene's slot-index proof.

The event gets a stable page identity derived from the actual allocator page containing its pointer.

That is why the render can later show page occupancy truthfully.

Notice the difference from the pool lab. There, slot identity was recomputed from one backing block. Here, page identity is derived once at allocation time by scanning the slab's current page list, then stamped onto the event so the render and trace can reason about the allocation the learner just triggered.

That wording matters. The scene is not re-scanning every live pointer every frame. It captures the page bucket at allocation time and uses that as learner-facing evidence afterward.

## Step 6: Read the Other Identity Fields Together

The helper also stamps:

- `lane`
- `state_index`
- `sequence_id`
- `label`

These fields matter because they let the learner answer three separate questions:

- where was this event allocated? -> `page_index`
- which transport state created it? -> `state_index`
- where is it in the overall stream? -> `sequence_id`

That is excellent proof-oriented design.

It lets the learner answer several different questions without leaving the running scene:

- which page owns this event?
- which transport phase created it?
- how far along is it in the total event stream?

There is one more visual bridge hidden in the same helper: `event->wx` is offset by `page_index` and a small sequence wobble, while `event->wy` uses the requested row plus `lane`. That means allocation metadata is immediately turned into visible placement, not just stored for later logs.

## Step 7: Notice the Initial Eight Events Are Part of the Lesson

At fill time, the scene pushes eight `note` events immediately.

That means the learner does not start from an empty slab.

They start from:

- a real first page
- visible initial occupancy
- a base transport mode

So the module can spend its time on growth and reuse instead of on empty-scene bootstrapping.

The initial eight events are therefore not incidental setup. They are part of the lesson plan.

## Worked Example: Reading One Seeded Event Correctly

Suppose the first seeded event is allocated while `state_index == 0`.

What should the learner be able to infer?

1. it lives on whatever page `slab_alloc(...)` returned from the initial page set
2. `page_index` tells you which slab page contains its pointer
3. `state_index` says which transport mode created it
4. `sequence_id` tells you where it sits in the scene's total history
5. `events[]` only stores a pointer to it so the scene can render and update it

That is a complete identity story for one same-size allocation.

## Common Mistake: Thinking `events[]` Is the Allocator

It is not.

`events[]` is only the scene's list of visible event pointers.

The real allocator is `scene->slab` and its pages.

That distinction matters because the event list can be reordered or trimmed without changing the slab model itself.

The pool lab already taught you not to confuse presentation state with allocator state. This lesson extends that rule to a more complex scene.

## JS-to-C Bridge

This is like keeping one visible queue of active records while the real allocator manages page-backed storage underneath. The UI list is not the allocator. It is only a window into what the allocator currently holds.

## Try Now

Open `src/game/main.c` and `src/utils/arena.h`, then do these checks:

1. Read `game_fill_slab_audio(...)` and explain why the scene starts with eight events already pushed.
2. Read `slab_alloc(...)` and explain when a new page is created.
3. Read `slab_audio_push_event(...)` and explain why page index, state index, and sequence ID are all stamped onto each event.
4. Explain the difference between the visible `events[]` array and the underlying slab allocator.

## Exercises

1. Explain why page identity needs to be derived from pointer location rather than guessed later.
2. Explain why freeing the oldest visible event does not invalidate the slab lesson.
3. Explain the full order of operations inside `slab_audio_push_event(...)`.
4. Describe what the seeded fill is trying to teach before the learner presses anything.

## Runtime Proof Checkpoint

At this point, you should be able to explain the first slab allocation story: the scene boots with one slab page, seeds it with initial same-size events, and stamps each event with page/state/sequence identity so later page growth and reuse can be read directly from the runtime.

## Recap

This lesson explained how the slab scene starts and allocates events:

- fill initializes one slab-backed scene with visible starting occupancy
- `slab_alloc(...)` reuses existing page slots before growing
- `slab_audio_push_event(...)` stamps page and transport identity onto each event
- the visible event list is only a scene-level view onto the slab

Next, we follow the update loop so the learner can see how transport phases, burst input, spawn cadence, and page-growth detection work together to create the pressure story this module is built around.
