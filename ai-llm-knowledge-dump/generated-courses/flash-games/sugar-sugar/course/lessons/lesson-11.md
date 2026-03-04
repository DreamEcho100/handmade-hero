# Lesson 11: Color Filters and Multi-Stream Routing

## What we're building

Add `ColorFilter` zones — rectangular areas where grains instantly change color as
they pass through.  Pair these with color-specific cups to create puzzle levels where
the player must draw lines that split the sugar stream and route each color correctly.

## What you'll learn

- AABB overlap test for filters (simpler than cups — no absorption)
- How `GRAIN_COLOR` enum maps to both physics state and render color
- The visual design of a filter zone (colored rectangle with label)
- How levels combine filters + cups + multiple emitters
- Why filter checks run after cup checks in the per-grain update

## Prerequisites

- Lesson 10 complete (win detection and cup fill working)

---

## Step 1: Filter check inside `update_grains`

Color filters are the simplest entity: no state changes to the grain except color.
Add the check **after** cup absorption (so a grain that enters a cup is removed
before the filter check can override its color):

```c
/* src/game.c  —  filter check inside update_grains (after cup check) */

/* ---- Color filter check (AABB) ----
 *
 * When a grain passes through a filter zone its color changes immediately.
 * The grain continues to fall and behave normally — only its color is affected.
 *
 * Note: filter check runs AFTER cup check.  If a grain lands in a cup first,
 * it is removed (goto next_grain) and never reaches the filter check.
 * This matches the visual expectation: a cup "catches" grains before any
 * filter below it can change their color. */
{
  int gx = (int)p->x[i], gy = (int)p->y[i];
  for (int f = 0; f < lv->filter_count; f++) {
    ColorFilter *flt = &lv->filters[f];
    if (gx >= flt->x && gx < flt->x + flt->w &&
        gy >= flt->y && gy < flt->y + flt->h) {
      p->color[i] = (uint8_t)flt->output_color;
      /* No break — multiple overlapping filters use the last-applied rule. */
    }
  }
}
```

---

## Step 2: Render color filters

Filters need to be visually distinct so the player understands what they do.

```c
/* src/game.c  —  render_filters */

/* Grain color → display color (for filter tint) */
static const uint32_t g_filter_colors[GRAIN_COLOR_COUNT] = {
  [GRAIN_WHITE]  = GAME_RGB(220, 220, 200),  /* pale cream   */
  [GRAIN_RED]    = GAME_RGB(229,  57,  53),  /* vivid red    */
  [GRAIN_GREEN]  = GAME_RGB( 67, 160,  71),  /* vivid green  */
  [GRAIN_ORANGE] = GAME_RGB(251, 140,   0),  /* vivid orange */
};

static void render_filters(const LevelDef *lv, GameBackbuffer *bb) {
  for (int f = 0; f < lv->filter_count; f++) {
    const ColorFilter *flt = &lv->filters[f];

    /* Semi-transparent tinted fill (alpha ≈ 80/255 ≈ 30%) */
    draw_rect_blend(bb, flt->x, flt->y, flt->w, flt->h,
                    g_filter_colors[flt->output_color], 80);

    /* Solid border */
    draw_rect_outline(bb, flt->x, flt->y, flt->w, flt->h,
                      g_filter_colors[flt->output_color]);
  }
}
```

Add to `render_playing` (before rendering grains so grains appear on top):

```c
render_filters(&state->level, bb);
render_cups(&state->level, bb);
/* then render grains */
```

---

## Step 3: Example level using filters

```c
/* src/levels.c  —  Level 3: "Color Split" */
[3] = {
  .index = 3,
  .emitter_count = 1,
  .emitters = {
    [0] = { .x = 320, .y = 30, .grains_per_second = 80 },
  },
  .cup_count = 2,
  .cups = {
    [0] = { .x = 100, .y = 380, .w = 100, .h = 60,
            .required_color = GRAIN_RED, .required_count = 100 },
    [1] = { .x = 440, .y = 380, .w = 100, .h = 60,
            .required_color = GRAIN_GREEN, .required_count = 100 },
  },
  .filter_count = 2,
  .filters = {
    [0] = { .x = 80,  .y = 200, .w = 120, .h = 40,
            .output_color = GRAIN_RED },
    [1] = { .x = 440, .y = 200, .w = 120, .h = 40,
            .output_color = GRAIN_GREEN },
  },
},
```

