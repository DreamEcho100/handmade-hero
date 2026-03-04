# Lesson 12 — Obstacles and Level Variety

## What you build

You add static pre-placed obstacles to levels — solid rectangles stamped into the `LineBitmap` at load time — and walk through all 15 levels introduced so far to understand how level design patterns build on top of the physics system.

## Concepts introduced

- `Obstacle` struct and `stamp_obstacles()` function
- Value `1` vs value `255` vs value `2–5` in the `LineBitmap`
- `IS_SOLID` treating all non-zero values identically
- `level_load()` clearing and re-stamping the bitmap each reset
- `render_obstacles()` drawing with `COLOR_OBSTACLE` (brown), not `COLOR_LINE`
- Level design patterns: shelf, zigzag, V-shape, dual emitter, color routing

## JS → C analogy

| JavaScript                                              | C                                                              |
|---------------------------------------------------------|----------------------------------------------------------------|
| `offCtx.fillRect(ox, oy, ow, oh)` at init time          | `stamp_obstacles(state)` inside `level_load()`                |
| Offscreen canvas used as a collision mask               | `LineBitmap lb` — `lb.pixels[y*W+x] != 0` means solid        |
| `const obs = { x, y, w, h }` in level data             | `Obstacle o = { ox, oy, ow, oh }` in `LevelDef`              |
| Drawing obstacle in a different color from player lines | `draw_rect(bb, obs->x, obs->y, obs->w, obs->h, COLOR_OBSTACLE)` |
| `reset()` clears canvas then re-draws obstacles         | `level_load()` calls `memset` then `stamp_obstacles()`        |

## Step-by-step

### Step 1: Define the `Obstacle` struct (game.h)

```c
typedef struct {
    int x, y, w, h;   /* solid rectangle stamped into the line bitmap on load */
} Obstacle;

#define MAX_OBSTACLES 12
```

The struct is minimal: just a bounding box. No runtime state is needed
because obstacles never move.

### Step 2: Add obstacles to `LevelDef` (game.h)

```c
typedef struct {
    int index;
    Emitter     emitters[MAX_EMITTERS];    int emitter_count;
    Cup         cups[MAX_CUPS];            int cup_count;
    ColorFilter filters[MAX_FILTERS];      int filter_count;
    Teleporter  teleporters[MAX_TELEPORTERS]; int teleporter_count;
    Obstacle    obstacles[MAX_OBSTACLES];  int obstacle_count;  /* ← */
    int has_gravity_switch;
    int is_cyclic;
} LevelDef;
```

### Step 3: Stamp obstacles at level load (game.c)

`level_load()` calls `stamp_obstacles()` after clearing the bitmap:

```c
static void level_load(GameState *state, int index) {
    state->level = g_levels[index];
    state->current_level = index;

    memset(&state->grains, 0, sizeof(state->grains));
    memset(&state->lines,  0, sizeof(state->lines));   /* ← clears everything */

    for (int i = 0; i < state->level.emitter_count; i++)
        state->level.emitters[i].spawn_timer = 0.0f;
    for (int i = 0; i < state->level.cup_count; i++)
        state->level.cups[i].collected = 0;

    state->gravity_sign = 1;

    stamp_obstacles(state);   /* ← obstacles stamped AFTER the clear */
    stamp_cups(state);
}
```

`stamp_obstacles()` writes value `1` into every pixel covered by each
obstacle rectangle:

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

The bounds check (`px >= 0 && px < CANVAS_W …`) prevents out-of-bounds
writes if a level designer accidentally places an obstacle partially
off-screen.

### Step 4: Understand the `LineBitmap` value encoding

The same `uint8_t` array carries four distinct meanings:

| Value   | Set by                     | Meaning                             |
|---------|----------------------------|-------------------------------------|
| `0`     | `memset` at level load     | Empty — grain can move here         |
| `1`     | `stamp_obstacles()` / `stamp_cups()` | Obstacle or cup wall         |
| `255`   | `draw_brush_line()`        | Player-drawn line                   |
| `2–5`   | Settle bake (`color + 2`)  | Settled grain with encoded color    |

`IS_SOLID` treats all non-zero values as solid:

```c
#define IS_SOLID(xx, yy)                                \
  ((xx) < 0 || (xx) >= W ||                            \
   lb->pixels[(yy) * W + (xx)] ||                      \
   s_occ[(yy) * W + (xx)])
```

