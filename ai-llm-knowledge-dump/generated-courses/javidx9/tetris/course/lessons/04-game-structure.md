# Lesson 04: Game Structure

## What we're building

The same moving square from Lesson 03, but now the code is split into three
layers: **`game.h`** (shared types and declarations), **`game.c`** (all game
logic and rendering), and **`main_raylib.c`** (platform glue). Nothing visible
changes on screen — this is a pure architectural refactor. After this lesson
the project structure matches the final Tetris codebase.

## What you'll learn

- `#ifndef` header guards — preventing double-inclusion across multiple files
- Multi-file compilation — why you list every `.c` file on the command line
- `extern` — declaring that a variable is defined in *another* file
- `= {0}` zero-initialisation — why it beats leaving things uninitialised
- `static` at file scope — making a variable or function private to one file
- Forward declarations — telling the compiler a function exists before its body
- The three-layer architecture: game never calls platform APIs

## Prerequisites

Lessons 01–03 — backbuffer, drawing primitives, and input system.

---

## The three-layer architecture

```
┌─────────────────────────────────────────────────────────────┐
│  main_raylib.c  (Platform Layer)                            │
│    • Raylib / OS calls only live here                       │
│    • Translates OS events → GameInput                       │
│    • Calls game_init(), game_update(), game_render()        │
│    • Uploads backbuffer pixels to GPU each frame            │
└──────────────────────────┬──────────────────────────────────┘
                           │  passes: GameInput*, Backbuffer*, float dt
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  game.c  (Game Layer)                                       │
│    • Pure C — no Raylib, no OS headers                      │
│    • game_init()   — initialise GameState                   │
│    • game_update() — advance simulation by one frame        │
│    • game_render() — write pixels into Backbuffer           │
└──────────────────────────┬──────────────────────────────────┘
                           │  uses types from
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  game.h  (Shared Types & Declarations)                      │
│    • GameState, GameInput, GameButtonState, constants       │
│    • Function declarations (prototypes)                     │
│    • Included by both game.c and main_raylib.c              │
└─────────────────────────────────────────────────────────────┘
```

> **Handmade Hero principle:** The game is a DLL / translation unit. It knows
> nothing about the OS, windowing, or audio hardware. It receives data
> (`GameInput`, `Backbuffer`) and writes data back. The platform mediates
> everything else. You could swap `main_raylib.c` for `main_sdl.c` and
> `game.c` would compile unchanged.
>
> JS analogy: think of `game.h` as a TypeScript interface file, `game.c` as
> a pure-logic module, and `main_raylib.c` as the Node/browser adapter that
> calls the module's exports.

---

## Step 1: `game.h` — shared types and declarations

Create `src/game.h`. At this lesson stage `GameState` has only three fields.

