# Lesson 09: Homes + Win Condition

## What we're building

Add the five home cells at the top of the screen, detect when the frog
successfully reaches one, and trigger the win condition when all five are
filled.  We also add scoring, the win overlay, and restart logic — completing
the core gameplay loop.

## What you'll learn

- Goal detection with a tile-coordinate boundary check (`frog_y == 0`)
- `homes_reached` counter and the `PHASE_WIN` state transition
- Score accumulation: `+10` per hop, `+50` per home reached
- `best_score` preservation across `game_init()` resets
- Win overlay rendering and restart-on-any-key
- `PHASE_DEAD` + `lives == 0` game-over path

## Prerequisites

Lessons 01–08 complete and building.  Sprites rendering correctly.

---

## Step 1 — Home cell detection

The frog reaches a home when it hops to the top row (`frog_y == 0`).
We check within the `game_update()` hop handler:

```c
/* After moving frog up (frog_y -= 1) */
if ((int)state->frog_y == 0) {
    /* Frog reached the top row — check if it's a home cell */
    int home_col = (int)state->frog_x;  /* tile column 0..15 */
    /* Home cells are at columns 2, 4, 6, 8, 10 (example layout) */
    int is_home_cell = (home_col % 2 == 0) && home_col >= 2 && home_col <= 10;

    if (is_home_cell) {
        state->homes_reached++;
        state->score += 50;
        game_play_sound(state, SOUND_HOME_REACHED, 0.0f);
        state->frog_y = 9.0f;  /* respawn at bottom */

        if (state->homes_reached >= 5) {
            if (state->score > state->best_score)
                state->best_score = state->score;
            state->phase = PHASE_WIN;
            game_play_sound(state, SOUND_HOME_REACHED, 0.0f);
        }
    } else {
        /* Hit a wall — die */
        state->phase      = PHASE_DEAD;
        state->dead_timer = 1.2f;
        state->lives--;
        game_play_sound(state, SOUND_SPLASH, 0.0f);
    }
}
```

**JS analogy** — this is a simple boundary check:

```js
if (frogY === 0) {
    const homeColumns = [2, 4, 6, 8, 10];
    if (homeColumns.includes(frogX)) {
        homesReached++;
        score += 50;
    }
}
```

---

## Step 2 — PHASE_WIN state

```c
/* In game_update */
if (state->phase == PHASE_WIN) {
    /* Wait for restart key; reset game on press */
    if (input->restart || /* any directional key */) {
        int best = state->best_score;
        game_init(state, assets_dir);     /* full reset */
        state->best_score = best;         /* preserve best score */
    }
    return;  /* no movement during win screen */
}
```

The `best_score` field is saved and restored because `game_init()` does a
`memset(&state, 0, …)` which would clear it:

```c
void game_init(GameState *state, const char *assets_dir) {
    int   best_score         = state->best_score;
    int   samples_per_second = state->audio.samples_per_second;
    float master_volume      = state->audio.master_volume;
    float sfx_volume         = state->audio.sfx_volume;

    memset(state, 0, sizeof(*state));

    state->best_score              = best_score;
    state->audio.samples_per_second = samples_per_second;
    state->audio.master_volume      = master_volume;
    state->audio.sfx_volume         = sfx_volume;
    /* ... rest of init ... */
}
```

**Why this pattern matters:** In JS you'd just keep `bestScore` outside the
state object.  In C, because we `memset` the entire `GameState`, we must
explicitly save any persistent values beforehand.

---

## Step 3 — Win overlay rendering

```c
/* In game_render */
if (state->phase == PHASE_WIN) {
    /* Semi-transparent dark overlay */
    draw_rect_blend(bb, 0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK, 160);

    /* Win text — centred */
    draw_text(bb, "YOU WIN!", SCREEN_W/2 - 32, SCREEN_H/2 - 8,
              COLOR_YELLOW);
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE: %d", state->score);
    draw_text(bb, buf, SCREEN_W/2 - 28, SCREEN_H/2 + 8, COLOR_WHITE);
    draw_text(bb, "PRESS R TO RESTART",
              SCREEN_W/2 - 72, SCREEN_H/2 + 24, COLOR_WHITE);
}
```

---

## Step 4 — Game-over path

```c
/* In PHASE_DEAD countdown */
if (state->dead_timer <= 0.0f) {
    if (state->lives <= 0) {
        /* Don't respawn — stay in PHASE_DEAD with lives == 0.
           game_render shows the game-over overlay. */
        game_play_sound(state, SOUND_GAME_OVER, 0.0f);
    } else {
        /* Respawn */
        state->frog_x = SCREEN_CELLS_W / (2 * TILE_CELLS);
        state->frog_y = 9.0f;
        state->phase  = PHASE_PLAYING;
    }
}
```

```c
/* In game_render — game over overlay */
if (state->phase == PHASE_DEAD && state->lives <= 0) {
    draw_rect_blend(bb, 0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK, 160);
    draw_text(bb, "GAME OVER", SCREEN_W/2 - 36, SCREEN_H/2 - 8,
              COLOR_RED);
    draw_text(bb, "PRESS R TO RESTART",
              SCREEN_W/2 - 72, SCREEN_H/2 + 8, COLOR_WHITE);
}
```

---

## Step 5 — Scoring

```c
/* +10 per hop (in the hop handler) */
if (state->frog_y < old_frog_y)   /* moved toward top */
    state->score += 10;

/* +50 for reaching a home */
state->score += 50;
```

---

## Build and run

```bash
# X11 backend
./build-dev.sh --backend=x11 -r

# Raylib backend
./build-dev.sh --backend=raylib -r
```

---

## Expected result

- Frog fills homes one by one (counter shown in HUD).
- After five homes, win overlay appears with final score.
- After all lives lost, game-over overlay appears.
- Press R to restart from either overlay.
- `BEST SCORE` persists across restarts within a session.

## Exercises

1. Add a `BEST: %d` line to the win overlay using `state->best_score`.
2. Only award `+50` for the *first* time a home is reached (add a
   `homes_filled[5]` bitfield and check it before incrementing).
3. Add a short flash on the home cell before the frog respawns.

## What's next

Lesson 10 adds the bitmap font and HUD — score, timer, and lives counter
displayed every frame using `FONT_8X8[128][8]` and `draw_text()`.
