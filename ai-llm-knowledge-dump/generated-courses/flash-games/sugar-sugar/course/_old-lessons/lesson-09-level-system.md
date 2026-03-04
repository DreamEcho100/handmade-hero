# Lesson 09 — Level System

## What you build

You create `levels.c` — a separate translation unit containing all 30 level
definitions as a static global array — and wire it to `game.c` through an
`extern` declaration in `game.h`. You implement `level_load()`, which copies a
level definition into the game state and stamps obstacles and cup walls into
`LineBitmap`.

## Concepts introduced

- C99 designated initialisers (`[0] = { .field = val }`)
- Zero-initialisation of unspecified struct fields
- `extern` declarations vs. definitions across two `.c` files
- Struct copy (`state->level = g_levels[idx]`)
- `level_load()`: copy, clear, reset, stamp
- Two-translation-unit compilation (`game.c` + `levels.c` → one binary)
- Why keeping level data in its own file speeds recompilation

## JS → C analogy

| JavaScript                                         | C                                                          |
|----------------------------------------------------|------------------------------------------------------------|
| `const levels = [...]` in `levels.js`              | `LevelDef g_levels[TOTAL_LEVELS] = { ... }` in `levels.c` |
| `export default levels`                            | `extern LevelDef g_levels[TOTAL_LEVELS]` in `game.h`       |
| `import levels from './levels.js'`                 | `#include "game.h"` in `game.c` (declaration only)         |
| `const state = { ...levels[idx] }`                 | `state->level = g_levels[idx]` (struct copy)               |
| `state.grains = []`                                | `memset(&state->grains, 0, sizeof(state->grains))`         |

## Step-by-step

### Step 1: Understand the `LevelDef` struct

`LevelDef` is defined in `game.h`. It holds all entities that make up one
puzzle:

```c
typedef struct {
    int index;
    Emitter     emitters[MAX_EMITTERS];     int emitter_count;
    Cup         cups[MAX_CUPS];             int cup_count;
    ColorFilter filters[MAX_FILTERS];       int filter_count;
    Teleporter  teleporters[MAX_TELEPORTERS]; int teleporter_count;
    Obstacle    obstacles[MAX_OBSTACLES];   int obstacle_count;
    int has_gravity_switch;
    int is_cyclic;
} LevelDef;
```

Every array has a parallel `_count` field. Code that loops over emitters always
loops `for (int i = 0; i < lv->emitter_count; i++)` — it never reads past the
populated entries.

### Step 2: Create `levels.c` with designated initialisers

Create a new file `src/levels.c`. Include `game.h` for the types. Define the
global array:

```c
#include "game.h"

/* Helper macros to keep the data readable */
#define EMITTER(px, py, rate)          { (px), (py), (rate), 0.0f }
#define CUP(cx,cy,cw,ch, col, req)     { (cx),(cy),(cw),(ch), (col),(req), 0 }
#define FILTER(fx,fy,fw,fh, col)       { (fx),(fy),(fw),(fh), (col) }
#define OBS(ox,oy,ow,oh)               { (ox),(oy),(ow),(oh) }
#define TELE(ax,ay,bx,by,r)            { (ax),(ay),(bx),(by),(r) }

LevelDef g_levels[TOTAL_LEVELS] = {

[0] = {
    .index         = 0,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 240) },
    .cup_count     = 1,
    .cups          = { CUP(278, 370, 85, 100, GRAIN_WHITE, 150) },
},

[1] = {
    .index         = 1,
    .emitter_count = 1,
    .emitters      = { EMITTER(60, 20, 240) },
    .cup_count     = 1,
    .cups          = { CUP(490, 370, 85, 100, GRAIN_WHITE, 150) },
},

/* ... 28 more levels ... */

};
```

#### Designated initialisers explained

The syntax `[0] = { .field = val }` is a **C99 designated initialiser**. It
lets you name the array index and struct field being initialised rather than
relying on positional order.

Benefits:

1. **Self-documenting**: `.cup_count = 1` is unambiguous; the 12th positional
   argument is not.
2. **Zero-fill for free**: any field you do not mention is zero-initialised by
   the compiler. Level 1 has `.filter_count = 0`, `.teleporter_count = 0` etc.
   automatically — you do not have to write them.
