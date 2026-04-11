# Lesson 45: Final HUD Composition and Runtime Proof

## Frontmatter

- Module: `09-runtime-instrumentation-and-proof-panels`
- Lesson: `45`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
  - `src/utils/render.h`
- Target symbols:
  - `game_draw_hud`
  - `arena_get_stats`
  - `make_cursor`
  - `cursor_newline`
  - `game_scene_name`
  - `game_scene_summary`
  - `game_scene_action_hint`
  - `game_describe_audio_evidence`
  - `game_draw_scene_trace_panel`

## Observable Outcome

By the end of this lesson, you should be able to explain how the final HUD turns scene-machine state, allocator evidence, audio evidence, logs, prompts, and proof panels into one deliberate teaching overlay rather than one pile of disconnected debug text.

## Prerequisite Bridge

Lessons 41 through 44 built the pieces of the proof system:

- exercise masks
- prompt and observation guidance
- the main trace panel
- the audio trace panel and warning meter

Lesson 45 closes the module by showing how those pieces fit together in the real HUD draw path.

## Why This Lesson Exists

The runtime wants its overlay to answer a learner's questions in a sensible order.

If the HUD dumped all numbers at once, it would feel like debug noise.

Instead the HUD is composed as a teaching sequence.

That sequence is what this lesson explains.

## Step 1: Start With `game_draw_hud(...)`

The function begins by acquiring three categories of data:

```c
perm_stats = arena_get_stats(perm);
level_stats = arena_get_stats(level);
scratch_stats = arena_get_stats(scratch);
text = make_cursor(hud, 0.7f, 8.9f);
```

This is already a strong architectural signal.

Before drawing any lines, the HUD explicitly gathers:

- allocator evidence
- a layout cursor for ordered text flow

So the overlay is grounded in live subsystem state from the first line onward.

## Step 2: Read the Cursor as the Top-Level Layout Engine

The HUD uses:

- `make_cursor(...)`
- `cursor_newline(...)`

to lay out the summary section line by line.

That means the course is reusing Module 06's HUD layout machinery directly inside the proof system.

This is important because it shows that the instrumentation layer is not architecturally separate from the earlier coordinate/layout work.

## Step 3: The First Lines Are Orientation, Not Raw Data

The HUD begins with:

1. scene name
2. scene summary
3. scene action hint

That is the correct teaching order.

The learner first needs to know:

```text
where am I?
what is this scene about?
what should I press here?
```

Only after that does it make sense to show deeper state lines.

## Visual: Top-Level HUD Information Order

```text
scene name
scene summary
scene action hint
controls
camera hints
scene-machine state
override state
allocator stats
audio summary
scene-specific summary
recent logs
trace panels
```

That is the overlay as a teaching sequence.

## Step 4: Read the Controls and Camera Lines as Shared Orientation

The HUD prints one controls line and one camera line before scene-machine detail.

That is a small but useful design choice.

It means the overlay always reminds the learner of the core interaction vocabulary before showing scene-specific evidence.

So the HUD is not only introspection. It is also runtime onboarding.

## Step 5: Read the Scene-Machine Summary Lines

The HUD next prints summaries like:

```text
State current:... requested:... previous:... transitions:... scale:... fps:...
Override compile:... runtime:... forced:... frame:...
```

These lines surface the scene machine instead of hiding it.

That matters because the learner can now correlate:

- request policy
- active scene identity
- transition count
- override mode
- frame count

with what they see happening on screen.

## Step 6: Read the Rebuild-Failure Line as Honest Runtime Reporting

If rebuild failure is active, the HUD emits a warning line instead of silently pretending all scene state is fine.

That line includes:

- failure scene
- failure count
- last failure frame

This is a good proof-oriented design rule:

```text
if an important runtime boundary can fail,
the overlay should expose that failure clearly
```

## Step 7: Read the Allocator Evidence Line

The HUD prints:

```text
Arena used perm:... level:... scratch:... | live_scopes:... reloads:... enters:...
```

This is one of the strongest lines in the whole overlay because it combines:

- allocator usage
- current scratch temp depth
- scene reload count
- scene enter count

in one place.

That is exactly what a proof-oriented overlay should do: show related state from multiple subsystems together.

## Worked Example: Why Reload and Enter Counts Belong Beside Arena Usage

If the learner presses `R` repeatedly in the level-reload scene, the HUD can show reload count climbing beside level-arena usage.

That makes it much easier to connect:

```text
this ownership event happened
and this lifetime bucket changed with it
```

That is a richer lesson than showing memory numbers alone.

## Step 8: Read the Audio Summary Lines

The HUD next prints:

- a compact sequencer/voice/mix summary line
- a scene-aware evidence line via `game_describe_audio_evidence(...)`

This creates a two-level audio presentation:

### Top HUD

Compact summary.

### Lower audio trace panel

Detailed interpretation.

That layering keeps the overlay readable while still allowing deeper diagnosis below.

## Step 9: Notice the Scratch Allocation for Reused Text Buffers

