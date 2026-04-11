# Lesson 53: Level Arena Reset, Rebuild, and Scene-Enter Rules

## Frontmatter

- Module: `11-level-reload-lab`
- Lesson: `53`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `game_app_update`
  - `game_rebuild_scene`
  - `arena_reset`
  - `scene_reload_count`
  - `scene_enter_count`
  - `game_audio_apply_scene_profile`
  - `game_audio_trigger_scene_enter`
  - `game_fill_level_reload`

## Observable Outcome

By the end of this lesson, you should be able to explain the rebuild rule in one sentence: a Level Reload rebuild discards the entire level-owned payload and constructs a fresh one inside the level arena, while the surrounding app-level state continues running and records that rebuild as history.

## Why This Lesson Exists

Lesson 52 defined the ownership boundary.

This lesson explains how the runtime enforces it.

The useful question is not only:

```text
does this scene rebuild?
```

The useful question is:

```text
what exactly is destroyed, what exactly survives, and how does the runtime
prove the difference?
```

The Level Reload Lab matters because it teaches a mixed lifetime model.

- one owner, the level arena, is allowed to discard its whole scene payload
- another owner, the long-lived app state, stays alive and remembers what happened

If those two layers blur together, the rest of the lab stops making sense.

## First Reading Strategy

Read the rebuild path in two layers:

1. `game_app_update(...)` decides whether this frame is a scene entry, a scene switch, or a same-scene reload
2. `game_rebuild_scene(...)` performs the actual owner reset and refill

That separation keeps policy distinct from ownership work.

## Step 0: Name the Two Owners Before Reading the Code

Before you follow any function calls, put the ownership model into plain language:

- `GameAppState` is long-lived session state
- `GameLevelState` is one level-owned payload living inside the level arena

That means a manual `R` press in Level Reload should not restart the entire program. It should only replace the level-owned payload.

If you start from that expectation, the code becomes much easier to read correctly.

## Step 1: Trace All Three Rebuild Entry Conditions in `game_app_update(...)`

`game_app_update(...)` can call `game_rebuild_scene(...)` from three different situations:

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

Those cases are not equivalent.

### Startup or missing level payload

`entering_new_scene = 1`

The runtime is creating the initial payload for the current scene.

### Scene navigation

`entering_new_scene = 1`

The scene machine changed scenes, so the runtime is both entering a new scene and rebuilding that scene's level-owned payload.

### Manual reload with `R`

`entering_new_scene = 0`

The scene stays the same. Only its level payload is rebuilt.

That third case is the one this module is teaching most directly.

## Step 2: Read `game_rebuild_scene(...)` as One Owner Dropping Everything It Owns

The function begins like this:

```c
current = game->scene.current;
game->level_state = NULL;
arena_reset(level);
level_state = ARENA_PUSH_ZERO_STRUCT(level, GameLevelState);
```

This is the central rebuild pattern.

The runtime does not free `formations`, then free `entities`, then clear a dozen scene-local fields one by one. Instead, it removes the whole level-owned payload in one owner-level operation:

```text
reset the level arena
allocate a fresh GameLevelState
fill the new scene payload from scratch
```

That is why arenas fit this lesson so well. The code can express a whole-scene lifetime boundary directly.

## Visual: Reload Ownership Boundary

```text
before reload
  GameAppState
    -> long-lived counters, logs, scene machine, audio object
  level arena
    -> GameLevelState
       -> LevelReloadScene
          -> formations[]
          -> entities[]

press R
  -> game_rebuild_scene(..., entering_new_scene = 0)
  -> game->level_state = NULL
  -> arena_reset(level)

after reload
  same GameAppState survives
  level arena now contains
    -> new GameLevelState
       -> new LevelReloadScene
          -> new formations[]
          -> new entities[]
```

The old level payload does not survive in pieces. It is replaced wholesale.

## Step 3: Separate What Survives From What Resets

The most important subtlety in the whole lesson is that `game_rebuild_scene(...)` is not rebuilding the whole application.

`GameAppState` survives.

That means the runtime keeps all of this across a manual reload:

- the current scene machine state
- exercise progress arrays
- log history
- `scene_reload_count[]`
- `scene_enter_count[]`
- transition history
- the long-lived audio object itself
- frame index and other app-level bookkeeping

What gets reset is the active level-owned payload and everything allocated inside the level arena for that scene.

## Visual: What Survives Versus What Resets

| Survives rebuild                                 | Resets on rebuild                                                                                                      |
| ------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------- |
| `GameAppState` container                         | `GameLevelState`                                                                                                       |
| scene machine state                              | `LevelReloadScene` payload                                                                                             |
| exercise progress masks                          | `formations[]`                                                                                                         |
| event log history                                | contiguous `entities[]` strip                                                                                          |
| `scene_reload_count[]` and `scene_enter_count[]` | `build_serial`, `reload_seed`, `pulse`, `scan_t`, `rebuild_flash`, `highlighted_entity` inside the fresh scene payload |
| audio object                                     | scene-local warning heat for this scene                                                                                |

Keep those columns separate. If both columns reset together, the lesson becomes "restart the app." If neither resets, the lesson stops being about rebuild at all.

## Step 4: Read the Counter Rules as Lifetime Bookkeeping

Inside `game_rebuild_scene(...)`:

```c
game->scene_reload_count[current]++;
if (entering_new_scene)
  game->scene_enter_count[current]++;
```

These counters teach two different histories.

- `scene_reload_count[current]` counts every rebuild of the current scene payload
- `scene_enter_count[current]` counts only true scene-entry events