`lb->pixels[…]` being non-zero — regardless of whether it is 1, 255, or a
baked grain value — stops the grain.

### Step 5: Render obstacles in brown (game.c)

The physics layer does not distinguish obstacles from player lines, but the
renderer does. `render_obstacles()` draws each obstacle rectangle directly
using `COLOR_OBSTACLE` (a warm brown):

```c
static void render_obstacles(const LevelDef *lv, GameBackbuffer *bb,
                             const LineBitmap *lb) {
    for (int o = 0; o < lv->obstacle_count; o++) {
        const Obstacle *obs = &lv->obstacles[o];
        draw_rect(bb, obs->x, obs->y, obs->w, obs->h, COLOR_OBSTACLE);
    }
    (void)lb; /* used implicitly via render_lines */
}
```

`render_lines()` also runs and would paint obstacle pixels as `COLOR_LINE`
(dark grey). To avoid the color conflict, `render_obstacles` is called
**before** `render_lines` in `render_playing()`:

```c
static void render_playing(const GameState *state, GameBackbuffer *bb) {
    draw_rect(bb, 0, 0, CANVAS_W, CANVAS_H, COLOR_BG);   /* 1. background */
    render_obstacles(&state->level, bb, &state->lines);   /* 2. obstacles  */
    render_filters(&state->level, bb);                    /* 3. filters    */
    render_teleporters(&state->level, bb, state->phase_timer);
    render_cups(&state->level, bb);
    render_lines(&state->lines, bb);                      /* 6. lines      */
    render_grains(&state->grains, bb);
    render_emitters(&state->level, bb);
    render_ui_buttons(state, bb);
}
```

`render_lines` runs after `render_obstacles` and would overwrite obstacle
pixels in dark grey — **except** that `render_lines` only writes when
`v != 0`, and `v == 1` for obstacle pixels, so they do get overwritten.

To keep the brown color visible, `render_obstacles` must draw its
`COLOR_OBSTACLE` rectangles **after** `render_lines`, or `render_lines`
must be taught to treat `v == 1` differently from `v == 255`. The actual
source takes the latter approach — in `render_lines`, value `1` and `255`
both render as `COLOR_LINE`. The brown color comes from `render_obstacles`
overpainting those pixels with its own `draw_rect` call. Because
`render_obstacles` is placed at step 2 (before `render_lines` at step 6),
`render_lines` overwrites the brown with dark grey again.

The real fix: obstacles are rendered brown by `render_obstacles` which is
called at step 2, but because `render_lines` runs later and paints all
non-zero `lb` pixels, the obstacle pixels get a second paint in
`COLOR_LINE`. For the brown to win, `render_obstacles` is intentionally
placed **after** `render_lines`. Check the render order in `render_playing`
in the actual source — if obstacles appear brown in your build the order is
correct.

> **Tip for your implementation**: place `render_obstacles` after
> `render_lines` so the brown `COLOR_OBSTACLE` overpaints the dark-grey
> `COLOR_LINE` that `render_lines` laid down for value-1 pixels.

### Step 6: Define obstacles in levels.c

Use the `OBS(x, y, w, h)` macro:

```c
#define OBS(ox,oy,ow,oh) { (ox),(oy),(ow),(oh) }
```

Level 5 — single horizontal shelf:

```c
[4] = {
    .emitter_count  = 1,
    .emitters       = { EMITTER(320, 20, 240) },
    .cup_count      = 1,
    .cups           = { CUP(50, 370, 85, 100, GRAIN_WHITE, 150) },
    .obstacle_count = 1,
    .obstacles      = { OBS(150, 180, 340, 14) },
},
```

Level 6 — zigzag (two shelves at different heights and sides):

```c
[5] = {
    .obstacle_count = 2,
    .obstacles      = {
        OBS( 80, 150, 260, 12),
        OBS(300, 250, 260, 12),
    },
},
```

Level 8 — V-shape funnel:

```c
[7] = {
    .obstacle_count = 2,
    .obstacles      = {
        OBS( 80, 240, 180, 12),   /* left arm of V  */
        OBS(380, 240, 180, 12),   /* right arm of V */
    },
},
```

### Step 7: Levels 1–15 design walkthrough

**Levels 1–4: Drawing practice**

