# Lesson 10 — Restart and Speed Scaling

## What We're Building

Handle the game-over state: block all input except restart, reset the game while preserving the high score, and implement the speed-scaling formula that makes the game progressively harder.

---

## The Concept

### JS Analogy: A Modal Dialog That Blocks All Input Except "OK"

When a modal dialog is open in a web app, all keyboard events targeted at the rest of the page are swallowed. Only the dialog's own buttons respond. In code:

```js
function handleInput(state, event) {
  if (state.phase === GAME_PHASE.GAME_OVER) {
    if (event.key === 'r' || event.key === ' ') restartGame(state);
    return;  // all other input blocked
  }
  // ... normal gameplay input
}
```

`GAME_PHASE_GAME_OVER` in `snake_update` works identically. The `switch` dispatches to a case that only checks `input->restart` and returns immediately otherwise. The playing logic is unreachable until `snake_init()` sets `phase = GAME_PHASE_PLAYING`.

---

### `snake_init` — memset + Best Score Preservation

`memset(s, 0, sizeof(*s))` zeros every byte of the `GameState` struct. This is C's equivalent of `state = { ...defaultGameState }` in JavaScript — a clean slate.

The problem: `best_score` is part of the same struct and gets wiped. The fix is a save-and-restore pattern:

```c
// JS equivalent:
// const best = state.best_score;
// state = { ...defaultState };
// state.best_score = best;
```

The same pattern applies to `audio.samples_per_second` — the platform sets this field once before the game loop starts, and it must survive every restart.

---

### Speed Scaling Formula

`move_interval` starts at `0.15f` (one step every 150 ms ≈ 6.7 steps/second).

Every 3 points scored, the interval decreases by `0.01f` (10 ms faster), down to a floor of `0.05f` (50 ms ≈ 20 steps/second).

```
score  0: interval = 0.15s   (6.7 steps/s)
score  3: interval = 0.14s   (7.1 steps/s)
score  6: interval = 0.13s   (7.7 steps/s)
…
score 30: interval = 0.05s   (20  steps/s — floor, no further reduction)
```

The floor prevents the game from becoming physically impossible at high scores.

---

## The Code

### `GAME_PHASE_GAME_OVER` Case — File: `src/game.c`, `snake_update`

```c
/* ── GAME_PHASE_GAME_OVER ─────────────────────────────────────────────────
 * Only restart input is accepted in game-over state.
 * This is the "modal dialog" — every other input is silently ignored.
 *
 * JS: if (state.phase === GAME_PHASE.GAME_OVER) {
 *       if (input.restart) restartGame(state);
 *       return;
 *     } */
case GAME_PHASE_GAME_OVER: {
    if (input->restart) {
        snake_init(s);
        /* snake_init resets phase to GAME_PHASE_PLAYING — no need to set here */
    }
    break;
}
```

`input->restart` is an `int` (0 or 1), not a `GameButtonState`. It is set to 1 by the platform when R or Space is pressed, and reset to 0 at the start of each frame by `prepare_input_frame`. It fires once per press.

---

### `snake_init` — File: `src/game.c`

```c
void snake_init(GameState *s) {
    /* Step 1: save values that must survive the memset */
    int saved_best = s->best_score;
    int saved_sps  = s->audio.samples_per_second;
    int i;

    /* Step 2: zero the entire struct.
     * JS: s = { ...defaultState }
     * Every field — score, length, phase, timer — is cleared in one call. */
    memset(s, 0, sizeof(*s));

    /* Step 3: restore preserved values */
    s->best_score               = saved_best;
    s->audio.samples_per_second = saved_sps;

    /* Step 4: set initial snake — 10 segments, horizontal, centred */
    s->head   = 9;
    s->tail   = 0;
    s->length = 10;
    for (i = 0; i < 10; i++) {
        s->segments[i].x = 10 + i;   /* columns 10..19 */
        s->segments[i].y = GRID_HEIGHT / 2;
    }

    s->direction      = SNAKE_DIR_RIGHT;
    s->next_direction = SNAKE_DIR_RIGHT;
    s->move_timer     = 0.0f;
    s->move_interval  = 0.15f;  /* comfortable starting speed */
    s->grow_pending   = 0;
    s->score          = 0;
    s->phase          = GAME_PHASE_PLAYING;

    /* Seed PRNG for food placement.
     * JS: Math.random() is always seeded; in C we must do it manually. */
    srand((unsigned)time(NULL));
    snake_spawn_food(s);
}
```

---

### Speed Scaling — File: `src/game.c`, food-eaten branch in `snake_update`

