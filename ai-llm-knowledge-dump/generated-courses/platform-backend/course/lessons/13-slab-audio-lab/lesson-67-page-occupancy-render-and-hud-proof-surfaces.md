# Lesson 67: Page Occupancy Render and HUD Proof Surfaces

## Frontmatter

- Module: `13-slab-audio-lab`
- Lesson: `67`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_render_slab_audio`
  - `slab_audio_count_events_for_page`
  - `slab_audio_state_name`
  - `game_draw_hud`
  - `game_scene_trace_step_evidence`
  - `game_draw_scene_trace_panel`
  - `Slab mode:%s events:%d pages:%zu growth:%u bursts:%u phase_swaps:%u`

## Why This Lesson Exists

Lesson 66 explained how the scene creates pressure.

This lesson explains how the runtime proves that pressure visually.

The Slab scene would be hard to read if it only printed page counts in text.

Instead, it builds a three-layer proof surface:

- transport-state blocks
- moving event cards
- page occupancy boards

Then it ties those visuals back into the HUD and trace panel.

## Observable Outcome

By the end of this lesson, you should be able to explain why the slab page boards are the strongest visual proof in the module: they show stable page occupancy while transport blocks and event cards explain how that occupancy was produced.

## First Reading Strategy

Read the render path from top to bottom and then back into the HUD:

1. transport state blocks
2. event cards in the middle field
3. page occupancy boards
4. explanatory world label
5. HUD metric line and trace evidence

This scene is easy to misread if you start from the text only. The visuals and text are designed to explain each other.

## Step 0: Ask What the Learner Could Not Prove Without the Boards

If the scene only showed transport state and moving event cards, the learner could tell that traffic was changing but would struggle to prove when one page became two or how full each page was.

The page boards exist to keep allocator shape visible even while transport mode and event motion keep changing.

## Step 1: Read the Transport Blocks at the Top of the Scene

`game_render_slab_audio(...)` first draws one block per transport state.

The active state is brightly highlighted and labeled with `slab_audio_state_name(...)`.

This matters because the learner should always know which state is currently driving spawn cadence and pressure.

The slab allocator lesson depends on that context.

The top row is therefore not ornamental. It is the cause panel for everything happening lower in the scene.

## Step 2: Read the Event Cards as Live Same-Size Payload

Each live `SlabEvent` draws as a colored card with its label.

The color comes from `state_index` at creation time.

That means the learner can watch mixed traffic from different transport phases continue to occupy the slab at the same time.

This is a stronger proof than a single counter because it shows overlapping event lifetimes across states.

That overlap is important. The scene is not resetting all traffic on every mode change. Older events can persist while newer ones arrive, which is exactly the kind of overlap that can fill pages.

## Visual: How the Scene Explains Itself

```text
top row
  -> current transport state

middle field
  -> live same-size events moving through time

bottom row
  -> page occupancy boards showing how many slots each page holds