```c
/* src/game.h */

/*
 * Header guard — prevents this file from being processed twice in the same
 * translation unit (compilation unit = one .c file + all its #includes).
 *
 * WHY is this needed?
 *   main_raylib.c includes game.h.
 *   game.c        includes game.h.
 *   If main_raylib.c somehow also included game.c (or if two headers included
 *   each other), the struct/typedef definitions would appear twice and the
 *   compiler would error: "redefinition of 'GameState'".
 *
 * HOW it works:
 *   First time processed:
 *     #ifndef GAME_H  → GAME_H not yet defined → enter the block
 *     #define GAME_H  → mark as visited
 *     ... all content ...
 *     #endif
 *
 *   Second time processed (same compilation unit):
 *     #ifndef GAME_H  → GAME_H IS defined → SKIP everything to #endif
 *
 * JS equivalent: ES modules are deduplicated automatically by the module
 * system. C has no module system — this guard IS the deduplication.
 *
 * Naming convention: use the file path, ALL_CAPS, with / and . replaced by _.
 */
#ifndef GAME_H
#define GAME_H

/*
 * Our own utility headers.
 *
 * Because game.h is in src/ and these are in src/utils/, we use a relative
 * path. Any file that includes game.h gets these too (transitive inclusion).
 *
 * This is the same as:
 *   JS: import type { Backbuffer } from './utils/backbuffer';
 *   C:  #include "utils/backbuffer.h"
 */
#include "utils/backbuffer.h"  /* Backbuffer struct + GAME_RGB macros */

/* ── Game-wide constants ─────────────────────────────────────────────────
 *
 * Defined here so both the game layer (game.c) and the platform layer
 * (main_raylib.c) can use them without disagreeing on values.
 *
 * JS: export const FIELD_WIDTH = 12;
 * C:  #define FIELD_WIDTH 12
 *
 * Note: these are NOT variables — they have no address, no type, no storage.
 * The preprocessor replaces every occurrence of FIELD_WIDTH with the literal
 * text "12" before the compiler even starts.
 */
#define FIELD_WIDTH   12   /* columns including left+right walls */
#define FIELD_HEIGHT  18   /* rows    including floor            */
#define CELL_SIZE     30   /* pixels per grid cell               */
#define SIDEBAR_WIDTH 200  /* extra pixels right of the field    */

/* ── Color constants ─────────────────────────────────────────────────────
 *
 * Gathered here so game.c and main_raylib.c share the same palette.
 */
#define COLOR_BLACK     GAME_RGB(0,   0,   0  )
#define COLOR_DARK_BLUE GAME_RGB(10,  10,  40 )
#define COLOR_WHITE     GAME_RGB(255, 255, 255)
#define COLOR_GRAY      GAME_RGB(128, 128, 128)
#define COLOR_RED       GAME_RGB(255, 80,  80 )
#define COLOR_YELLOW    GAME_RGB(255, 255, 0  )

/* ══════════════════════════════════════════════════════════════════════════
 * Input types (moved here from game_input.h)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * In the final game all types live in game.h. We consolidate now.
 */
#define BUTTON_COUNT 4

typedef struct {
  int half_transition_count;
  int ended_down;
} GameButtonState;

typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState move_left;
      GameButtonState move_right;
      GameButtonState move_down;
      GameButtonState action;
    };
  };
} GameInput;

/*
 * UPDATE_BUTTON and prepare_input_frame:
 * declared here, defined/implemented in game.c (for prepare_input_frame) and
 * as a macro expansion everywhere (for UPDATE_BUTTON).
 *
 * UPDATE_BUTTON is a macro so it can be used in both main_raylib.c and game.c
 * without being "defined" in a specific .c file.
 */
#define UPDATE_BUTTON(button, is_down)                             \
  do {                                                             \
    if ((button).ended_down != (is_down)) {                        \
      (button).half_transition_count++;                            \
      (button).ended_down = (is_down);                             \
    }                                                              \
  } while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * GameState — the entire mutable state of the game
 * ══════════════════════════════════════════════════════════════════════════
 *
 * At this lesson stage we only need three fields.
 * Later lessons will add: field grid, current piece, score, timers, audio.
 *
 * Handmade Hero principle: ALL mutable state lives in this struct.
 * No hidden static variables, no singletons, no global state.
 * Passing &state everywhere makes data flow explicit — you can always find
 * where a value was changed.
 *
 * JS equivalent:
 *   // Single source of truth (like a Redux store):
 *   const state: GameState = { playerX: 255, playerY: 245, shouldQuit: false };
 *
 * C:
 *   typedef struct { int player_x; int player_y; int should_quit; } GameState;
 *   GameState game_state = {0};  // zero-init: player_x=0, player_y=0, should_quit=0
 *   game_init(&game_state);      // fill in starting values
 *
 * WHY `= {0}` zero-initialisation?
 *   C local variables are NOT initialised by default — they contain whatever
 *   bytes happened to be on the stack. A struct with 50 fields where one field
 *   is garbage is a subtle, hard-to-find bug.
 *   `= {0}` sets EVERY field (including nested structs, arrays, padding) to
 *   zero. It's the C equivalent of TypeScript's `satisfies` + default values.
 *
 *   JS: every unset property is `undefined`. C: every unset field is garbage.
 */
typedef struct {
  int player_x;    /* top-left x coordinate of the player square */
  int player_y;    /* top-left y coordinate of the player square */
  int should_quit; /* 1 = user requested exit; platform will break the loop */
} GameState;

/* ══════════════════════════════════════════════════════════════════════════
 * Function declarations — implemented in game.c
 * ══════════════════════════════════════════════════════════════════════════
 *
 * These are "forward declarations" (also called "prototypes").
 * The compiler needs to know the signature of each function before it can
 * typecheck calls to it. The actual function BODIES are in game.c.
 *
 * JS/TS equivalent:
 *   // game.d.ts:
 *   export declare function gameInit(state: GameState): void;
 *   export declare function gameUpdate(state: GameState, input: GameInput, dt: number): void;
 *   export declare function gameRender(bb: Backbuffer, state: GameState): void;
 *
 * C: void game_init(GameState *state);
 *    void game_update(GameState *state, GameInput *input, float delta_time);
 *    void game_render(Backbuffer *backbuffer, GameState *state);
 *
 * The function bodies in game.c must exactly match these signatures or the
 * compiler will report a type mismatch error.
 */

/* Reset ALL game state to initial values. Call once at startup. */
void game_init(GameState *state);

/*
 * prepare_input_frame — copy held state from old frame; reset transition counters.
 * Must be called BEFORE the platform reads new key events.
 */
void prepare_input_frame(GameInput *old_input, GameInput *new_input);

/*
 * Advance the game simulation by one frame.
 *   state      — mutable game state (read AND written)
 *   input      — this frame's button states (read only)
 *   delta_time — seconds since the last frame (e.g. 0.016 at 60 fps)
 */
void game_update(GameState *state, GameInput *input, float delta_time);

/*
 * Write the current game state as pixels into `backbuffer`.
 * Pure memory writes — no OS calls, no OpenGL.
 */
void game_render(Backbuffer *backbuffer, GameState *state);

#endif /* GAME_H */
```

