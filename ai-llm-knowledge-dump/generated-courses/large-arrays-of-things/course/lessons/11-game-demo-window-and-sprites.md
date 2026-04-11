# Lesson 11: Game Demo --- Window, Sprites, and the Pool

> **What you'll build:** A real graphical application. A window opens, a player appears as a colored rectangle, and the things pool you built in lessons 02--10 manages every entity on screen. By the end of this lesson you will see pixels on a screen that come directly from iterating your pool.

---

## What you'll learn

- How to separate game types from platform code using `game.h`
- How to write `game_init` (initialize pool, create the player) and `game_render` (iterate pool, draw rectangles)
- The platform contract: how backends (Raylib, X11) provide a window, input, and a pixel buffer
- HUD text rendering with a bitmap font baked into the pixel buffer
- The scene selector system: keys 1--9, P, L, G, M, I to switch between demos

## Prerequisites

- **Lesson 10** completed (you have the full pool system: things.h, things.c with add/remove/get/iterator)
- **Platform Foundation Course** completed (you understand how a backend opens a window, creates a pixel buffer, and uploads it to the GPU each frame)

If you have not done the Platform Foundation Course, this lesson gives you the minimal subset. You will copy the platform files and understand what they do at a high level without needing to write them from scratch.

---

## Step 1 --- The architecture: three layers

Before we write any code, let us draw the picture. The entire application has three layers, and every layer depends on exactly one other layer --- the one below it.

```
 ┌─────────────────────────────────────────────────────────────┐
 │                      Platform Layer                         │
 │   (src/platforms/raylib/main.c  OR  src/platforms/x11/main.c)│
 │                                                             │
 │   Owns: window, pixel buffer, GPU texture, input polling    │
 │   Calls: game_init(), game_update(), game_render()          │
 │   Knows about: platform.h (which re-exports game.h)         │
 │   Does NOT know about: thing_pool internals, trolls, player │
 └────────────────────────────┬────────────────────────────────┘
                              │ calls game API
                              ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                       Game Layer                            │
 │   (src/game/game.h  +  src/game/game.c)                    │
 │                                                             │
 │   Owns: game_state (which contains thing_pool)              │
 │   Calls: thing_pool_add(), thing_pool_get_ref(), iterator   │
 │   Knows about: things/things.h (pool API)                   │
 │   Does NOT know about: X11, Raylib, OpenGL, pixel formats   │
 └────────────────────────────┬────────────────────────────────┘
                              │ calls pool API
                              ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                      Things Pool                            │
 │   (src/things/things.h  +  src/things/things.c)             │
 │                                                             │
 │   Owns: thing[], used[], generations[], free list, iterator │
 │   Knows about: nothing. Zero platform dependencies.         │
 └─────────────────────────────────────────────────────────────┘
```

Each layer has exactly one dependency: the layer below it. The platform layer never touches the pool directly. The game layer never touches Raylib or X11. The pool has zero external dependencies --- it does not even include `<stdio.h>`.

> **Why?** In JavaScript terms, think of it like this: `things.h` is a standalone npm package with zero dependencies. `game.c` imports that package and uses it. `main.c` imports the game module and wires it to the browser (the platform). If you want to swap the platform --- say, from Raylib to SDL --- you replace `main.c` and nothing else changes. If you want to swap the game --- say, from trolls to spaceships --- you rewrite `game.c` and neither the platform nor the pool changes.

This separation is not academic. It is how Casey Muratori structures Handmade Hero: the platform layer provides a buffer and input, the game layer fills the buffer, and neither layer knows about the other's internals. It is how every well-structured game engine works.

### The file layout on disk

Here is how the source files map to the three layers:

```
course/src/
├── things/
│   ├── things.h        ← Pool types + API (Lessons 02-10)
│   └── things.c        ← Pool implementation
├── game/
│   ├── game.h          ← Game types + API (this lesson)
│   └── game.c          ← Game implementation
├── platforms/
│   ├── raylib/
│   │   └── main.c      ← Raylib backend (copy from Platform Foundation Course)
│   └── x11/
│       └── main.c      ← X11/GLX backend (copy from Platform Foundation Course)
├── platform.h          ← Minimal contract: re-exports game.h, defines constants
└── test_harness.c      ← Terminal test runner (Lessons 02-10)
```

The `things/` directory is pure data structure code with zero dependencies. The `game/` directory imports `things/` and adds game logic. The `platforms/` directory imports `game/` (via `platform.h`) and adds window/input/rendering. Clean dependency flow, bottom to top.

### Why does this matter to a web developer?

If you have built backend microservices, this is the same principle. Your database layer does not know about HTTP. Your business logic layer imports the database layer but does not know about Express or Fastify. Your routing layer imports the business logic and wires it to HTTP endpoints.

In a game engine, the layers are: data structures (pool) -> game logic -> platform integration. Same principle, different domain.

---

## Step 2 --- game.h: defining the game types

Create the directory structure if it does not exist:

```bash
mkdir -p src/game
```

Now create `src/game/game.h`. This header defines everything the game layer exposes to the platform layer. The platform needs to know four things: how big the screen is, what input looks like, what the game state is, and what three functions to call.

### Constants

Start with the file header and includes:

```c
/* game.h */
#ifndef GAME_H
#define GAME_H

#include "../things/things.h"
#include <stdint.h>
#include <stdbool.h>
```

The `#ifndef` / `#define` / `#endif` pattern is an **include guard**. It prevents the compiler from processing this file twice if two different `.c` files both include it.

> **Why?** In JavaScript, `import { Pool } from './pool.js'` is automatically deduplicated by the module system. C has no module system. If `game.h` is included twice, you get "duplicate typedef" errors. The include guard prevents that.

We include `things.h` because `game_state` will contain a `thing_pool`. We include `<stdint.h>` for `uint32_t` (the pixel type) and `<stdbool.h>` for `bool`.

Now the constants:

```c
#define SCREEN_W 800
#define SCREEN_H 600

#define PLAYER_SPEED    200.0f  /* pixels per second */
#define PLAYER_SIZE      20.0f
#define TROLL_SIZE       15.0f
#define SPAWN_INTERVAL    1.0f  /* seconds between troll spawns */
#define MAX_TROLLS        20
```

These are `#define` constants. The preprocessor replaces every `SCREEN_W` with `800` before the compiler sees the code.

> **Why `#define` instead of `const int`?** In C (unlike C++), a `const int` cannot be used as an array size at file scope. `#define` works everywhere. You will see this pattern in every C codebase. It is the standard way to declare game constants.