```

That is a very good visual teaching stack.

## Step 3: Read `slab_audio_count_events_for_page(...)` as the Occupancy Bridge

The page boards are not guessed.

The render counts how many currently visible events belong to each page.

Then it draws one mini-grid of slot occupancy per page.

That means the learner can see page usage directly instead of only reading `page_count` in text.

One important implementation detail is easy to miss: the page boards are counting occupancy from the stamped `page_index` on visible events. That means this surface is deliberately a scene-facing proof board, not a raw dump of every free-list pointer inside the allocator.

That design is appropriate for this module. The learner is supposed to read how many live cards the scene currently associates with each page, then compare that against the HUD and trace, instead of parsing allocator internals directly.

## Step 4: Read the Page Boards as the Allocator Diagram

For each page, the scene draws:

- one big page background card
- one set of tiny slots inside it
- each tiny slot filled or dimmed depending on occupancy count

That is the slab equivalent of the Pool scene's slot board.

But there is one crucial difference:

```text
Pool board shows one fixed backing store
Slab board shows multiple pages that can appear over time
```

That is the allocator escalation the learner should feel.

The pool board taught stable slots inside one bounded backing region. The slab board teaches stable pages that can multiply under pressure.

The grid itself is also intentionally simple: three columns of tiny slot cells with `GAME_SLAB_SLOTS_PER_PAGE` total positions. That gives the learner a page-shaped occupancy sketch without turning the scene into a memory debugger.

## Step 5: Read the Slab HUD Metric Line

In `game_draw_hud(...)`, the scene prints:

```text
Slab mode:%s events:%d pages:%zu growth:%u bursts:%u phase_swaps:%u
```

This line is excellent because it compresses the whole module into one summary:

- current transport mode
- current live event count
- current page count
- total page-growth events observed
- manual burst injections
- total state transitions

The world render shows the spatial meaning.

The HUD shows the system summary.

Together they answer two different questions:

- what is the allocator doing right now in space?
- what has the system accumulated over time?

## Step 6: Read the Slab Trace Evidence Fields

For slab steps, `game_scene_trace_step_evidence(...)` appends:

- `events=%d`
- `bursts=%u`
- `pages=%zu`
- `mode=%s`

This is a very clean mapping.

Each exercise step is backed by one direct live field.

So the trace panel tells the learner not only what should happen, but which runtime field proves it happened.

That is especially useful in the slab module because several signals can move at once. The trace panel narrows the learner's attention back to the field that matters for the current proof step.

## Step 7: Notice the Scene's Own Teaching Label

The render also prints:

```text
transport state changes push events across slab pages
```

That is the module's core claim in one sentence.

The scene is explicitly telling the learner how to interpret the page boards below.

That direct label is doing real pedagogical work. It ties transport-state language to allocator-state evidence in one sentence.

## Worked Example: Reading One New Page Correctly

Suppose the HUD changes from `pages:1` to `pages:2`.

What should the learner inspect next?

1. the bottom row should now show a second page board
2. some event cards should now carry positions associated with that new page
3. the transport row should explain which mode was active during the pressure spike
4. the trace panel should treat page count as a completed proof step, not just a visual impression

That is a stronger reading than only noticing a number went up.

## Common Mistake: Treating `page_count` as the Only Important Slab Metric

That is too shallow.

`page_count` tells you that growth happened.

The page boards, event cards, and current mode tell you why it happened and what the scene is doing with that capacity now.

## JS-to-C Bridge

This is like pairing a mode indicator, a stream of live queue items, and a set of page-occupancy panels. One number tells you growth happened. The full set of surfaces tells you how the system got there and what it is doing next.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read `game_render_slab_audio(...)` and explain the role of the transport blocks, event cards, and page boards.
2. Read `slab_audio_count_events_for_page(...)` and explain why page occupancy is derived rather than guessed.
3. Find the Slab HUD metric line and explain why `mode`, `pages`, `growth`, and `phase_swaps` belong together.
4. Read the Slab trace evidence suffixes and explain what each one proves.

## Exercises

1. Explain why the page boards are the key allocator diagram in this scene.
2. Explain why event color/state labeling matters for understanding page pressure.
3. Explain the difference between the top transport blocks and the bottom page boards.
4. Describe the full visual proof stack of the Slab scene.

## Runtime Proof Checkpoint

At this point, you should be able to explain how the Slab scene proves allocator behavior on screen: transport blocks show the active pressure mode, event cards show live same-size payload, page boards show occupancy across one or more slab pages, and the HUD/trace summarize those same facts in text.

## Recap

This lesson explained the Slab scene proof surfaces:

- transport blocks explain current mode
- event cards show live same-size allocations across time
- page boards show occupancy across slab pages
- the HUD and trace panel compress those visuals into precise proof text

Next, we connect those visuals to the guided prompts, warning strings, and recovery messaging so the learner can interpret pressure, growth, and later reuse without confusing cause and severity.
