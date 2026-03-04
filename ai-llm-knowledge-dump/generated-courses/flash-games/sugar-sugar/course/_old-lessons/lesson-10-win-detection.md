# Lesson 10 — Win Detection

## What you build

You implement `check_win()` and the cup absorption logic that counts grains
as they enter a cup's interior, handles wrong-color discards and cup overflow,
and transitions the game to `PHASE_LEVEL_COMPLETE` (then to `PHASE_FREEPLAY`
after all 30 levels).

## Concepts introduced

- Cup interior bounds: one pixel inset from the wall columns
- Color matching: `required_color == GRAIN_WHITE` accepts any color
- Wrong-color discard: remove grain silently, no count
- Full-cup overflow: grain stays active, piles up inside
- `check_win()`: loop over cups, return false on first unfilled cup
- Phase transition: `change_phase(PHASE_LEVEL_COMPLETE)` → auto-advance after 2 s
- All-levels-complete path: `current_level + 1 >= TOTAL_LEVELS` → `PHASE_FREEPLAY`

## JS → C analogy

| JavaScript                                              | C                                                                     |
|---------------------------------------------------------|-----------------------------------------------------------------------|
| `cups.every(c => c.collected >= c.required)`            | `for each cup: if collected < required → return 0; return 1`         |
| `dispatch({ type: 'WIN' })`                             | `change_phase(state, PHASE_LEVEL_COMPLETE)`                           |
| `cup.required === null` (any color)                     | `cup->required_color == GRAIN_WHITE`                                  |
| `grains.splice(i, 1)` to remove a grain                | `p->active[i] = 0; s_occ[…] = 0; goto next_grain`                   |
| `if (cup.count < cup.max) cup.count++`                  | `if (cup->collected < cup->required_count) { cup->collected++; ... }` |

## Step-by-step

### Step 1: Recall the cup geometry

`stamp_cups()` (Lesson 09) wrote `1` bytes at:

- `x = cup->x` (left wall)
- `x = cup->x + cup->w - 1` (right wall)
- `y = cup->y + cup->h - 1` (bottom wall)

The top is open. The interior is the space between the walls:

```
    cup->x      cup->x+cup->w-1
       |   interior   |         ← cup->y (open top)
       |              |
       |              |
       |______________|         ← cup->y+cup->h-1 (solid bottom)
```

The absorption zone skips the wall columns themselves:

```
interior x: [cup->x + 1  ..  cup->x + cup->w - 2]    (exclusive of walls)
interior y: [cup->y       ..  cup->y + cup->h - 2]    (exclusive of bottom)
```

In `game.c`:

```c
int ix0 = cup->x + 1,     ix1 = cup->x + cup->w - 1;   /* half-open: [ix0, ix1) */
int iy0 = cup->y,         iy1 = cup->y + cup->h - 1;   /* half-open: [iy0, iy1) */
```

Check:

```c
if (gx >= ix0 && gx < ix1 && gy >= iy0 && gy < iy1) {
    /* grain is inside the cup interior */
}
```

> **Why inset by 1?** The wall pixels at `x = cup->x` and `x = cup->x+w-1`
> are solid (`IS_SOLID` returns true). A grain physically touching the left
> wall is at `gx = cup->x`. If absorption included that column, the grain
> would be counted while still colliding with the wall — before it has
> actually fallen inside. Skipping the wall columns ensures only grains that
> have cleared the rim are absorbed.

### Step 2: Color matching

Each cup has a `required_color` field. `GRAIN_WHITE` is a wildcard that accepts
any grain color:

```c
int right_color = (cup->required_color == GRAIN_WHITE ||
                   p->color[i] == (uint8_t)cup->required_color);
```

The full absorption condition: right color **and** cup not yet full:

```c
if (right_color && cup->collected < cup->required_count) {
    cup->collected++;
    s_occ[gy * W + gx] = 0;   /* remove from occupancy map immediately */
    p->active[i] = 0;          /* free the pool slot */
    goto next_grain;
}
```

Three things happen in order:

1. Increment the counter.
2. Clear `s_occ` at the grain's position. The grain is gone — subsequent
   grains in this frame must not see it as solid.
3. Deactivate the pool slot (`active[i] = 0`).
4. `goto next_grain` skips the remark and settle check — the grain no longer
   exists.

### Step 3: Wrong-color discard

If the cup has a required color and the grain is a different color, the grain
is silently removed (no count, no penalty):

```c
} else if (!right_color) {
    s_occ[gy * W + gx] = 0;
    p->active[i] = 0;
    goto next_grain;
}
```

This matches the original Sugar Sugar mechanic: routing a red grain into a
green cup doesn't subtract from the green count; the grain just vanishes.

### Step 4: Full-cup overflow

When the cup is at `collected >= required_count` and the incoming grain is the
correct color, the grain is **not** absorbed:

```c
/* Cup is full with correct color: grain stays active.
 * The solid walls/bottom will cause it to pile up and
 * eventually spill over the open top rim. */
```

There is no explicit code for this case — the `if/else if` does not have an
`else` branch that removes the grain. The grain continues its normal physics
update: it hits the solid bottom wall of the cup, bounces, and eventually
slides or piles up inside the cup. As the pile grows it may overflow through
the open top.

This is intentional: it prevents grains from disappearing into a "black hole"
cup once it is full, and gives the player visual feedback that the cup is
overflowing.

### Step 5: The full cup-check block in context

Here is the complete cup-check block from `update_grains()` in `game.c`:

```c
{
    int gx = (int)p->x[i], gy = (int)p->y[i];
    for (int c = 0; c < lv->cup_count; c++) {
        Cup *cup = &lv->cups[c];
        int ix0 = cup->x + 1,        ix1 = cup->x + cup->w - 1;
        int iy0 = cup->y,             iy1 = cup->y + cup->h - 1;
        if (gx >= ix0 && gx < ix1 && gy >= iy0 && gy < iy1) {
            int right_color = (cup->required_color == GRAIN_WHITE ||
                               p->color[i] == (uint8_t)cup->required_color);
            if (right_color && cup->collected < cup->required_count) {
                /* Check BEFORE incrementing so we detect the exact fill moment */
                int just_filled = (cup->collected + 1 >= cup->required_count);
                cup->collected++;
                if (just_filled) {
                    /* Cup reached 100% — reward the player with a chime */
                    game_play_sound(&state->audio, SOUND_CUP_FILL);
                }
                s_occ[gy * W + gx] = 0;
                p->active[i] = 0;
                goto next_grain;
            } else if (!right_color) {
                s_occ[gy * W + gx] = 0;
                p->active[i] = 0;
                goto next_grain;
            }
            /* full cup, right color: fall through — physics handles it */
        }
    }
}
```

The outer loop iterates every cup each frame for every grain. With `MAX_CUPS = 8`
and `MAX_GRAINS = 4096`, the worst case is `4096 × 8 = 32768` AABB tests per
frame — trivial for a modern CPU (all integers, no branches taken for most
grains).

### Step 6: Implement `check_win()`

```c
static int check_win(GameState *state) {
    LevelDef *lv = &state->level;
    for (int c = 0; c < lv->cup_count; c++) {
        if (lv->cups[c].collected < lv->cups[c].required_count)
            return 0;   /* at least one cup not yet full */
    }
    return 1;           /* all cups filled */
}
```

This is the C equivalent of `cups.every(c => c.collected >= c.required)`.
Returns `0` (false) on the first unfilled cup; returns `1` (true) only after
scanning all cups without finding a shortage.

`check_win` is called at the end of `update_playing()`, after `update_grains`:

```c
if (check_win(state)) {
    int next = state->current_level + 1;
    if (next < TOTAL_LEVELS && next >= state->unlocked_count)
        state->unlocked_count = next + 1;   /* unlock the next level */
    change_phase(state, PHASE_LEVEL_COMPLETE);
}
```

### Step 7: Level advance after `PHASE_LEVEL_COMPLETE`

In `update_level_complete()` (Lesson 08), after `phase_timer > 2.0f` or a
click:

```c
int next = state->current_level + 1;
if (next >= TOTAL_LEVELS) {
    change_phase(state, PHASE_FREEPLAY);
} else {
    level_load(state, next);
    change_phase(state, PHASE_PLAYING);
}
```

The two paths:

- **Normal advance**: load the next level definition, reset simulation, start
  playing. `current_level` is updated inside `level_load`.
- **All levels complete** (`next >= 30`): enter `PHASE_FREEPLAY`. The last
  level's layout is still in `state->level`, so the player can continue
  experimenting in sandbox mode.

### Step 8: Cup progress rendering

`render_cups()` in `game.c` shows a rising fill bar and a numeric countdown
above each cup:

```c
int filled_h = full
    ? (cup->h - 1)
    : (cup->collected * (cup->h - 1)) / cup->required_count;

draw_rect(bb, cup->x + 1,
              cup->y + (cup->h - 1) - filled_h,
              cup->w - 2, filled_h,
              full ? COLOR_CUP_FILL_FULL : COLOR_CUP_FILL);
```

The fill bar rises from the bottom. `(cup->h - 1)` is used instead of
`cup->h` to leave room for the bottom wall pixel. When the cup is full the
fill turns from blue (`COLOR_CUP_FILL`) to green (`COLOR_CUP_FILL_FULL`) and
"OK" replaces the numeric counter:

```c
if (full) {
    draw_text_scaled(bb, tx, cup->y - 20, "OK", COLOR_CUP_FILL_FULL, 2);
} else {
    int remaining = cup->required_count - cup->collected;
    /* ... draw remaining count ... */
}
```

