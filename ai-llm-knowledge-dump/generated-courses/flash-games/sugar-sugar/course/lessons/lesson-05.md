# Lesson 05: Grain Pool (Struct-of-Arrays) and the Build Script

## What we're building

Replace the single `Grain` with a `GrainPool` that holds up to 4096 grains
simultaneously.  Add a slow emitter that spawns new grains from the top center
every frame so you can watch them pile up.  Also introduce `build-dev.sh` so
you never have to type the long `clang` command again.

## What you'll learn

- **Struct-of-Arrays (SoA) vs Array-of-Structs (AoS)** — cache performance
- Pool allocation with a high-watermark counter
- The `build-dev.sh` script — backend selection, always-debug, `-r` flag
- Why `static` local arrays in C live in BSS, not on the stack
- Color lookup tables with designated initialisers

## Prerequisites

- Lesson 04 complete (single grain working)

---

## Step 1: Struct-of-Arrays — why and how

In Lesson 04 we had one grain with fields `x, y, vx, vy`.  With 4096 grains
the naive choice is an array of structs:

```c
/* AoS (Array of Structs) — naive layout */
struct Grain { float x, y, vx, vy; uint8_t color, active; };
struct Grain pool[4096];
```

When the physics loop iterates over `pool[i].vy += GRAVITY * dt`, it loads the
entire `Grain` struct (at least 20 bytes) into cache for every iteration, even
though this step only needs `vy`.  Color and active are loaded for free and then
ignored — wasted cache bandwidth.

The **Struct of Arrays** layout flips this:

```c
/* SoA (Struct of Arrays) — cache-friendly layout */
typedef struct {
  float   x[MAX_GRAINS];      /* all X positions in one block        */
  float   y[MAX_GRAINS];
  float   vx[MAX_GRAINS];
  float   vy[MAX_GRAINS];
  uint8_t color[MAX_GRAINS];
  uint8_t active[MAX_GRAINS];
  uint8_t still[MAX_GRAINS];  /* settle counter — added in Lesson 07 */
  uint8_t tpcd[MAX_GRAINS];   /* teleport cooldown — added in Lesson 12 */
  int     count;              /* high-watermark of used slots */
} GrainPool;
```

The gravity loop now touches only `vy[0..N]` — a single contiguous 16 KB block
that fits entirely in L1 cache.  `color[]` and `active[]` are in completely
separate cache lines and are never loaded during the gravity pass.

**JS analogy:** SoA is like storing an array of objects as separate typed arrays
(`Float32Array` for x, `Float32Array` for y…) instead of one big `ArrayBuffer`
with structs packed inside.  `Float32Array` is what GPU pipelines actually prefer.

---

## Step 2: `GrainPool` in `game.h`

```c
/* src/game.h  —  GrainPool and grain colors (Lesson 05) */

#define MAX_GRAINS 4096

/* Grain color enum — GRAIN_WHITE means "accepts any cup" */
typedef enum {
  GRAIN_WHITE  = 0,
  GRAIN_RED    = 1,
  GRAIN_GREEN  = 2,
  GRAIN_ORANGE = 3,
  GRAIN_COLOR_COUNT
} GRAIN_COLOR;

typedef struct {
  float   x[MAX_GRAINS];
  float   y[MAX_GRAINS];
  float   vx[MAX_GRAINS];
  float   vy[MAX_GRAINS];
  uint8_t color[MAX_GRAINS];
  uint8_t active[MAX_GRAINS];
  uint8_t still[MAX_GRAINS];  /* consecutive still frames (Lesson 07) */
  uint8_t tpcd[MAX_GRAINS];   /* teleport cooldown (Lesson 12)        */
  int     count;
} GrainPool;

/* Updated GameState */
typedef struct {
  int should_quit;
  int mouse_x, mouse_y;
  GrainPool grains;      /* replaces single Grain from Lesson 04 */
  float spawn_timer;     /* seconds since last grain spawn       */
} GameState;
```

---

## Step 3: Pool allocation and spawning