> **Why `#ifndef GAME_H / #define GAME_H / #endif`?** Without these guards,
> if two `.c` files both `#include "game.h"`, the preprocessor pastes the
> entire file into each one — that's fine. But if one `.c` file includes a
> header that includes `game.h`, AND also directly includes `game.h`, the
> `typedef struct { ... } GameState;` appears twice in that translation unit
> and the compiler errors. The guard makes the file safe to include any number
> of times.

---

## Step 2: `game.c` — all game logic

Create `src/game.c`. This file implements the functions declared in `game.h`.
It includes no Raylib headers — it is completely platform-independent.

```c
/* src/game.c */

/*
 * Include our own header FIRST. This verifies that game.c's implementations
 * match the declarations in game.h. If a return type differs, the compiler
 * catches it here.
 *
 * JS: like importing from your own module — if you export a function that
 * doesn't match its declared type, TypeScript catches it.
 */
#include "game.h"
#include "utils/draw-shapes.h"  /* draw_rect */

/*
 * #include <stdlib.h> — standard library: malloc, free, abs, etc.
 * #include <string.h> — string/memory: memset, memcpy
 *
 * These are angle-bracket includes: the compiler searches the system
 * include paths. Your own headers use quote includes: "game.h".
 */
#include <stdlib.h>
#include <string.h>

/* ── Constants local to this file ───────────────────────────────────────
 *
 * `static` at file scope means this symbol is PRIVATE to game.c.
 * It won't be exported as a linker symbol — other .c files can't access it.
 *
 * JS equivalent:
 *   // In a module, unexported constants are private:
 *   const SQUARE_SIZE = 50;   // not exported, not visible outside this file
 *
 * C:
 *   static const int SQUARE_SIZE = 50;  // private to game.c
 *   #define SQUARE_SIZE 50              // also common — no storage, no address
 *
 * WHY mark things static?
 *   Large programs have many .c files. If two files both define a non-static
 *   variable `int count = 0;`, the linker errors: "duplicate symbol 'count'".
 *   `static` prevents that by keeping the symbol local to its file.
 */
static const int SQUARE_SIZE = 50;
static const int SPEED       = 200; /* pixels per second */

/* ══════════════════════════════════════════════════════════════════════════
 * game_init — reset everything to a known starting state
 * ══════════════════════════════════════════════════════════════════════════
 *
 * This is like a class constructor in JS:
 *   class Game {
 *     constructor() {
 *       this.playerX = screenWidth / 2;
 *       this.playerY = screenHeight / 2;
 *       this.shouldQuit = false;
 *     }
 *   }
 *
 * In C there is no `this`. We receive a pointer to the struct and fill it in.
 *
 * WHY does the caller zero-init with `= {0}` AND call game_init?
 *   `= {0}` guarantees there are no random-garbage fields.
 *   game_init() then sets the meaningful starting values (e.g. player at centre).
 *   If game_init() is called again later (restart), it resets to the same clean
 *   state regardless of what was in the struct before.
 */
void game_init(GameState *state) {
  /*
   * memset: fill a block of memory with a byte value.
   * memset(state, 0, sizeof(GameState)) sets every byte to 0.
   *
   * JS: Object.assign(state, { playerX: 0, playerY: 0, shouldQuit: 0 });
   * C:  memset(state, 0, sizeof(*state));
   *
   * sizeof(*state) = sizeof(GameState) — the size of the struct in bytes.
   * Using sizeof(*state) instead of sizeof(GameState) means the size is
   * automatically correct even if you later change the type of `state`.
   */
  memset(state, 0, sizeof(*state));

  /* Place the player at the centre of the field */
  int field_pixel_width  = FIELD_WIDTH  * CELL_SIZE;
  int field_pixel_height = FIELD_HEIGHT * CELL_SIZE;
  state->player_x = field_pixel_width  / 2 - SQUARE_SIZE / 2;
  state->player_y = field_pixel_height / 2 - SQUARE_SIZE / 2;
}

/* ══════════════════════════════════════════════════════════════════════════
 * prepare_input_frame — declared in game.h, defined here
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Copies `ended_down` from old_input to new_input for each button, and
 * resets `half_transition_count` to 0.
 *
 * Call BEFORE the platform reads new key events each frame.
 * (See Lesson 03 for the full explanation.)
 */
void prepare_input_frame(GameInput *old_input, GameInput *new_input) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    new_input->buttons[i].ended_down          = old_input->buttons[i].ended_down;
    new_input->buttons[i].half_transition_count = 0;
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * game_update — advance the simulation by one frame
 * ══════════════════════════════════════════════════════════════════════════
 *
 * This is like the `update(dt)` method on a game object in JS:
 *   update(dt: number, input: GameInput): void {
 *     if (input.moveLeft.endedDown)  this.playerX -= SPEED * dt;
 *     if (input.moveRight.endedDown) this.playerX += SPEED * dt;
 *     ...
 *   }
 *
 * Parameters:
 *   state      — pointer to game state (we read AND write it)
 *   input      — pointer to this frame's input (we only READ it)
 *   delta_time — seconds since last frame (float: fractional seconds, e.g. 0.016)
 *
 * Note: `float delta_time` — C has two floating-point types:
 *   float  — 32-bit (6-7 significant digits)
 *   double — 64-bit (15-16 significant digits)
 * For game logic, float is accurate enough and slightly faster on some hardware.
 */
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Check quit condition */
  if (input->action.ended_down && input->action.half_transition_count > 0) {
    /* action button just pressed — could trigger quit in a real game */
  }

  /* ── Movement ────────────────────────────────────────────────────────── */
  if (input->move_left.ended_down)  state->player_x -= (int)(SPEED * delta_time);
  if (input->move_right.ended_down) state->player_x += (int)(SPEED * delta_time);
  if (input->move_down.ended_down)  state->player_y += (int)(SPEED * delta_time);

  /*
   * We handle UP directly from the struct's union. In the final game we'll
   * add a proper `move_up` button; for now we reuse `action` for up movement
   * to keep BUTTON_COUNT at 4 and the union correct.
   * For this demo, up is handled in the platform (see main_raylib.c).
   */

  /* ── Clamp to field boundaries ──────────────────────────────────────── */
  int field_px_width  = FIELD_WIDTH  * CELL_SIZE;
  int field_px_height = FIELD_HEIGHT * CELL_SIZE;

  if (state->player_x < 0)
    state->player_x = 0;
  if (state->player_x > field_px_width - SQUARE_SIZE)
    state->player_x = field_px_width - SQUARE_SIZE;
  if (state->player_y < 0)
    state->player_y = 0;
  if (state->player_y > field_px_height - SQUARE_SIZE)
    state->player_y = field_px_height - SQUARE_SIZE;
}

/* ══════════════════════════════════════════════════════════════════════════
 * game_render — draw the current game state into the backbuffer
 * ══════════════════════════════════════════════════════════════════════════
 *
 * This function ONLY writes to backbuffer->pixels.
 * No Raylib calls, no OS calls, no platform-specific code.
 * It could be tested in isolation by creating a Backbuffer on the heap.
 *
 * JS analogy:
 *   render(ctx: CanvasRenderingContext2D): void {
 *     ctx.fillStyle = '#0a0a28';
 *     ctx.fillRect(0, 0, width, height);
 *     ctx.fillStyle = '#ff5050';
 *     ctx.fillRect(this.playerX, this.playerY, SQUARE_SIZE, SQUARE_SIZE);
 *   }
 *
 * C: writes uint32_t pixel values through draw_rect() calls.
 */
void game_render(Backbuffer *backbuffer, GameState *state) {
  int screen_w = backbuffer->width;
  int screen_h = backbuffer->height;

  /* ── Background ────────────────────────────────────────────────────── */
  draw_rect(backbuffer, 0, 0, screen_w, screen_h, COLOR_DARK_BLUE);

  /* ── Field border ──────────────────────────────────────────────────── */
  int field_w = FIELD_WIDTH  * CELL_SIZE;
  int field_h = FIELD_HEIGHT * CELL_SIZE;
  draw_rect(backbuffer, 0, 0, field_w, 2, COLOR_GRAY);            /* top    */
  draw_rect(backbuffer, 0, field_h-2, field_w, 2, COLOR_GRAY);    /* bottom */
  draw_rect(backbuffer, 0, 0, 2, field_h, COLOR_GRAY);            /* left   */
  draw_rect(backbuffer, field_w-2, 0, 2, field_h, COLOR_GRAY);    /* right  */

  /* ── Player square ─────────────────────────────────────────────────── */
  draw_rect(backbuffer, state->player_x, state->player_y,
            SQUARE_SIZE, SQUARE_SIZE, COLOR_RED);

  /* ── Sidebar area (blank for now) ──────────────────────────────────── */
  draw_rect(backbuffer, field_w, 0, screen_w - field_w, screen_h,
            GAME_RGB(20, 20, 50));
}
```

