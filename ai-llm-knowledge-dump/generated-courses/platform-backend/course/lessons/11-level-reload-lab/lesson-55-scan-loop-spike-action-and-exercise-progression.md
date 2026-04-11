# Lesson 55: Scan Loop, Spike Action, and Exercise Progression

## Frontmatter

- Module: `11-level-reload-lab`
- Lesson: `55`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_update_level_reload`
  - `game_app_update`
  - `button_just_pressed`
  - `game_mark_scene_exercise`
  - `game_update_scene_exercise_progress`
  - `GAME_EXERCISE_STEP_1`
  - `GAME_EXERCISE_STEP_2`
  - `GAME_EXERCISE_STEP_3`
  - `GAME_EXERCISE_STEP_4`

## Observable Outcome

By the end of this lesson, you should be able to explain the three different kinds of Level Reload change: passive traversal over a stable payload, real ownership reset on `R`, and a pressure spike on `SPACE` that looks dramatic without reallocating anything.

## Why This Lesson Exists

Lesson 54 explained what the rebuilt payload contains.

This lesson explains how the runtime behaves after that payload exists.

The important teaching goal is not only that the scene can rebuild. The important teaching goal is that the learner stops equating visible motion with allocation work.

The Level Reload Lab deliberately creates two superficially similar experiences:

- a real rebuild event
- a fake-out spike event

The learner has to prove which one changed ownership.

## First Reading Strategy

Read this lesson in the same order the runtime executes the ideas:

1. passive update over the current payload
2. same-scene rebuild on `R`
3. scan spike on `SPACE`
4. exercise progression that forces the learner to prove the difference

That order matters because `R` and `SPACE` are not two equally strong kinds of mutation. One rebuilds the scene payload. The other only agitates it.

## Step 0: Ask What Can Change Without Allocating Anything

Before reading the update path, ask the key diagnostic question:

```text
what visible things could keep changing every frame even if the entity strip
never moved in memory?
```

The answer is most of the scene's motion:

- `pulse`
- `scan_t`
- `rebuild_flash` decay
- `highlighted_entity`
- per-entity `heat`

That baseline matters. If the learner forgets that motion can happen over stable data, the spike path becomes misleading.

## Step 1: Read `game_update_level_reload(...)` as Stable Traversal

`game_update_level_reload(...)` begins with:

```c
scene->pulse += dt;
scene->scan_t += dt * 0.95f;
scene->rebuild_flash -= dt * 0.7f;
if (scene->rebuild_flash < 0.0f)
  scene->rebuild_flash = 0.0f;
```

Then it updates focus and heat:

```c
int active_index =
    ((int)(scene->pulse * 4.0f) + scene->highlighted_entity) %
    scene->entity_count;
scene->highlighted_entity = active_index;
...
scene->entities[i].heat = 0.15f + 0.55f * wave;
```

This is the passive story of the scene.

- the scan head advances
- the focus shifts
- the prior flash cools down
- the entities glow differently over time

None of that rebuilds ownership.

It is read-write activity over the current scene payload, not fresh allocation.

## Visual: Stable Ownership, Moving Evidence

```text
after one rebuild
  -> one contiguous entity strip exists

every frame after that
  -> scan_t advances
  -> highlighted entity moves
  -> heat waves change
  -> rebuild_flash cools

ownership stays the same until the next R
```

That is the conceptual base the rest of the lesson depends on.

## Step 2: Notice the Order Inside `game_app_update(...)`

The order in `game_app_update(...)` is important.

First, the runtime checks startup, scene switching, and reload:

```c
if (!game->level_state) {
  game_rebuild_scene(game, level, 1);
} else if (game->scene.requested != game->scene.current) {
  ...
  game_rebuild_scene(game, level, 1);
} else if (do_reload) {
  game_rebuild_scene(game, level, 0);
}
```

Only after that does the Level Reload scene update run:

```c
game_update_level_reload(&game->level_state->as.level_reload, dt);
if (play_tone_pressed) {
  ... spike path ...
}
```

That means the runtime is intentionally separating:

- ownership reset decisions
- passive scene traversal
- learner-injected pressure spike

This is not accidental ordering. It is the teaching order of the scene.

## Step 3: Treat `R` as Ownership Work

Pressing `R` does not mean "make the scene look busy." It means:

```text
discard the current level-owned payload and construct a fresh one
```

That happens through:

```c
do_reload = button_just_pressed(input->buttons.reload_scene);
...
game_rebuild_scene(game, level, 0);
```

The visible consequences of a real rebuild are deeper than one flash:

- `scene_reload_count` advances at the app level
- `game_fill_level_reload(...)` receives the new reload count
- `build_serial` changes
- `reload_seed` changes
- formations are regenerated
- the contiguous entity strip is regenerated
- `rebuild_flash` starts high on the fresh scene payload

That is ownership change.

## Step 4: Treat `SPACE` as Pressure Without Rebuild

The Level Reload `SPACE` path lives later in the scene branch:

```c
if ((game->exercise[GAME_SCENE_LEVEL_RELOAD].progress_mask &
     GAME_EXERCISE_STEP_3) != 0u) {
  game_mark_scene_exercise(game, GAME_SCENE_LEVEL_RELOAD,
                           GAME_EXERCISE_STEP_4,
                           "scan spike without rebuild");
}
scene->pulse += 0.55f;
scene->rebuild_flash = 1.0f;
if (scene->entity_count > 0) {
  scene->highlighted_entity =
      (scene->highlighted_entity + 5) % scene->entity_count;
}
game_log_eventf(game, "level scan spike -> entity %d",
                scene->highlighted_entity);
