# Lesson 22 -- HUD and Score Display

## Observable Outcome
A white "Dist: Xm" counter appears in the top-left corner of the screen, updating in real time as the player flies forward. A gold "Score: X" display appears in the top-right corner, combining distance score, pellet bonuses (10 points each), and near-miss bonuses (50 points each). Both readouts remain fixed on screen regardless of camera position.

## New Concepts (max 3)
1. Screen-space HUD rendering: drawing text after all game elements, without camera offset
2. Score composition from multiple sources: distance-based base score plus event-based bonuses
3. snprintf for safe integer-to-string conversion in a fixed-size buffer

## Prerequisites
- Lesson 06 (draw_text, GLYPH_W, bitmap font rendering)
- Lesson 05 (player physics, total_distance accumulation)
- Lesson 10 (pellet collection, near-miss detection)

## Background

The HUD (Heads-Up Display) is the player's window into the game's internal state. Without it, scoring is invisible -- the player cannot tell whether collecting pellets matters more than distance, or how close they are to beating a high score. A good HUD communicates essential information without cluttering the small 320x180 viewport.

Our HUD has exactly two elements: distance and score. Distance shows how far the player has traveled in meters (total_distance / 10.0f), providing a concrete sense of progress. Score combines three sources: distance points, pellet collection bonuses (10 points per pellet), and near-miss bonuses (50 points for narrowly dodging an obstacle). This multi-source scoring rewards skilled play -- a player who dodges obstacles closely and collects pellets scores dramatically higher than one who plays safely.

The critical architectural decision is render order. HUD elements must draw after all game elements (background, floor, obstacles, player, particles) so they appear on top. They must also ignore camera offset -- a HUD element that scrolls with the world is useless. We pass pixel coordinates directly to draw_text, treating the backbuffer as a screen-space canvas rather than a world-space one.

Text positioning for the score uses a right-alignment trick: compute the pixel width of the formatted string (character count times 8 pixels per character at scale 1), then subtract from the screen width with a small margin. This keeps the score display flush against the right edge regardless of how many digits the number has.

## Code Walkthrough

### Step 1: Score computation in game_update (game/main.c)

The score is recomputed every frame from three components:

```c
/* Collect pellets */
int pellets = obstacles_collect_pellets(&state->obstacles,
                                        state->player.hitbox);
if (pellets > 0) {
  game_audio_play(&state->audio, CLIP_PELLET);
}
state->pellet_count += pellets;

/* Near-miss bonus */
if (state->player.state == PLAYER_STATE_NORMAL) {
  int near = obstacles_check_near_miss(&state->obstacles,
                                       state->player.hitbox);
  state->near_miss_count += near;
}

/* Compute final score */
int distance_score = (int)(state->player.total_distance / 10.0f);
state->score = distance_score + state->pellet_count * 10 +
               state->near_miss_count * 50;
```

Distance is accumulated in the player's physics update. The `/10.0f` conversion turns world units into an approximate "meters" value for display. Pellets are worth 10 points each and near-misses are worth 50 -- this weighting encourages aggressive play.

### Step 2: Distance tracking in player_update (player.c)

Distance accumulates every frame during normal gameplay:

```c
/* Integrate position */
player->x += player->speed * dt * dt;
player->y += player->virt_speed * dt * dt;

/* Track distance */
player->total_distance += player->speed * dt * dt;
```

This uses the same double-delta physics model as horizontal movement. The `total_distance` field is a float that grows continuously and is never reset during a run.

### Step 3: The render_hud function (game/main.c)

```c
static void render_hud(GameState *state, Backbuffer *bb) {
  char buf[64];

  /* Distance (top-left) */
  int distance_m = (int)(state->player.total_distance / 10.0f);
  snprintf(buf, sizeof(buf), "Dist: %dm", distance_m);
  draw_text(bb, 4.0f, 4.0f, 1, buf, 1.0f, 1.0f, 1.0f, 1.0f);

  /* Score (top-right area) */
  snprintf(buf, sizeof(buf), "Score: %d", state->score);
  int text_w = (int)strlen(buf) * 8;
  draw_text(bb, (float)(bb->width - text_w - 4), 4.0f, 1, buf,
            1.0f, 0.9f, 0.3f, 1.0f);
}
```

The distance display is white (1.0, 1.0, 1.0) at position (4, 4) -- a 4-pixel margin from the top-left corner. The score display is gold (1.0, 0.9, 0.3) and is right-aligned: the x-position is `bb->width - text_w - 4`, where `text_w` is the string length times GLYPH_W (8 pixels per character at scale 1).

### Step 4: Render order in game_render (game/main.c)

The render function calls HUD drawing after all world elements:

```c
void game_render(GameState *state, Backbuffer *backbuffer, ...) {
  /* 1. Background (parallax) */
  render_background(state, backbuffer);

  /* 2. Floor */
  render_floor(backbuffer);

  /* 3. Obstacles */
  obstacles_render(&state->obstacles, backbuffer, state->camera.x);

  /* 4. Player */
  player_render(&state->player, backbuffer, state->camera.x);

  /* 5. Particles */
  particles_draw(&state->particles, backbuffer, state->camera.x);

  /* 6. HUD -- screen space, no camera offset */
  render_hud(state, backbuffer);

  /* 7. Game over overlay (if applicable) */
  if (state->phase == GAME_PHASE_GAMEOVER) { /* ... */ }
}
```

Steps 3-5 pass `state->camera.x` for world-to-screen conversion. Step 6 passes only the backbuffer -- no camera offset. The HUD draws directly in pixel coordinates.

### Step 5: snprintf for safe string formatting

```c
char buf[64];
snprintf(buf, sizeof(buf), "Dist: %dm", distance_m);
```

`snprintf` is used instead of `sprintf` to prevent buffer overflows. The second argument (`sizeof(buf)`) guarantees that no more than 64 bytes are written, including the null terminator. Even if `distance_m` is an absurdly large number, the output is safely truncated.

### Step 6: Right-alignment calculation

```c
int text_w = (int)strlen(buf) * 8;
draw_text(bb, (float)(bb->width - text_w - 4), 4.0f, 1, buf, ...);
```

Each character in our bitmap font is 8 pixels wide at scale 1 (GLYPH_W = 8). For the string "Score: 1234" (11 characters), `text_w = 88` pixels. On a 320-pixel-wide backbuffer, the x-position becomes `320 - 88 - 4 = 228`. As the score grows and the string gets longer, the text shifts left to stay right-aligned.

## Common Mistakes

**Drawing HUD elements with camera offset.** If you subtract `camera_x` from HUD positions, the text scrolls off-screen as the player moves. HUD coordinates are screen-space: (4, 4) means 4 pixels from the top-left of the backbuffer, always.

**Using sprintf instead of snprintf.** A score value with many digits could overflow a small buffer. While 64 bytes is generous for our format strings, the habit of using `snprintf` everywhere prevents buffer overflow bugs as a class of error.

**Computing text width at the wrong scale.** At scale 1, each character is 8 pixels wide. At scale 2, each character is 16 pixels wide. The formula must be `strlen(buf) * GLYPH_W * scale`. Using the wrong scale causes right-aligned text to be offset incorrectly.

**Recomputing score from only one source.** If you forget to add `near_miss_count * 50` to the score formula, near-miss bonuses appear to do nothing. The player sees the "NEAR!" indicator but the score does not change, which is confusing and discouraging.

**Drawing the HUD before world elements.** If the HUD renders before obstacles and particles, game objects draw on top of the HUD text. The score becomes unreadable whenever an obstacle passes through the top corner of the screen.