`PLAYER_SPEED` is in **pixels per second**. We will multiply it by `dt` (delta time in seconds per frame) to get displacement per frame. At 60 FPS, `dt` is about 0.0167 seconds, so the player moves about 3.3 pixels per frame. This is the same frame-rate-independent velocity model used in every game.

`MAX_TROLLS` caps the total trolls that will ever be spawned --- not alive at once, but spawned total. This is a demo limiter so the game does not run forever. The pool itself holds up to `MAX_THINGS` (1024) simultaneously.

### The input struct

```c
typedef struct game_input {
    bool left;
    bool right;
    bool up;
    bool down;
    bool space;
    bool tab_pressed;
    int  scene_key;    /* 0=none, 1-9=scenes, 10-14=labs */
} game_input;
```

The platform layer fills this struct every frame by polling keyboard state. The game layer reads it without knowing whether the keys came from Raylib, X11, SDL, or a replay file.

In JavaScript, you would write:

```js
const input = { left: false, right: false, up: false, down: false };
window.addEventListener('keydown', e => {
    if (e.key === 'ArrowLeft') input.left = true;
});
window.addEventListener('keyup', e => {
    if (e.key === 'ArrowLeft') input.left = false;
});
```

Same idea --- a data object that represents the current state of the keyboard. The difference is that in C it is a struct, and the platform backend fills it instead of DOM event listeners.

The `scene_key` field deserves explanation. The demo has 14 scenes (9 lesson scenes + 5 lab scenes). Pressing a number key or a letter key (P, L, G, M, I) sets `scene_key` to the corresponding scene number. The game layer reads it and switches scenes. This keeps scene-switching logic out of the platform layer.

### The game state

```c
typedef struct game_state {
    thing_pool   pool;
    thing_ref    player_ref;
    float        spawn_timer;
    int          trolls_spawned;
    int          trolls_killed;
    int          slots_reused;
    unsigned int rng_state;
    int          current_scene;
    /* ... scene-specific state omitted for clarity ... */
} game_state;
```

This is the entire game in one struct. Everything. The pool holds all entities. `player_ref` is a generational reference to the player. The counters are for the HUD. `rng_state` is the random number generator's state. `current_scene` tracks which demo scene is active.

> **Why `thing_ref player_ref` instead of `thing_idx player_idx`?** The ref includes the generation. If some bug removed the player and a troll took its slot, `thing_pool_get_ref` would return the nil sentinel instead of silently returning the troll. Defensive by default. This is the generational ID system from Lesson 09 at work.

In JavaScript terms, `game_state` is like your React component's state object --- one object holds everything, and the "render" function reads it to produce output.

### The public API

```c
void game_init(game_state *state);
void game_update(game_state *state, const game_input *input, float dt);
void game_render(const game_state *state, uint32_t *pixels, int width, int height);
void game_hud(const game_state *state, char *buf, int buf_size);

#endif /* GAME_H */
```

Four functions. That is the entire game API:

1. **`game_init`** --- call once at startup and on reset. Zeros everything, initializes the pool, creates the player.
2. **`game_update`** --- call once per frame. Moves entities, spawns enemies, checks collisions. Takes `dt` (seconds since last frame) for frame-rate independence.
3. **`game_render`** --- call once per frame after update. Writes colored pixels into the buffer. Takes `const game_state *` because rendering should never modify game state.
4. **`game_hud`** --- builds the HUD text string for the current scene. The render function draws it into the pixel buffer using the bitmap font.

> **Why?** This is exactly Casey Muratori's pattern from Handmade Hero: `GameUpdateAndRender` takes a platform-provided backbuffer and input. We split update and render for clarity, but the idea is the same. The platform provides resources. The game fills them.

Here is the complete `game.h` that you need for this lesson. The full version in the source code has additional scene-specific fields --- for now, focus on the core:

```c
/* ================================================================== */
/*               game.h --- LOATs Game Demo                           */
/* ================================================================== */
#ifndef GAME_H
#define GAME_H

#include "../things/things.h"
#include <stdint.h>
#include <stdbool.h>

/* Constants */
#define SCREEN_W 800
#define SCREEN_H 600

#define PLAYER_SPEED    200.0f
#define PLAYER_SIZE      20.0f
#define TROLL_SIZE       15.0f
#define SPAWN_INTERVAL    1.0f
#define MAX_TROLLS        20

/* Input */
typedef struct game_input {
    bool left;
    bool right;
    bool up;
    bool down;
    bool space;
    bool tab_pressed;
    int  scene_key;
} game_input;

/* Game state */
typedef struct game_state {
    thing_pool   pool;
    thing_ref    player_ref;
    float        spawn_timer;
    int          trolls_spawned;
    int          trolls_killed;
    int          slots_reused;
    unsigned int rng_state;
    int          current_scene;
} game_state;

/* API */
void game_init(game_state *state);
void game_update(game_state *state, const game_input *input, float dt);
void game_render(const game_state *state, uint32_t *pixels, int width, int height);
void game_hud(const game_state *state, char *buf, int buf_size);

#endif /* GAME_H */
```

**Compile checkpoint:** You cannot compile yet (no `.c` file), but the header should parse cleanly. If you have `things.h` from Lesson 10, this header introduces no new dependencies.

### What the full game.h looks like

The actual `game.h` in the source code is larger because it includes scene-specific state for all 14 scenes and the 5 labs (particle data, spatial partition data, memory arena data, etc.). Do not let that size intimidate you. The core is what we just walked through:

```
┌──────────────────────────────────────────────────────────┐
│                     game.h                               │
│                                                          │
│  Constants:    SCREEN_W, SCREEN_H, speeds, sizes         │
│  Input struct: game_input { left, right, up, down, ... } │
│  State struct: game_state { pool, player_ref, ... }      │
│  API:          game_init, game_update, game_render        │
│                                                          │
│  Everything else is scene-specific data that you can     │
│  ignore until you reach those specific lessons/labs.     │
└──────────────────────────────────────────────────────────┘
```

The scene-specific fields (`s5_with_next`, `s6_saved_ref`, `particle_archetype[]`, etc.) are all zero-initialized by `memset` and only used when the corresponding scene is active. The zero-init architecture means unused fields do not cause problems --- they just sit at zero.

---

## Step 3 --- game.c: the RNG and the pixel drawing function

Create `src/game/game.c`. This file implements the four API functions. We will build it piece by piece.

