# Lesson 08: The State Machine — GAME_PHASE and change_phase()

## What we're building

Introduce the `GAME_PHASE` enum and a `change_phase()` function that centralises
all transitions between game states.  The game now has four phases: title screen,
playing, level-complete celebration, and freeplay (post-credits sandbox).

## What you'll learn

- Finite State Machines in C — enum + switch pattern
- Why one `change_phase()` function beats scattered boolean flags
- `phase_timer` — per-phase elapsed seconds for timed transitions
- How `game_update` dispatches to per-phase update functions
- How `game_render` dispatches to per-phase render functions
- Escape handling: playing → title, title → quit

## Prerequisites

- Lesson 07 complete (settle + bake working)

---

## Step 1: `GAME_PHASE` enum in `game.h`

```c
/* src/game.h  —  GAME_PHASE */

/* JS analogy: like React's useReducer "action type" enum.
 * Each value names a distinct mutually-exclusive screen/mode.
 * switch(state.phase) replaces nested if/else chains. */
typedef enum {
  PHASE_TITLE,           /* title screen + level select grid */
  PHASE_PLAYING,         /* active puzzle: simulation + drawing */
  PHASE_LEVEL_COMPLETE,  /* brief celebration before advancing */
  PHASE_FREEPLAY,        /* sandbox after completing all levels */
  PHASE_COUNT            /* not a valid phase; used for array sizing */
} GAME_PHASE;
```

---

## Step 2: Expand `GameState`

```c
/* src/game.h  —  updated GameState */
typedef struct {
  int should_quit;

  /* State machine */
  GAME_PHASE phase;
  float      phase_timer;  /* seconds elapsed in the current phase */

  /* Level progress (Lesson 09 fills in the level details) */
  int current_level;
  int unlocked_count;

  /* Simulation */
  GrainPool  grains;
  LineBitmap lines;
  int gravity_sign;     /* +1 = down (normal), -1 = up (flipped) */

  /* Input-driven UI hover state — set in game_update, read in game_render */
  int title_hover;      /* which level button is hovered (-1 = none) */
  int reset_hover;
  int gravity_hover;

  /* Audio (added in Lesson 15) */
  /* GameAudioState audio; */
} GameState;
```

---

## Step 3: `change_phase()` — the single transition point

```c
/* src/game.c  —  change_phase (forward-declared at top of file) */
static void change_phase(GameState *state, GAME_PHASE next);

/* change_phase is called from:
 *   game_init       → initial phase (TITLE)
 *   update_title    → when a level is clicked (PLAYING)
 *   update_playing  → when Escape pressed (TITLE) or win detected (LEVEL_COMPLETE)
 *   update_level_complete → advance to next level or FREEPLAY
 *
 * Having a SINGLE function for all transitions means:
 *   - Audio triggers live in ONE place (added in Lesson 15)
 *   - Reset logic (phase_timer = 0) is always correct
 *   - Adding a new phase only requires extending this switch
 */
static void change_phase(GameState *state, GAME_PHASE next) {
  state->phase       = next;
  state->phase_timer = 0.0f;

  /* Audio triggers will be added here in Lesson 15. */
}
```

---

## Step 4: `game_init` starts in PHASE_TITLE

```c
/* src/game.c  —  game_init */
void game_init(GameState *state, GameBackbuffer *backbuffer) {
  memset(state, 0, sizeof(*state));

  backbuffer->width  = CANVAS_W;
  backbuffer->height = CANVAS_H;
  backbuffer->pitch  = CANVAS_W * 4;

  state->unlocked_count = 1;  /* only level 1 accessible at start */
  state->gravity_sign   = 1;  /* normal downward gravity          */
  state->title_hover    = -1; /* nothing hovered yet              */

  change_phase(state, PHASE_TITLE);
}
```

---

## Step 5: Per-phase update functions

```c
/* src/game.c  —  update functions (one per phase) */

/* Forward declarations — defined later in file */
static void update_title(GameState *state, GameInput *input);
static void update_playing(GameState *state, GameInput *input, float dt);
static void update_level_complete(GameState *state, GameInput *input, float dt);
static void update_freeplay(GameState *state, GameInput *input, float dt);

void game_update(GameState *state, GameInput *input, float dt) {
  if (dt > 0.1f) dt = 0.1f;

  switch (state->phase) {
  case PHASE_TITLE:
    update_title(state, input);
    break;
  case PHASE_PLAYING:
    update_playing(state, input, dt);
    break;
  case PHASE_LEVEL_COMPLETE:
    update_level_complete(state, input, dt);
    break;
  case PHASE_FREEPLAY:
    update_freeplay(state, input, dt);
    break;
  case PHASE_COUNT:
    break; /* satisfies -Wswitch; never reached */
  }
}

/* Title screen — no simulation; just wait for a level click (Lesson 09). */
static void update_title(GameState *state, GameInput *input) {
  state->title_hover = -1;

  /* For now: press any key to start level 0 */
  if (BUTTON_PRESSED(input->enter) || BUTTON_PRESSED(input->mouse.left)) {
    /* level_load(state, 0); */  /* added in Lesson 09 */
    change_phase(state, PHASE_PLAYING);
  }

  if (BUTTON_PRESSED(input->escape))
    state->should_quit = 1;
}

/* Playing — simulation + drawing + win detection (Lessons 09–12). */
static void update_playing(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  /* Escape → back to title */
  if (BUTTON_PRESSED(input->escape)) {
    change_phase(state, PHASE_TITLE);
    return;
  }

  /* Mouse drawing */
  if (input->mouse.left.ended_down) {
    draw_brush_line(&state->lines,
                    input->mouse.prev_x, input->mouse.prev_y,
                    input->mouse.x, input->mouse.y, BRUSH_RADIUS);
  }

  spawn_grains(state, dt);
  update_grains(state, dt);

  /* Win detection added in Lesson 10 */
}

/* Level-complete: brief pause then advance. */
static void update_level_complete(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  int advance = (state->phase_timer > 2.0f)  /* auto-advance after 2 s */
             || BUTTON_PRESSED(input->mouse.left)
             || BUTTON_PRESSED(input->enter);

  if (advance) {
    /* Go back to title for now; Lesson 09 adds real level progression. */
    change_phase(state, PHASE_TITLE);
  }
}

/* Freeplay — same as playing but never triggers win detection. */
static void update_freeplay(GameState *state, GameInput *input, float dt) {
  state->phase_timer += dt;

  if (BUTTON_PRESSED(input->escape))
    change_phase(state, PHASE_TITLE);

  if (input->mouse.left.ended_down) {
    draw_brush_line(&state->lines,
                    input->mouse.prev_x, input->mouse.prev_y,
                    input->mouse.x, input->mouse.y, BRUSH_RADIUS);
  }

  spawn_grains(state, dt);
  update_grains(state, dt);
}
```