Inside the HUD path:

```c
buf = ARENA_PUSH_ARRAY(scratch, 160, char);
```

This is a nice example of the course eating its own cooking.

The overlay itself uses scratch-lifetime memory for temporary formatted text buffers.

That is exactly what the scratch arena is for.

So the proof system is not only explaining the allocator model. It is actively using it.

## Step 10: Read the Scene-Specific Summary Line Before the Logs

After the shared summaries, the HUD emits one scene-specific detail line, for example:

- arena phase/depth/rewinds
- level formations/entities/scan state
- pool active/collisions/reuse pressure
- slab mode/events/pages/growth

This line acts as a bridge from general runtime state into scene-specific evidence.

Only then does the overlay list recent log entries.

That order is intentional. The logs are most useful when the learner already knows which subsystem summary they are looking at.

## Step 11: The Log Ring Buffer Adds Temporal Context

The HUD iterates recent `logs[]` entries and draws the newest ones first.

That means the overlay is not only a snapshot of current state. It also contains a short recent history of important events.

This is valuable because many proof moments are about transitions:

- enter
- reload
- warning probe
- completion
- growth event

The logs make those moments visible even after the exact frame they happened on has passed.

## Step 12: Read `game_draw_scene_trace_panel(...)` as the Final Lower-Half Attachment

At the bottom of the HUD flow, the runtime calls:

```c
game_draw_scene_trace_panel(game, backbuffer, hud);
```

That single call brings in:

- the step rows
- evidence suffixes
- prompt line
- observation line
- the audio trace panel beneath it

So the lower half of the HUD is not a separate overlay system. It is part of the main HUD composition pipeline.

## Step 13: Tie the HUD Back to the Render Pipeline

In `game_app_render(...)`, the sequence is:

```c
world_ctx = game_make_world_context(...);
hud_ctx = game_make_hud_context(...);
...
game_render_scene(game, backbuffer, &world_ctx);
game_draw_hud(game, backbuffer, fps, scale_mode, &hud_ctx, perm, level,
              scratch);
```

This is the final structural proof that the HUD is part of the runtime architecture.

The scene is rendered with the world context.

The proof overlay is rendered with the HUD context.

That cleanly reuses Module 06's world-vs-HUD context split.

## Visual: Full Overlay Architecture

```text
world render
  -> scene visuals in world context

HUD render
  -> orientation lines
  -> control hints
  -> scene-machine summaries
  -> allocator and audio summaries
  -> scene-specific summary line
  -> recent log entries
  -> main trace panel
     -> prompt / observation
     -> audio trace panel
```

That is the full proof-oriented overlay stack.

## Practical Exercises

### Exercise 1: Explain the Order

Why does the HUD start with scene name, summary, and action hint before the allocator or trace-panel lines?

Expected answer:

```text
because the learner first needs orientation and interaction context before deeper proof and subsystem evidence become useful
```

### Exercise 2: Explain Allocator Visibility

Why does the HUD call `arena_get_stats(...)` directly instead of leaving allocator state hidden inside `arena.h`?

Expected answer:

```text
because allocator behavior is part of what the course is teaching, so the runtime overlay needs to surface live memory evidence explicitly
```

### Exercise 3: Explain Scratch Usage

Why is `ARENA_PUSH_ARRAY(scratch, 160, char)` a good fit for temporary HUD text formatting?

Expected answer:

```text
because those formatted strings only need to live for the current frame, which is exactly the scratch arena's intended lifetime
```

## Common Mistakes

### Mistake 1: Treating the HUD as random debug text

The line order is intentional and pedagogical.

### Mistake 2: Thinking the lower proof panels replace the top summary lines

They complement them. The top gives quick orientation; the bottom provides detailed proof.

### Mistake 3: Forgetting that the HUD itself uses the course's own lifetime and coordinate abstractions

It relies on scratch allocations and HUD render-context layout helpers directly.

## Troubleshooting Your Understanding

### "Why not draw only the big trace panels and skip the top summary section?"

Because the learner still needs quick orientation, controls, scene-machine state, and compact subsystem summaries before reading detailed proof panels.

### "Why is the HUD drawn after `game_render_scene(...)`?"

Because it is an overlay rendered in HUD space on top of the world scene, not part of the world content itself.

## Recap

You now understand the final HUD composition:

1. the overlay begins with orientation and controls
2. it surfaces scene-machine, allocator, and audio summaries from live state
3. it uses scratch lifetime for temporary formatted text
4. it adds scene-specific summary lines and recent logs
5. it attaches the trace and audio proof panels as the detailed lower half
6. it is rendered through a HUD context after the world scene

## Module Bridge

Module 09 has now finished the runtime proof layer:

- compact progress masks
- dynamic guidance
- evidence-upgraded trace rows
- audio cause/severity panels
- one fully composed proof-oriented HUD

That means later modules no longer need to invent their own instrumentation model. They can build on this one and teach each scene through the proof system that already exists.
