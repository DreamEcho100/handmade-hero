# Lesson 50: Arena Render Surface, Depth Meters, and HUD Metrics

## Frontmatter

- Module: `10-arena-lifetimes-lab`
- Lesson: `50`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/render_explicit.h`
- Target symbols:
  - `game_render_arena_lab`
  - `game_draw_world_label`
  - `world_rect_px_x`
  - `world_rect_px_y`
  - `world_w`
  - `world_h`
  - `active_depth`
  - `checkpoint_count`
  - `simulated_temp_bytes`
  - `peak_temp_bytes`

## Observable Outcome

By the end of this lesson, you should be able to explain how the Arena Lab uses both world-space diagrams and HUD metrics to make the same lifetime story readable at a glance and verifiable with exact numbers.

## Why This Lesson Exists

The Arena Lab would be much weaker if all of its proof stayed in abstract counters and strings.

It becomes persuasive because the scene also draws a live visual model of the lifetime story.

This lesson explains how the Arena render surface and the top HUD metric line reinforce the same ideas from different angles.

## First Reading Strategy

Read the visible proof surface from most visual to most exact:

1. stable bars and phase banner
2. checkpoint cards and depth meter
3. packet motion as illustrative flow
4. top HUD metric line as exact confirmation

That keeps you from treating every moving part as equally authoritative.

## Step 0: Separate Authoritative Proof From Illustration

Before reading the renderer, make one distinction explicit:

```text
checkpoint cards and depth meter are direct proof surfaces
packet motion is an explanatory metaphor
```

That distinction makes the scene much easier to interpret correctly.

## Step 1: Read `game_render_arena_lab(...)` as a Diagram Renderer

This render function is not trying to look like a real game level.

It is drawing an explanatory diagram:

- four lifetime bars
- one phase banner
- a stack of checkpoint cards
- a temp-depth meter
- moving packet markers
- explanatory world labels

That is why this scene reads so clearly.

It is a diagram built from live state.

## Visual: The Scene's Main Layers

```text
top-middle   -> current phase name + summary
middle       -> perm / level / scratch / temp lifetime bars
upper area   -> visible checkpoint stack
right meter  -> current temp depth
lower flow   -> scratch packets entering and leaving
HUD line     -> phase, depth, temp bytes, peak, rewinds, cycles
```

This is the real teaching composition.

## Step 2: Read the Four Bars as a Stable Vocabulary Strip

The render loop iterates `scene->bars` and draws each rectangle with its title and summary.

These bars are the most stable part of the scene.

They exist to keep the learner oriented while the transient checkpoint state changes above and around them.

This is why the labels matter:

- `perm` -> session owner
- `level` -> scene rebuilds
- `scratch` -> frame-local work
- `temp` -> nested checkpoint

The scene is effectively re-teaching the allocator taxonomy on every frame.

## Step 3: Read the Phase Banner as the Current Interpretation Layer

The large banner at the top is drawn from:

- `scene->phase_name`
- `scene->phase_summary`

This is an important layer.

Without it, the bars and checkpoint cards would still be visible, but the learner would have to infer what phase transition they are currently seeing.

The banner removes that ambiguity.

It answers:

```text
what is this exact moment trying to teach?
```

## Step 4: Read the Checkpoint Cards as a Visual Stack

The checkpoint loop draws one small vertical card per active checkpoint.

Each card uses:

- the checkpoint label
- a fill amount
- color

and the number of cards drawn is controlled by `checkpoint_count`.

So the learner can read nesting depth in two ways at once:

- textually through phase summary and HUD numbers
- visually through the number of visible checkpoint cards

That redundancy is good.

It makes the lesson easier to grasp quickly.

## Step 5: Read the Temp-Depth Meter as the Simplest Proof Surface

The meter loop draws three horizontal bars and lights them based on:

```c
int live = i < scene->active_depth;
```

This is maybe the cleanest visual proof in the whole scene.

You can tell current nesting depth immediately.

That is why the trace step for “two temp scopes are live” becomes so readable.

The meter is a compressed visual proof of the same data.

## Step 6: Read the Packet Flow as Motion, Not Ownership

The moving packet markers are driven by `scene->beat` and `packet_flow`.

These packets are not the authoritative checkpoint state.

They are a motion metaphor for transient work entering, nesting, rewinding, and vanishing.

That distinction matters.

The scene wisely separates:

```text
proof state        -> bars, checkpoint cards, depth meter, HUD metrics
illustrative motion -> packet flow animation
```

That keeps the animation from becoming the only explanation.

## Step 7: Connect the World Scene Back to the Top HUD Metric Line

In `game_draw_hud(...)`, the Arena-specific metric line is:

```text
Arena phase:%s depth:%d temp_bytes:%zu peak:%zu rewinds:%u cycles:%u
```

This line is excellent because it complements the world render instead of repeating it blindly.

The world render gives shape.

The HUD line gives exact numbers.

Together they let the learner move fluidly between intuition and measurement.

## Step 8: Notice the Evidence System Is Layered, Not Redundant

For the Arena Lab, the learner has at least five simultaneous evidence surfaces:

- phase banner
- checkpoint cards
- depth meter
- top HUD metric line
- trace panel and prompt/observation layer

This is not accidental over-instrumentation.

It is how the course teaches the same allocator idea in multiple readable forms.

## Worked Example: Why The HUD Metric Line Does Not Replace The World Diagram

The HUD line can tell you:

```text
depth:2 temp_bytes:208 peak:352 rewinds:1 cycles:3
```

but it cannot show the immediate visual stack shape the way checkpoint cards and the depth meter can.

The world diagram gives intuition first. The HUD line gives exact confirmation second. The lesson works because both layers are present.

## Common Mistake: Thinking the Scene Render Alone Proves the Allocator Story

It helps, but it does not stand alone.

The render surface is strongest when read together with:

- the HUD metrics
- the trace panel
- the prompt/observation text
- the event log

That is why the course keeps calling the whole thing a proof layer, not only a scene visualization.

## JS-to-C Bridge

This is like pairing a live diagram with a metrics panel. The diagram gives you a fast mental picture. The metrics panel confirms the exact numbers. Neither is as good alone as they are together.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find the loop that draws the four lifetime bars and explain why those bars do not depend on the current phase.
2. Find the phase banner draw calls and explain why both `phase_name` and `phase_summary` are needed.
3. Find the checkpoint-card loop and explain how `checkpoint_count` affects the visible stack.
4. Find the temp-depth meter loop and explain how it turns `active_depth` into an immediate visual proof.
5. Find the Arena-specific HUD metric line in `game_draw_hud(...)` and explain how it complements the world scene.

## Exercises

1. Explain why the Arena Lab render surface is better thought of as a live diagram than a normal game scene.
2. Explain why exact metrics belong in the HUD while shape and flow belong in the world scene.
3. Explain why the packet flow should not be mistaken for the real checkpoint stack.
4. Describe the Arena Lab evidence composition in one paragraph.

## Runtime Proof Checkpoint

At this point, you should be able to explain how the Arena Lab makes lifetime behavior visible on screen: stable bars define the ownership vocabulary, checkpoint cards and depth meters show transient nesting, packet motion illustrates flow, and the top HUD metrics provide exact numerical confirmation.

## Recap

This lesson unpacked the Arena Lab's visible proof surfaces:

- the world scene is a live explanatory diagram
- stable bars provide the lifetime vocabulary
- checkpoint cards and depth meters make transient nesting visible
- the HUD metric line adds exact numerical evidence

Next, we turn the whole module into an end-to-end guided runtime walkthrough using forced-scene builds and a concrete step-by-step verification path.