```c
/* Speed up every 3 points, but don't go below floor.
 *
 * score % 3 == 0: fires at scores 3, 6, 9, 12, …
 * move_interval > 0.05f: floor guard — never faster than 20 steps/second.
 *
 * JS: if (state.score % 3 === 0 && state.moveInterval > 0.05)
 *       state.moveInterval -= 0.01; */
if (s->score % 3 == 0 && s->move_interval > 0.05f)
    s->move_interval -= 0.01f;
```

---

### Game-Over Overlay Rendering — File: `src/game.c`, `snake_render`

The overlay is drawn last (painter's algorithm — on top of everything):

```c
if (s->phase == GAME_PHASE_GAME_OVER) {
    int field_y = HEADER_ROWS * CELL_SIZE;
    int field_w = GRID_WIDTH  * CELL_SIZE;
    int field_h = GRID_HEIGHT * CELL_SIZE;
    int cx      = field_w / 2;
    int cy      = field_y + field_h / 2;

    /* Semi-transparent black overlay — alpha=180 ≈ 70% opaque.
     * draw_rect_blend applies the alpha "over" formula per pixel.
     * JS: ctx.globalAlpha = 180/255; ctx.fillStyle = '#000'; ctx.fillRect(...); */
    draw_rect_blend(bb, 0, field_y, field_w, field_h, GAME_RGBA(0,0,0,180));

    /* Red border: 4 thin rectangles forming a frame */
    draw_rect(bb, 0,           field_y,               field_w, 4, COLOR_RED);
    draw_rect(bb, 0,           field_y + field_h - 4, field_w, 4, COLOR_RED);
    draw_rect(bb, 0,           field_y,               4, field_h, COLOR_RED);
    draw_rect(bb, field_w - 4, field_y,               4, field_h, COLOR_RED);

    /* "GAME OVER" at scale 3 (24×24 px glyphs) — centred horizontally */
    {
        int scale   = 3;
        int str_len = 9;  /* strlen("GAME OVER") */
        int str_w   = str_len * GLYPH_STRIDE * scale;
        draw_text(bb, cx - str_w / 2, cy - 36, "GAME OVER", COLOR_RED, scale);
    }

    /* Final score at scale 2 */
    snprintf(buf, sizeof(buf), "SCORE %d", s->score);
    {
        int sw = (int)strlen(buf) * GLYPH_STRIDE * 2;
        draw_text(bb, cx - sw / 2, cy + 6, buf, COLOR_WHITE, 2);
    }

    /* Restart and quit hints */
    draw_text(bb, cx - (9 * GLYPH_STRIDE * 2) / 2, cy + 30, "R RESTART", COLOR_WHITE, 2);
    draw_text(bb, cx - (6 * GLYPH_STRIDE * 2) / 2, cy + 54, "Q QUIT",    COLOR_GRAY,  2);
}
```

The snake body and head are already drawn with `COLOR_DARK_RED` when `phase == GAME_PHASE_GAME_OVER` (set earlier in `snake_render`). The overlay renders on top.

---

## What To Notice

- **`snake_init` is called from two places:** once at startup (from `main`) and once when the player restarts. Both paths must end up in a clean state — `memset` guarantees that even fields we don't explicitly set are zero.
- **The overlay is `const GameState *`-friendly.** `snake_render` takes a `const` pointer. The overlay just reads `s->phase` and `s->score` — no mutation.
- **Speed scaling fires on score, not time.** This means a slow player who eats food infrequently never gets a speed increase — they always play at their current comfort level.
- **`GAME_RGBA(0,0,0,180)` is not `COLOR_BLACK`.** `COLOR_BLACK` is `GAME_RGB(0,0,0)` — fully opaque. The overlay needs an alpha component so the game board is still visible beneath it.

---

## Exercises

1. **Test the memset.** Print every field of `GameState` before and after `snake_init()`. Confirm all fields are zero except the ones explicitly set. What value does `GAME_PHASE_PLAYING` have? (Hint: enums start at 0 by default.)

2. **Tune the speed curve.** Change the scaling to fire every 5 points and decrease by `0.02f`. At what score does it hit the floor? Is the game easier or harder?

3. **Animate the game-over overlay.** Instead of an instant full overlay, fade it in over 1 second. Store `float overlay_alpha` in `GameState` and increment it by `delta_time / 1.0f` during `GAME_PHASE_GAME_OVER`. Use it as the alpha component of `GAME_RGBA(0,0,0, (int)(overlay_alpha * 180))`.