```c
/* src/game.c  —  grain_alloc: find a free slot */

/* Returns the index of a free slot (active==0), or -1 if the pool is full.
 *
 * Why a high-watermark instead of always scanning from 0?
 * In the first few seconds most of the 4096 slots are unused.  Scanning
 * from 0 would find a free slot in slot 0 every time, until the first grain
 * moves on and vacates slot 0.  The high-watermark grows as slots fill up;
 * once a grain is freed (active=0) the NEXT allocation re-uses that slot.
 *
 * In the steady state (grains settling and being recycled), the average
 * scan length is short because freed slots reappear at low indices. */
static int grain_alloc(GrainPool *p) {
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) return i;
  }
  if (p->count < MAX_GRAINS) {
    return p->count++;
  }
  return -1;  /* pool full */
}

/* src/game.c  —  spawn_grains: emit one grain per fixed interval */
static void spawn_grains(GameState *state, float dt) {
  state->spawn_timer += dt;
  float interval = 1.0f / 60.0f;  /* 60 grains per second */

  while (state->spawn_timer >= interval) {
    state->spawn_timer -= interval;

    int slot = grain_alloc(&state->grains);
    if (slot < 0) break;  /* pool full */

    /* Spawn near the top center with a tiny random x spread so grains
     * don't all pile on the exact same column. */
    int spread = (rand() % 7) - 3;  /* -3 .. +3 */

    state->grains.active[slot] = 1;
    state->grains.color[slot]  = GRAIN_WHITE;
    state->grains.still[slot]  = 0;
    state->grains.tpcd[slot]   = 0;
    state->grains.x[slot]  = (float)(CANVAS_W / 2 + spread);
    state->grains.y[slot]  = 10.0f;
    state->grains.vx[slot] = 0.0f;
    state->grains.vy[slot] = 0.0f;
  }
}
```

---

## Step 4: Update `game_update` to handle the pool

```c
/* src/game.c  —  game_update with pool update */

/* Color lookup table: GRAIN_COLOR enum → pixel color.
 * Defined as static const here in game.c so it compiles once.
 * Lesson 09 moves this to a proper extern const declaration. */
static const uint32_t g_grain_colors[GRAIN_COLOR_COUNT] = {
  [GRAIN_WHITE]  = COLOR_CREAM,
  [GRAIN_RED]    = COLOR_RED,
  [GRAIN_GREEN]  = COLOR_GREEN,
  [GRAIN_ORANGE] = COLOR_ORANGE,
};

static void update_grains(GameState *state, float dt) {
  GrainPool *p = &state->grains;
  int W = CANVAS_W, H = CANVAS_H;
  float grav = GRAVITY;  /* positive = downward */

  for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;

    /* Gravity */
    p->vy[i] += grav * dt;
    if (p->vy[i] >  MAX_VY) p->vy[i] =  MAX_VY;
    if (p->vy[i] < -MAX_VY) p->vy[i] = -MAX_VY;
    if (p->vx[i] >  MAX_VX) p->vx[i] =  MAX_VX;
    if (p->vx[i] < -MAX_VX) p->vx[i] = -MAX_VX;

    /* Sub-step integration */
    float total_dy = p->vy[i] * dt;
    float abs_dy   = total_dy < 0 ? -total_dy : total_dy;
    int   steps    = (int)abs_dy + 1;
    if (steps > 32) steps = 32;
    float sdy = total_dy / (float)steps;

    for (int s = 0; s < steps; s++) {
      float ny = p->y[i] + sdy;
      int   iy = (int)ny;

      if (iy >= H) {
        /* Hit the floor */
        p->y[i] = (float)(H - 1);
        float imp = p->vy[i] < 0 ? -p->vy[i] : p->vy[i];
        p->vy[i] = (imp > GRAIN_BOUNCE_MIN) ? -imp * GRAIN_BOUNCE : 0.0f;
        break;
      }
      if (iy < 0) {
        p->y[i]  = 0.0f;
        p->vy[i] = 0.0f;
        break;
      }
      p->y[i] = ny;
    }

    /* Horizontal movement + wall clamp */
    p->x[i] += p->vx[i] * dt;
    if (p->x[i] < 0.0f)                  { p->x[i] = 0.0f;               p->vx[i] = 0.0f; }
    if (p->x[i] >= (float)W)             { p->x[i] = (float)(W-1);        p->vx[i] = 0.0f; }

    /* Remove grains that fall off the bottom
     * (will not happen with floor collision, but guards against bugs) */
    if ((int)p->y[i] >= H) {
      p->active[i] = 0;
    }
  }
}

void game_update(GameState *state, GameInput *input, float dt) {
  if (dt > 0.1f) dt = 0.1f;
  state->mouse_x = input->mouse.x;
  state->mouse_y = input->mouse.y;
  if (BUTTON_PRESSED(input->escape)) state->should_quit = 1;

  spawn_grains(state, dt);
  update_grains(state, dt);
}
```

---

## Step 5: Render all active grains