### File header and includes

```c
/* game.c --- LOATs Game Demo Implementation */
#include "game.h"
#include <string.h> /* memset */
```

One include: our own header. One standard library include: `string.h` for `memset`. That is it. No platform headers. No Raylib. No X11. The game layer is completely platform-independent.

### The RNG (random number generator)

Before we can spawn trolls at random positions (in Lesson 12), we need a random number generator. We are NOT going to use `rand()` from the standard library.

```c
static unsigned int rng_next(unsigned int *state)
{
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float rng_float(unsigned int *state)
{
    return (float)(rng_next(state) & 0xFFFF) / 65536.0f;
}
```

This is a **xorshift32** random number generator. Three XOR-shift operations produce a new pseudo-random number from the previous one. The state is a single `unsigned int`.

> **Why not `rand()`?** `rand()` uses hidden global state. If you call it from two different systems, the sequences interfere. Xorshift32 keeps its state in a variable you control --- `rng_state` inside `game_state`. This means the sequence is reproducible: given the same seed, you get the same trolls at the same positions every time. In JavaScript, `Math.random()` also uses hidden global state, but you rarely notice because JavaScript is single-threaded. In C game engines, hidden global state is a landmine.

`rng_float` returns a value in [0, 1). The `& 0xFFFF` masks to 16 bits (0--65535), then divides by 65536. Good enough for placing trolls on screen.

Both functions are `static` --- visible only inside `game.c`. They are internal utilities, not part of the public API.

> **Why?** In JavaScript, a `static` function in C is like a module-scoped function that is not exported. Only code inside `game.c` can call `rng_next` and `rng_float`. The platform layer cannot call them directly.

### The pixel drawing function

```c
static void draw_rect(uint32_t *pixels, int buf_w, int buf_h,
                       int rx, int ry, int rw, int rh, uint32_t color)
{
    if (!pixels) return;
    int x0 = rx < 0 ? 0 : rx;
    int y0 = ry < 0 ? 0 : ry;
    int x1 = (rx + rw) > buf_w ? buf_w : (rx + rw);
    int y1 = (ry + rh) > buf_h ? buf_h : (ry + rh);

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            pixels[y * buf_w + x] = color;
        }
    }
}
```

This function fills a rectangle in the pixel buffer. Let us walk through it.

**Parameters:**

- `pixels` --- the raw pixel buffer. Each element is a `uint32_t` representing one pixel in RGBA format.
- `buf_w`, `buf_h` --- buffer dimensions (800 x 600 for us).
- `rx`, `ry`, `rw`, `rh` --- rectangle position and size.
- `color` --- the fill color as a packed 32-bit integer (e.g., `0xFFFF8800` for opaque orange).

**Clipping:** The four lines after the null check clamp the rectangle to the buffer bounds. If `rx` is -5, `x0` becomes 0. If `rx + rw` exceeds `buf_w`, `x1` becomes `buf_w`.

> **Why?** In JavaScript, `ctx.fillRect(-10, -10, 50, 50)` clips automatically --- the canvas API handles it. In C, YOU are the canvas API. If you write `pixels[-1]`, you corrupt whatever memory lives before the buffer. This is a buffer overflow --- one of the most dangerous bugs in C. The clipping code prevents it.

**The pixel formula:** `pixels[y * buf_w + x]` converts a 2D coordinate to a 1D array index.

```
Memory layout of the pixel buffer (800 wide):

Row 0:  pixels[0]   pixels[1]   pixels[2]   ... pixels[799]
Row 1:  pixels[800] pixels[801] pixels[802] ... pixels[1599]
Row 2:  pixels[1600] ...
...

Pixel at (x=5, y=3):
  index = 3 * 800 + 5 = 2405
  pixels[2405] = color
```

This is the same layout as `ImageData.data` in JavaScript's Canvas API, except we use 32-bit elements instead of 4 separate bytes per pixel.

The `if (!pixels) return` check at the top exists because later, the HUD function calls scene render functions with `pixels = NULL` to collect only the HUD text without drawing. A null check here prevents a crash in that code path.

> **Why are both functions `static`?** The `static` keyword on a function in C means "only visible inside this file." It is like a module-private function that is not exported. The platform layer cannot call `draw_rect` or `rng_float` --- they are internal implementation details of the game layer.

Let us trace through a concrete `draw_rect` call to make sure you can read it:

```
draw_rect(pixels, 800, 600, 395, 295, 20, 20, 0xFFFF8800)

  rx=395, ry=295, rw=20, rh=20

  Clipping:
    x0 = max(395, 0) = 395
    y0 = max(295, 0) = 295
    x1 = min(395+20, 800) = 415
    y1 = min(295+20, 600) = 315

  Loop: y from 295 to 314 (20 rows)
        x from 395 to 414 (20 columns)
        pixels[y * 800 + x] = 0xFFFF8800

  Total pixels written: 20 * 20 = 400

  Result: a 20x20 orange square at position (395, 295)
```

At 800x600 resolution, the pixel buffer has 480,000 elements. Writing 400 of them takes under 1 microsecond. The clipping logic adds negligible overhead --- four comparisons per call, not per pixel.

---

## Step 4 --- game_init: creating the player

Now the first real game function. This is where the pool meets the game.

```c
void game_init(game_state *state)
{
    int saved_scene = state->current_scene;
    memset(state, 0, sizeof(*state));
    state->current_scene = (saved_scene >= 1 && saved_scene <= 14)
                            ? saved_scene : 8;

    thing_pool_init(&state->pool);
    state->rng_state = 12345;

    state->player_ref = thing_pool_add(&state->pool, THING_KIND_PLAYER);
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    if (thing_is_valid(player)) {
        player->x     = (float)SCREEN_W / 2.0f;
        player->y     = (float)SCREEN_H / 2.0f;
        player->size  = PLAYER_SIZE;
        player->color = 0xFFFF8800; /* orange */
    }
}
```

Let us trace through this line by line.

**`int saved_scene = state->current_scene;`** --- We save the current scene number before zeroing. When the player presses R to reset, we want to reset the current scene, not jump back to the default.

**`memset(state, 0, sizeof(*state));`** --- Zero the entire game state. Every field becomes zero: the pool is zeroed, `player_ref` is `{0, 0}`, all counters are 0, `rng_state` is 0. This is the "constructor."

