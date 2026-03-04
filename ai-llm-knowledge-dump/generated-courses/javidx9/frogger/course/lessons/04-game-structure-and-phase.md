# Lesson 04: Game Structure + GAME_PHASE

## What we're building

The moving green rectangle from Lesson 03 becomes a proper frog sprite
placeholder (a green square) positioned at tile `(8, 9)`.  We introduce
`GameState`, the `GAME_PHASE` enum, and the `game_init` / `game_update` /
`game_render` split — the same three-function architecture used in every
course in this series.

The frog hops with arrow keys.  When it reaches the top row the phase switches
to `PHASE_WIN`; pressing R restarts.  A debug string in the corner shows the
current phase so we can see the state machine working.

---

## What you'll learn

- `GameState` — all game data in one struct, no global variables
- `GAME_PHASE` enum — replacing bare `int dead` with an explicit state machine
- `game_init` / `game_update` / `game_render` — the clean separation of concerns
- `memset(&state, 0, sizeof(state))` — zeroing a struct safely
- Why game code must never call X11 / Raylib APIs
- The `JUST_PRESSED` pattern from Lesson 03 applied to frog movement
- Preserving `best_score` across a `memset`-based reset

---

## Prerequisites

- Lessons 01–03: window, drawing primitives, input double-buffer

---

## Concepts

### 1. `GAME_PHASE` — Explicit State Machine

The reference Frogger source uses a bare `int dead` field.  That works for two
states but gets awkward fast: what does `dead==2` mean?  Can you be dead AND
winning simultaneously?

We use a `typedef enum` — the same pattern used in Tetris, Snake, and Asteroids:

```c
typedef enum {
    PHASE_PLAYING = 0,
    PHASE_DEAD,
    PHASE_WIN
} GAME_PHASE;
```

```c
// JS equivalent:
// type GamePhase = 'playing' | 'dead' | 'win';
// let phase: GamePhase = 'playing';
```

> **Course Note:** The reference uses `int dead`.  The course replaces it with
> `GAME_PHASE` to match the Tetris/Snake/Asteroids naming convention, to prevent
> category errors (`if (dead && win)` is impossible by construction), and to
> make adding new phases trivial (e.g., `PHASE_START_SCREEN` in Lesson 12).

The enum value is stored in `GameState.phase`.  `game_update` writes it;
`game_render` reads it for conditional rendering (death flash, win overlay).

Dispatch uses `switch`:

```c
switch (state->phase) {
    case PHASE_DEAD:
        /* handle death countdown */
        break;
    case PHASE_WIN:
        /* wait for restart */
        break;
    case PHASE_PLAYING:
        /* normal gameplay */
        break;
}
```

```js
// JS equivalent:
// switch (this.phase) {
//   case 'dead':    this.handleDead(dt);    break;
//   case 'win':     this.handleWin(dt);     break;
//   case 'playing': this.handlePlaying(dt); break;
// }
```

### 2. `GameState` — All Game Data in One Struct

No global variables for game data.  Everything lives in `GameState`, allocated
on the stack in `main()` and passed by pointer:

```c
typedef struct {
    float      frog_x;           /* frog tile X position (float for river drift) */
    float      frog_y;           /* frog tile Y position                         */
    float      time;             /* accumulated game time (for lane_scroll)       */
    GAME_PHASE phase;
    int        homes_reached;
    float      dead_timer;       /* death flash countdown                        */
    int        score;
    int        lives;
    int        best_score;       /* preserved across game_init() reset           */
} GameState;
```

```c
// JS analogy:
// class FroggerGame {
//   frogX = 8; frogY = 9;
//   time = 0; phase = 'playing';
//   homesReached = 0; deadTimer = 0;
//   score = 0; lives = 3; bestScore = 0;
// }
```

**Why not global?**  Globals make testing and restarting hard.  With everything
in a struct you can reset the entire game state with one `memset` + a few
preserved fields (like `best_score`):

```c
void game_init(GameState *state) {
    int best = state->best_score;   /* preserve across reset */
    memset(state, 0, sizeof(GameState));
    state->best_score = best;

    state->frog_x = 8.0f;
    state->frog_y = 9.0f;
    state->phase  = PHASE_PLAYING;
    state->lives  = 3;
}
```

