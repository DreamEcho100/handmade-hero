# Lesson 43: Trace Panel Structure, Step Rows, and Evidence Suffixes

## Frontmatter

- Module: `09-runtime-instrumentation-and-proof-panels`
- Lesson: `43`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_draw_trace_panel_box`
  - `game_draw_trace_step`
  - `game_scene_trace_step_evidence`
  - `game_scene_trace_step_label`
  - `game_draw_scene_trace_panel`

## Observable Outcome

By the end of this lesson, you should be able to explain how one trace row becomes a proof statement with focus styling, completion styling, and a live evidence suffix, and how the full scene trace panel is assembled from those reusable pieces.

## Prerequisite Bridge

Lessons 41 and 42 established proof state and guidance strings.

Lesson 43 turns that meaning into a visible panel.

The key question is:

```text
how does the runtime draw a proof checklist
without falling back to a static label list?
```

The answer is the trace-panel composition path.

## Why This Lesson Exists

The trace panel is not just one more HUD box.

It is the runtime's main proof surface.

Each row wants to tell the learner three things at once:

- what the step is
- whether it is active or completed
- what live metric proves completion when it has happened

That is why the panel uses multiple helpers instead of one huge inline drawing blob.

## Step 1: Start With the Panel Box Helper

The frame helper is:

```c
static void game_draw_trace_panel_box(Backbuffer *backbuffer,
                                      const RenderContext *hud,
                                      const char *title)
```

It draws:

- the panel body rectangle
- the title strip rectangle
- the title label

This is the panel chrome layer.

That separation matters because scene-specific proof content should not have to redraw basic panel framing from scratch every time.

## Step 2: Read `game_draw_trace_step(...)` as the Step-Row Renderer

The step-row helper is:

```c
static void game_draw_trace_step(..., const char *label,
                                 int active, int completed)
```

It maps state into color treatment.

### Default state

- muted box color
- muted text color

### Completed state

- green marker
- bright text

### Active state

- gold marker
- bright text

This means the row communicates status and focus visually before the learner even reads the label.

## Visual: Step Style Mapping

```text
completed -> green marker + bright text
active    -> gold marker  + bright text
idle      -> muted marker + muted text
```

That is a small but effective visual grammar.

## Step 3: Notice That Active and Completed Are Separate Questions

This is easy to miss.

The helper does not ask for one combined state enum.

It asks for:

- `active`
- `completed`

That is useful because the panel wants to show:

```text
which proof step is the current focus
and which proofs are already earned
```

Those are related, but not identical, ideas.

## Step 4: Read `game_scene_trace_step_evidence(...)`

This helper is where the checklist becomes a proof system.

It takes:

- current scene
- one step mask

and writes a small evidence string into a buffer.

Examples by scene:

### Arena lab

- `manual=...`
- `depth=...`
- `rewinds=...`

### Level-reload lab

- `serial=...`
- `flash=...`
- `scan=...`
- `focus=...`

### Pool lab

- `active=...`
- `hits=...`
- `free=...`
- `reuse=...`

### Slab lab

- `events=...`
- `bursts=...`
- `pages=...`
- `mode=...`

That is the runtime's answer to the question:

```text
what metric proves this step happened?
```

## Worked Example: Why Evidence Suffixes Matter

Without evidence, a completed row only says:

```text
the runtime claims this step is done
```

With evidence, the row can say:

```text
the runtime claims this step is done,
and here is the live metric that justified that claim
```

That is much stronger pedagogically.

## Step 5: Read `game_scene_trace_step_label(...)`

This helper combines the base label and optional evidence suffix.

Its rule is:

### Incomplete step

Return the base label unchanged.

### Completed step

Ask `game_scene_trace_step_evidence(...)` for an evidence string and append it in brackets if one exists.

So the label itself upgrades when proof is earned.

## Visual: Label Upgrade

```text
before proof:
  "4 reuse the hottest returned slot"

after proof:
  "4 reuse the hottest returned slot [reuse=3]"
```

That bracketed suffix is what turns the row from a to-do item into a proof statement.

## Step 6: Read `game_draw_scene_trace_panel(...)` as the Scene-Aware Compositor

The panel compositor does several jobs in order:

1. read `progress_mask` and `required_mask`
2. compute `active_step` via `game_scene_exercise_focus_step(...)`
3. choose scene-specific title, base labels, and detail text
4. build upgraded labels with `game_scene_trace_step_label(...)`
5. draw each row with `game_draw_trace_step(...)`
6. draw the detail line, prompt line, and observation line
7. attach the audio trace panel beneath it

That is the whole panel pipeline.

## Step 7: Notice the Scene-Specific Labels Are Still Kept Explicit

The runtime does not try to auto-generate all proof labels from data tables.

Instead it keeps the per-scene label text explicit in the switch cases:

- arena trace
- level trace
- pool trace
- slab trace

That is reasonable here because the wording itself is part of the teaching design.

The shared helpers provide structure; the scene branches provide meaning.

## Step 8: Notice the Detail Line Under the Rows

After drawing the four steps, the compositor builds one scene-specific detail summary such as:

- manual/depth/rewinds and exercise counts for arena
- serial/focus/seed and exercise counts for level
- reuse/hottest/free and exercise counts for pool
- mode/pages/growth and exercise counts for slab

This line is a bridge between the step rows and the larger scene metrics.

It reinforces that the trace panel is not an isolated checklist. It is tied to real runtime state.

## Visual: Trace Panel Composition

```text
panel box + title
  -> row 1
  -> row 2
  -> row 3
  -> row 4
  -> scene detail line
  -> prompt line
  -> observation line
  -> audio trace panel below
```

That is the panel as the learner experiences it.

## Practical Exercises

### Exercise 1: Explain Evidence Suffixes

Why are bracketed evidence suffixes stronger than a plain completed checkbox?

Expected answer:

```text
because they show the live metric that justified completion instead of only asserting that the step is done
```

### Exercise 2: Explain Shared Versus Scene-Specific Parts

Why does the runtime use shared row/panel helpers but still keep scene-specific labels and evidence branches?

Expected answer:

```text
because framing and row styling are reusable structure, while the actual proof wording and evidence metrics are specific to what each scene teaches
```

### Exercise 3: Explain `active_step`

Why does the panel need an active-step highlight instead of only completed coloring?

Expected answer:

```text
because the panel is meant to guide the learner toward the next missing proof, not only summarize which proofs are already done
```

## Common Mistakes

### Mistake 1: Treating the trace panel as only a pretty checklist

It is a proof compositor driven by masks and live metrics.

### Mistake 2: Thinking the detail line duplicates the rows

It summarizes a slightly different slice of scene state and completion totals.

### Mistake 3: Forgetting that label text changes after proof

The label upgrade is part of the runtime's evidence model.

## Troubleshooting Your Understanding

### "Why not just draw 'complete' beside each row?"

Because the course wants proof statements backed by live evidence, not only status tags.

### "Why does the panel still need prompt and observation lines if the rows already exist?"

Because the rows summarize structured proof goals, while prompt and observation lines actively coach the learner through what to do and what to notice next.

## Recap

You now understand the main trace-panel composition:

1. shared panel framing draws the box and title strip
2. shared row rendering maps active/completed state to visual styling
3. scene-specific evidence helpers generate proof suffixes
4. label helpers upgrade completed rows with bracketed evidence
5. the scene-aware compositor assembles rows, detail text, guidance, and the lower audio trace

## Next Lesson

Lesson 44 looks at that lower audio trace panel: scene-aware metric lines, cause and anomaly text, and the warning meter that teaches severity separately from cause.