---

## Step 6: Per-phase render dispatch

```c
/* src/game.c  —  game_render dispatch */

static void render_title(const GameState *state, GameBackbuffer *bb);
static void render_playing(const GameState *state, GameBackbuffer *bb);
static void render_level_complete(const GameState *state, GameBackbuffer *bb);
static void render_freeplay(const GameState *state, GameBackbuffer *bb);

void game_render(const GameState *state, GameBackbuffer *bb) {
  switch (state->phase) {
  case PHASE_TITLE:
    render_title(state, bb);
    break;
  case PHASE_PLAYING:
    render_playing(state, bb);
    break;
  case PHASE_LEVEL_COMPLETE:
    render_level_complete(state, bb);
    break;
  case PHASE_FREEPLAY:
    render_freeplay(state, bb);
    break;
  case PHASE_COUNT:
    break;
  }
}

static void render_title(const GameState *state, GameBackbuffer *bb) {
  (void)state;
  /* Clear to background — full title screen added in Lesson 13. */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++) bb->pixels[i] = COLOR_BG;

  /* Placeholder title text — proper font rendering added in Lesson 13. */
  draw_rect(bb, CANVAS_W/2 - 80, 80, 160, 30, COLOR_BTN_NORMAL);
  draw_rect_outline(bb, CANVAS_W/2 - 80, 80, 160, 30, COLOR_BTN_BORDER);
}

static void render_playing(const GameState *state, GameBackbuffer *bb) {
  /* Clear */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++) bb->pixels[i] = COLOR_BG;

  /* Draw lines + grains (same code as Lesson 06–07) */
  const uint8_t *lp = state->lines.pixels;
  for (int py = 0; py < CANVAS_H; py++) {
    for (int px = 0; px < CANVAS_W; px++) {
      uint8_t v = lp[py * CANVAS_W + px];
      if (!v) continue;
      if (v == 1 || v == 255) draw_pixel(bb, px, py, COLOR_LINE);
      else if (v >= 2 && v < 2 + GRAIN_COLOR_COUNT)
        draw_pixel(bb, px, py, g_grain_colors[v - 2]);
    }
  }

  const GrainPool *p = &state->grains;
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;
    draw_pixel(bb, (int)p->x[i], (int)p->y[i], g_grain_colors[p->color[i]]);
  }
}

static void render_level_complete(const GameState *state, GameBackbuffer *bb) {
  render_playing(state, bb);  /* show the final state underneath */
  /* Semi-transparent green overlay — real text added in Lesson 13. */
  draw_rect_blend(bb, 0, CANVAS_H/2 - 30, CANVAS_W, 60, COLOR_COMPLETE, 180);
}

static void render_freeplay(const GameState *state, GameBackbuffer *bb) {
  render_playing(state, bb);  /* freeplay looks identical to playing */
}
```

---

## State machine diagram

```
game_init()
    └─ change_phase(PHASE_TITLE)
           │
           ▼
      PHASE_TITLE ──[level click]──► PHASE_PLAYING
           │                              │
           └──[Escape]──► quit            ├──[Escape]──────────► PHASE_TITLE
                                          ├──[win detected]────► PHASE_LEVEL_COMPLETE
                                          └──[R or button]────► reload level (stay)
                                                   │
                                                   ▼
                                        PHASE_LEVEL_COMPLETE
                                                   │
                                          [click / 2s timer]
                                                   │
                                     ┌─────────────┴────────────────┐
                              more levels                      all levels done
                                     │                              │
                               PHASE_PLAYING               PHASE_FREEPLAY
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:** On startup you see a placeholder title screen (a rectangle).  Press Enter or click to enter PHASE_PLAYING — the simulation runs.  Press Escape to return to the title screen.

---

## Exercises

1. Add a `PHASE_PAUSED` state.  Wire `P` key to toggle between `PHASE_PLAYING` and `PHASE_PAUSED` without resetting the simulation.
2. Why does `update_level_complete` use `phase_timer > 2.0f` instead of a frame count?  What would break if you used `frame_count > 120`?
3. Change the auto-advance timer to 3 seconds and verify it works at both 30 fps and 120 fps.  (Hint: it should work correctly already — why?)

---

## What's next

In Lesson 09 we build the full level system — `LevelDef`, `Emitter`, `Cup`, and
`levels.c` — so the game actually has something to play.
