# Lesson 09: Level System — LevelDef, Emitters, Cups, and levels.c

## What we're building

Add the `LevelDef` type and a `levels.c` file with 30 hand-crafted puzzle levels.
Wire `level_load()` into the state machine so clicking a level button on the title
screen actually starts that puzzle.  After this lesson the game is playable end-to-end.

## What you'll learn

- `LevelDef` and its entity arrays (`Emitter`, `Cup`, `Obstacle`, etc.)
- C99 designated initialisers — the cleanest way to write level data
- `levels.c` + `extern` declaration — separating data from logic
- `level_load()` — stamping cups and obstacles into the `LineBitmap`
- `stamp_cups()` and `stamp_obstacles()` — baking level geometry into physics
- The title-screen level-select grid

## Prerequisites

- Lesson 08 complete (state machine working)

---

## Step 1: Level entity types in `game.h`

```c
/* src/game.h  —  level entities */

/* Emitter: continuously spawns grains from its spout position. */
typedef struct {
  int   x, y;              /* pixel position of the spout         */
  int   grains_per_second; /* emission rate (e.g. 80)             */
  float spawn_timer;       /* internal: seconds since last emit   */
} Emitter;

/* Cup: the goal — collect enough grains of the right color. */
typedef struct {
  int        x, y, w, h;       /* bounding box (pixels)           */
  GRAIN_COLOR required_color;  /* GRAIN_WHITE = accepts any color */
  int        required_count;   /* grains needed to fill the cup   */
  int        collected;        /* grains received so far          */
} Cup;

/* ColorFilter: grains passing through this zone change color. */
typedef struct {
  int         x, y, w, h;    /* bounding box of the filter zone  */
  GRAIN_COLOR output_color;  /* color grains get after passing   */
} ColorFilter;

/* Teleporter: two linked portals — grains entering A exit at B and vice versa. */
typedef struct {
  int ax, ay; /* portal A center (pixels) */
  int bx, by; /* portal B center (pixels) */
  int radius; /* trigger radius  (pixels) */
} Teleporter;

/* Obstacle: a solid rectangle pre-stamped into the line bitmap. */
typedef struct {
  int x, y, w, h;
} Obstacle;

#define MAX_EMITTERS    2
#define MAX_CUPS        8
#define MAX_FILTERS     4
#define MAX_TELEPORTERS 2
#define MAX_OBSTACLES  12

/* LevelDef: everything needed to initialise one puzzle. */
typedef struct {
  int         index;
  Emitter     emitters[MAX_EMITTERS];
  int         emitter_count;
  Cup         cups[MAX_CUPS];
  int         cup_count;
  ColorFilter filters[MAX_FILTERS];
  int         filter_count;
  Teleporter  teleporters[MAX_TELEPORTERS];
  int         teleporter_count;
  Obstacle    obstacles[MAX_OBSTACLES];
  int         obstacle_count;
  int         has_gravity_switch; /* 1 = show the gravity flip button */
  int         is_cyclic;          /* 1 = sugar wraps bottom→top       */
} LevelDef;

/* Total level count — defined in levels.c */
#define TOTAL_LEVELS 30
extern LevelDef g_levels[TOTAL_LEVELS];
```

---

## Step 2: `levels.c` — data separated from logic

The level data lives in its own translation unit.  This keeps `game.c` readable
and lets you edit level data without recompiling game logic.

