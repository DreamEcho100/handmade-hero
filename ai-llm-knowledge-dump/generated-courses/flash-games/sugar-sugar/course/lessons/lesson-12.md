# Lesson 12: Teleporters, Gravity Flip, and UI Buttons

## What we're building

Add the final two physics gadgets — teleporters (circle-triggered instant transport)
and gravity flip (inverts all grain movement) — and the on-screen Reset and Gravity
buttons in the HUD strip at the bottom of the playing screen.

## What you'll learn

- Circle distance test for teleporter entry detection
- Teleport cooldown (`tpcd`) — prevents a grain from immediately re-entering the portal it just exited
- `gravity_sign` — a single integer `±1` that inverts the entire physics engine
- On-screen button hit testing with shared layout constants (prevents hover/render mismatch)
- The reset shortcut (R key) and gravity shortcut (G key)

## Prerequisites

- Lesson 11 complete (color filters and multi-stream levels working)

---

## Step 1: Teleporters inside `update_grains`

Add after the color-filter check (and before the settle check):

```c
/* src/game.c  —  teleporter check inside update_grains */

/* ---- Teleporter check (circle distance) ----
 *
 * Each Teleporter has two portals A and B.  When a grain comes within
 * tp->radius pixels of portal A it is teleported to portal B (and vice versa).
 *
 * Cooldown: after teleporting, tpcd[i] is set to 6 frames.  On each frame
 * it decrements toward 0.  While > 0 the grain is ineligible for teleport.
 * Without cooldown, a grain at portal B would immediately be caught by the
 * same teleporter (if B is also within the trigger radius) and bounce forever. */
if (p->tpcd[i] > 0) {
  p->tpcd[i]--;
}

{
  int gx = (int)p->x[i], gy = (int)p->y[i];

  if (p->tpcd[i] == 0) {
    for (int t = 0; t < lv->teleporter_count; t++) {
      Teleporter *tp = &lv->teleporters[t];

      /* Squared distance to portal A */
      int da = (gx - tp->ax) * (gx - tp->ax) + (gy - tp->ay) * (gy - tp->ay);
      /* Squared distance to portal B */
      int db = (gx - tp->bx) * (gx - tp->bx) + (gy - tp->by) * (gy - tp->by);
      int r2 = tp->radius * tp->radius;

      if (da <= r2) {
        /* Grain is inside portal A → teleport to portal B */
        p->x[i]   = (float)tp->bx;
        p->y[i]   = (float)tp->by;
        p->tpcd[i] = 6;
        break;
      }
      if (db <= r2) {
        /* Grain is inside portal B → teleport to portal A */
        p->x[i]   = (float)tp->ax;
        p->y[i]   = (float)tp->ay;
        p->tpcd[i] = 6;
        break;
      }
    }
  }
}
```

Add cooldown decrement at the top of each grain's update (before sub-steps):

```c
/* Near the top of the per-grain loop, before sub-steps */
if (p->tpcd[i] > 0) p->tpcd[i]--;
```

---

## Step 2: Gravity flip

`gravity_sign` is already in `GameState` (`+1` or `-1`).  The physics engine already
uses it via `float grav = GRAVITY * (float)state->gravity_sign;`.

Wire the G key and the on-screen button:

```c
/* src/game.c  —  update_playing: gravity flip input */

/* G key shortcut */
if (state->level.has_gravity_switch && BUTTON_PRESSED(input->gravity)) {
  state->gravity_sign = -state->gravity_sign;
  /* SFX trigger added in Lesson 15 */
}
```

Also handle the cyclic (wrap-around) behavior for levels where grains should
re-enter from the opposite edge instead of disappearing:

```c
/* src/game.c  —  update_grains: replace "remove grain at floor" with: */

if (iy >= H) {
  if (lv->is_cyclic) {
    /* Wrap to top of screen — sugar falls in a loop */
    p->y[i] = 1.0f;
    p->x[i] = nx;
    continue;
  } else {
    p->active[i] = 0;
    goto next_grain;
  }
}
```

---

## Step 3: On-screen HUD buttons

Layout constants shared between update and render so hit tests always match:

```c
/* src/game.c  —  UI layout constants (near top of file, after includes) */
#define UI_BTN_Y    (CANVAS_H - 38)  /* y position of button row        */
#define UI_BTN_H    28               /* button height                   */
#define UI_RESET_X  10               /* x of Reset button               */
#define UI_RESET_W  70               /* width of Reset button           */
#define UI_GRAV_X   88               /* x of Gravity button             */
#define UI_GRAV_W   100              /* width of Gravity button         */
```

Add hover detection to `update_playing`:

```c
/* src/game.c  —  update_playing: button hover + click */

int mx = input->mouse.x, my = input->mouse.y;

/* Hover state (updated every frame for rendering) */
state->reset_hover =
  (mx >= UI_RESET_X && mx < UI_RESET_X + UI_RESET_W &&
   my >= UI_BTN_Y   && my < UI_BTN_Y  + UI_BTN_H);

state->gravity_hover =
  state->level.has_gravity_switch &&
  (mx >= UI_GRAV_X && mx < UI_GRAV_X + UI_GRAV_W &&
   my >= UI_BTN_Y  && my < UI_BTN_Y  + UI_BTN_H);

/* Click handling */
if (BUTTON_PRESSED(input->mouse.left)) {
  if (state->reset_hover) {
    level_load(state, state->current_level);
    return;  /* level_load resets everything; don't continue this frame */
  }
  if (state->gravity_hover) {
    state->gravity_sign = -state->gravity_sign;
    /* Don't return — other input can still run */
  }
}

/* R key shortcut for reset */
if (BUTTON_PRESSED(input->reset)) {
  level_load(state, state->current_level);
  return;
}
```

Guard drawing so the player can't draw into the button strip:

```c
/* src/game.c  —  drawing guard in update_playing */

/* Don't draw into the bottom UI strip */
int over_ui = (my >= UI_BTN_Y - 4);
if (input->mouse.left.ended_down && !over_ui) {
  draw_brush_line(&state->lines,
                  input->mouse.prev_x, input->mouse.prev_y,
                  input->mouse.x, input->mouse.y, BRUSH_RADIUS);
}
```

---

## Step 4: Render teleporters

```c
/* src/game.c  —  render_teleporters */

static void render_teleporters(const LevelDef *lv, GameBackbuffer *bb) {
  for (int t = 0; t < lv->teleporter_count; t++) {
    const Teleporter *tp = &lv->teleporters[t];
    int r = tp->radius;

    /* Portal A — cyan */
    draw_circle(bb,         tp->ax, tp->ay, r, COLOR_PORTAL_A);
    draw_circle_outline(bb, tp->ax, tp->ay, r + 2, COLOR_PORTAL_A);

    /* Portal B — orange */
    draw_circle(bb,         tp->bx, tp->by, r, COLOR_PORTAL_B);
    draw_circle_outline(bb, tp->bx, tp->by, r + 2, COLOR_PORTAL_B);
  }
}
```

---

## Step 5: Render UI buttons

```c
/* src/game.c  —  render_ui_buttons */

static void render_ui_buttons(const GameState *state, GameBackbuffer *bb) {
  /* ---- Reset button ---- */
  uint32_t reset_bg = state->reset_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
  draw_rect(bb, UI_RESET_X, UI_BTN_Y, UI_RESET_W, UI_BTN_H, reset_bg);
  draw_rect_outline(bb, UI_RESET_X, UI_BTN_Y, UI_RESET_W, UI_BTN_H,
                    COLOR_BTN_BORDER);
  /* Label added in Lesson 13 (font system) */

  /* ---- Gravity button (only if level supports it) ---- */
  if (state->level.has_gravity_switch) {
    uint32_t grav_bg = state->gravity_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
    draw_rect(bb, UI_GRAV_X, UI_BTN_Y, UI_GRAV_W, UI_BTN_H, grav_bg);
    draw_rect_outline(bb, UI_GRAV_X, UI_BTN_Y, UI_GRAV_W, UI_BTN_H,
                      COLOR_BTN_BORDER);
  }
}
```

Add to `render_playing`:

```c
render_teleporters(&state->level, bb);
render_filters(&state->level, bb);
render_cups(&state->level, bb);
/* render grains */
render_ui_buttons(state, bb);
```

---

## Example level with teleporters and gravity flip

```c
/* src/levels.c  —  Level 8: "Teleport + Gravity" */
[8] = {
  .index = 8,
  .emitter_count = 1,
  .emitters = {
    [0] = { .x = 320, .y = 30, .grains_per_second = 70 },
  },
  .cup_count = 1,
  .cups = {
    [0] = { .x = 270, .y = 380, .w = 100, .h = 60,
            .required_color = GRAIN_WHITE, .required_count = 120 },
  },
  .teleporter_count = 1,
  .teleporters = {
    [0] = { .ax = 100, .ay = 240, .bx = 540, .by = 240, .radius = 15 },
  },
  .has_gravity_switch = 1,
},
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:**
- Levels with teleporters show cyan and orange circles.  Grains entering one circle appear at the other.
- Levels with `has_gravity_switch = 1` show the Gravity button in the HUD.  Clicking it (or pressing G) flips grain gravity.
- The Reset button clears the level and all drawn lines.

---

## Exercises

1. Create a level where the solution requires exactly one gravity flip: grains fall down, you flip, they fill a cup above the emitter.
2. What happens if `tpcd` is set to 0 instead of 6?  Run the simulation with `teleporter_count = 1` and `ax == bx, ay == by` (same position for both portals) to visualise the infinite-loop bug.
3. Add a visual indicator on the Gravity button that shows the current gravity direction (e.g., draw a small down-arrow or up-arrow).

---

## What's next

In Lesson 13 we add the bitmap font system and the complete HUD — level numbers,
button labels, and the title-screen level-select grid with proper text.