```js
// JS equivalent:
// reset() {
//   const best = this.bestScore;
//   Object.assign(this, new FroggerGame());  // reset all fields
//   this.bestScore = best;
// }
```

### 3. The Three-Function Split

```
game_init(state)               ← called once at startup; load data
game_update(state, input, dt)  ← called each frame; pure logic
game_render(bb, state)         ← called each frame; pure drawing
```

The key rule: **game code calls no OS or library APIs**.  `game.c` includes no
`X11/Xlib.h`, no `raylib.h`.  The only types it uses are defined in `game.h`
and `utils/backbuffer.h`.

This means the game compiles and runs identically on both backends — and could
be unit-tested without a window at all.

```c
// game_update: reads GameInput, writes GameState
// game_render: reads GameState, writes Backbuffer
// Never:       game.c calls XOpenDisplay() or InitWindow()
```

```js
// The same separation works in JS:
// function update(state, input, dt) { /* pure logic */  }
// function render(ctx, state)        { /* pure drawing */ }
// // Neither calls document.createElement or fetch()
```

### 4. Naming Convention

> **Course Note:** The reference Frogger source names the functions
> `frogger_init`, `frogger_tick`, and `frogger_render`.  The course renames
> them `game_init`, `game_update`, and `game_render` — matching the convention
> from the Tetris/Snake/Asteroids courses.  This consistency means students can
> navigate all four game sources without constantly context-switching the naming
> in their heads.  `game_update` is preferred over `game_tick` because it
> describes what the function does (update state) rather than when it runs (tick).

---

## Step 1 — Minimal `src/game.h` (Lesson 04 stage)

```c
/* game.h — Frogger Game Types (Lesson 04: GameState + GAME_PHASE) */
#ifndef FROGGER_GAME_H
#define FROGGER_GAME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils/backbuffer.h"

/* ── Debug Utilities ─────────────────────────────────────────────────── */
#ifdef DEBUG
  #define DEBUG_TRAP()  __builtin_trap()
  #define ASSERT(cond, msg) \
      do { if (!(cond)) { \
          fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d\n", \
                  (msg), __FILE__, __LINE__); \
          DEBUG_TRAP(); \
      } } while (0)
#else
  #define DEBUG_TRAP()        ((void)0)
  #define ASSERT(cond, msg)   ((void)0)
#endif

/* ── Input (from platform.h; re-declared here for game code) ─────────── */
#define BUTTON_COUNT 4
typedef struct { int ended_down; int half_transition_count; } GameButtonState;
typedef struct {
    union {
        GameButtonState buttons[BUTTON_COUNT];
        struct {
            GameButtonState move_up, move_down, move_left, move_right;
        };
    };
    int quit;
    int restart;
} GameInput;

/* ── GAME_PHASE ─────────────────────────────────────────────────────────
   Course Note: reference uses "int dead".  We use a typed enum to prevent
   category errors and to match the Tetris/Snake/Asteroids convention.    */
typedef enum {
    PHASE_PLAYING = 0,
    PHASE_DEAD,
    PHASE_WIN
} GAME_PHASE;

/* ── GameState ───────────────────────────────────────────────────────── */
typedef struct {
    float      frog_x;       /* tile position (float for sub-tile river drift) */
    float      frog_y;
    float      time;         /* accumulated seconds (for lane_scroll in Lesson 06) */
    GAME_PHASE phase;
    int        homes_reached;
    float      dead_timer;
    int        score;
    int        lives;
    int        best_score;   /* preserved across game_init() memset */
} GameState;

/* ── Public API ──────────────────────────────────────────────────────── */

/* Reset transition counts; copy ended_down from old frame.              */
void prepare_input_frame(GameInput *old_input, GameInput *current_input);

/* Zero GameState, preserve best_score, place frog at (8,9), lives=3.   */
void game_init(GameState *state);

/* Pure game logic — no drawing, no platform calls.                      */
void game_update(GameState *state, const GameInput *input, float dt);

/* Write full frame into bb — no platform calls.                         */
void game_render(Backbuffer *bb, const GameState *state);

#endif /* FROGGER_GAME_H */
```

---

## Step 2 — Minimal `src/game.c` (Lesson 04 stage)