```c
/* src/levels.c  —  Sugar, Sugar | Level Data (first 3 levels shown) */

#include "game.h"

/* C99 designated initialisers let you name every field explicitly.
 * Unspecified fields are zero-initialised (cups start with collected=0, etc.).
 *
 * JS analogy: like an object literal with default values:
 *   const level = { emitters: [...], cups: [...], ...rest: defaults }
 */
LevelDef g_levels[TOTAL_LEVELS] = {

  /* ------------------------------------------------------------------ */
  /* LEVEL 0: "Tutorial" — single emitter, single cup, no obstacles     */
  /* ------------------------------------------------------------------ */
  [0] = {
    .index = 0,
    .emitter_count = 1,
    .emitters = {
      [0] = { .x = 320, .y = 30, .grains_per_second = 60 },
    },
    .cup_count = 1,
    .cups = {
      [0] = { .x = 270, .y = 380, .w = 100, .h = 60,
              .required_color = GRAIN_WHITE, .required_count = 120 },
    },
  },

  /* ------------------------------------------------------------------ */
  /* LEVEL 1: "Funnel" — one emitter, one cup, one obstacle ramp        */
  /* ------------------------------------------------------------------ */
  [1] = {
    .index = 1,
    .emitter_count = 1,
    .emitters = {
      [0] = { .x = 160, .y = 30, .grains_per_second = 70 },
    },
    .cup_count = 1,
    .cups = {
      [0] = { .x = 450, .y = 380, .w = 100, .h = 60,
              .required_color = GRAIN_WHITE, .required_count = 120 },
    },
    .obstacle_count = 1,
    .obstacles = {
      [0] = { .x = 100, .y = 200, .w = 200, .h = 6 },
    },
  },

  /* ------------------------------------------------------------------ */
  /* LEVEL 2: "Split" — two emitters, two cups                          */
  /* ------------------------------------------------------------------ */
  [2] = {
    .index = 2,
    .emitter_count = 2,
    .emitters = {
      [0] = { .x = 160, .y = 30, .grains_per_second = 60 },
      [1] = { .x = 480, .y = 30, .grains_per_second = 60 },
    },
    .cup_count = 2,
    .cups = {
      [0] = { .x = 80,  .y = 380, .w = 100, .h = 60,
              .required_color = GRAIN_WHITE, .required_count = 100 },
      [1] = { .x = 460, .y = 380, .w = 100, .h = 60,
              .required_color = GRAIN_WHITE, .required_count = 100 },
    },
  },

  /* Levels 3–29: fill in similarly — shown abbreviated here */
  /* ... */
};
```

Add `src/levels.c` to the build script's `SHARED_SRCS`:

```bash
# build-dev.sh  —  update SHARED_SRCS
SHARED_SRCS="src/game.c src/levels.c"
```

---

## Step 3: `level_load()` — reset and stamp geometry

```c
/* src/game.c  —  level_load */

static void level_load(GameState *state, int index) {
  /* Copy the level definition from the read-only global array.
   * 'state->level' is a live copy we can mutate (timers, collected counts). */
  state->level = g_levels[index];
  state->current_level = index;

  /* Clear all grains and drawn lines — fresh slate for each level. */
  memset(&state->grains, 0, sizeof(state->grains));
  memset(&state->lines,  0, sizeof(state->lines));

  /* Reset emitter spawn timers in the live copy. */
  for (int i = 0; i < state->level.emitter_count; i++)
    state->level.emitters[i].spawn_timer = 0.0f;

  /* Reset cup collected counts in the live copy. */
  for (int i = 0; i < state->level.cup_count; i++)
    state->level.cups[i].collected = 0;

  /* Normal gravity on every level load. */
  state->gravity_sign = 1;

  /* Stamp pre-set obstacles and cup walls into the line bitmap. */
  stamp_obstacles(state);
  stamp_cups(state);
}

/* stamp_obstacles: bake Obstacle rectangles into the line bitmap.
 * Grains collide with these exactly like drawn lines. */
static void stamp_obstacles(GameState *state) {
  LevelDef *lv = &state->level;
  for (int i = 0; i < lv->obstacle_count; i++) {
    Obstacle *o = &lv->obstacles[i];
    for (int py = o->y; py < o->y + o->h; py++)
      for (int px = o->x; px < o->x + o->w; px++)
        if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H)
          state->lines.pixels[py * CANVAS_W + px] = 1;
  }
}

/* stamp_cups: bake Cup walls into the line bitmap.
 *
 * Cup geometry (x, y, w, h):
 *
 *     x         x+w-1
 *     |  (open)  |    ← y   (top is open — grains fall in)
 *     |          |
 *     |__________|    ← y+h-1 (solid bottom)
 *
 * We stamp left wall, right wall, and bottom.  The top is left open. */
static void stamp_cups(GameState *state) {
  LevelDef   *lv = &state->level;
  LineBitmap *lb = &state->lines;

  for (int c = 0; c < lv->cup_count; c++) {
    Cup *cup = &lv->cups[c];
    int cx = cup->x, cy = cup->y, cw = cup->w, ch = cup->h;

    /* Left wall */
    for (int py = cy; py < cy + ch; py++)
      if (cx >= 0 && cx < CANVAS_W && py >= 0 && py < CANVAS_H)
        lb->pixels[py * CANVAS_W + cx] = 1;

    /* Right wall */
    for (int py = cy; py < cy + ch; py++) {
      int rx = cx + cw - 1;
      if (rx >= 0 && rx < CANVAS_W && py >= 0 && py < CANVAS_H)
        lb->pixels[py * CANVAS_W + rx] = 1;
    }

    /* Bottom wall */
    for (int px = cx; px < cx + cw; px++) {
      int by = cy + ch - 1;
      if (px >= 0 && px < CANVAS_W && by >= 0 && by < CANVAS_H)
        lb->pixels[by * CANVAS_W + px] = 1;
    }
  }
}
```