> **Why `static const int SQUARE_SIZE`?** `static` at file scope makes the
> variable visible only within `game.c`. If two files both defined a non-static
> `int SQUARE_SIZE`, the linker would error "duplicate symbol". `static` avoids
> that by keeping the symbol local. It's similar to not exporting a variable
> from a JS module.

> **Why `memset(state, 0, sizeof(*state))` inside `game_init` even when
> `= {0}` was used at the call site?** `game_init` can be called multiple times
> (e.g. game restart). The caller's `= {0}` only runs once, at declaration time.
> `memset` inside `game_init` guarantees a clean slate every time it's called.

---

## Step 3: `main_raylib.c` — the thin platform layer

Replace `src/main_raylib.c` with the lesson-04 version. Notice how it is now
purely glue code — it contains no game logic and no rendering code.

```c
/* src/main_raylib.c — Lesson 04 complete */

/*
 * The platform layer includes game.h (which brings in backbuffer.h,
 * the types, the constants, and the function declarations).
 * It does NOT include game.c — that's a common beginner mistake.
 * game.c is a separate *compilation unit*; the linker joins them.
 *
 * JS: like `import { gameInit, gameUpdate, gameRender } from './game'`
 *     but game.c is compiled separately and linked at the end.
 */
#include "game.h"

#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

int main(void) {
  /* ── Window size from game constants ──────────────────────────────────
   *
   * FIELD_WIDTH, FIELD_HEIGHT, CELL_SIZE, SIDEBAR_WIDTH are all defined in
   * game.h via #define. The platform layer uses them here to compute the
   * window size without hard-coding numbers.
   *
   * This is the contract: game.h owns the constants; platform.c uses them.
   */
  int screen_width  = (FIELD_WIDTH  * CELL_SIZE) + SIDEBAR_WIDTH; /* 560 */
  int screen_height =  FIELD_HEIGHT * CELL_SIZE;                   /* 540 */

  /* ── Allocate backbuffer ──────────────────────────────────────────────
   *
   * `= {0}`: zero-initialises the struct (all fields = 0, pointer = NULL).
   * We then fill in the real values and call malloc.
   *
   * The backbuffer is allocated here (platform layer), because memory
   * allocation is an OS-level concern. game_render() just writes to it.
   */
  Backbuffer backbuffer = {0};
  backbuffer.width           = screen_width;
  backbuffer.height          = screen_height;
  backbuffer.bytes_per_pixel = 4;
  backbuffer.pitch           = screen_width * 4;
  backbuffer.pixels = (uint32_t *)malloc(
      (size_t)screen_width * (size_t)screen_height * sizeof(uint32_t)
  );
  if (!backbuffer.pixels) {
    fprintf(stderr, "ERROR: could not allocate backbuffer\n");
    return 1;
  }

  /* ── Raylib window setup ─────────────────────────────────────────────── */
  InitWindow(screen_width, screen_height, "Tetris - Lesson 04");
  SetTargetFPS(60);

  Image img = {
    .data    = backbuffer.pixels,
    .width   = backbuffer.width,
    .height  = backbuffer.height,
    .mipmaps = 1,
    .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
  };
  Texture2D texture = LoadTextureFromImage(img);

  /* ── Game state ───────────────────────────────────────────────────────
   *
   * GameState lives on the STACK (local variable in main).
   * `= {0}` zeroes all fields before game_init fills in real values.
   *
   * JS: const gameState = gameInit();   // factory-style
   * C:  GameState game_state = {0};     // zero first
   *     game_init(&game_state);          // then initialise
   *
   * The `&` passes a pointer to game_state. game_init() writes through
   * the pointer to set player_x, player_y, etc.
   *
   * WHY stack and not heap (malloc)?
   *   GameState is a fixed-size struct. It doesn't need to outlive main().
   *   Stack allocation is simpler: no malloc, no free, automatic cleanup
   *   when main returns. We only malloc things that are variable-size (like
   *   the pixel array) or must outlive their declaring scope.
   */
  GameState game_state = {0};
  game_init(&game_state);

  /* ── Double-buffered input ────────────────────────────────────────────
   *
   * Two input structs: current frame and previous frame.
   * Same pattern as Lesson 03 — see that lesson for the full explanation.
   */
  GameInput inputs[2]      = {0};
  GameInput *current_input = &inputs[0];
  GameInput *old_input     = &inputs[1];

  /* ── Main loop ────────────────────────────────────────────────────────
   *
   * Loop order (every frame):
   *   1. Input   — translate OS events into GameInput
   *   2. Update  — advance game logic
   *   3. Render  — write pixels into backbuffer
   *   4. Display — upload pixels to GPU, present frame
   *
   * This order is non-negotiable:
   *   - If you render before update, you show the previous frame's state.
   *   - If you read input after update, the game reacts one frame late.
   */
  while (!WindowShouldClose() && !game_state.should_quit) {
    float dt = GetFrameTime();

    /* ── 1. Input ──────────────────────────────────────────────────────
     *
     * prepare_input_frame MUST come before any UPDATE_BUTTON calls.
     * It copies ended_down from old→current and resets transition counters.
     *
     * Note: prepare_input_frame is DECLARED in game.h and DEFINED in game.c.
     * The compiler trusts the declaration here; the linker resolves it to
     * the function body in game.c at link time.
     *
     * JS: "trust me it exists" → C: that IS what a forward declaration does.
     */
    prepare_input_frame(old_input, current_input);

    /* Quit keys */
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
      game_state.should_quit = 1;
    }

    /* Movement (WASD / arrow keys) */
    UPDATE_BUTTON(current_input->move_left,
                  IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
    UPDATE_BUTTON(current_input->move_right,
                  IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
    UPDATE_BUTTON(current_input->move_down,
                  IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
    UPDATE_BUTTON(current_input->action,
                  IsKeyDown(KEY_UP) || IsKeyDown(KEY_W));

    /* ── 2. Update ─────────────────────────────────────────────────────
     *
     * game_update() is DEFINED in game.c. This call crosses the
     * platform→game boundary. The platform hands off:
     *   - &game_state : pointer to the game's state
     *   - current_input: this frame's button data
     *   - dt           : seconds since last frame
     *
     * After this call, game_state.player_x / player_y are updated.
     * The platform does not know HOW the update works — it just calls it.
     */
    game_update(&game_state, current_input, dt);

    /* ── 3. Render ─────────────────────────────────────────────────────
     *
     * game_render() writes pixels into backbuffer.
     * No Raylib calls happen inside game_render — it is pure memory writes.
     *
     * We pass &backbuffer (a pointer) so game_render can modify
     * backbuffer.pixels without copying the entire struct.
     */
    game_render(&backbuffer, &game_state);

    /* ── 4. Display ────────────────────────────────────────────────────
     *
     * The platform uploads the CPU pixel array to the GPU and presents it.
     * This is the ONLY place any Raylib rendering call happens.
     * The game layer knows nothing about textures, OpenGL, or GPUs.
     */
    UpdateTexture(texture, backbuffer.pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    EndDrawing();

    /* Swap input buffers — current becomes old for next frame */
    GameInput temp   = *current_input;
    *current_input   = *old_input;
    *old_input       = temp;
  }

  /* ── Cleanup ─────────────────────────────────────────────────────────── */
  UnloadTexture(texture);
  CloseWindow();
  free(backbuffer.pixels);
  backbuffer.pixels = NULL;

  return 0;
}
```