```c
/* game.c — Frogger Game Logic + Rendering (Lesson 04) */
#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "utils/draw-shapes.h"

/* ── prepare_input_frame ────────────────────────────────────────────────── */
void prepare_input_frame(GameInput *old_input, GameInput *current_input) {
    int btn;
    for (btn = 0; btn < BUTTON_COUNT; btn++) {
        current_input->buttons[btn].ended_down =
            old_input->buttons[btn].ended_down;
        current_input->buttons[btn].half_transition_count = 0;
    }
    current_input->quit    = 0;
    current_input->restart = 0;
}

/* ── game_init ──────────────────────────────────────────────────────────── */
void game_init(GameState *state) {
    int best = state->best_score;   /* preserve best score across restart */
    memset(state, 0, sizeof(GameState));
    state->best_score = best;

    state->frog_x = 8.0f;   /* centre of the bottom safe strip (tile 8, row 9) */
    state->frog_y = 9.0f;
    state->phase  = PHASE_PLAYING;
    state->lives  = 3;
}

/* ── game_update ────────────────────────────────────────────────────────── */
void game_update(GameState *state, const GameInput *input, float dt) {
    /* Cap dt to avoid huge jumps when the window is minimised or the
       debugger pauses execution for several seconds.                     */
    if (dt > 0.1f) dt = 0.1f;

    /* Restart: any phase can restart */
    if (input->restart) {
        game_init(state);
        return;
    }

    /* ── PHASE_DEAD: countdown before respawn ──────────────────────── */
    if (state->phase == PHASE_DEAD) {
        state->dead_timer -= dt;
        if (state->dead_timer <= 0.0f) {
            if (state->lives <= 0) {
                state->dead_timer = 0.0f;   /* hold at 0 — wait for restart */
            } else {
                state->frog_x = 8.0f;
                state->frog_y = 9.0f;
                state->phase  = PHASE_PLAYING;
            }
        }
        return;
    }

    /* ── PHASE_WIN: just wait for restart ──────────────────────────── */
    if (state->phase == PHASE_WIN) {
        return;
    }

    /* ── PHASE_PLAYING ──────────────────────────────────────────────── */
    state->time += dt;

    /* "Just pressed" = first frame of a press (not auto-repeat).
       ended_down=1 AND half_transition_count>0.
       JS: event.type==='keydown' && !event.repeat                       */
#define JUST_PRESSED(btn) ((btn).ended_down && (btn).half_transition_count > 0)

    if (JUST_PRESSED(input->move_up)    && state->frog_y > 0.0f) {
        state->frog_y -= 1.0f;
        state->score  += 10;
    }
    if (JUST_PRESSED(input->move_down)  && state->frog_y < 9.0f)
        state->frog_y += 1.0f;
    if (JUST_PRESSED(input->move_left)  && state->frog_x > 0.0f)
        state->frog_x -= 1.0f;
    if (JUST_PRESSED(input->move_right) && state->frog_x < 15.0f)
        state->frog_x += 1.0f;

#undef JUST_PRESSED

    /* Simple win condition (lesson 04 only): reach the top row           */
    if ((int)state->frog_y == 0) {
        state->homes_reached++;
        if (state->score > state->best_score)
            state->best_score = state->score;
        state->phase = PHASE_WIN;
    }

    /* Simple death: fall off left/right edge                             */
    if (state->frog_x < 0.0f || state->frog_x > 15.0f) {
        state->lives--;
        state->phase      = PHASE_DEAD;
        state->dead_timer = 0.5f;
    }
}

/* ── game_render ────────────────────────────────────────────────────────── */
/* Helper: draw a small debug string using simple coloured squares.
   Full font arrives in Lesson 10; this is a placeholder.                */
static void debug_bar(Backbuffer *bb, int y, uint32_t color, const char *label) {
    (void)label;  /* no font yet — just draw a coloured bar                */
    draw_rect(bb, 0, y, bb->width, 8, color);
}

void game_render(Backbuffer *bb, const GameState *state) {
    /* 1. Clear to black */
    draw_rect(bb, 0, 0, bb->width, bb->height, COLOR_BLACK);

    /* 2. Draw a simple 10-row lane background (coloured bands) */
    /* River rows 1–3 = blue, pavement rows 4,9 = grey,
       road rows 5–8 = dark grey, home row 0 = dark green               */
    static const uint32_t lane_colors[10] = {
        GAME_RGBA( 0, 80, 0, 255),   /* 0 home    */
        GAME_RGBA( 0, 0, 100, 255),  /* 1 river   */
        GAME_RGBA( 0, 0, 120, 255),  /* 2 river   */
        GAME_RGBA( 0, 0, 110, 255),  /* 3 river   */
        GAME_RGBA(60, 60, 60, 255),  /* 4 pavement*/
        GAME_RGBA(40, 40, 40, 255),  /* 5 road    */
        GAME_RGBA(40, 40, 40, 255),  /* 6 road    */
        GAME_RGBA(40, 40, 40, 255),  /* 7 road    */
        GAME_RGBA(40, 40, 40, 255),  /* 8 road    */
        GAME_RGBA(60, 60, 60, 255),  /* 9 pavement*/
    };
    {
        int row;
        for (row = 0; row < 10; row++) {
            /* Each lane is 8 tiles tall = 64 pixels */
            draw_rect(bb, 0, row * 64, bb->width, 64, lane_colors[row]);
        }
    }

    /* 3. Draw frog (green square placeholder, flashing on PHASE_DEAD) */
    {
        int show_frog = 1;
        if (state->phase == PHASE_DEAD) {
            /* Flash: alternate show/hide at 20 Hz */
            int flash = (int)(state->dead_timer / 0.05f);
            show_frog = (flash % 2 == 0);
        }
        if (show_frog) {
            int fx = (int)(state->frog_x * 64.0f);  /* tile → pixel */
            int fy = (int)(state->frog_y * 64.0f);
            draw_rect(bb, fx + 8, fy + 8, 48, 48,
                      GAME_RGBA(0, 200, 0, 255));    /* green body  */
            draw_rect(bb, fx + 16, fy + 4, 8, 8,
                      GAME_RGBA(0, 220, 0, 255));    /* left eye    */
            draw_rect(bb, fx + 40, fy + 4, 8, 8,
                      GAME_RGBA(0, 220, 0, 255));    /* right eye   */
        }
    }

    /* 4. Phase indicator bar at the bottom */
    {
        uint32_t phase_color;
        switch (state->phase) {
            case PHASE_PLAYING: phase_color = COLOR_GREEN;  break;
            case PHASE_DEAD:    phase_color = COLOR_RED;    break;
            case PHASE_WIN:     phase_color = COLOR_YELLOW; break;
            default:            phase_color = COLOR_WHITE;  break;
        }
        /* A coloured bar at the very bottom shows current phase */
        draw_rect(bb, 0, bb->height - 16, bb->width, 16, phase_color);

        /* Score indicator: one white block per 10 points */
        {
            int blocks = state->score / 10;
            int i;
            for (i = 0; i < blocks && i < 60; i++)
                draw_rect(bb, 4 + i * 16, bb->height - 14, 12, 12, COLOR_WHITE);
        }

        /* Lives indicator: one cyan block per life */
        {
            int i;
            for (i = 0; i < state->lives; i++)
                draw_rect(bb, bb->width - 20 - i * 20, bb->height - 14,
                          16, 12, COLOR_CYAN);
        }
    }

    /* 5. Win overlay */
    if (state->phase == PHASE_WIN) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);
        /* Yellow cross in the centre — font arrives in Lesson 10 */
        draw_rect(bb, bb->width/2 - 4, bb->height/2 - 32, 8, 64, COLOR_YELLOW);
        draw_rect(bb, bb->width/2 - 32, bb->height/2 - 4, 64, 8, COLOR_YELLOW);
    }

    /* 6. Game-over overlay */
    if (state->phase == PHASE_DEAD && state->lives <= 0) {
        draw_rect_blend(bb, 0, 0, bb->width, bb->height, COLOR_OVERLAY_DIM);
        draw_rect(bb, bb->width/2 - 32, bb->height/2 - 4, 64, 8, COLOR_RED);
    }
}
```

