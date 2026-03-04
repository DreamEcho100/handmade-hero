# Lesson 04: One Falling Grain — Gravity, Velocity, and Floor Collision

## What we're building

A single grain of sugar that spawns at the top center of the screen, accelerates
downward under gravity, bounces lightly off the floor, and disappears when it
leaves the canvas.  No other grains yet — just one particle, but with the full
physics loop that will run thousands of them.

## What you'll learn

- Delta-time integration: `velocity += gravity * dt`, `position += velocity * dt`
- Why we cap both `delta_time` and terminal velocity
- Simple floor collision with a coefficient of restitution (bounce)
- Sub-step integration — preventing fast particles from "tunneling" through thin surfaces
- The `Grain` type and why floats (not ints) store positions

## Prerequisites

- Lesson 03 complete (input working on both backends)

---

## Step 1: Physics constants and the Grain type

```c
/* src/game.h  —  physics constants (add after color defines) */

/* Gravity — pixels per second² downward.
 * 280 px/s² feels like light sugar: slow enough to follow drawn lines
 * without racing off-screen, fast enough to look like a real pour.
 * (Earth gravity at 1 px = 1 cm would be 980 px/s² — far too fast.) */
#define GRAVITY     280.0f

/* Terminal velocity cap — without this, a grain in freefall forever would
 * eventually move thousands of pixels per frame and tunnel through walls. */
#define MAX_VY      600.0f
#define MAX_VX      200.0f

/* Bounce coefficient: fraction of |vy| preserved on a floor hit.
 * 0.25 gives a small satisfying pop before the grain settles.
 * Only applied when |vy| > GRAIN_BOUNCE_MIN so micro-taps don't prevent settling. */
#define GRAIN_BOUNCE      0.25f
#define GRAIN_BOUNCE_MIN  20.0f

/* src/game.h  —  Grain type (single grain for now) */

/* Why float positions, not int?
 *
 * At 60 fps, dt ≈ 0.016 s.  gravity * dt ≈ 4.5 px/frame.  After 10 frames
 * a falling grain has moved ~45 px.  If we stored position as int, the
 * fractional part (0.3 px, 0.7 px…) would be truncated every frame —
 * accumulated rounding error makes the simulation inconsistent at different
 * frame rates.  Float keeps the sub-pixel precision intact.
 *
 * JS analogy: same reason you never truncate a scrollPosition to integer
 * each frame in a smooth-scroll animation. */
typedef struct {
  float x, y;   /* position in pixels (sub-pixel precision) */
  float vx, vy; /* velocity in pixels / second              */
  int   active; /* 1 = in simulation, 0 = removed           */
} Grain;
```

---

## Step 2: Add a single grain to `GameState`

```c
/* src/game.h  —  GameState for Lesson 04 */
typedef struct {
  int should_quit;
  int mouse_x, mouse_y;

  /* Single grain for Lesson 04 */
  Grain grain;
} GameState;
```

---

## Step 3: Spawn and update the grain