game_play_sound_at(&game->audio, SOUND_TONE_MID);
game_fire_scene_warning_probe(game, GAME_SCENE_LEVEL_RELOAD);
```

This path does a lot of visible work.

- pulse jumps
- flash rises again
- focus jumps forward
- the log records a spike
- a tone plays
- warning pressure warms up

But the spike path never calls `game_rebuild_scene(...)`.

That is the central contrast:

```text
SPACE creates activity and evidence
R changes ownership
```

## Step 5: Explain Why `SPACE` Reuses `rebuild_flash`

One of the most useful design tricks in this scene is:

```c
scene->rebuild_flash = 1.0f;
```

At first glance, this makes rebuild and spike look more similar than they need to.

That is deliberate.

If rebuild and spike used completely different visual signals, the learner could identify them by superficial appearance alone. By reusing the same flash channel, the scene forces the learner to read deeper proof surfaces:

- `build_serial`
- `reload_seed`
- scan traversal
- event log wording
- warning cause

The lesson becomes stronger because one glow effect is no longer enough evidence.

## Step 6: Read the Exercise Progression as a Proof Script

The scene marks progress in two places.

### Direct manual events in `game_app_update(...)`

- `R` marks step 1: `manual level rebuild requested`
- `SPACE` always produces the spike event immediately
- `SPACE` only marks step 4 after step 3 is already complete

That last rule matters. The spike is always allowed to happen. The checkbox is not.

The learner must first prove they already observed rebuild and traversal correctly.

### Passive observation in `game_update_scene_exercise_progress(...)`

```c
if (scene->build_serial > 1u && scene->rebuild_flash > 0.0f) {
  game_mark_scene_exercise(... GAME_EXERCISE_STEP_2,
                           "rebuild flash observed");
}
if ((game->exercise[game->scene.current].progress_mask &
     GAME_EXERCISE_STEP_1) != 0u &&
    scene->scan_t > 0.35f) {
  game_mark_scene_exercise(... GAME_EXERCISE_STEP_3,
                           "contiguous strip scanned");
}
```

So the intended learning order is:

1. request a rebuild
2. observe rebuild evidence on the fresh payload
3. observe scan traversal over stable rebuilt data
4. inject a spike that raises pressure without rebuilding

This turns the exercise system into a guided proof procedure, not a random checklist.

## Step 7: Notice What Does Not Change During `SPACE`

During the spike path, the scene does not change:

- `build_serial`
- `reload_seed`
- `formation_count`
- `entity_count`
- ownership of `formations`
- ownership of `entities`

That is the strongest evidence in the scene.

The learner is being trained not to confuse:

```text
visual pressure
```

with:

```text
ownership change
```

## Worked Comparison: One `R` Frame Versus One `SPACE` Frame

Suppose the scene is already running.

### Press `R`

- `game_rebuild_scene(...)` runs
- the level arena payload is replaced
- new build serial and seed appear
- scan starts again on fresh data

### Press `SPACE`

- no rebuild function runs
- current payload stays in place
- flash and focus jump anyway
- the warning line warms up
- serial and seed stay put

That is the whole point of the lab compressed into one comparison.

## Common Mistake: Assuming Step 4 Should Trigger on the First Ever Spike

The runtime does not do that anymore, and the lesson should not teach it.

The spike event itself can happen at any time.

But the final exercise step only marks after the earlier rebuild-and-scan proof path is already established. Otherwise the learner could complete the walkthrough without ever proving the stable-traversal part of the scene.

## JS-to-C Bridge

This is like comparing a real data refresh against a highlight animation layered on top of existing data. Both make the interface feel active. Only one proves the underlying model was rebuilt.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read `game_update_level_reload(...)` and list the passive state changes that happen without any rebuild.
2. Read the order in `game_app_update(...)` and explain why reload handling happens before the Level Reload spike path.
3. Find the Level Reload `SPACE` branch and explain why it is a pressure event rather than a rebuild.
4. Read `game_update_scene_exercise_progress(...)` and explain how steps 2 and 3 are earned through observation instead of direct button presses.
5. Identify which fields are intentionally left unchanged during a spike.

## Exercises

1. Explain why the scene reuses `rebuild_flash` for both real rebuilds and scan spikes.
2. Explain why `scan_t` is a better proof of stable traversal than `pulse` alone.
3. Explain the full four-step exercise order in Level Reload terms, including the gate on step 4.
4. Describe how the scene teaches the difference between visible excitement and actual ownership change.

## Runtime Proof Checkpoint

At this point, you should be able to explain the Level Reload control loop precisely: `R` rebuilds the level-owned payload, passive update traverses that stable payload over time, and `SPACE` injects localized pressure without reallocating the payload or changing the ownership evidence.

## Recap

This lesson separated the main kinds of Level Reload state change:

- passive update animates and scans stable data
- `R` performs real level-payload rebuild work
- `SPACE` creates pressure and evidence without ownership change
- exercise progress is intentionally split between manual requests and passive observation

Next, we connect those behaviors to the proof surfaces on screen: the world render, the trace panel, the audio trace, and the top HUD metric line.