> **Why?** In JavaScript, you would write `this.state = { pool: new Pool(), playerRef: null, score: 0 }`. In C, `memset(0)` achieves the same thing in one call, because we designed every struct so that all-zeros is a valid empty/nil state. `kind = 0` means `THING_KIND_NIL`. All indices at 0 mean "points to nil sentinel." All floats at 0 mean safe defaults. This is the zero-init architecture from Lesson 06.

**`thing_pool_init(&state->pool);`** --- Reinitialize the pool. This sets `next_empty = 1` (slot 0 is the nil sentinel). The memset already zeroed the pool, but `pool_init` sets the one bookkeeping field that is not zero.

**`state->rng_state = 12345;`** --- Seed the RNG with a fixed value. Every run produces the same troll positions. In a shipped game you would seed with something unpredictable, but for a demo, reproducibility is more useful for debugging.

**The player creation block** is the first time you see the pool used in a real game context:

```
Step 1:  thing_pool_add(&state->pool, THING_KIND_PLAYER)
         → allocates slot 1 (slot 0 is nil), sets kind=PLAYER
         → returns ref {index=1, generation=0}

Step 2:  thing_pool_get_ref(&state->pool, state->player_ref)
         → checks: index=1 in range? YES. used[1]? YES. gen match? YES.
         → returns &things[1]

Step 3:  thing_is_valid(player)
         → checks: player->kind != THING_KIND_NIL? YES (it is PLAYER)
         → returns true

Step 4:  Set position, size, color on the player thing.
```

> **Why check `thing_is_valid` right after adding?** We just initialized a 1024-slot pool and added one entity --- it cannot possibly be full. But checking is free (one comparison), and it is the correct pattern. If you later reduce `MAX_THINGS` to 4 for testing, or if `game_init` is called in a state where something went wrong, this check saves you from a silent nil write. Always use the pattern. Always.

**`0xFFFF8800` --- the color.** This is packed RGBA: `FF` alpha (fully opaque), `FF` red, `88` green, `00` blue. That gives us orange. The pixel format is `0xAARRGGBB`.

After `game_init` completes, the pool looks like this:

```
things[]:
  [0] NIL sentinel  (kind=0, x=0, y=0, all zeros)
  [1] PLAYER        (kind=PLAYER, x=400, y=300, size=20, color=orange)
  [2] (empty)
  [3] (empty)
  ...
  [1023] (empty)

used[]:
  [0] false   [1] true   [2] false   ...

generations[]:
  [0] 0       [1] 0      [2] 0       ...

next_empty = 2
first_free = 0  (empty free list)

player_ref = { index=1, generation=0 }
```

One entity. One slot used. The rest of the pool is empty, waiting.

### The CPU backbuffer: how pixels get to the screen

Before we write `game_render`, let us understand the rendering pipeline. In web development, you call `ctx.fillRect()` and the browser handles everything. In our architecture, you need to understand three distinct pieces:

```
 ┌───────────────────────────┐
 │    CPU Pixel Buffer       │  ← game_render writes here
 │    (uint32_t[480000])     │     one uint32_t per pixel
 │    lives in main RAM      │     800 * 600 = 480,000 pixels
 └────────────┬──────────────┘
              │ UpdateTexture() / glTexSubImage2D()
              │ copies 1.8 MB from CPU to GPU
              ▼
 ┌───────────────────────────┐
 │    GPU Texture             │  ← lives on the graphics card
 │    same 800x600 pixels    │     fast to draw, slow to update
 └────────────┬──────────────┘
              │ DrawTexture() / fullscreen quad
              │ GPU draws texture to screen
              ▼
 ┌───────────────────────────┐
 │    Screen                  │  ← what the player sees
 │    800x600 physical pixels │
 └───────────────────────────┘
```

The CPU pixel buffer is a flat array of `uint32_t` values. Each value represents one pixel in `0xAARRGGBB` format:

```
0xFFFF8800
  ││││││││
  ││││││└┘── Blue:  0x00 = 0
  ││││└┘──── Green: 0x88 = 136
  ││└┘────── Red:   0xFF = 255
  └┘──────── Alpha: 0xFF = 255 (fully opaque)

Result: bright orange
```

Every frame, the game clears this buffer (fills with background color), draws entities into it, draws HUD text into it, and then the platform layer copies it to the GPU and displays it. This is called **software rendering** because the CPU does all the pixel work. The GPU just acts as a display surface.

> **Why not use the GPU for drawing?** We could. Raylib has `DrawRectangle()`. But then `game_render` would need Raylib as a dependency, and the X11 backend would need its own drawing functions. By rendering to a CPU buffer, the game code is platform-independent. Both backends just call `UpdateTexture()` or `glTexSubImage2D()` to upload the same buffer.

The 1.8 MB upload per frame (800 * 600 * 4 bytes) takes about 0.1--0.5 milliseconds on modern hardware. At 60 FPS, that is less than 3% of our 16.67ms frame budget. Not free, but cheap enough for a demo.

---

## Step 5 --- game_render: drawing what's in the pool

Now the render function. This is where the pool produces visible output.

```c
void game_render(const game_state *state, uint32_t *pixels,
                 int width, int height)
{
    /* Phase 1: Clear to dark background */
    uint32_t bg = 0xFF1A1A2E; /* dark navy */
    for (int i = 0; i < width * height; i++) {
        pixels[i] = bg;
    }

    /* Phase 2: Draw all living things as colored rectangles */
    thing_pool *pool = (thing_pool *)&state->pool;

    for (thing_iter it = thing_iter_begin(pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *t = thing_iter_get(&it);
        int rx = (int)t->x;
        int ry = (int)t->y;
        int rs = (int)t->size;
        draw_rect(pixels, width, height, rx, ry, rs, rs, t->color);
    }
}
```

Two phases: clear, then draw.

**Phase 1 --- Clear:** Fill every pixel with `0xFF1A1A2E` --- a dark navy blue. This is the background. Every frame we clear the entire buffer and redraw everything. This is the standard approach for CPU-rendered games.

In JavaScript: `ctx.fillStyle = '#1A1A2E'; ctx.fillRect(0, 0, 800, 600);`

**Phase 2 --- Draw:** Iterate over every living thing in the pool and draw a colored rectangle at its position.

This is the pool iterator from Lesson 10 doing real work. Let us trace through what happens right after `game_init`:

```
thing_iter_begin(&pool):
  → starts at index 1 (skip nil sentinel at 0)
  → checks used[1]? YES (player is alive)
  → current = 1

thing_iter_valid(&it):
  → current=1, which is > 0 and < MAX_THINGS
  → returns true

thing_iter_get(&it):
  → returns &things[1] (the player)

draw_rect(pixels, 800, 600, 400, 300, 20, 20, 0xFFFF8800):
  → fills a 20x20 orange rectangle at (400, 300)

thing_iter_next(&it):
  → current becomes 2
  → checks used[2]? NO. used[3]? NO. ... keeps scanning ...
  → current reaches next_empty (2), which equals MAX_THINGS? No, but
     used[2] through used[1023] are all false
  → current passes MAX_THINGS
  → iterator is now invalid

thing_iter_valid(&it):
  → current >= MAX_THINGS
  → returns false → loop ends
```

One thing in the pool, one rectangle drawn. When we add trolls in Lesson 12, the same loop draws them too --- no changes needed to the render function.

> **Why?** In JavaScript terms, this is like React's `render()` method mapping state to DOM elements. The pool is the state. The pixel buffer is the DOM. The iterator is `state.entities.map(entity => <Rect {...entity} />)`. One loop handles every entity type because every thing has `x`, `y`, `size`, and `color`.

The `(thing_pool *)&state->pool` cast deserves explanation. The iterator API takes a `thing_pool *` (non-const) because that is what `thing_iter_begin` expects. But `game_render` takes `const game_state *` because rendering should not modify the game. We cast away const here. This is a minor impurity --- a cleaner solution would be a `const_thing_iter` API, but that doubles the iterator code for no real benefit.

---

## Step 6 --- game_update: stub for now

For this lesson, we need a `game_update` that compiles but does nothing. Movement and spawning come in Lesson 12.

```c
void game_update(game_state *state, const game_input *input, float dt)
{
    /* Lesson 12 fills this in: movement, spawning, collision. */
    (void)state;
    (void)input;
    (void)dt;
}
```

The `(void)` casts suppress "unused parameter" warnings. We compile with `-Werror`, so unused parameters cause a build failure. This is the standard C pattern for "I know I am not using these yet."

> **Why?** In JavaScript, unused function parameters are silently ignored. In C with strict warnings enabled (`-Wall -Wextra -Werror`), unused parameters are treated as errors. The `(void)` cast tells the compiler "I know, I did it on purpose."

### The HUD function (stub)

```c
void game_hud(const game_state *state, char *buf, int buf_size)
{
    snprintf(buf, buf_size, "Scene 8: Default Movement Demo  [R]Reset [Esc]Quit");
}
```

This writes a line of text into a buffer. The platform layer (or the render function) will draw this text onto the pixel buffer. For now it is static text --- Lesson 12 will add live counters.

---

## Step 7 --- The platform layer: opening a window

The platform layer opens a window, allocates a pixel buffer, runs the game loop, feeds input to the game, and uploads pixels to the GPU. You have two options: Raylib (simpler) or X11 (more educational, more code).

**Both backends are copied from the Platform Foundation Course.** If you have done that course, you already understand this code. If you have not, here is what each piece does.

### The Raylib backend (src/platforms/raylib/main.c)

Create the directory:

```bash
mkdir -p src/platforms/raylib
```

The file has five sections. Let us walk through each one.

**Section 1 --- Includes and main:**

```c
#include "raylib.h"
#include "../../platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
```

We include Raylib's header (provides `InitWindow`, `DrawTexture`, etc.), our platform header (which re-exports `game.h` and `things.h`), and standard library headers.

**Section 2 --- Window and pixel buffer:**

```c
    /* Window */
    InitWindow(SCREEN_W, SCREEN_H, GAME_TITLE);
    SetTargetFPS(TARGET_FPS);

    /* CPU pixel buffer */
    uint32_t *pixels = (uint32_t *)malloc(SCREEN_W * SCREEN_H * sizeof(uint32_t));
    if (!pixels) return 1;
    memset(pixels, 0, SCREEN_W * SCREEN_H * sizeof(uint32_t));

    /* GPU texture */
    Image img = {
        .data    = pixels,
        .width   = SCREEN_W,
        .height  = SCREEN_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D tex = LoadTextureFromImage(img);
```

`InitWindow` creates an 800x600 OS window with an OpenGL context. `malloc` allocates 1,920,000 bytes (~1.8 MB) for the pixel buffer. `LoadTextureFromImage` creates a GPU texture that we will update every frame.

> **Why `malloc` here when the course says "no malloc in the game loop"?** This allocation happens once at startup, not per frame. The rule is: no allocation inside the hot loop. Allocating a pixel buffer at startup is fine --- it is the same principle as pre-allocating the `things[]` array.

The key trick: our game writes to the CPU pixel buffer (`pixels`), and every frame we call `UpdateTexture(tex, pixels)` to copy those pixels to the GPU texture. Then Raylib draws the texture to the screen. The game never calls Raylib drawing functions directly.

> **Why?** Because `game_render` writes `uint32_t` values into a flat array. That is platform-independent. If we used Raylib's `DrawRectangle()` directly, the game code would be tied to Raylib. By rendering to a pixel buffer, the game works with any backend that can upload pixels to a texture.

**Why PIXELFORMAT_UNCOMPRESSED_R8G8B8A8?** This tells Raylib our pixel layout: Red byte, Green byte, Blue byte, Alpha byte, 8 bits each, no compression. This matches our `0xAARRGGBB` packed format on little-endian systems (which is nearly all modern hardware). On a big-endian system, the byte order would be reversed and colors would look wrong --- but you are almost certainly on x86 or ARM, both little-endian.

**Section 3 --- Game state and main loop:**

```c
    /* Game state */
    game_state state;
    memset(&state, 0, sizeof(state));
    state.current_scene = 8;
    game_init(&state);

    /* Main loop */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        /* Input */
        game_input input = {0};
        input.left  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
        input.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
        input.up    = IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W);
        input.down  = IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S);
        input.space = IsKeyPressed(KEY_SPACE);

        /* Scene selection: number keys 1-9 */
        for (int k = KEY_ONE; k <= KEY_NINE; k++) {
            if (IsKeyPressed(k)) {
                input.scene_key = k - KEY_ONE + 1;
                break;
            }
        }
        /* Lab keys */
        if (IsKeyPressed(KEY_P)) input.scene_key = 10;
        if (IsKeyPressed(KEY_L)) input.scene_key = 11;
        if (IsKeyPressed(KEY_G)) input.scene_key = 12;
        if (IsKeyPressed(KEY_M)) input.scene_key = 13;
        if (IsKeyPressed(KEY_I)) input.scene_key = 14;
        if (IsKeyPressed(KEY_TAB)) input.tab_pressed = true;

        /* Reset */
        if (IsKeyPressed(KEY_R)) {
            game_init(&state);
        }

        /* Update and render */
        game_update(&state, &input, dt);
        game_render(&state, pixels, SCREEN_W, SCREEN_H);

        /* Upload to GPU */
        UpdateTexture(tex, pixels);
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexture(tex, 0, 0, WHITE);
        EndDrawing();
    }
```