3. **Re-ordering safety**: if you add a new field to `LevelDef` in the middle,
   existing designated initialisers still compile correctly; positional ones
   would shift and silently corrupt data.

Example — Level 5 adds obstacles:

```c
[4] = {
    .index           = 4,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 240) },
    .cup_count       = 1,
    .cups            = { CUP(50, 370, 85, 100, GRAIN_WHITE, 150) },
    .obstacle_count  = 1,
    .obstacles       = { OBS(150, 180, 340, 14) },
    /* .filter_count, .teleporter_count, etc. → implicitly 0 */
},
```

#### Why a separate file?

`levels.c` is ~500 lines of static data. `game.c` is ~1400 lines of logic.
With two separate `.c` files, the compiler only recompiles `game.c` when you
change game logic, and only recompiles `levels.c` when you tweak a level. If
both were in one file, every logic fix would force re-parsing 500 lines of
level data — wasted work at bigger scales.

### Step 3: Declare `g_levels` in `game.h`

`game.h` already contains this line:

```c
#define TOTAL_LEVELS 30
extern LevelDef g_levels[TOTAL_LEVELS];
```

**`extern` means "this symbol exists somewhere; trust me, the linker will find
it".** `game.h` is the declaration. `levels.c` is the definition (the actual
storage). Both `game.c` and any other `.c` file that includes `game.h` can
reference `g_levels` without owning a copy of it.

