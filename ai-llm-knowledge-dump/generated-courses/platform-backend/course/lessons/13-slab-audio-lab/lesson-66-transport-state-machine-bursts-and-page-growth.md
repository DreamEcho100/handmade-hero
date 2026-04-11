# Lesson 66: Transport State Machine, Bursts, and Page Growth

## Frontmatter

- Module: `13-slab-audio-lab`
- Lesson: `66`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_update_slab_audio`
  - `state_index`
  - `state_timer`
  - `phase_changes`
  - `event_accum`
  - `page_growth_count`
  - `burst_count`
  - `game_fire_scene_warning_probe`

## Why This Lesson Exists

Lesson 65 explained how slab events are allocated and labeled.

This lesson explains where the pressure comes from.

The slab scene does not rely on a single button press to teach everything.

It has an internal state machine that keeps changing:

- event cadence
- event lifetime decay
- audio cue timing
- pressure on page occupancy

That state machine is what makes the later page-growth proof meaningful.

## Observable Outcome

By the end of this lesson, you should be able to explain how `game_update_slab_audio(...)` turns transport state into allocator pressure: timer-driven mode changes alter spawn cadence and TTL decay, `SPACE` adds burst load, and page growth is detected explicitly when existing pages stop being enough.

## First Reading Strategy

Read the update loop in time order:

1. transport timer and state advance
2. state-specific spawn and decay parameters
3. automatic stream generation
4. manual burst path
5. page-growth detection
6. per-event decay and free path

That order mirrors the runtime and prevents one common mistake: treating page growth as if it happens independently from the transport machine.

## Step 0: Pressure Comes From More Than One Knob

The slab scene is not driven by one single variable.

Page pressure depends on both:

- how fast new events arrive
- how slowly old events disappear

That is why the scene changes both `spawn_interval` and `ttl_decay` by state.

## Step 1: Read the Transport Timer First

At the top of `game_update_slab_audio(...)`:

```c
scene->state_timer += dt;
if (scene->state_timer >= 2.2f) {
  scene->state_timer = 0.0f;
  scene->state_index = (scene->state_index + 1u) % GAME_SLAB_TRANSPORT_COUNT;
  scene->phase_changes++;
  ...
}
```

So the scene has a built-in transport machine.

Every 2.2 seconds, it advances to the next transport state and logs that change.

This is why the module can teach allocator pressure as part of a broader runtime story instead of as a static benchmark.

The scene is effectively giving the allocator a moving workload profile. That makes the growth event feel like a consequence of the system's behavior instead of a hard-coded scripted jump.

## Step 2: Read Spawn Cadence and Decay as State-Dependent Pressure

The update function selects `spawn_interval` and `ttl_decay` based on `state_index`.

That means different transport states change two pressure levers at once:

- how often new events are created
- how quickly existing events disappear

That is exactly the kind of system that can push one slab page from comfortable occupancy into growth pressure.

The learner should make a habit of reading those two levers together. A faster spawn rate with fast decay might still stay manageable. A faster spawn rate with slower effective turnover is where growth pressure becomes much easier to trigger.

The exact state table is worth reading once, because the module is teaching real parameter changes rather than a vague "faster/slower" story:

- `record`: `spawn_interval = 0.28f`, `ttl_decay = 0.95f`
- `queue`: `spawn_interval = 0.18f`, `ttl_decay = 1.05f`
- `playback`: `spawn_interval = 0.12f`, `ttl_decay = 1.15f`
- `flush`: `spawn_interval = 0.42f`, `ttl_decay = 1.80f`

So `playback` is the densest arrival phase, while `flush` deliberately slows arrivals and burns TTL down faster. That is why the final exercise step waits for `flush`: it is the phase where reuse after the growth spike becomes easiest to see.

## Visual: Pressure Comes From Two Levers At Once

```text
lighter pressure
  -> slower spawn cadence
  -> faster turnover

heavier pressure
  -> faster spawn cadence
  -> slower turnover

burst pressure
  -> manual extra events on top of the current transport mode
```

That is why page growth is not caused by one variable. It is caused by the interaction between arrival rate, lifetime decay, and manual bursts.

## Visual: Why State Changes Matter

```text
slow state
  -> fewer new events
  -> lighter page pressure

fast state
  -> more new events
  -> stronger page pressure

flush-like state
  -> later reuse becomes visible