> **Why doesn't `main_raylib.c` include `game.c`?** Because `#include` is text
> paste — including `game.c` would literally paste all of `game.c` into
> `main_raylib.c`, creating one massive translation unit. Worse, `prepare_input_frame`
> and `game_init` would be defined twice if anything else also included `game.c`.
> The correct model: each `.c` file is compiled separately; the linker joins them.
> Headers (`.h` files) contain declarations (prototypes, types, macros) that are
> safe to include in multiple places. Source files (`.c`) contain definitions
> (function bodies) and are compiled exactly once.

---

## Build and run

```bash
# From src/ — list EVERY .c file that contains code you use
clang -I. main_raylib.c game.c utils/draw-shapes.c -lraylib -lm -o tetris
./tetris
```

**Understanding the command:**

| Part | Meaning |
|------|---------|
| `main_raylib.c` | Platform layer — contains `main()` |
| `game.c` | Game layer — contains `game_init`, `game_update`, `game_render`, `prepare_input_frame` |
| `utils/draw-shapes.c` | Drawing library — contains `draw_rect`, `draw_rect_blend` |
| `-I.` | Search `.` for header files (`"game.h"` → `./game.h`) |
| `-lraylib -lm` | Link Raylib and the C math library |

> **Handmade Hero principle:** No build system yet — just a single command.
> Understanding what each file contributes is more valuable than automating it
> away with CMake or Make. When something fails to link, you'll know exactly
> which `.c` file is missing.

