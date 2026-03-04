# Lesson 10: Win Detection — Cup Fill, Level Unlock, and Progress

## What we're building

Check whether all cups in a level are full after every simulation step.  When the win
condition is met, unlock the next level and transition to `PHASE_LEVEL_COMPLETE`.
Add the cup absorption logic inside `update_grains` so falling grains are collected.

## What you'll learn

- AABB (Axis-Aligned Bounding Box) intersection for cup collection
- Why we test the cup **interior** (not the exact pixel-edge walls)
- `check_win()` — looping over all cups to find any that are not yet full
- Unlocking the next level via `unlocked_count`
- The `level_complete` and `level_advance` flow
- Persistent unlock state across phase transitions

## Prerequisites

- Lesson 09 complete (LevelDef and level_load working)

---

## Step 1: Cup absorption inside `update_grains`

Add a cup-check block after teleporters (before the settle check) for each grain:

```c
/* src/game.c  —  cup check inside update_grains (after sub-step loop) */

/* ---- Cup check (AABB — interior only, walls excluded) ----
 *
 * We check the *interior* of the cup:  x+1 .. x+w-2,  y .. y+h-2
 * because stamp_cups() placed solid wall pixels at the exact edges.
 * Testing the interior prevents double-processing a grain that is
 * physically colliding with the wall pixels.
 *
 * When the cup is full the grain is NOT absorbed — it rests on the
 * solid bottom wall and eventually spills back out over the open top. */
{
  int gx = (int)p->x[i], gy = (int)p->y[i];

  for (int c = 0; c < lv->cup_count; c++) {
    Cup *cup = &lv->cups[c];

    /* Interior bounds: one pixel inside each wall */
    int ix0 = cup->x + 1,     ix1 = cup->x + cup->w - 1;
    int iy0 = cup->y,          iy1 = cup->y + cup->h - 1;

    if (gx >= ix0 && gx < ix1 && gy >= iy0 && gy < iy1) {

      int color_ok = (cup->required_color == GRAIN_WHITE ||
                      p->color[i] == (uint8_t)cup->required_color);

      if (color_ok && cup->collected < cup->required_count) {
        /* Absorb the grain: count it and remove from simulation. */
        cup->collected++;
        if (cup->collected == cup->required_count) {
          /* Cup just reached 100% — SFX triggered in Lesson 15 */
        }
        s_occ[gy * W + gx] = 0;  /* clear occupancy — grain is gone */
        p->active[i] = 0;
        goto next_grain;

      } else if (!color_ok) {
        /* Wrong color: discard silently.
         * The original Sugar Sugar game does the same — no penalty,
         * but the grain disappears (it can't pollute the cup). */
        s_occ[gy * W + gx] = 0;
        p->active[i] = 0;
        goto next_grain;
      }

      /* Cup is full with correct color: grain stays active.
       * It will pile up inside and eventually spill over the rim. */
    }
  }
}
```

---

## Step 2: `check_win()` — all cups filled?

```c
/* src/game.c  —  check_win */

/* Returns 1 if every cup in the active level is filled, 0 otherwise.
 * Called once per frame at the end of update_playing. */
static int check_win(GameState *state) {
  LevelDef *lv = &state->level;
  for (int c = 0; c < lv->cup_count; c++) {
    if (lv->cups[c].collected < lv->cups[c].required_count)
      return 0; /* at least one cup not yet full */
  }
  return 1; /* all cups filled */
}
```

---

## Step 3: Wire win detection into `update_playing`

```c
/* src/game.c  —  update_playing: add win check at the end */

static void update_playing(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  /* ... (drawing, reset, escape, simulation — from Lesson 08) ... */

  spawn_grains(state, dt);
  update_grains(state, dt);

  /* ---- Win check ---- */
  if (check_win(state)) {
    /* Unlock the next level if not already unlocked. */
    int next = state->current_level + 1;
    if (next < TOTAL_LEVELS && next >= state->unlocked_count)
      state->unlocked_count = next + 1;

    change_phase(state, PHASE_LEVEL_COMPLETE);
  }
}
```