Also update `spawn_grains` to use the level's emitters instead of a hardcoded position:

```c
/* src/game.c  —  spawn_grains: use level emitters */
static void spawn_grains(GameState *state, float dt) {
  LevelDef  *lv = &state->level;
  GrainPool *p  = &state->grains;

  for (int e = 0; e < lv->emitter_count; e++) {
    Emitter *em = &lv->emitters[e];
    em->spawn_timer += dt;
    float interval = 1.0f / (float)em->grains_per_second;

    while (em->spawn_timer >= interval) {
      em->spawn_timer -= interval;
      int slot = grain_alloc(p);
      if (slot < 0) break;

      /* Small spread so grains don't all pile on the exact same column */
      int spread = (rand() % 7) - 3;

      p->active[slot] = 1;
      p->color[slot]  = GRAIN_WHITE;
      p->still[slot]  = 0;
      p->tpcd[slot]   = 0;
      p->x[slot]      = (float)(em->x + spread);
      p->y[slot]      = (float)em->y;
      p->vx[slot]     = 0.0f;
      p->vy[slot]     = (state->gravity_sign > 0) ? 40.0f : -40.0f;
    }
  }
}
```

---

## Step 4: Add `LevelDef level` to `GameState`

```c
/* src/game.h  —  add to GameState */
typedef struct {
  int should_quit;
  GAME_PHASE phase;
  float      phase_timer;
  int        current_level;
  int        unlocked_count;
  LevelDef   level;          /* ← live copy of the active level */
  GrainPool  grains;
  LineBitmap lines;
  int        gravity_sign;
  int        title_hover;
  int        reset_hover;
  int        gravity_hover;
} GameState;
```

---

## Step 5: Wire `level_load` into the state machine

Update `update_title` to load the clicked level:

```c
/* src/game.c  —  update_title: layout constants + click logic */

/* These constants are shared between update_title and render_title
 * so hover detection always matches what's drawn on screen. */
#define TITLE_BTN_W   70
#define TITLE_BTN_H   40
#define TITLE_BTN_GAP  8
#define TITLE_COLS     6
#define TITLE_GRID_X  \
  ((CANVAS_W - (TITLE_COLS * (TITLE_BTN_W + TITLE_BTN_GAP) - TITLE_BTN_GAP)) / 2)
#define TITLE_GRID_Y  160

static void update_title(GameState *state, GameInput *input) {
  int mx = input->mouse.x;
  int my = input->mouse.y;
  state->title_hover = -1;

  for (int i = 0; i < TOTAL_LEVELS; i++) {
    int col = i % TITLE_COLS, row = i / TITLE_COLS;
    int bx = TITLE_GRID_X + col * (TITLE_BTN_W + TITLE_BTN_GAP);
    int by = TITLE_GRID_Y + row * (TITLE_BTN_H + TITLE_BTN_GAP);

    if (mx >= bx && mx < bx + TITLE_BTN_W &&
        my >= by && my < by + TITLE_BTN_H) {
      state->title_hover = i;

      if (BUTTON_PRESSED(input->mouse.left) && i < state->unlocked_count) {
        level_load(state, i);
        change_phase(state, PHASE_PLAYING);
      }
    }
  }

  if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;
}
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:** The title screen shows a 6-column grid of 30 level buttons.  Level 1 is clickable; the rest are locked (greyed out — added in Lesson 13).  Clicking Level 1 starts the simulation with the emitter pouring from `(320, 30)` and a cup at the bottom center.

---

## Exercises

1. Add Level 3 with two emitters, two colored cups (red and green), and a filter zone in the middle that turns grains red on the left half and green on the right.
2. Change `required_count` for Level 0 to 5 and verify the level completes quickly.
3. What happens if `stamp_cups` and `stamp_obstacles` are called in the wrong order?  (Hint: try calling `stamp_cups` first then placing an obstacle that overlaps a cup wall.)

---

## What's next

In Lesson 10 we add win detection — checking cup fill percentages — and the
`SOUND_CUP_FILL` / `SOUND_LEVEL_COMPLETE` audio triggers that make completing
a level feel rewarding.
