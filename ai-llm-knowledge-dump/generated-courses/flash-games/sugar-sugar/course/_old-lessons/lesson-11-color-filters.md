# Lesson 11 — Color Filters

## What you build

You add colored filter zones to the canvas that tint any grain passing through them, and update cup acceptance logic so that colored cups only count grains with the matching color.

## Concepts introduced

- `GRAIN_COLOR` enum (`GRAIN_WHITE=0`, `GRAIN_RED=1`, `GRAIN_GREEN=2`, `GRAIN_ORANGE=3`)
- `uint8_t color[MAX_GRAINS]` per-grain field inside `GrainPool` (SoA layout)
- `ColorFilter` struct: axis-aligned bounding box plus `output_color`
- Per-frame AABB filter check that writes `grain.color[i]`
- Cup color acceptance: `GRAIN_WHITE` accepts any, others must match
- `LineBitmap` baked-grain encoding: `value = grain_color + 2`
- Rendering live grains and baked grains in their correct colors

## JS → C analogy

| JavaScript                                              | C                                                              |
|---------------------------------------------------------|----------------------------------------------------------------|
| `grain.color = 'white'`                                 | `p->color[slot] = GRAIN_WHITE;`                                |
| `if (insideFilter(g, f)) g.color = f.outputColor`       | `p->color[i] = (uint8_t)flt->output_color;`                   |
| `cup.requiredColor === null` (accept anything)          | `cup->required_color == GRAIN_WHITE`                           |
| `grain.color === cup.color`                             | `p->color[i] == (uint8_t)cup->required_color`                 |
| `canvas.getContext('2d').fillStyle = colorFor(grain)`   | `uint32_t c = g_grain_colors[p->color[i]];`                   |
| `offscreenCtx.putImageData(…)` with encoded int         | `lb->pixels[by*W+bx] = (uint8_t)(p->color[i] + 2);`          |

## Step-by-step

### Step 1: Define the `GRAIN_COLOR` enum (game.h)

Open `game.h`. The color vocabulary is a C `enum`—an integer type with named
constants:

```c
typedef enum {
    GRAIN_WHITE  = 0,   /* default — any cup accepts white grains */
    GRAIN_RED    = 1,
    GRAIN_GREEN  = 2,
    GRAIN_ORANGE = 3,
    GRAIN_COLOR_COUNT
} GRAIN_COLOR;
```

`GRAIN_WHITE = 0` means a zero-initialised grain pool slot is already
"white" — no extra setup needed.

### Step 2: Add `color[]` to `GrainPool` (game.h)

The grain pool uses a Struct-of-Arrays (SoA) layout. Add one new array:

```c
typedef struct {
    float   x[MAX_GRAINS];
    float   y[MAX_GRAINS];
    float   vx[MAX_GRAINS];
    float   vy[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];   /* ← NEW: GRAIN_COLOR value per slot */
    uint8_t active[MAX_GRAINS];
    uint8_t tpcd[MAX_GRAINS];
    uint8_t still[MAX_GRAINS];
    int     count;
} GrainPool;
```

`uint8_t` (0–255) is wide enough for four color values. Using a full `int`
would waste 3 bytes per grain and pollute the cache on the gravity loop.

### Step 3: Initialise color on spawn (game.c)

In `spawn_grains()`, every new grain starts as white:

```c
p->active[slot] = 1;
p->color[slot]  = GRAIN_WHITE;   /* ← NEW */
p->tpcd[slot]   = 0;
p->still[slot]  = 0;
```

`GRAIN_WHITE` is 0, so this is technically redundant after `memset(&state->grains, 0, …)` in `level_load`, but being explicit documents intent.

### Step 4: Define the `ColorFilter` struct (game.h)

```c
typedef struct {
    int x, y, w, h;            /* bounding box of the filter zone (pixels)  */
    GRAIN_COLOR output_color;  /* grains inside get this color               */
} ColorFilter;
```

Filters are simple axis-aligned rectangles. The game supports up to `MAX_FILTERS = 4` per level.

### Step 5: Run the filter check each frame (game.c)

Inside `update_grains()`, after the teleporter check and before the settle
check, add:

```c
/* ---- Color filter check (AABB) ---- */
{
    int gx = (int)p->x[i], gy = (int)p->y[i];
    for (int f = 0; f < lv->filter_count; f++) {
        ColorFilter *flt = &lv->filters[f];
        if (gx >= flt->x && gx < flt->x + flt->w &&
            gy >= flt->y && gy < flt->y + flt->h) {
            p->color[i] = (uint8_t)flt->output_color;
        }
    }
}
```

Key observations:
- The assignment is idempotent — the grain keeps the last matching filter's
  color if it overlaps multiple filters.
- No early exit: a grain could pass through a second filter immediately
  after the first, so we check all of them.

### Step 6: Update the cup acceptance check (game.c)

In the cup-check block inside `update_grains()`:

```c
int right_color = (cup->required_color == GRAIN_WHITE ||
                   p->color[i] == (uint8_t)cup->required_color);
if (right_color && cup->collected < cup->required_count) {
    cup->collected++;
    s_occ[gy * W + gx] = 0;
    p->active[i] = 0;
    goto next_grain;
} else if (!right_color) {
    /* Wrong color: discard silently — no penalty. */
    s_occ[gy * W + gx] = 0;
    p->active[i] = 0;
    goto next_grain;
}
```

- `GRAIN_WHITE` acts as a wildcard: any grain color satisfies it.
- A wrong-color grain entering a colored cup is silently discarded so it does not pile up inside.

### Step 7: Encode color into the `LineBitmap` on bake (game.c)

When a grain settles it is removed from the live pool and written into the
`LineBitmap` so physics can still treat it as solid:

```c
if (p->still[i] >= GRAIN_SETTLE_FRAMES) {
    int bx = (int)p->x[i], by = (int)p->y[i];
    if (bx >= 0 && bx < W && by >= 0 && by < H)
        lb->pixels[by * W + bx] = (uint8_t)(p->color[i] + 2);  /* +2 encodes color */
    p->active[i] = 0;
    goto next_grain;
}
```

The `+2` offset avoids collisions with the reserved values:

| `lb->pixels[…]` value | Meaning                                  |
|------------------------|------------------------------------------|
| `0`                    | Empty                                    |
| `1`                    | Obstacle or cup wall                     |
| `255`                  | Player-drawn line                        |
| `2` (`GRAIN_WHITE+2`)  | Baked white grain                        |
| `3` (`GRAIN_RED+2`)    | Baked red grain                          |
| `4` (`GRAIN_GREEN+2`)  | Baked green grain                        |
| `5` (`GRAIN_ORANGE+2`) | Baked orange grain                       |

Any non-zero value is treated as solid by `IS_SOLID`, so physics continues
to work without reading the color.

### Step 8: Build the grain-color lookup table (game.c)

```c
static const uint32_t g_grain_colors[GRAIN_COLOR_COUNT] = {
    [GRAIN_WHITE]  = COLOR_CREAM,
    [GRAIN_RED]    = COLOR_RED,
    [GRAIN_GREEN]  = COLOR_GREEN,
    [GRAIN_ORANGE] = COLOR_ORANGE,
};
```

These named pixel constants are defined in `game.h`:

```c
#define COLOR_CREAM  GAME_RGB(232, 213, 183)   /* default (white) grain */
#define COLOR_RED    GAME_RGB(229,  57,  53)
#define COLOR_GREEN  GAME_RGB( 67, 160,  71)
#define COLOR_ORANGE GAME_RGB(251, 140,   0)
```

### Step 9: Render live grains with their color (game.c)

In `render_grains()`, replace any hardcoded color with a lookup:

```c
static void render_grains(const GrainPool *p, GameBackbuffer *bb) {
    for (int i = 0; i < p->count; i++) {
        if (!p->active[i]) continue;
        int x = (int)p->x[i];
        int y = (int)p->y[i];
        uint32_t c = g_grain_colors[p->color[i]];   /* ← color lookup */
        draw_pixel(bb, x,     y,     c);
        draw_pixel(bb, x + 1, y,     c);
        draw_pixel(bb, x,     y + 1, c);
        draw_pixel(bb, x + 1, y + 1, c);
    }
}
```

### Step 10: Render baked grains with their color (game.c)

In `render_lines()`, decode the `+2` encoding:

```c
static void render_lines(const LineBitmap *lb, GameBackbuffer *bb) {
    int total = CANVAS_W * CANVAS_H;
    for (int i = 0; i < total; i++) {
        uint8_t v = lb->pixels[i];
        if (!v) continue;
        bb->pixels[i] = (v >= 2 && v <= 5)
            ? g_grain_colors[v - 2]   /* baked grain: use encoded color */
            : COLOR_LINE;              /* drawn line or obstacle wall    */
    }
}
```

### Step 11: Level 9 walkthrough — first color puzzle

From `levels.c`, Level 9 (index 8):

```c
[8] = {
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 240) },
    .cup_count     = 2,
    .cups          = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490, 370, 85, 100, GRAIN_WHITE,  100),
    },
    .filter_count  = 1,
    .filters       = { FILTER(60, 180, 100, 22, GRAIN_RED) },
},
```

Strategy:
1. Sugar falls from x=320. Left cup requires red; right cup accepts any.
2. Draw a line from the emitter leftward so some grains pass through the
   red filter at (60, 180).
3. Grains that pass through the filter become red and fall into the left cup.
4. Grains that fall straight down (or are routed right) remain white and
   fill the right cup.

### Step 12: Render colored filter zones (game.c)

In `render_filters()`, use semi-transparent color bands:

```c
static void render_filters(const LevelDef *lv, GameBackbuffer *bb) {
    static const uint32_t filter_colors[GRAIN_COLOR_COUNT] = {
        [GRAIN_WHITE]  = COLOR_CREAM,
        [GRAIN_RED]    = COLOR_RED,
        [GRAIN_GREEN]  = COLOR_GREEN,
        [GRAIN_ORANGE] = COLOR_ORANGE,
    };
    for (int f = 0; f < lv->filter_count; f++) {
        const ColorFilter *flt = &lv->filters[f];
        draw_rect_blend(bb, flt->x, flt->y, flt->w, flt->h,
                        filter_colors[flt->output_color], 140);
        draw_rect_outline(bb, flt->x, flt->y, flt->w, flt->h,
                          filter_colors[flt->output_color]);
    }
}
```

`draw_rect_blend` (alpha=140 out of 255) lets the background show through
the filter band, making it visually clear without obscuring the playing field.

## Common mistakes to prevent

- **Forgetting `+2` when baking**: writing `lb->pixels[…] = p->color[i]`
  instead of `p->color[i] + 2` would store `0` for white grains (invisible)
  and `1` for red grains (rendered as an obstacle line).
- **Rendering baked grains as `COLOR_LINE`**: checking `if (v != 0)` and
  always using `COLOR_LINE` ignores the color encoding; settled colored
  grains appear grey/dark.
- **Checking `v > 1` without an upper bound**: values 2–5 are baked grains;
  a `v > 5` value would index `g_grain_colors` out of bounds. Always check
  `v >= 2 && v <= 5` (or `v >= 2 && v < 2 + GRAIN_COLOR_COUNT`).
- **Using `int` for `color[]`**: wastes 3 bytes per grain; `uint8_t` is the
  correct type since values are 0–3.
- **Wrong-color grain not discarded**: if a red grain enters a white cup and
  `right_color` is not checked, `cup->collected` increments incorrectly.

## What to run

```bash
# From the project root
./build-dev.sh          # builds both X11 and Raylib binaries

# X11
./sugar_x11             # run the X11 version
# Navigate to Level 9 (index 8) on the title screen

# Raylib
./sugar_raylib          # run the Raylib version
```

Verify:
1. Grains falling through the red filter band turn red (not white/cream).
2. Red grains enter the left cup; cream grains enter the right cup.
3. Settled grains on a line stay red/cream rather than turning dark grey.
4. The filter band is semi-transparent (you can see lines drawn underneath it).

## Summary

This lesson wires the full color pipeline: a `GRAIN_COLOR` enum provides
four integer color IDs; each grain carries its ID in a `uint8_t color[]`
SoA field; the per-frame AABB filter check paints grains the moment they
enter a filter zone; the cup acceptance logic treats `GRAIN_WHITE` as a
wildcard while enforcing exact matches for colored cups; and the
`LineBitmap` stores the color ID using a `+2` offset that keeps the
encoding compatible with the existing 0/1/255 physics scheme, so both live
and baked grains render in their correct colors with a single lookup table.