---

## Step 3 — Updated `src/platform.h` (Lesson 04 stage)

Platform.h now defers to `game.h` for `GameInput` and `GameButtonState` — the
game types are the canonical definitions:

```c
/* platform.h — Platform API Contract (Lesson 04) */
#ifndef FROGGER_PLATFORM_H
#define FROGGER_PLATFORM_H

#include "game.h"   /* Backbuffer, GameInput, GameButtonState */

typedef struct {
    const char *title;
    int         width;
    int         height;
    Backbuffer  backbuffer;
    int         is_running;
} PlatformGameProps;

int    platform_init(PlatformGameProps *props);
double platform_get_time(void);
void   platform_get_input(GameInput *input, PlatformGameProps *props);
void   platform_shutdown(void);

static inline void platform_swap_input_buffers(GameInput *a, GameInput *b) {
    GameInput tmp = *a; *a = *b; *b = tmp;
}

#endif /* FROGGER_PLATFORM_H */
```

---

## Step 4 — Updated `src/main_raylib.c` (Lesson 04 stage)

The main loop now calls `game_init`, `game_update`, and `game_render`:

```c
/* main_raylib.c — Raylib Backend (Lesson 04: game structure) */
#include <string.h>
#include <stdint.h>
#include "raylib.h"
#include "platform.h"

static Texture2D g_texture;
static uint32_t  g_pixel_buf[SCREEN_W * SCREEN_H];

int platform_init(PlatformGameProps *props) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(props->width, props->height, props->title);
    SetTargetFPS(60);
    Image img = { .data=g_pixel_buf, .width=SCREEN_W, .height=SCREEN_H,
                  .mipmaps=1, .format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    g_texture = LoadTextureFromImage(img);
    SetTextureFilter(g_texture, TEXTURE_FILTER_POINT);
    props->backbuffer.pixels = g_pixel_buf;
    props->backbuffer.width  = SCREEN_W;
    props->backbuffer.height = SCREEN_H;
    props->backbuffer.pitch  = SCREEN_W * 4;
    props->is_running = 1;
    return 1;
}

double platform_get_time(void) { return GetTime(); }

void platform_get_input(GameInput *input, PlatformGameProps *props) {
    #define MAP_KEY(BTN, K1, K2) \
        do { int held = IsKeyDown(K1)||IsKeyDown(K2); UPDATE_BUTTON(BTN,held); } while(0)
    MAP_KEY(input->move_up,    KEY_UP, KEY_W);
    MAP_KEY(input->move_down,  KEY_DOWN, KEY_S);
    MAP_KEY(input->move_left,  KEY_LEFT, KEY_A);
    MAP_KEY(input->move_right, KEY_RIGHT, KEY_D);
    #undef MAP_KEY
    if (IsKeyPressed(KEY_R)) input->restart = 1;
    if (WindowShouldClose()||IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_Q))
        { input->quit=1; props->is_running=0; }
}

void platform_shutdown(void) { UnloadTexture(g_texture); CloseWindow(); }

int main(void) {
    PlatformGameProps props;
    memset(&props, 0, sizeof(props));
    props.title = "Frogger"; props.width = SCREEN_W; props.height = SCREEN_H;
    if (!platform_init(&props)) return 1;

    /* Game state on the stack — not heap, not global */
    GameState state;
    memset(&state, 0, sizeof(state));
    game_init(&state);

    GameInput inputs[2]; memset(inputs, 0, sizeof(inputs));

    while (!WindowShouldClose() && props.is_running) {
        float dt = GetFrameTime();
        if (dt > 0.066f) dt = 0.066f;

        platform_swap_input_buffers(&inputs[0], &inputs[1]);
        prepare_input_frame(&inputs[0], &inputs[1]);
        platform_get_input(&inputs[1], &props);
        if (inputs[1].quit) break;

        game_update(&state, &inputs[1], dt);
        game_render(&props.backbuffer, &state);

        UpdateTexture(g_texture, g_pixel_buf);
        {
            int sw=GetScreenWidth(), sh=GetScreenHeight();
            float sc=((float)sw/SCREEN_W<(float)sh/SCREEN_H)
                     ?(float)sw/SCREEN_W:(float)sh/SCREEN_H;
            int vw=(int)(SCREEN_W*sc), vh=(int)(SCREEN_H*sc);
            int vx=(sw-vw)/2, vy=(sh-vh)/2;
            BeginDrawing(); ClearBackground(BLACK);
            DrawTexturePro(g_texture,
                (Rectangle){0,0,SCREEN_W,SCREEN_H},
                (Rectangle){(float)vx,(float)vy,(float)vw,(float)vh},
                (Vector2){0,0}, 0, WHITE);
            EndDrawing();
        }
    }
    platform_shutdown();
    return 0;
}
```