---

## Step 4: Level advancement in `update_level_complete`

```c
/* src/game.c  —  update_level_complete: advance to next level */

static void update_level_complete(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  int advance = (state->phase_timer > 2.0f)
             || BUTTON_PRESSED(input->mouse.left)
             || BUTTON_PRESSED(input->enter);

  if (advance) {
    int next = state->current_level + 1;
    if (next >= TOTAL_LEVELS) {
      /* All 30 levels complete — unlock sandbox mode. */
      change_phase(state, PHASE_FREEPLAY);
    } else {
      level_load(state, next);
      change_phase(state, PHASE_PLAYING);
    }
  }
}
```

---

## Step 5: Cup progress rendering

Add a visual fill-bar inside each cup so the player can see progress.

```c
/* src/game.c  —  render_cups (called from render_playing) */

static void render_cups(const LevelDef *lv, GameBackbuffer *bb) {
  for (int c = 0; c < lv->cup_count; c++) {
    const Cup *cup = &lv->cups[c];
    int full = (cup->collected >= cup->required_count);

    /* Draw empty cup interior */
    draw_rect(bb, cup->x + 1, cup->y, cup->w - 2, cup->h - 1, COLOR_CUP_EMPTY);

    /* Draw fill bar rising from the bottom */
    if (cup->required_count > 0) {
      int filled_h = full
        ? (cup->h - 1)
        : (cup->collected * (cup->h - 1)) / cup->required_count;
      int fill_y = cup->y + (cup->h - 1) - filled_h;
      uint32_t fill_color = full ? COLOR_CUP_FILL_FULL : COLOR_CUP_FILL;
      draw_rect(bb, cup->x + 1, fill_y, cup->w - 2, filled_h, fill_color);
    }

    /* Cup outline (over the fill so walls are visible) */
    draw_rect_outline(bb, cup->x, cup->y, cup->w, cup->h, COLOR_CUP_BORDER);
  }
}
```

Call `render_cups` from `render_playing`:

```c
static void render_playing(const GameState *state, GameBackbuffer *bb) {
  /* ... clear, lines, grains (from Lesson 08) ... */

  render_cups(&state->level, bb);
}
```

---

## Understanding cup absorption

```
Grain at (gx, gy) entering cup[c]:

  Cup walls (stamped as solid in LineBitmap):
    left:   x = cup->x
    right:  x = cup->x + cup->w - 1
    bottom: y = cup->y + cup->h - 1

  Interior (absorption zone):
    x in [cup->x+1, cup->x+cup->w-2]
    y in [cup->y,   cup->y+cup->h-2]

  Why not the wall pixels?
    The wall pixels are in LineBitmap — IS_SOLID returns true for them.
    A grain collides with the wall and slides — it never "is at" a wall pixel.
    Testing only the interior guarantees we absorb grains that have already
    passed through the open top and are inside the cup cavity.
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:**
- Level 0 shows a cup with a blue fill bar that rises as grains land inside.
- When the bar reaches 100% (bright green), a brief celebration overlay appears.
- After 2 seconds (or on click), Level 1 loads automatically.
- Level 2 is now unlocked on the title screen.

---

## Exercises

1. Add a `collected_visual` float field to `Cup` that lags behind `collected` (lerp at 0.05 per frame).  Use it for the fill bar so it animates smoothly.
2. What happens with `required_color = GRAIN_RED` if the player never routes grains through a filter?  Verify that white grains are rejected and disappear.
3. Change `required_count` to 999 for a test level.  Does the cup ever overflow?  Observe the pile-up inside when the cup is full.

---

## What's next

In Lesson 11 we add color filters — zones where passing grains change color —
and multi-emitter levels where the player must route different streams to different cups.
