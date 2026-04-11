# Lesson 23 -- Main Menu and Pause

## Observable Outcome
The game starts directly in the PLAYING phase (no menu delay). After dying and seeing the game over screen, pressing SPACE restarts the game: the player resets to the start position, obstacles clear, the score resets to zero, and background music restarts. All sprites and audio clips remain loaded -- the restart is instant. The full game flow cycle works: play, die, game over, restart, play again.

## New Concepts (max 3)
1. GamePhase state machine: LOADING, MENU, PLAYING, PAUSED, DYING, GAMEOVER -- with transitions driven by input and game events
2. Selective field reset on restart: preserve sprite/audio heap pointers while zeroing gameplay fields
3. Obstacle pool reset: deactivate all blocks, pellets, shields, and near-miss indicators without freeing sprite memory

## Prerequisites
- Lesson 21 (death sequence, GAMEOVER state, game over overlay)
- Lesson 01 (GameState struct, game_init)
- Lesson 12 (obstacles spawning, ObstacleManager)

## Background

A game without a restart loop is a demo, not a game. The restart flow is deceptively complex because the GameState struct contains both volatile gameplay data (player position, score, obstacle positions) and persistent resource data (loaded sprites, decoded audio clips, high score table). The naive approach -- `memset(state, 0, sizeof(*state))` -- destroys everything, including heap pointers to sprite pixels and audio samples. This causes crashes (use-after-free) or leaks (memory becomes unreachable without being freed).

The correct approach is surgical: reset only the gameplay fields while leaving resource pointers untouched. Player position, speed, score, and state go back to their initial values. Obstacle blocks, pellets, shields, and near-miss indicators are deactivated (their `active` flags set to 0) but their sprite references remain valid. The particle system is re-initialized (it only contains stack-allocated data). Audio is not reloaded -- we simply restart the music playback.

The GamePhase enum defines all possible states of the game's top-level state machine. In our current implementation, LOADING and MENU phases are defined but not actively used -- the game jumps straight to PLAYING on startup. The PAUSED phase is declared for future use (toggled by the P key). The active transitions are: PLAYING --> GAMEOVER (when player reaches PLAYER_STATE_GAMEOVER) and GAMEOVER --> PLAYING (when the player presses SPACE or R).

This incremental approach -- defining the full state machine but only implementing the paths you need now -- keeps the architecture extensible without shipping dead code that could contain bugs.

## Code Walkthrough

### Step 1: GamePhase enum (game/main.h)

```c
typedef enum {
  GAME_PHASE_LOADING,
  GAME_PHASE_MENU,
  GAME_PHASE_PLAYING,
  GAME_PHASE_PAUSED,
  GAME_PHASE_DYING,
  GAME_PHASE_GAMEOVER,
} GamePhase;
```

The enum defines six phases. Currently three are active: LOADING (immediately transitions to PLAYING), PLAYING, and GAMEOVER. The others are reserved for future expansion.

### Step 2: Initial game state (game/main.c)

```c
void game_init(GameState *state, PlatformGameProps *props) {
  (void)props;
  memset(state, 0, sizeof(*state));
  state->phase = GAME_PHASE_PLAYING; /* Start immediately */
  state->is_running = 1;
  state->camera.x = 0.0f;
  state->camera.y = 0.0f;
  state->camera.zoom = 1.0f;

  player_init(&state->player);
  obstacles_init(&state->obstacles);
  /* ... audio and particles init ... */
}
```

At initialization time, `memset` is safe because no heap resources exist yet. The game starts in PLAYING phase -- no loading screen, no menu. Players are in the action within one frame of launch.

### Step 3: The GAMEOVER handler with restart (game/main.c)

The restart logic is the most important code in this lesson. Every gameplay field is explicitly reset:

```c
case GAME_PHASE_GAMEOVER:
  if (button_just_pressed(input->buttons.restart) ||
      button_just_pressed(input->buttons.action)) {

    state->phase = GAME_PHASE_PLAYING;
    state->camera.x = 0.0f;
    state->camera.y = 0.0f;
    state->camera.zoom = 1.0f;
    state->score = 0;
    state->pellet_count = 0;
    state->near_miss_count = 0;
```

### Step 4: Player field reset (preserving sprites)

