# Lesson 14 — Teleporters and Gravity Flip

## What you build

You add two interacting mechanics: paired teleporter portals that instantly
warp grains from one location to another with a cooldown to prevent
re-entry loops, and a gravity-flip toggle that reverses the direction of
gravity for the entire simulation.

## Concepts introduced

- `Teleporter` struct: portal A center, portal B center, trigger radius
- `tpcd[]` per-grain teleport cooldown byte in `GrainPool`
- Squared-distance check (avoids `sqrt`) for portal detection
- Bidirectional warp: A→B and B→A
- Cooldown decrement: `if (tpcd[i] > 0) tpcd[i]--` each frame
- `gravity_sign` field in `GameState` (+1 = down, -1 = up)
- G key and on-screen FLIP button toggle
- `has_gravity_switch` flag in `LevelDef`
- Rendering portals as colored circles

## JS → C analogy

| JavaScript                                                             | C                                                                    |
|------------------------------------------------------------------------|----------------------------------------------------------------------|
| `if (dist(g, portalA) < r && g.tpCooldown === 0) { warp… }`           | `if (da <= r2 && p->tpcd[i] == 0) { p->x[i] = tp->bx; … }`        |
| `g.x = portalB.x; g.y = portalB.y; g.tpCooldown = 6;`                | `p->x[i] = (float)tp->bx; p->y[i] = (float)tp->by; p->tpcd[i]=6;` |
| `if (g.tpCooldown > 0) g.tpCooldown--;`                               | `if (p->tpcd[i] > 0) p->tpcd[i]--;`                                |
| `gravity = flipGravity ? -GRAVITY : GRAVITY`                          | `float grav = GRAVITY * (float)state->gravity_sign;`               |
| `if (keyPressed('G') && level.hasGravitySwitch) flipGravity = !flipGravity` | `if (BUTTON_PRESSED(input->gravity) && lv->has_gravity_switch) state->gravity_sign *= -1;` |

## Step-by-step

### Step 1: Define the `Teleporter` struct (game.h)

```c
typedef struct {
    int ax, ay;   /* portal A center (pixels) */
    int bx, by;   /* portal B center (pixels) */
    int radius;   /* trigger radius  (pixels) */
} Teleporter;

#define MAX_TELEPORTERS 2
```

The struct stores both portal positions together. Grains check distance
to both `(ax,ay)` and `(bx,by)` every frame.

### Step 2: Add `tpcd[]` to `GrainPool` (game.h)

```c
typedef struct {
    float   x[MAX_GRAINS];
    float   y[MAX_GRAINS];
    float   vx[MAX_GRAINS];
    float   vy[MAX_GRAINS];
    uint8_t color[MAX_GRAINS];
    uint8_t active[MAX_GRAINS];
    uint8_t tpcd[MAX_GRAINS];    /* teleport cooldown: frames until re-entry allowed */
    uint8_t still[MAX_GRAINS];
    int     count;
} GrainPool;
```

`uint8_t` is enough — the cooldown value is 6 frames, well within 0–255.

### Step 3: Initialise `tpcd` on spawn (game.c)

In `spawn_grains()`:

```c
p->active[slot] = 1;
p->color[slot]  = GRAIN_WHITE;
p->tpcd[slot]   = 0;    /* ← zero means "allowed to teleport immediately" */
p->still[slot]  = 0;
```

### Step 4: Decrement cooldown each frame (game.c)

Inside the per-grain loop in `update_grains()`, after the sub-step
integration and before the cup check:

```c
/* ---- Teleport cooldown ---- */
if (p->tpcd[i] > 0)
    p->tpcd[i]--;
```

This runs unconditionally so all active grains age out of their cooldown
even if they are nowhere near a portal.

### Step 5: Implement the teleport check (game.c)

```c
/* ---- Teleporter check (circle distance) ---- */
if (p->tpcd[i] == 0) {
    int gx = (int)p->x[i], gy = (int)p->y[i];
    for (int t = 0; t < lv->teleporter_count; t++) {
        Teleporter *tp = &lv->teleporters[t];
        int da = (gx - tp->ax) * (gx - tp->ax)
               + (gy - tp->ay) * (gy - tp->ay);
        int db = (gx - tp->bx) * (gx - tp->bx)
               + (gy - tp->by) * (gy - tp->by);
        int r2 = tp->radius * tp->radius;
        if (da <= r2) {
            /* Grain is inside portal A → warp to portal B */
            p->x[i]    = (float)tp->bx;
            p->y[i]    = (float)tp->by;
            p->tpcd[i] = 6;
            break;
        }
        if (db <= r2) {
            /* Grain is inside portal B → warp to portal A */
            p->x[i]    = (float)tp->ax;
            p->y[i]    = (float)tp->ay;
            p->tpcd[i] = 6;
            break;
        }
    }
}
```