---

## Updated `build-dev.sh` (Lesson 04)

Add `src/game.c` to the source list:

```bash
#!/bin/bash
set -e; mkdir -p build
BACKEND="raylib"; RUN_AFTER_BUILD=false; DEBUG_BUILD=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*) BACKEND="${1#*=}" ;;
        -r) RUN_AFTER_BUILD=true ;;
        -d) DEBUG_BUILD=true ;;
        --help) echo "Usage: $0 [--backend=x11|raylib] [-r] [-d]"; exit 0 ;;
        *) echo "Unknown: $1" >&2; exit 1 ;;
    esac; shift
done
# Add game.c here; draw-shapes.c added in Lesson 02
SOURCES="src/game.c src/utils/draw-shapes.c src/main_${BACKEND}.c"
LIBS="-lm"; ASSETS_FLAGS="-DASSETS_DIR=\"$(pwd)/assets\""
case "$BACKEND" in
    x11)    LIBS="$LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound" ;;
    raylib) LIBS="$LIBS -lraylib -lpthread -ldl" ;;
    *) echo "Unknown backend: $BACKEND" >&2; exit 1 ;;
esac
FLAGS="-Wall -Wextra -g -O0 $ASSETS_FLAGS"
[[ "$DEBUG_BUILD" == true ]] && FLAGS="$FLAGS -fsanitize=address,undefined -DDEBUG"
OUTPUT="./build/frogger"
clang $FLAGS -o "$OUTPUT" $SOURCES $LIBS
echo "Build OK: $OUTPUT"
[[ "$RUN_AFTER_BUILD" == true ]] && exec "$OUTPUT"
```