```c
    /* Reset player -- keep loaded sprites */
    state->player.x = PLAYER_START_X;
    state->player.y = PLAYER_START_Y;
    state->player.speed = PLAYER_MIN_SPEED;
    state->player.virt_speed = 0.0f;
    state->player.state = PLAYER_STATE_NORMAL;
    state->player.is_on_floor = 1;
    state->player.is_on_ceiling = 0;
    state->player.has_shield = 0;
    state->player.hurted = 0;
    state->player.hurt_timer = 0.0f;
    state->player.end_timer = 0.0f;
    state->player.total_distance = 0.0f;
    anim_reset(&state->player.anim_ground);
    anim_reset(&state->player.anim_dead);
```

Notice what is NOT reset: `sprite_slime`, `sprite_dead`, `sheet`, `sheet_dead`. These point to heap-allocated pixel data loaded during `player_init`. Zeroing them would leak memory and cause null pointer dereferences on the next render call.

### Step 5: Obstacle pool reset (preserving sprites)

```c
    /* Reset obstacles -- keep loaded sprites */
    for (int i = 0; i < MAX_OBSTACLE_BLOCKS; i++)
      state->obstacles.blocks[i].active = 0;
    state->obstacles.block_count = 0;
    state->obstacles.pellet_count = 0;
    for (int i = 0; i < MAX_PELLETS; i++)
      state->obstacles.pellets[i].active = 0;
    for (int i = 0; i < MAX_SHIELD_PICKUPS; i++)
      state->obstacles.shields[i].active = 0;
    for (int i = 0; i < MAX_NEAR_MISS_INDICATORS; i++)
      state->obstacles.near_misses[i].active = 0;
    state->obstacles.last_spawned_type = OBS_NONE;
    state->obstacles.next_spawn_x = PLAYER_START_X + 200.0f;
    state->obstacles.shield_timer =
        rng_float_range(&state->obstacles.rng, 20.0f, 40.0f);
```

The `next_spawn_x` is reset to 200 units ahead of the player start position, giving the player a brief safe zone before obstacles appear. The shield timer is re-randomized so the first shield pickup appears at a different time each run.

### Step 6: Particle and music restart

```c
    /* Reset particles (all stack data, safe to reinit) */
    particles_init(&state->particles);

    /* Restart music */
    game_audio_play_music(&state->audio, CLIP_BG_MUSIC, 0.3f);
  }
  break;
```

`particles_init` does a `memset(ps, 0, sizeof(*ps))` which is safe because the particle pool contains only value types (floats, ints) -- no heap pointers. The music is restarted by calling `game_audio_play_music`, which resets the music voice's position to 0 and sets it active.

### Step 7: Phase transition from PLAYING to GAMEOVER

In the PLAYING phase handler, the transition is a simple check:

```c
case GAME_PHASE_PLAYING: {
  /* ... physics, collision, scoring ... */

  if (state->player.state == PLAYER_STATE_GAMEOVER) {
    state->phase = GAME_PHASE_GAMEOVER;
  }
  break;
}
```

This decouples the player's internal state machine from the game's top-level phase. The player transitions through NORMAL -> DEAD -> GAMEOVER independently, and the game loop detects when the player has reached GAMEOVER.

### Step 8: Quit handling

```c
if (input->buttons.quit.ended_down) {
  state->is_running = 0;
  return;
}
```

ESC sets `is_running = 0`, which the platform backend checks to break out of the main loop. This works in any game phase.

## Common Mistakes

**Using memset to restart the game state.** This is the most dangerous mistake. `memset(state, 0, sizeof(*state))` zeroes ALL fields including `player.sprite_slime.pixels` (a heap pointer), `audio.loaded.clips[].samples` (heap pointers), and `obstacles.sprite_dagger.pixels` (heap pointer). The next frame will dereference null pointers and crash.

**Forgetting to reset `anim_dead.finished`.** Without `anim_reset(&state->player.anim_dead)` during restart, the death animation's `finished` flag is still true from the previous run. The next time the player dies, the animation immediately reports "finished" and the death sequence is skipped.

**Not resetting `next_spawn_x`.** If `next_spawn_x` keeps its value from the previous run (possibly thousands of units ahead), no obstacles spawn until the player catches up to that position. The new game feels empty for a long stretch.

**Resetting the RNG seed on restart.** The obstacle RNG should NOT be re-seeded to the same value each restart, or every run will have identical obstacle patterns. The current code lets the RNG continue from wherever it was, producing a different sequence each run while remaining deterministic within a run.