```c
/* src/game.c  —  game_render for Lesson 05 */
void game_render(const GameState *state, GameBackbuffer *bb) {
  /* Clear canvas */
  int total = bb->width * bb->height;
  for (int i = 0; i < total; i++)
    bb->pixels[i] = COLOR_BG;

  /* Draw all active grains.
   * Each grain is a single pixel — fast and correct for a sand simulation
   * where every grain corresponds to exactly one canvas pixel. */
  const GrainPool *p = &state->grains;
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i]) continue;
    int gx = (int)p->x[i];
    int gy = (int)p->y[i];
    uint32_t color = g_grain_colors[p->color[i] < GRAIN_COLOR_COUNT
                                     ? p->color[i] : GRAIN_WHITE];
    draw_pixel(bb, gx, gy, color);
  }

  /* Cursor circle */
  draw_circle(bb, state->mouse_x, state->mouse_y, 4, COLOR_LINE);
}
```

---

## Step 6: Introduce `build-dev.sh`

Now that we have two `.c` files to compile, a build script saves typing and
enforces consistent debug flags.

```bash
# build-dev.sh  —  Sugar, Sugar | Developer Build Script

#!/usr/bin/env bash
set -e  # exit immediately on error

BACKEND="raylib"
RUN_AFTER_BUILD=0

for arg in "$@"; do
  case "$arg" in
    --backend=x11)    BACKEND="x11"    ;;
    --backend=raylib) BACKEND="raylib" ;;
    -r|--run)         RUN_AFTER_BUILD=1 ;;
    *)
      echo "Unknown argument: $arg"
      echo "Usage: $0 [--backend=x11|raylib] [-r]"
      exit 1
    ;;
  esac
done

# Prefer clang, fall back to gcc
if   command -v clang > /dev/null 2>&1; then CC="clang"
elif command -v gcc   > /dev/null 2>&1; then CC="gcc"
else echo "Error: neither clang nor gcc found."; exit 1
fi

# Always debug — this is a dev build script.
# -O0 -g            : no optimisation, full debug symbols
# -DDEBUG           : enable ASSERT() macros
# -fsanitize=...    : AddressSanitizer + UndefinedBehaviorSanitizer
# -Wall -Wextra     : catch common mistakes
DEBUG_FLAGS="-O0 -g -DDEBUG -fsanitize=address,undefined"
COMMON_FLAGS="-Wall -Wextra -std=c99 $DEBUG_FLAGS"

mkdir -p build       # create output dir before compiling
OUT="build/game"     # always build/game — never per-backend names

SHARED_SRCS="src/game.c"

if [ "$BACKEND" = "x11" ]; then
  SRCS="src/main_x11.c $SHARED_SRCS"
  LIBS="-lX11 -lm"
else
  SRCS="src/main_raylib.c $SHARED_SRCS"
  LIBS="-lraylib -lm -lpthread -ldl"
fi

CMD="$CC $COMMON_FLAGS -o $OUT $SRCS $LIBS"
echo "Backend: $BACKEND | Output: ./$OUT"
echo "Running: $CMD"
$CMD
chmod +x "$OUT"
echo "Build successful → ./$OUT"

if [ "$RUN_AFTER_BUILD" = "1" ]; then
  echo "Running ./$OUT ..."
  ./"$OUT"
fi
```

**Build script rules (from course-builder.md):**

| Rule | Why |
|------|-----|
| Output always `./build/game` | Never per-backend names like `game_x11` — one binary, different backends |
| Always `-O0 -g -DDEBUG -fsanitize=…` | No release mode in the dev script — optimisers hide bugs |
| `-r` means "run after build" | Not "release mode" — never repurpose this flag |
| `mkdir -p build` before compile | Prevents a cryptic "no such file" error on a fresh checkout |
| Default backend: raylib | Matches the course reference |

Usage:

```bash
chmod +x build-dev.sh
./build-dev.sh                      # Raylib, debug
./build-dev.sh -r                   # Raylib, debug, then run
./build-dev.sh --backend=x11 -r    # X11, debug, then run
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:** A stream of cream-colored grains falls from the top center and piles up on the floor.  After a few seconds the pile grows upward.

---

## Exercises

1. Change the emission rate from 60 to 200 grains/second.  What happens to the pile?  Try 10 grains/second.
2. Add a second emitter at x=100.  How does the SoA layout compare to an AoS layout when you add this second emitter?
3. `grain_alloc` scans linearly from 0.  Profile it (print how many slots it scans on average) when the pool is 50% full.

---

## What's next

In Lesson 06 we add a `LineBitmap` — the key data structure that lets grains
collide with player-drawn lines and with each other.