If you defined `g_levels` inside `game.h` (without `extern`), every `.c` file
that includes `game.h` would try to define its own copy. The linker would see
two symbols named `g_levels` and report a "duplicate symbol" (or "multiple
definition") error.

```
/* ❌ Wrong — defined in a header included by multiple .c files */
LevelDef g_levels[30] = { ... };    /* in game.h */

/* ✅ Correct */
extern LevelDef g_levels[30];       /* declaration in game.h */
LevelDef g_levels[30] = { ... };    /* definition  in levels.c */
```

### Step 4: Implement `level_load()`

`level_load()` is a `static` (file-private) function in `game.c`:

```c
static void level_load(GameState *state, int index) {
    /* 1. Struct copy: pull the level definition into the live state. */
    state->level = g_levels[index];
    state->current_level = index;

    /* 2. Clear all active grains. */
    memset(&state->grains, 0, sizeof(state->grains));

    /* 3. Clear the line bitmap (player-drawn lines + baked grains). */
    memset(&state->lines, 0, sizeof(state->lines));

    /* 4. Reset emitter timers in the COPIED level (not the global one). */
    for (int i = 0; i < state->level.emitter_count; i++)
        state->level.emitters[i].spawn_timer = 0.0f;

    /* 5. Reset cup collected counters. */
    for (int i = 0; i < state->level.cup_count; i++)
        state->level.cups[i].collected = 0;

    /* 6. Reset gravity to normal. */
    state->gravity_sign = 1;

    /* 7. Stamp obstacles and cup walls into the line bitmap. */
    stamp_obstacles(state);
    stamp_cups(state);
}
```

#### Struct copy (`state->level = g_levels[index]`)

In C, assigning one struct to another copies all fields by value — just like
copying an object with a spread in JS (`{ ...g_levels[index] }`). `LevelDef`
contains no pointers, so a shallow copy is a complete copy.

After the copy `state->level` is independent of `g_levels[index]`. Modifying
`state->level.cups[0].collected` (as the simulation does each frame) does not
touch the original global array. This is why resetting the level (`R` key)
works: `level_load` copies the pristine original again.

#### Why `memset` for grains?

```c
memset(&state->grains, 0, sizeof(state->grains));
```

`GrainPool` is a struct-of-arrays. After `memset`:

- `grains.active[i] = 0` for all i — every slot is free.
- `grains.count = 0` — the high-watermark resets.
- `grains.x[i] = grains.y[i] = 0.0f` — no stale positions.

This is safe because `GrainPool` contains only `float` and `uint8_t` arrays;
zeroing them is always a valid initial state.

#### Why clear `lines` before stamping?

```c
memset(&state->lines, 0, sizeof(state->lines));
stamp_obstacles(state);
stamp_cups(state);
```

Clearing `lines` wipes player-drawn strokes and baked grains from the previous
level run. Then `stamp_obstacles` and `stamp_cups` write fresh `1` bytes for
the new level's solid geometry. The order matters: clear first, then stamp.

### Step 5: `stamp_obstacles()`

```c
static void stamp_obstacles(GameState *state) {
    LevelDef *lv = &state->level;
    for (int i = 0; i < lv->obstacle_count; i++) {
        Obstacle *o = &lv->obstacles[i];
        for (int py = o->y; py < o->y + o->h; py++) {
            for (int px = o->x; px < o->x + o->w; px++) {
                if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H)
                    state->lines.pixels[py * CANVAS_W + px] = 1;
            }
        }
    }
}
```

Obstacles become indistinguishable from player-drawn lines in the physics
layer — both are `1` bytes in `LineBitmap`. The renderer uses a separate
`render_obstacles()` call (drawing `COLOR_OBSTACLE` over the same rectangles)
to give them a different visual color.

### Step 6: `stamp_cups()`

Cups are stamped as three walls: left, right, and bottom. The top is left open
so grains can fall in.

```c
static void stamp_cups(GameState *state) {
    LevelDef *lv = &state->level;
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

The cup interior columns `[x+1 .. x+w-2]` are left empty. This matters for
the absorption check in Lesson 10: grains at exactly `x` or `x+w-1` are
touching the wall pixel and are not absorbed — only grains inside the interior
are counted.

### Step 7: Update `build-dev.sh`

Add `levels.c` to the compilation command so both translation units are linked
together:

```bash
# Raylib build
gcc -o sugar_sugar_raylib \
    src/game.c src/levels.c src/main_raylib.c \
    -lraylib -lm -Wall -Wextra

# X11 build
gcc -o sugar_sugar_x11 \
    src/game.c src/levels.c src/main_x11.c \
    -lX11 -lm -Wall -Wextra
```

The linker resolves `extern LevelDef g_levels[30]` (referenced in `game.c`)
to the definition in `levels.c`. If you forget `levels.c`, the linker emits:

```
undefined reference to `g_levels'
```

## Common mistakes to prevent

- **Defining `g_levels` in `game.c` AND `levels.c`**: the linker sees two
  definitions of the same global symbol and reports "multiple definition of
  `g_levels`". The definition belongs only in `levels.c`.
- **Defining `g_levels` inside `game.h`** (without `extern`): every `.c` file
  that includes `game.h` compiles its own definition → linker "duplicate
  symbol" error. Headers declare; source files define.
- **Forgetting to reset `cups[i].collected`**: if you load a level, fill a
  cup, press Reset, and the count is not zeroed, `check_win` sees a pre-filled
  cup and may immediately trigger level complete on reload.
- **Modifying `g_levels[idx]` directly** (e.g. `g_levels[idx].cups[0].collected++`):
  `g_levels` is the pristine master copy. Mutations should go to
  `state->level`, which is the working copy. Mutating the global means
  resetting the level no longer restores the original state.
- **Stamping obstacles before clearing `lines`**: leftover baked grains from
  the previous run would survive the load as phantom solid pixels.

## What to run

```bash
./build-dev.sh
```

Verify the level system:

1. Level 1 loads with a single centered emitter and one cup. Sugar falls
   straight in without drawing anything.
2. Level 5 (`index = 4`) loads with a horizontal shelf obstacle. The brown
   shelf is visible and solid — grains pile on top of it.
3. Pressing `R` resets the level: grains disappear, drawn lines vanish, and
   the cup counter resets to 0.
4. Completing level 1 loads level 2 automatically (emitter now on the left,
   cup on the right — you must draw a slope).

## Summary

The level system separates static level data from runtime game logic by placing
all 30 `LevelDef` definitions in `levels.c` and sharing them through an
`extern` declaration in `game.h`. C99 designated initialisers make each level's
fields self-documenting and automatically zero-fill omitted values. `level_load`
performs a value-copy of the chosen definition into `state->level` so the
simulation can freely mutate its working copy without touching the pristine
global array. Stamping obstacles and cup walls into `LineBitmap` immediately
after the copy means the physics engine has no special knowledge of either —
they are just non-zero bytes in the same flat bitmap as player-drawn lines.