---

## Build and Run

```bash
./build-dev.sh -r            # Raylib
./build-dev.sh --backend=x11 -r   # X11
```

---

## Expected Result

- Ten coloured horizontal bands fill the screen (black top → grey bottom).
- A green rectangle with two lighter "eyes" sits at column 8, row 9.
- **↑ W** — frog hops up one tile; the score bar at the bottom grows.
- **↓ ↑ ← →** — frog hops in all four directions.
- Moving off the right/left edge: frog disappears, the status bar turns **red**
  for 0.5 s then the frog reappears at `(8, 9)`.  Life count decreases.
- Reaching the top row: status bar turns **yellow**, all movement stops.
  A yellow cross appears.  Press **R** to restart.
- With 0 lives and in PHASE_DEAD: a dim overlay + red bar.  Press **R**.

---

## Understanding the Separation of Concerns

```
main()
  ├── platform_init()         // opens window
  ├── game_init(&state)       // places frog, sets lives=3
  └── game loop:
        ├── platform_get_input()    // reads keyboard → GameInput
        ├── game_update(&state, input, dt)   // moves frog, checks win
        └── game_render(&bb, &state)         // draws to pixel buffer
```

`game_update` is called before `game_render` — always.  This means `game_render`
always draws the state that was just computed; there is no "draw one frame stale"
bug.

Both `game_update` and `game_render` are **pure** in the sense that they call no
OS APIs.  You could write unit tests for `game_update` that run without a window.

---

## Exercises

1. Add `PHASE_START_SCREEN` to `GAME_PHASE`.  Show a "PRESS ENTER TO START"
   message in the render function for that phase.  Transition to `PHASE_PLAYING`
   when Enter is pressed.
2. Add a `score` field to `GameState` and increment it by 10 for each upward hop.
   Draw one white square per 10 points at the bottom.
3. Replace `game_init` in the restart handler with a raw `memset` — observe that
   `best_score` is lost.  Then restore the preserved-fields pattern.
4. Add a `max_lives = 5` constant and start with `state->lives = max_lives`
   in `game_init`.  Add five cyan blocks to the lives indicator.

---

## What's next

**Lesson 05** introduces the lane data system using **Data-Oriented Design**:
two separate arrays (`lane_speeds[]` and `lane_patterns[]`) replace a single
`Lane` struct array.  All 10 lanes appear as coloured rectangles — no scrolling
yet, but the tile-type colour coding is in place.