Every frame:

1. **Delta time:** `GetFrameTime()` returns seconds since the last frame. We cap at 0.1s to prevent the "spiral of death" --- if the app freezes for a second, we do not want `dt = 1.0` causing entities to teleport.
2. **Input:** Create a zeroed `game_input`, fill it from Raylib's keyboard state. Both arrow keys and WASD work.
3. **Reset:** Pressing R calls `game_init` again, resetting everything.
4. **Update + Render:** The game fills the pixel buffer.
5. **GPU upload:** `UpdateTexture` copies CPU pixels to the GPU. `DrawTexture` draws the texture. The screen shows the result.

The `dt` cap at 0.1 seconds (100ms) deserves explanation. Without the cap, if the window is minimized for 5 seconds, `dt` would be 5.0. At `PLAYER_SPEED = 200`, the player would move 1000 pixels in one frame --- teleporting off-screen. The cap says "even if 5 seconds passed, pretend only 0.1 seconds passed." This is a common defensive measure called preventing the "spiral of death."

> **Why?** In JavaScript, `requestAnimationFrame` stops firing when the tab is hidden. When you switch back, the first `dt` can be enormous. Games handle this by capping `dt`. Same idea here.

**Section 4 --- Cleanup:**

```c
    UnloadTexture(tex);
    free(pixels);
    CloseWindow();
    return 0;
}
```

Reverse order of creation: unload texture, free pixel buffer, close window. LIFO cleanup, like popping a stack.

### The platform contract (platform.h)

This file is tiny:

```c
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include "game/game.h"

#ifndef GAME_TITLE
#define GAME_TITLE "Large Arrays of Things -- Demo"
#endif

#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

#endif /* PLATFORM_H */
```

Its only job is to re-export `game.h` and define window constants. In the full Platform Foundation Course, `platform.h` defines a multi-function contract with `platform_init`, `platform_update`, `platform_render`, and `platform_shutdown`. For this demo, we skip that formality --- the backend `main.c` calls the game functions directly.

### The X11 backend (src/platforms/x11/main.c)

The X11 backend does the same thing as the Raylib backend --- opens a window, allocates a pixel buffer, runs the game loop --- but using raw X11 and OpenGL calls instead of Raylib. It is about 215 lines.

You do NOT need to write this from scratch. Copy it from the Platform Foundation Course. The important things to understand:

- `XOpenDisplay` / `XCreateWindow` create the OS window
- `glXCreateContext` creates an OpenGL context for rendering
- `glGenTextures` / `glTexImage2D` create a GPU texture
- Each frame: `glTexSubImage2D` uploads CPU pixels to the GPU
- A fullscreen quad (`glBegin(GL_QUADS)`) draws the texture
- Input is handled via `XNextEvent` with `KeyPress` / `KeyRelease` events
- A simple `nanosleep` provides ~60 FPS frame limiting

The X11 backend is more educational than the Raylib backend because you see every platform call explicitly. But it is also more code with more boilerplate. Both produce identical output because the game code is the same.

> **Why?** Having two backends proves the architecture works. If the game code were entangled with Raylib, the X11 backend would require rewriting game logic. Because `game.c` only writes to a `uint32_t *pixels` buffer, both backends work by just providing that buffer and uploading it to the GPU.

---

## Step 8 --- Build and run: first visual output

Build with the Raylib backend:

```bash
cd course/
./build-dev.sh --backend=raylib -r
```

What you should see:

- An 800x600 window opens with a dark navy background
- An orange 20x20 rectangle sits at the center of the screen
- The HUD text appears at the top-left
- Nothing moves yet (that is Lesson 12)
- Press R to reset, Esc to quit

If you have X11 development libraries installed, you can also build the X11 backend:

```bash
./build-dev.sh --backend=x11 -r
```

Both backends produce identical output because the game code is the same --- only the platform layer differs.

What is happening under the hood, every frame:

```
Frame N:
  1. Platform polls keyboard → fills game_input (all false, nothing pressed)
  2. game_update(&state, &input, 0.0167) → stub, does nothing
  3. game_render(&state, pixels, 800, 600):
     a. Clear: fill 480,000 pixels with 0xFF1A1A2E (dark navy)
     b. Iterate pool:
        - Skip slot 0 (nil sentinel)
        - Slot 1: PLAYER at (400, 300), size 20, color orange
          → draw_rect fills 20x20 pixels with 0xFFFF8800
        - Slot 2+: unused → iterator stops
     c. Draw HUD text into pixel buffer using bitmap font
  4. UpdateTexture: copy 1.8 MB from CPU to GPU
  5. DrawTexture: GPU draws the texture to the screen
  6. SwapBuffers: the screen shows the result
```

480,000 pixels cleared, 400 pixels filled (20 x 20 rectangle), 1.8 MB uploaded to the GPU. All in under 1 millisecond. The pool iterator visits exactly one slot. The CPU barely notices.

That orange square is the first visible proof that your pool works. For ten lessons, the pool lived in terminal output --- `printf("slot 3: TROLL")`. Now it produces pixels on a screen. The pool is real.

### Troubleshooting

If the window does not open:

- **Raylib not installed:** Run `sudo apt install libraylib-dev` (Ubuntu/Debian) or build Raylib from source.
- **X11 not installed:** Run `sudo apt install libx11-dev libxkbcommon-dev libgl-dev`.
- **Build fails with "things.h not found":** Make sure your directory structure matches the layout in Step 1. The include path `"../things/things.h"` in `game.h` depends on `game.h` being in `src/game/`.
- **Window opens but is all black:** Check that `game_render` is being called. Add a `printf("render\n")` inside it to verify.

### Verification checklist

- [ ] Window opens at 800x600 with dark navy background
- [ ] Orange 20x20 rectangle visible at screen center
- [ ] HUD text visible at top-left
- [ ] Nothing moves (that is Lesson 12)
- [ ] Press Esc to quit cleanly
- [ ] Press R to reset (same state, since nothing changes yet)