**Why squared distance?**  
`sqrt` is expensive and imprecise for integer coordinates. Comparing
`dx*dx + dy*dy <= radius*radius` is both faster and exact.

**Why `break` after warp?**  
After warping, the grain is at the destination portal. Continuing the loop
would immediately test that destination portal against the distance check,
potentially warping back. `break` exits the teleporter loop; the cooldown
prevents re-entry on the next frame.

### Step 6: Why the cooldown is necessary

Without `tpcd`, consider what happens in a single frame:

1. Grain enters portal A (distance ≤ radius) → warped to portal B.
2. Loop continues; grain is now at portal B (distance = 0 ≤ radius) → warped
   back to portal A.
3. Loop continues; grain is at portal A again → warped to portal B.
4. …infinite warp loop within one call to `update_grains`.

The `break` after each warp prevents steps 3+ in the same frame, but
without the cooldown the grain would re-enter on the very next frame
(distance at `bx,by` is 0 ≤ radius) and oscillate forever.

With `tpcd = 6`, the grain is blocked from teleporting for 6 frames
(≈ 100 ms at 60 fps) — long enough for it to travel away from the
destination portal under gravity.

### Step 7: Add `gravity_sign` to `GameState` (game.h)

```c
typedef struct {
    GAME_PHASE phase;
    float      phase_timer;
    int        current_level;
    int        unlocked_count;
    LevelDef   level;
    GrainPool  grains;
    LineBitmap lines;
    int        gravity_sign;   /* +1 = down (normal), -1 = up (flipped) */
    int        title_hover;
    int        reset_hover;
    int        gravity_hover;
    int        should_quit;
} GameState;
```

### Step 8: Initialise and reset `gravity_sign` (game.c)

In `game_init()`:

```c
state->gravity_sign = 1;   /* gravity pulls downward */
```

In `level_load()`:

```c
state->gravity_sign = 1;   /* reset to normal on each level load */
```

### Step 9: Apply `gravity_sign` in physics (game.c)

In `update_grains()`, at the top of the function:

```c
float grav = GRAVITY * (float)state->gravity_sign;
```

Then apply it per grain:

```c
p->vy[i] += grav * dt;
```

`GRAVITY` is defined as a positive constant (e.g. `800.0f` px/s²). When
`gravity_sign = +1`, `grav` is positive — grains accelerate downward (+Y).
When `gravity_sign = -1`, `grav` is negative — grains accelerate upward.

The initial vertical velocity on spawn also respects gravity direction:

```c
p->vy[slot] = (state->gravity_sign > 0) ? 40.0f : -40.0f;
```

### Step 10: Handle the G key and FLIP button (game.c)

In `update_playing()`:

```c
/* ---- Gravity flip (G key shortcut) ---- */
if (state->level.has_gravity_switch && BUTTON_PRESSED(input->gravity)) {
    state->gravity_sign = -state->gravity_sign;
}

/* ---- On-screen GRAVITY button click ---- */
if (BUTTON_PRESSED(input->mouse.left)) {
    if (state->gravity_hover) {
        state->gravity_sign = -state->gravity_sign;
    }
}
```

`state->gravity_sign *= -1` toggles between +1 and −1. Using multiplication
rather than an if/else is shorter and has the same effect.

### Step 11: Render the FLIP button (game.c)

In `render_ui_buttons()`:

```c
if (state->level.has_gravity_switch) {
    int gx = UI_GRAV_X, gy = UI_BTN_Y, gw = UI_GRAV_W, gh = UI_BTN_H;
    uint32_t g_bg = state->gravity_hover ? COLOR_BTN_HOVER : COLOR_BTN_NORMAL;
    draw_rect(bb, gx, gy, gw, gh, g_bg);
    draw_rect_outline(bb, gx, gy, gw, gh, COLOR_BTN_BORDER);
    const char *label = (state->gravity_sign > 0) ? "FLIP v" : "FLIP ^";
    draw_text(bb, gx + 8, gy + (gh - 8) / 2, label, COLOR_UI_TEXT);
}
```

The label changes dynamically: `"FLIP v"` when gravity is normal (down),
`"FLIP ^"` when it is flipped (up).

### Step 12: Render teleporter portals (game.c)