---

## Expected result

Visually identical to Lesson 03:
- Dark background with a grey field border
- Red 50 × 50 square at the field centre
- Arrow keys / WASD move the square
- Up / W key mapped to `action` button (moves square up)
- Escape / Q quits
- The sidebar area is a slightly different shade (showing the render split)

---

## Common mistakes

> **Common mistake:**
> ```c
> /* main_raylib.c */
> #include "game.c"   /* WRONG — never include a .c file */
> ```
> This fails because: `game.c` contains function **definitions** (`game_init`,
> `game_update`, etc.). If you `#include "game.c"` into `main_raylib.c`, those
> functions are defined inside `main_raylib.c`'s translation unit. If any other
> file also defines them (or if you also list `game.c` on the command line),
> the linker errors: **"duplicate symbol 'game_init'"**.
>
> **Correct version:** Include `"game.h"` (declarations only). List `game.c`
> on the compiler command line so it is compiled as a separate translation unit.

---

> **Common mistake:**
> ```c
> GameState game_state;          /* no = {0} */
> game_init(&game_state);
> ```
> This might work — `game_init` calls `memset(state, 0, ...)` which zeroes
> everything. But if someone later calls a function that reads `game_state`
> BEFORE `game_init` (perhaps in an error path), they read garbage.
>
> **Correct version:** Always `= {0}`:
> ```c
> GameState game_state = {0};   /* zeroed immediately at declaration */
> game_init(&game_state);        /* then properly initialised        */
> ```