For cups with a specific required color, a colored dot is drawn in the center:

```c
if (cup->required_color != GRAIN_WHITE) {
    draw_circle(bb, cup->x + cup->w / 2, cup->y + (cup->h / 2) + 4, 6,
                g_grain_colors[cup->required_color]);
}
```

### Step 9: Verify against a multi-cup level

Level 3 (index 2) has two cups and one emitter:

```c
[2] = {
    .index         = 2,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 270) },
    .cup_count     = 2,
    .cups          = {
        CUP( 60, 370, 85, 100, GRAIN_WHITE, 100),
        CUP(490, 370, 85, 100, GRAIN_WHITE, 100),
    },
},
```

`check_win` returns `1` only when **both** cups have `collected >= 100`. If you
draw a slope that routes all sugar into one cup, the other stays at 0 and the
level never completes — you must split the stream.

Level 9 (index 8) introduces color:

```c
[8] = {
    .cups = {
        CUP( 60, 370, 85, 100, GRAIN_RED,  100),
        CUP(490, 370, 85, 100, GRAIN_WHITE, 100),
    },
    .filters = { FILTER(60, 180, 100, 22, GRAIN_RED) },
},
```

The left cup requires red grains. Routing sugar through the filter colors it
red; those grains count toward the left cup. White grains that bypass the
filter count toward the right cup (which accepts any color). If you route a
red grain into the right cup it is **discarded** (wrong color for a
`GRAIN_WHITE` cup? No — `GRAIN_WHITE` cup accepts anything. But a red grain
in the `GRAIN_RED` left cup once it's full would pass through and be active;
a red grain in the `GRAIN_WHITE` right cup is accepted and counted).

Actually re-reading the logic: `GRAIN_WHITE` cup (`required_color == GRAIN_WHITE`)
sets `right_color = 1` for any incoming grain. So the right cup accepts grains
of all colors.

## Common mistakes to prevent

- **Using `>=`/`<=` on both edges for the absorption check** (e.g.
  `gx >= cup->x && gx <= cup->x + cup->w - 1`): this includes the wall
  pixels. A grain physically stopped against the wall would be absorbed before
  it enters the cup interior, over-counting.
- **Not clearing `s_occ` when absorbing a grain**: if `s_occ[gy*W+gx]` stays
  set after `p->active[i] = 0`, subsequent grains in the same frame think that
  pixel is still occupied and are deflected — causing grains to "bounce" off
  an invisible ghost.
- **Calling `check_win` before `update_grains`**: cups are updated during
  `update_grains`. Calling `check_win` first would use stale `collected`
  counts and miss the grains that entered this frame.
- **Treating full-cup grains as absorbed**: a grain in a full cup must remain
  active so physics can pile it up and let it spill. If you absorb it silently,
  the cup appears to suck in unlimited grains with no visible overflow.
- **Forgetting to unlock the next level**: `state->unlocked_count` must be
  incremented when a level is won. Without this, completing level 1 does not
  make level 2 clickable on the title screen.
- **Off-by-one on `TOTAL_LEVELS`**: `next >= TOTAL_LEVELS` is the correct
  overflow guard (zero-based index 29 is the last level;
  `current_level + 1 = 30 >= 30` → freeplay). Using `next > TOTAL_LEVELS`
  would try to load a non-existent level 30 before entering freeplay.

## What to run

```bash
./build-dev.sh
```

Test each scenario:

1. **Level 1 (straight drop)**: open, let sugar fall. Cup fills, "LEVEL
   COMPLETE!" appears after the last required grain.
2. **Level 3 (split stream)**: draw a "V" slope to route sugar into both cups.
   Confirm the level does not complete until *both* show "OK".
3. **Level 9 (color filter)**: route part of the stream through the red filter
   to the left cup; let the rest fall into the right cup. Verify wrong-color
   grains entering the left cup before they're colored are discarded (count
   stays at 0).
4. **Overflow**: let a cup fill up. Keep the emitter running. Grains should
   pile up inside the cup and eventually overflow over the rim rather than
   disappearing.
5. **All 30 levels**: complete level 30. The game should enter freeplay mode
   showing the "FREEPLAY" banner instead of a level-complete screen.

## Summary

Win detection in Sugar Sugar is a simple all-cups-filled check, but getting
the cup absorption logic exactly right requires care at several boundaries:
the absorption zone must be inset one pixel from each wall to avoid counting
grains that are still in contact with the solid rim; color matching uses
`GRAIN_WHITE` as a wildcard; wrong-color grains are discarded silently; and
full cups allow overflow rather than absorbing grains into a void. The
`check_win` function itself is minimal — one loop, one early return — and sits
at the end of `update_playing` so it sees the frame's final grain positions.
Level advance after `PHASE_LEVEL_COMPLETE` either loads the next puzzle or
enters the open-ended `PHASE_FREEPLAY` sandbox, completing the full game arc.