In this level the player must draw a line that splits the stream: some grains
fall left through the red filter into the red cup, others fall right through
the green filter into the green cup.

---

## Step 4: Render cup color hints

Show the required color on the cup wall so the player knows which cup wants which color:

```c
/* src/game.c  —  render_cups update: tint empty cup interior to hint color */

static void render_cups(const LevelDef *lv, GameBackbuffer *bb) {
  for (int c = 0; c < lv->cup_count; c++) {
    const Cup *cup = &lv->cups[c];
    int full = (cup->collected >= cup->required_count);

    /* Tinted empty area (subtle hint of required color) */
    uint32_t hint_color = (cup->required_color == GRAIN_WHITE)
      ? COLOR_CUP_EMPTY
      : g_filter_colors[cup->required_color];
    draw_rect_blend(bb, cup->x + 1, cup->y, cup->w - 2, cup->h - 1,
                    hint_color, 40);  /* very faint tint */

    /* Fill bar */
    if (cup->required_count > 0) {
      int filled_h = full
        ? (cup->h - 1)
        : (cup->collected * (cup->h - 1)) / cup->required_count;
      int fill_y = cup->y + (cup->h - 1) - filled_h;
      uint32_t fill_color = full
        ? COLOR_CUP_FILL_FULL
        : ((cup->required_color == GRAIN_WHITE)
             ? COLOR_CUP_FILL
             : g_filter_colors[cup->required_color]);
      draw_rect(bb, cup->x + 1, fill_y, cup->w - 2, filled_h, fill_color);
    }

    draw_rect_outline(bb, cup->x, cup->y, cup->w, cup->h, COLOR_CUP_BORDER);
  }
}
```

---

## Step 5: Emitter jitter (for natural-looking streams)

Now that levels have emitters in `levels.c` we add the final two emitter tweaks:

```c
/* src/game.c  —  spawn_grains: add jitter and spread (constants in game.h) */

/* EMITTER_JITTER: random vx spread at spawn (±2 px/s velocity).
 * Tiny — keeps the pour as a tight focused stream but prevents all grains
 * starting at the exact same column. */
#define EMITTER_JITTER  2.0f

/* EMITTER_SPREAD: random x position offset at spawn (±3 px).
 * Without this, every grain spawns at exactly (em->x, em->y) — they all
 * collide at the same pixel on frame 1 and separate into 3 discrete "satellite"
 * streams.  A small spread distributes impact across several pixels. */
#define EMITTER_SPREAD  3

/* Inside the spawn loop, replace the fixed position with: */
float jitter  = ((float)(rand() % 1000) / 1000.0f - 0.5f) * EMITTER_JITTER * 2.0f;
int   x_spread = (rand() % (2 * EMITTER_SPREAD + 1)) - EMITTER_SPREAD;

p->x[slot]  = (float)(em->x + x_spread);
p->y[slot]  = (float)em->y;
p->vx[slot] = jitter;
p->vy[slot] = (state->gravity_sign > 0) ? 40.0f : -40.0f;
```

---

## Build and run

```bash
./build-dev.sh -r
```

**Expected output:**
- Level 3 shows one emitter at the top center, two filter zones (red left, green right), and two color-hinted cups at the bottom.
- Grains passing through a filter zone turn red or green.
- The player must draw a V-shaped line to split the stream; red grains fill the left cup, green the right.

---

## Exercises

1. Add an `ORANGE` filter that converts any grain to orange.  Create a level with a red filter, an orange filter, and three cups: red, orange, white.
2. What happens if a grain passes through two overlapping filters?  (It takes the color of the last filter it overlaps — is this the behavior you want?)
3. Make a filter zone that ONLY colorizes `GRAIN_WHITE` grains (already-colored grains pass through unchanged).  What code change makes this possible?

---

## What's next

In Lesson 12 we add the remaining level gadgets: UI buttons (reset + gravity flip),
teleporters that instantly move grains from one portal to another, and the gravity-flip
mechanic that inverts the simulation.