---

> **Common mistake:**
> ```c
> /* Forget to add game.c to the compile command */
> clang -I. main_raylib.c utils/draw-shapes.c -lraylib -lm -o tetris
> ```
> This fails with a linker error:
> `Undefined symbols: "game_init", "game_update", "game_render", "prepare_input_frame"`
>
> The compiler found the declarations (in `game.h`) but the linker couldn't find
> the definitions — because `game.c` was never compiled.
>
> **Correct version:**
> ```bash
> clang -I. main_raylib.c game.c utils/draw-shapes.c -lraylib -lm -o tetris
> ```

---

> **Common mistake:** defining the same `#define` in multiple headers without
> a guard:
> ```c
> /* utils/backbuffer.h — no guard */
> #define CELL_SIZE 30
>
> /* game.h — no guard, includes backbuffer.h, also defines CELL_SIZE */
> #define CELL_SIZE 30   /* ← "CELL_SIZE redefined" warning */
> ```
> **Correct version:** Always use header guards, and define each constant in
> exactly one canonical location (here, `game.h`).

---

## Exercises

1. **Add a `score` field.** Add `int score;` to `GameState`. In `game_update`,
   increment `score` each frame the action button is held. In `game_render`,
   use `printf` to print the score to the terminal. Confirm it increments when
   you hold W/Up and stops when you release.