---

## Step 9 --- HUD text: seeing the pool state on screen

The demo renders HUD text directly into the CPU pixel buffer using a bitmap font. This means both backends (Raylib and X11) show identical text --- there is no platform-specific text rendering.

The font is stored as a static array in `game.c`:

```c
static const uint8_t FONT_8X8[128][8] = {
    /* 128 ASCII characters, 8 bytes each (8x8 pixel bitmap) */
    /* 0x41 'A' = */ {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    /* ... */
};
```

Each character is an 8x8 grid of pixels encoded as 8 bytes. Each byte represents one row, and each bit represents one pixel. The `draw_text_buf` function reads these bitmaps and writes the corresponding pixels into the buffer:

```c
static void draw_text_buf(uint32_t *pixels, int buf_w, int buf_h,
                          int x, int y, const char *text, uint32_t color)
{
    for (int i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i] & 0x7F;
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (FONT_8X8[ch][row] & (1 << col)) {
                    int px = x + i * 8 + col;
                    int py = y + row;
                    if (px >= 0 && px < buf_w && py >= 0 && py < buf_h)
                        pixels[py * buf_w + px] = color;
                }
            }
        }
    }
}
```

For each character in the string, for each row of the 8x8 bitmap, for each column: if the bit is set, write a pixel. The bounds check prevents writing outside the buffer.

Let us trace through rendering the letter "A" (ASCII 0x41):

```
FONT_8X8[0x41] = {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}

Row 0: 0x0C = 0000 1100    →    ..XX....
Row 1: 0x1E = 0001 1110    →    .XXXX...
Row 2: 0x33 = 0011 0011    →    XX..XX..
Row 3: 0x33 = 0011 0011    →    XX..XX..
Row 4: 0x3F = 0011 1111    →    XXXXXX..
Row 5: 0x33 = 0011 0011    →    XX..XX..
Row 6: 0x33 = 0011 0011    →    XX..XX..
Row 7: 0x00 = 0000 0000    →    ........

Result:
  ..XX....
  .XXXX...
  XX..XX..
  XX..XX..
  XXXXXX..
  XX..XX..
  XX..XX..
  ........
```

Each "X" becomes a colored pixel in the buffer. Each "." is left as the background color. The font table has 128 entries (one per ASCII character), each 8 bytes (one per row). The entire font is 1024 bytes --- smaller than a single tweet's worth of text.

> **Why?** In JavaScript, you would call `ctx.fillText("hello", 10, 10)`. In our CPU-rendered approach, we ARE the text renderer. There is no `fillText`. There is only the pixel buffer and some bitmap data. This is what "software rendering" means --- every pixel is your responsibility. The upside: both backends (Raylib and X11) show identical text, character-for-character, because the text is rendered into the same pixel buffer.

The `game_render` function calls `game_hud` to build the text string, then calls `draw_text_buf` for each line. The HUD shows the current scene name, the key bindings, and scene-specific stats (like "Spawned: 5 Killed: 2 Reused: 1").

The HUD rendering works like this:

```
1. game_hud(state, hud_text, 512):
   → builds "Scene 8: Default Movement Demo  [1-9]Scene [P]Part..."

2. game_render splits hud_text on '\n' characters:
   → line 1: "Scene 8: Default Movement Demo  [1-9]Scene..."
   → line 2: "Spawned: 5  Killed: 2  Reused: 1  ..."

3. draw_text_buf draws each line:
   → line 1 at y=4 (white)
   → line 2 at y=14 (light gray)

4. Each character: 8x8 pixels written into the buffer
   → "Scene" = 5 characters * 8 columns = 40 pixels wide
```

---

## Step 10 --- Scene selector: switching between demos

The game demo has 14 scenes, each demonstrating a different aspect of the pool system. The default scene is 8 (the movement demo), which is what Lesson 12 fills in.

| Key | Scene | What it demonstrates |
|-----|-------|---------------------|
| 1 | Singly-Linked Inventory | Lesson 04's intrusive singly-linked list |
| 2 | Circular Doubly-Linked Inventory | Lesson 05's circular doubly-linked list |
| 3 | Kind-Check vs used[] Liveness | Lesson 07 Variant A comparison |
| 4 | Fat Struct Info | Lesson 02's fat struct layout |
| 5 | Free List ON vs OFF | Lesson 08's free list comparison |
| 6 | Generational IDs Demo | Lesson 09's stale ref detection |
| 7 | Prepend vs Append Order | Lesson 05's append variant |
| 8 | Default Movement Demo | Player + trolls (Lesson 11--12) |
| 9 | Separate Pools | Lesson 13's separate-pools variant |
| P | Particle Laboratory | 1000 particles, 6 archetypes |
| L | Data Layout Toggle Lab | AoS vs SoA vs Hybrid comparison |
| G | Spatial Partition Lab | Grid, Hash, Quadtree collision |
| M | Memory Arena Lab | malloc vs arena vs pool allocation |
| I | Infinite Growth Lab | Stress test: entities spawn forever |

Press any of these keys at any time and the demo instantly switches. Press R to reset the current scene.

The scene dispatch lives in `game_update`:

```c
void game_update(game_state *state, const game_input *input, float dt)
{
    /* Check for scene switch */
    if (input->scene_key >= 1 && input->scene_key <= NUM_SCENES) {
        state->current_scene = input->scene_key;
        memset(state, 0, sizeof(*state));
        state->current_scene = input->scene_key;
        scene_init_dispatch(state);
        return;
    }

    /* Dispatch to scene-specific update */
    switch (state->current_scene) {
        case 1: scene1_update(state, input, dt); break;
        case 2: scene2_update(state, input, dt); break;
        /* ... */
        case 8: scene8_update(state, input, dt); break;
        /* ... */
    }
}
```

Each scene has its own `_init`, `_update`, and `_render` functions. The game dispatches to the right one based on `current_scene`. This is a simple state machine --- the scene number is the state, and the key press is the transition.

> **Why?** This structure lets you explore every lesson's concept visually. Scene 5 shows the free list running with and without recycling. Scene 6 shows generational IDs detecting stale refs in real time. Scene 8 (our main demo) shows the full game loop. Each scene is self-contained.

How scene switching works internally:

```
1. User presses '5' on keyboard
2. Platform backend: input.scene_key = 5
3. game_update reads scene_key:
   a. Saves current_scene = 5
   b. memset(state, 0, sizeof(*state))  ← zeros EVERYTHING
   c. state->current_scene = 5
   d. scene_init_dispatch(state)         ← calls scene5_init
   e. return (skip update this frame)
4. Next frame: game_update dispatches to scene5_update
5. game_render dispatches to scene5_render

The memset in step (b) is the zero-init architecture at work.
Scene 5 starts with a clean slate — pool empty, counters zero,
all fields at their default (zero) values. No "cleanup old scene"
code needed.
```

This is why the zero-init design from Lesson 06 matters beyond individual entities. The entire game state can be reset with `memset(0)`. Switching scenes is trivial because every scene starts from the same clean state.

The `scene_name()` function returns a human-readable string for the HUD:

```c
const char *scene_name(int scene)
{
    switch (scene) {
        case 1: return "Singly-Linked Inventory";
        case 2: return "Circular Doubly-Linked Inventory";
        /* ... */
        case 8: return "Default Movement Demo";
        /* ... */
        case 14: return "Infinite Growth Lab";
        default: return "Unknown";
    }
}
```

Every scene maps directly to a lesson concept. Playing with the scenes is the fastest way to see what each lesson built.

---

## Common mistakes

**Forgetting to clip draw_rect.** In JavaScript, `ctx.fillRect(-10, -10, 50, 50)` clips automatically. In C, writing `pixels[-1]` corrupts memory before the buffer. Always clip to buffer bounds.

**Not checking `thing_is_valid` after `pool_add`.** Even when you "know" the pool is not full, the check is free and protects you if the pool state is ever unexpected. Always use the pattern.

**Casting `const` away carelessly.** The `(thing_pool *)&state->pool` cast in `game_render` is a controlled exception. Do not make it a habit. If you find yourself casting away const everywhere, your API needs a `const` variant.

**Using `rand()` instead of the explicit RNG.** `rand()` works but uses hidden global state. If you later add a second system that also calls `rand()`, the sequences will interfere and your "random" troll positions will change depending on unrelated code. Use an explicit state variable.

**Writing to the nil sentinel.** If a `thing_pool_get_ref` returns the nil sentinel (because the ref is stale) and you write to it --- `player->x = 100` when `player` is `&things[0]` --- you corrupt the nil sentinel. In debug builds, `thing_pool_assert_nil_clean` catches this. In release builds, it is silently overwritten. The fix: always check `thing_is_valid` before writing.

**Not understanding the pixel format.** `0xFFFF8800` is not RGB --- it is AARRGGBB. The first byte (`FF`) is alpha, not red. If your colors look wrong, check the byte order. On little-endian systems (x86, ARM), the bytes in memory are reversed: `00 88 FF FF`. The GPU texture format must match.

**Forgetting to call game_update before game_render.** The update function processes input and modifies game state. The render function reads game state and writes pixels. If you swap the order, the first frame after a key press will not show the movement --- it renders the OLD state, then updates to the new state. The pipeline is: input -> update -> render -> display.

---

## Connection to your work

If you have built web applications with React or Vue, the architecture in this lesson maps directly:

| This lesson | React equivalent |
|---|---|
| `game_state` | Component state / Redux store |
| `game_init` | Constructor / `useEffect([], ...)` |
| `game_update` | Event handlers + `useReducer` dispatch |
| `game_render` | `render()` / JSX return |
| `thing_pool` iterator | `state.entities.map(e => <Entity key={e.id} {...e} />)` |
| Platform backend | Browser / DOM / Canvas API |
| `pixels[]` buffer | `ImageData.data` |
| `draw_rect` | `ctx.fillRect()` |
| `draw_text_buf` | `ctx.fillText()` |
| `game_input` struct | `KeyboardEvent` handler state |
| Scene selector | Route switching / tab navigation |

The difference is that in React, the framework handles the rendering pipeline, the diffing, and the DOM updates. Here, you own the entire pipeline. You write every pixel. You scan every entity. There is no framework between your code and the hardware.

This is not harder --- it is more direct. And it gives you complete control over performance, memory layout, and behavior.

### The game loop vs the event loop

In JavaScript, the browser runs an event loop:

```js
// Browser event loop (simplified):
while (true) {
    processEvents();        // keydown, click, fetch callbacks
    runTimers();            // setTimeout, setInterval
    runAnimationFrame();    // requestAnimationFrame callbacks
    paint();                // browser renders DOM to screen
}
```

In our C game, the main loop does the same thing explicitly:

```c
// Game main loop:
while (!WindowShouldClose()) {
    pollInput();           // read keyboard state
    game_update(&state);   // process input, update entities
    game_render(&state);   // fill pixel buffer
    uploadToGPU();         // display the result
}
```

Same structure. Same purpose. The difference: the browser hides the loop from you. In C, you own it. You control exactly when input is read, when entities are updated, and when pixels are drawn. No framework surprises.

---

## Summary

Let us review what we built and what each piece does:

| File | Layer | Purpose |
|---|---|---|
| `things.h` + `things.c` | Pool | Entity storage, lifecycle, iteration. Zero platform deps. |
| `game.h` | Game | Types: input, state, constants. Declares the 4-function API. |
| `game.c` | Game | Implementation: init, update (stub), render, hud. Uses pool API. |
| `platform.h` | Contract | Re-exports game.h, defines GAME_TITLE and TARGET_FPS. |
| `platforms/raylib/main.c` | Platform | Raylib window, input, pixel upload. Calls game API. |
| `platforms/x11/main.c` | Platform | X11/GLX window, input, pixel upload. Same game API. |

The data flows one way: Platform reads input -> Game processes input and updates state -> Game renders state to pixels -> Platform uploads pixels to GPU.

The dependency flows one way: Platform depends on Game depends on Pool. No circular dependencies. No back-channels. No global state (except the RNG, which is explicitly stored in `game_state`).

This is the architecture pattern that will carry you through the rest of the course, through the labs, and into your own projects. The pool is the foundation. The game layer is the logic. The platform layer is the shell.

---

## What's next

The window is open. The player is visible. But nothing moves. In Lesson 12, we fill in `game_update` with:

- **Player movement** --- WASD/arrow keys move the orange rectangle
- **Troll spawning** --- blue trolls appear every second
- **Collision detection** --- moving into a troll removes it
- **Free list in action** --- new trolls land in recycled slots
- **Generational IDs in action** --- stale refs to dead trolls return nil

Every pool feature from lessons 02--10 will be exercised live on screen.