That means the runtime can distinguish:

- entering the scene for the first time or via navigation
- rebuilding the same scene while remaining inside it

This distinction is not bookkeeping trivia. The lab needs it so the learner can separate scene navigation from same-scene ownership reset.

## Worked Timeline: Enter Versus Reload

```text
boot scene
  -> game_rebuild_scene(..., 1)
  -> reload_count++
  -> enter_count++

press R in Level Reload
  -> game_rebuild_scene(..., 0)
  -> reload_count++
  -> enter_count unchanged

switch to another scene and come back later
  -> game_rebuild_scene(..., 1)
  -> reload_count++
  -> enter_count++
```

That is exactly the kind of difference later proof panels can surface meaningfully.

## Step 5: See How App-Level History Seeds Fresh Scene State

After allocation, the rebuild function dispatches by scene:

```c
case GAME_SCENE_LEVEL_RELOAD:
  game_fill_level_reload(level_state, level,
                         game->scene_reload_count[current]);
  break;
```

That means `game_fill_level_reload(...)` is not inventing its own serial. It is handed the current app-level rebuild history.

Inside `game_fill_level_reload(...)`, that reload count becomes fresh scene-local evidence:

- `build_serial = reload_count`
- `reload_seed = reload_count * 17u + 3u`
- the new formations and entity strip are generated from that seed

This is the bridge that makes the rebuild visible. A long-lived app counter feeds a newly constructed level payload.

## Step 6: Notice That Audio State Is Reinitialized, Not Recreated From Nothing

The rebuild function also does this:

```c
game->audio_warning_heat[current] = 0.0f;
game->audio_warning_peak[current] = 0.0f;
...
game_audio_apply_scene_profile(&game->audio, (int)current);
game_audio_trigger_scene_enter(&game->audio, (int)current);
```

This shows another mixed lifetime pattern.

- the audio object itself survives in `GameAppState`
- scene-specific warning pressure for the current scene is reset
- the scene profile is reapplied because rebuild is treated as a meaningful re-entry into that scene's audio identity

So rebuild is more than a graphics refresh. It is a scene reinitialization event layered inside a long-lived app shell.

## Step 7: See Why Wholesale Rebuild Is the Right Lesson Shape

The Level Reload Lab could have been implemented as piecemeal mutation:

- modify some entities in place
- patch some labels
- recycle only selected arrays

But that would teach a weaker concept.

This scene exists to show the cleanest possible version of one idea:

```text
one owner can discard one whole payload and rebuild it from scratch
```

That is why the code is intentionally blunt. It makes the ownership boundary obvious.

## Worked Example: Press `R` After Watching The Scan

Suppose the learner has already watched the scan head move across the stable strip.

Then they press `R`.

What should they expect?

- the app does not restart
- the log history remains available
- the exercise steps already earned still exist
- `scene_reload_count` advances
- a new `GameLevelState` is allocated in the level arena
- `build_serial` and `reload_seed` change inside the new scene payload
- formations and entities are regenerated from the new seed

That is the exact contrast the next lessons depend on.

## Common Mistake: Assuming Reload Also Erases the Learner's Proof Trail

It does not.

The scene payload resets.

The app-level proof trail does not.

That is why the learner can compare multiple rebuilds across one run instead of losing the surrounding history each time.

## JS-to-C Bridge

This is like keeping one long-lived application shell while rebuilding one route-scoped data cache from scratch. Navigation state, logs, and shared services survive. The route-owned payload is replaced wholesale.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find all three call sites that reach `game_rebuild_scene(...)` and explain which ones count as true scene entry.
2. Read the top of `game_rebuild_scene(...)` and explain why `arena_reset(level)` is enough to discard the whole Level Reload payload.
3. Find the reload and enter counters and explain why they are incremented differently.
4. Read `game_fill_level_reload(...)` and explain how app-level rebuild history becomes scene-local proof through `build_serial` and `reload_seed`.
5. Find the audio reset and scene-profile calls and explain what survives versus what is reinitialized.

## Exercises

1. Explain why a manual `R` press is a same-scene rebuild, not a scene entry.
2. Explain why `scene_reload_count` is a stronger proof source than one purely local scene counter.
3. Describe the difference between app-lifetime state and level-arena-owned state in one paragraph.
4. Explain why the Level Reload Lab would be less clear if it mutated entities in place instead of rebuilding the whole payload.
5. Find the audio-warning reset and scene-audio calls and explain what they imply about reload semantics.

## Exercises

1. Explain what survives a Level Reload rebuild and what does not.
2. Explain why the runtime sets `game->level_state = NULL` before rebuilding.
3. Explain why reload count is the right value to pass into `game_fill_level_reload(...)`.
4. Describe why this scene is a good example of arena-style teardown.

## Runtime Proof Checkpoint

At this point, you should be able to explain the complete reload mechanism: `R` triggers a rebuild of the level arena, `GameLevelState` is replaced wholesale, scene-local counters and audio warning heat reset, and scene-global app state remains alive around that rebuild.

## Recap

This lesson traced the Level Reload rebuild pipeline:

- `game_app_update(...)` distinguishes reload from scene entry
- `game_rebuild_scene(...)` resets the whole level arena
- `GameAppState` survives while `GameLevelState` is replaced
- reload counts feed the next scene fill and proof fields
- scene-local warning and audio setup restart on rebuild

Next, we inspect `game_fill_level_reload(...)` so the learner can see exactly what the fresh payload contains and why the rebuilt scene resolves into three cluster headers plus one contiguous entity strip.