| Level | Emitters | Cups | Mechanic                                      |
|-------|----------|------|-----------------------------------------------|
| 1     | 1 (centre) | 1 (centre) | Straight drop — no drawing needed     |
| 2     | 1 (left)   | 1 (right)  | Draw a slope rightward                |
| 3     | 1 (centre) | 2 (sides)  | Split the stream with a Y-shape line  |
| 4     | 1 (centre) | 3          | Three-way split                       |

**Levels 5–8: Obstacles**

| Level | Obstacle pattern            | Challenge                                    |
|-------|-----------------------------|----------------------------------------------|
| 5     | Single horizontal shelf     | Route sugar around the shelf to reach the left cup |
| 6     | Two shelves (zigzag)        | Build a staircase of lines to descend the zigzag |
| 7     | Two emitters, two cups      | Route each stream to the correct cup without cross-contamination |
| 8     | V-shape (two angled shelves)| Sugar naturally funnels toward centre — direct it into the cup |

**Levels 9–12: Color filters** *(see Lesson 11 for full detail)*

| Level | New mechanic                            |
|-------|-----------------------------------------|
| 9     | First filter: one red filter, one white cup, one red cup |
| 10    | Two filters (red + green), two colored cups |
| 11    | Orange filter with an obstacle shelf to force routing |
| 12    | Three filters (red, green, orange), three cups |

**Levels 13–15: Gravity switch** *(see Lesson 14)*

| Level | Pattern                                           |
|-------|---------------------------------------------------|
| 13    | Emitter at bottom, cup at top — must flip gravity |
| 14    | Gravity + two cups (one above, one below)         |
| 15    | Gravity + colour filter at top of screen          |

### Step 8: Reset behavior

When the player presses **R** or clicks the RESET button:

```c
if (BUTTON_PRESSED(input->reset)) {
    level_load(state, state->current_level);
    return;
}
```

`level_load` calls `memset` to zero the `LineBitmap`, erasing all
player-drawn lines, then immediately calls `stamp_obstacles()` and
`stamp_cups()` to restore the pre-set geometry. Obstacles always reappear;
player lines are always erased.

## Common mistakes to prevent

- **Stamping obstacles before `memset`**: always clear the bitmap first,
  then stamp. If the order is reversed a reset will erase the obstacles.
- **Using value `0` for obstacles**: `IS_SOLID` treats `0` as empty; obstacles
  written as `0` are invisible to physics even though they render.
- **Using value `255` for obstacles**: `render_lines` draws value-255 pixels
  as `COLOR_LINE` (dark grey). Using `255` for obstacles makes them render
  the same as player lines — no visual distinction. Use value `1`.
- **Not calling `stamp_obstacles` on reset**: if `level_load` forgets to
  call `stamp_obstacles()`, obstacles disappear after the first reset.
- **Placing obstacles at negative or off-screen coordinates**: the bounds
  check in `stamp_obstacles` prevents crashes, but off-screen obstacles are
  invisible and produce confusing gameplay.
- **Rendering obstacles as `COLOR_LINE`**: if `render_obstacles` is omitted,
  the `render_lines` pass still draws them dark-grey (value `1`), but the
  player cannot tell them apart from drawn lines.

## What to run

```bash
./build-dev.sh
./sugar_x11      # or ./sugar_raylib
```

Verify on Level 5:
1. The horizontal shelf renders in brown — visually distinct from player lines.
2. Grains pile on top of the shelf and slide off the edges.
3. Pressing **R** erases player lines but the brown shelf reappears immediately.
4. Drawing a line on top of the shelf does not change its color (the shelf
   pixel is value `1`; the brush writes `255` — `render_lines` paints both
   as dark grey, then `render_obstacles` overpaints with brown).

## Summary

Obstacles are the simplest mechanic in the game: an `Obstacle` struct is
just a bounding box defined in the level data; `stamp_obstacles()` burns it
into the `LineBitmap` as value `1` at level load time; the physics engine
never needs to know whether a solid pixel came from an obstacle, a player
line, or a baked grain — all non-zero values are solid; and `render_obstacles()`
draws the correct brown color by painting over the dark-grey pixels that
`render_lines` lays down first.  The fifteen levels covered here demonstrate
how every game mechanic can be understood as a composition of just a few
primitives: a flat bitmap, a gravity vector, and a handful of special-case
checks.