```

The scene is not only animating transport labels.

It is changing allocator pressure.

That is the essential bridge between the audio-side terminology and the allocator-side lesson.

## Step 3: Read the Automatic Event Stream

After choosing cadence, the scene does:

```c
scene->event_accum += dt;
if (scene->event_accum >= spawn_interval) {
  slab_audio_push_event(scene, slab_audio_state_name(scene->state_index),
                        1.6f + (float)(scene->generated_count % 4) * 1.15f);
  scene->event_accum = 0.0f;
}
```

That means each transport state creates a different steady stream of same-size events, all flowing through the same slab allocator.

The allocator lesson is emerging from state-driven traffic.

Also notice that the event label comes from `slab_audio_state_name(scene->state_index)`. The event stream therefore preserves evidence of which mode produced which traffic, even after the transport machine has already moved on.

The helper call also varies the vertical band with `1.6f + (generated_count % 4) * 1.15f`, so the automatic stream lands in four repeating rows instead of collapsing into one unreadable strip.

## Step 4: Read `SPACE` as a Burst Injector, Not a Full Explanation

Manual burst input does this:

```c
slab_audio_push_event(scene, "burst", 2.2f);
slab_audio_push_event(scene, "burst", 3.4f);
slab_audio_push_event(scene, "burst", 4.6f);
scene->burst_count++;
game_log_eventf(game, "slab burst -> events %d", scene->event_count);
game_fire_scene_warning_probe(game, GAME_SCENE_SLAB_AUDIO_LAB);
```

This is the deliberate stress path.

Like earlier modules, `SPACE` is a prove-it button.

But it is not the whole scene.

The deeper lesson is how that burst lands inside a continuously changing transport machine.

That distinction keeps the button from becoming a misleading shortcut. `SPACE` is a prove-it accelerator, not the source of all allocator behavior in the scene.

## Step 5: Read Page Growth Detection as the Central Proof Hook

Later in the update loop:

```c
if (scene->slab.page_count > scene->prev_page_count) {
  scene->page_growth_count +=
      (unsigned int)(scene->slab.page_count - scene->prev_page_count);
  scene->prev_page_count = scene->slab.page_count;
  game_log_eventf(game, "slab grew to %zu pages", scene->slab.page_count);
}
```

This is the main allocator proof in the module.

The scene does not guess that growth probably happened.

It compares current page count against the previous known value and logs the exact transition.

That is an important engineering choice. The scene treats allocator growth as a measurable state transition, not as something the learner is expected to eyeball from a crowded screen.

## Step 6: Read Event Freeing as the Later Reuse Phase

Every frame, events lose TTL.

When an event expires:

```c
slab_free(&scene->slab, event);
scene->events[i] = scene->events[scene->event_count - 1];
scene->event_count--;
```

That means the scene is constantly teaching both halves of slab behavior:

- growth when capacity is exceeded
- later reuse when expired events return same-size slots to existing pages

This is why the final exercise step is about reaching a later flush-like state after growth.

Growth alone would teach only the spike. The scene wants to teach the aftermath too.

There is also a small motion cue tied to the state machine: every live event advances in `wx`, but `playback` pushes them at `0.28f * dt` while the other modes use `0.08f * dt`. So the busiest transport state changes both allocator pressure and visible card speed.

## Worked Example: A Growth Spike Versus a Later Flush

Suppose state `2` is generating events quickly and `SPACE` adds three more.

What sequence should the learner expect?

1. event count rises sharply
2. the current page may run out of free slots
3. `scene->slab.page_count` increases and the event log records growth
4. later the transport machine advances toward `flush`
5. old events expire faster and the grown page set begins serving reused capacity

That full sequence is the lesson. Stopping at step 3 misses half of it.

## Visual: Growth Spike Timeline

```text
transport state speeds up
  -> automatic event pressure rises
  -> SPACE can inject extra burst load
  -> free slots run out on the current page
  -> slab allocates another page
  -> later states keep running
  -> expired events begin returning slots for reuse
```

The lesson is not only "a page appeared." The lesson is "a page appeared because pressure rose, and later reuse still happened inside the expanded allocator."

## Step 7: Notice How the Exercise Steps Map to the State Machine

`game_update_scene_exercise_progress(...)` marks the slab steps as:

- step 1: `event_count > 0`
- step 2: `burst_count > 0`
- step 3: `page_growth_count > 0`
- step 4: `phase_changes >= 3u && state_index == 3u`

That is a very clean progression:

1. record same-size work
2. crowd the scene deliberately
3. force growth
4. wait for a later reuse-friendly state

It also explains why `phase_changes` belongs in the proof flow. The final step is temporal as well as allocational.

## Common Mistake: Thinking Page Growth Is the End of the Lesson

It is not.

Growth is only the pressure spike.

The scene also wants the learner to see what happens afterward:

```text
new page exists
later transport states keep reusing that expanded capacity
```

That is why the final step waits for a later phase instead of ending at the page-growth event.

## JS-to-C Bridge

This is like a queue processor that changes load profile by mode. Sometimes the existing pages are enough. Under heavier bursts, a new page appears. Later, as traffic changes, the system reuses that larger footprint instead of growing forever.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read `game_update_slab_audio(...)` and explain how the transport timer advances the state machine.
2. Read the spawn-interval switch and explain why different states apply different allocator pressure.
3. Read the `SPACE` burst path and explain why it is a stress tool rather than the whole lesson.
4. Read the page-growth detection block and explain why it is the scene's main allocator proof hook.

## Exercises

1. Explain why `phase_changes` belongs in the exercise logic for this scene.
2. Explain how `spawn_interval` and `ttl_decay` work together to create growth pressure.
3. Explain why the scene logs page growth instead of only drawing it.
4. Describe the full four-step exercise progression in slab terms.

## Runtime Proof Checkpoint

At this point, you should be able to explain the Slab scene pressure loop: transport states modulate event traffic, `SPACE` injects extra burst load, page growth is detected explicitly, and later expirations free same-size slots for reuse inside the grown page set.

## Recap

This lesson explained how the Slab scene creates allocator pressure:

- a timed transport state machine changes cadence and decay
- automatic and manual event creation both feed the slab
- page growth is detected and logged explicitly
- later expirations create the reuse phase after the growth spike

Next, we read the render, trace, and HUD surfaces so the learner can see how page occupancy and transport state are made visible on screen.