2. **Separate `prepare_input_frame`.** Currently it's defined in `game.c`. Try
   moving it to a new file `game_input.c` with a matching `game_input.h`. Add
   `game_input.c` to the compile command. This mirrors how larger projects
   split functionality across many files.

3. **Trace the linker's job.** Run the compile command in two steps to see
   what the linker does:
   ```bash
   # Step 1: compile each .c to an object file (.o)
   clang -I. -c main_raylib.c -o main_raylib.o
   clang -I. -c game.c -o game.o
   clang -I. -c utils/draw-shapes.c -o draw-shapes.o

   # Step 2: link them together
   clang main_raylib.o game.o draw-shapes.o -lraylib -lm -o tetris
   ```
   The `.o` files are machine code with unresolved symbols
   (e.g. `game_init` in `main_raylib.o` points to "somewhere, TBD"). The
   linker resolves those references by finding the matching symbols in the
   other `.o` files.

---

## What's next

In **Lesson 05** we introduce the Tetris field — a flat `unsigned char` array
representing the 12 × 18 grid — and draw the walls and floor. We'll add the
`field` array to `GameState`, write `game_init` to fill in the boundary cells,
and extend `game_render` to draw each cell as a coloured rectangle. The
architecture we built in this lesson makes that addition straightforward:
`GameState` grows, `game_init` fills it, `game_render` draws it — `main_raylib.c`
doesn't change at all.