```c
/* src/game.c  —  game_init: spawn the grain */
void game_init(GameState *state, GameBackbuffer *backbuffer) {
  memset(state, 0, sizeof(*state));
  backbuffer->width  = CANVAS_W;
  backbuffer->height = CANVAS_H;
  backbuffer->pitch  = CANVAS_W * 4;

  /* Spawn one grain near the top center */
  state->grain.x  = (float)(CANVAS_W / 2);
  state->grain.y  = 20.0f;
  state->grain.vx = 0.0f;
  state->grain.vy = 0.0f;         /* starts at rest; gravity will pull it */
  state->grain.active = 1;
}

/* src/game.c  —  game_update: integrate physics */
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Cap dt so a debugger pause doesn't send the grain across the universe. */
  if (delta_time > 0.1f) delta_time = 0.1f;

  state->mouse_x = input->mouse.x;
  state->mouse_y = input->mouse.y;

  if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;

  Grain *g = &state->grain;
  if (!g->active) return;

  /* --- Apply gravity (Euler integration) ---
   *
   * Euler: new_value = old_value + rate * dt
   *
   * Two steps:
   *   1. velocity changes due to acceleration (gravity)
   *   2. position changes due to velocity
   *
   * JS analogy: exactly like a CSS animation with a constant acceleration
   * applied each requestAnimationFrame tick. */
  g->vy += GRAVITY * delta_time;

  /* Clamp to terminal velocity so the grain can never move faster than
   * one canvas height per second — otherwise it could skip over thin lines. */
  if (g->vy >  MAX_VY) g->vy =  MAX_VY;
  if (g->vy < -MAX_VY) g->vy = -MAX_VY;
  if (g->vx >  MAX_VX) g->vx =  MAX_VX;
  if (g->vx < -MAX_VX) g->vx = -MAX_VX;

  /* --- Sub-step integration ---
   *
   * Problem: a fast grain moves (600 px/s × 0.016 s) = 9.6 pixels per frame.
   * If there is a 1-pixel-thick line 5 pixels below the grain, moving by
   * 9.6 px in one step jumps over it entirely — the grain "tunnels through".
   *
   * Solution: split the total movement into sub-steps of at most 1 px each.
   *
   * Example: total displacement = 9.6 px → 10 sub-steps of 0.96 px each.
   * Each sub-step checks for a collision before moving.  The grain stops
   * at the first occupied pixel it encounters. */
  float total_dy = g->vy * delta_time;
  float abs_dy   = total_dy < 0 ? -total_dy : total_dy;
  int   steps    = (int)abs_dy + 1;
  if (steps > 32) steps = 32;        /* cap for performance */
  float sdy = total_dy / (float)steps;

  for (int s = 0; s < steps; s++) {
    float ny = g->y + sdy;
    int   iy = (int)ny;

    /* --- Floor collision --- */
    if (iy >= CANVAS_H) {
      /* The grain hit the floor — bounce it upward. */
      g->y = (float)(CANVAS_H - 1);
      float impact = g->vy < 0 ? -g->vy : g->vy; /* |vy| at impact */
      if (impact > GRAIN_BOUNCE_MIN) {
        /* Strong enough hit → small bounce upward */
        g->vy = -impact * GRAIN_BOUNCE;
      } else {
        /* Very slow tap (gravity accumulation) → just stop */
        g->vy = 0.0f;
      }
      break; /* stop sub-stepping after collision */
    }

    /* --- Ceiling collision (for reversed gravity later) --- */
    if (iy < 0) {
      g->y  = 0.0f;
      g->vy = 0.0f;
      break;
    }

    /* --- No collision — move the grain --- */
    g->y = ny;
  }

  /* Apply horizontal movement (no collisions yet — added in Lesson 06) */
  g->x += g->vx * delta_time;
  if (g->x < 0.0f)                  { g->x = 0.0f;               g->vx = 0.0f; }
  if (g->x >= (float)CANVAS_W)      { g->x = (float)(CANVAS_W-1); g->vx = 0.0f; }
}
```

**What's happening — Euler integration:**

The "Euler method" is the simplest numerical integrator.  Each frame:
1. Velocity += acceleration × dt  (acceleration changes velocity)
2. Position += velocity × dt       (velocity changes position)

It accumulates tiny errors over time (a small phase drift) but for a grain-of-sand
simulation that lasts a few seconds before settling, it is perfectly accurate.

---

## Step 4: Render the grain

```c
/* src/game.c  —  game_render (Lesson 04) */
void game_render(const GameState *state, GameBackbuffer *bb) {
  /* Clear canvas */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++)
    bb->pixels[i] = COLOR_BG;

  /* Draw the single grain as a 2×2 pixel square — 1 px is hard to see. */
  if (state->grain.active) {
    int gx = (int)state->grain.x;
    int gy = (int)state->grain.y;
    draw_rect(bb, gx, gy, 2, 2, COLOR_CREAM);
  }

  /* Draw cursor circle */
  draw_circle(bb, state->mouse_x, state->mouse_y, 5, COLOR_LINE);
}
```

---

## Step 5: Add a `draw_circle` helper (if not already present)

```c
/* src/game.c  —  draw_circle */
static void draw_circle(GameBackbuffer *bb, int cx, int cy, int r, uint32_t color) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx * dx + dy * dy <= r * r)
        draw_pixel(bb, cx + dx, cy + dy, color);
}
```

---

## Build and run

```bash
# Raylib
clang -Wall -Wextra -std=c99 -O0 -g -DDEBUG -fsanitize=address,undefined \
      -o build/game src/main_raylib.c src/game.c \
      -lraylib -lm -lpthread -ldl
./build/game
```

**Expected output:** A single cream-colored 2×2 pixel square falls from near the top of the screen, accelerates, bounces once off the floor, and settles.

---

## Understanding the physics numbers

| Value | Expression | Meaning |
|-------|------------|---------|
| Max fall speed | 600 px/s | ~10 canvas heights/second |
| Frame displacement at MAX_VY | 600 × 0.016 = 9.6 px | 10 sub-steps |
| Bounce height at 600 px/s impact | 600 × 0.25 = 150 px/s | ~0.5 canvas |
| Bounce threshold | 20 px/s | Below this, no bounce |

---

## Exercises

1. Change `GRAIN_BOUNCE` to `0.8` and observe the grain bouncing high repeatedly.  Then try `0.0` (no bounce at all).
2. Remove the `if (delta_time > 0.1f)` cap and observe what happens when you pause the debugger for a few seconds then resume.
3. Add a `respawn` flag: when the grain settles on the floor (both `g->vy == 0` and `g->y >= CANVAS_H - 1`), reset it to the top center with `vy = 0`.

---

## What's next

In Lesson 05 we replace the single `Grain` with a `GrainPool` — a fixed-size
struct-of-arrays that holds 4096 grains simultaneously, and introduce
`build-dev.sh` to simplify the compile command.