```c
static void render_teleporters(const LevelDef *lv, GameBackbuffer *bb,
                               float phase_timer) {
    for (int t = 0; t < lv->teleporter_count; t++) {
        const Teleporter *tp = &lv->teleporters[t];
        draw_circle(bb, tp->ax, tp->ay, tp->radius, COLOR_PORTAL_A);
        draw_circle_outline(bb, tp->ax, tp->ay, tp->radius + 2, COLOR_PORTAL_A);
        draw_circle(bb, tp->bx, tp->by, tp->radius, COLOR_PORTAL_B);
        draw_circle_outline(bb, tp->bx, tp->by, tp->radius + 2, COLOR_PORTAL_B);
    }
}
```

`COLOR_PORTAL_A` is blue (`GAME_RGB(100, 180, 255)`); `COLOR_PORTAL_B` is
orange (`GAME_RGB(255, 140, 0)`). The outline ring (radius + 2) creates a
visible border that makes portals stand out against the background.

`phase_timer` is passed in for potential pulse animation — the current
source uses a toggling `pulse` variable based on the timer for a 0/1
toggle, though this is mostly a visual placeholder.

### Step 13: Level 13 — gravity introduction (levels.c)

```c
[12] = {
    .index              = 12,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 440, 240) },   /* emitter at BOTTOM */
    .cup_count          = 1,
    .cups               = { CUP(278, 30, 85, 80, GRAIN_WHITE, 150) },  /* cup at TOP */
    .has_gravity_switch = 1,
},
```

- The emitter is at y=440 (near bottom); the cup is at y=30 (near top).
- With normal gravity (sign=+1), grains fall downward — they can never reach
  the cup.
- The player must press G (or click FLIP) to reverse gravity so grains rise.

### Step 14: Level 21 — teleporter introduction (levels.c)

```c
[20] = {
    .index            = 20,
    .emitter_count    = 1,
    .emitters         = { EMITTER(320, 20, 240) },
    .cup_count        = 1,
    .cups             = { CUP(60, 370, 85, 100, GRAIN_WHITE, 150) },
    .teleporter_count = 1,
    .teleporters      = { TELE(320, 150, 100, 350, 18) },
},
```

Portal A is at (320, 150) — directly below the emitter.
Portal B is at (100, 350) — to the left and near the cup.
Grains falling from the emitter enter portal A and emerge at portal B,
landing in the left cup.

## Common mistakes to prevent

- **No cooldown**: grain enters A, warps to B, immediately re-enters B
  (still within radius=18), warps back to A — infinite oscillation every
  frame. Always set `tpcd[i] = 6` after a warp.
- **Not checking `tpcd[i] == 0` before warp**: a grain that just warped has
  `tpcd = 6`. Without the guard, the cooldown decrement happens first, then
  the check runs — but the order of operations (decrement then check) means
  you need to decrement before the teleport check OR check `== 0` before
  allowing warp. The source decrements first (line 725) and checks `== 0`
  (line 779) — the order is: integrate → decrement cooldown → cup check →
  color filter → teleport check. This is correct.
- **Applying gravity_sign only to `vy` accumulation but not to spawn `vy`**:
  a flipped-gravity level spawns grains at positive `vy` (downward) even
  though gravity is pulling them up — they fall away from the cup
  immediately. Set `p->vy[slot] = (state->gravity_sign > 0) ? 40.0f : -40.0f`.
- **Rendering portal A and B with swapped colors**: A is blue, B is orange.
  Swapping them confuses players about which portal connects to which.
- **Forgetting `has_gravity_switch` check**: showing the FLIP button on
  levels that have no gravity mechanic clutters the UI and breaks the
  cognitive model ("why is this button here?").

## What to run

```bash
./build-dev.sh
./sugar_raylib     # or ./sugar_x11
```

Verify on Level 13 (index 12):
1. Emitter is at the bottom; grains fall toward the bottom edge and disappear.
2. Press G — grains immediately start rising toward the cup at the top.
3. Cup fills. Level completes.

Verify on Level 21 (index 20):
1. Grains fall from center and pass through the blue portal.
2. Grains appear at the orange portal (left side, low) and fall into the cup.
3. A grain that has just warped does not immediately warp back (cooldown
   working).
4. Press R to reset; portals redraw correctly.

## Summary

Teleporters work by comparing each grain's integer position against a
squared trigger radius — no `sqrt` needed, just `dx*dx + dy*dy <= r*r`.
When a grain enters a portal it is instantly relocated to the partner portal
and given a 6-frame cooldown that blocks re-entry; without the cooldown the
grain would oscillate between the two portals indefinitely within a single
frame.  Gravity flip is simpler: `gravity_sign` is either +1 or −1; the
physics loop multiplies `GRAVITY` by it before applying it as acceleration;
toggling is a single `*= -1`.  Both mechanics are gated by per-level flags
(`has_gravity_switch`, `teleporter_count > 0`) so they only appear in levels
designed for them.
