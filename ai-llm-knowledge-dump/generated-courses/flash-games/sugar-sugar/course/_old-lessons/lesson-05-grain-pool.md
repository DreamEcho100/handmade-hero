# Lesson 05 — Grain Pool

## What you build

A pool of up to 4096 simultaneously active grains using the Struct-of-Arrays layout, an emitter that spawns grains at a configurable rate with position spread and velocity jitter, and a high-watermark allocator that recycles freed grain slots without fragmenting the pool.

## Concepts introduced

- Struct-of-Arrays (SoA) vs Array-of-Structs (AoS) — cache efficiency reasoning
- `GrainPool` definition: parallel `float x[]`, `y[]`, `vx[]`, `vy[]`, `uint8_t color[]`, `active[]`
- `MAX_GRAINS = 4096` and pool size in bytes
- `grain_alloc` — high-watermark scan and slot recycling
- `active[]` flag — why it replaces deletion
- `Emitter` struct and `spawn_timer` accumulation pattern
- `EMITTER_JITTER` (velocity spread) vs `EMITTER_SPREAD` (position spread)
- Why both jitter and spread are needed for a natural-looking pour

---

## JS → C analogy

| JavaScript                                                          | C equivalent                                          |
|---------------------------------------------------------------------|-------------------------------------------------------|
| `Array.from({length:4096}, () => ({x:0, y:0, vx:0, vy:0}))`       | AoS: `struct Grain pool[MAX_GRAINS]` (not what we use)|
| `{ x: new Float32Array(4096), y: new Float32Array(4096), ... }`    | SoA: `GrainPool` (what we actually use)              |
| `pool[i].active = false`                                            | `p->active[i] = 0`                                   |
| `pool.findIndex(g => !g.active)` or expand array                   | `grain_alloc()` — scan 0..count, then extend count   |
| `emitter.spawnTimer += dt`                                          | `em->spawn_timer += dt;`                             |
| `while (spawnTimer >= interval) { spawn(); spawnTimer -= interval; }` | exact same pattern in `spawn_grains`               |
| `vx += (Math.random() - 0.5) * spread`                             | `jitter = ((float)(rand()%1000)/1000.f - 0.5f) * EMITTER_JITTER * 2.f` |

---

## Step-by-step

### Step 1: AoS vs SoA — understanding the choice

**Array-of-Structs (AoS)** — the intuitive layout:

```c
/* AoS — what many developers write first */
typedef struct {
    float   x, y;
    float   vx, vy;
    uint8_t color;
    uint8_t active;
    uint8_t tpcd;
    uint8_t still;
} Grain;

Grain pool[MAX_GRAINS];  /* 4096 Grain structs, each ~24 bytes */
```

**Struct-of-Arrays (SoA)** — what the game uses:

```c
/* SoA — actual GrainPool from game.h */
typedef struct {
  float   x[MAX_GRAINS];
  float   y[MAX_GRAINS];
  float   vx[MAX_GRAINS];
  float   vy[MAX_GRAINS];
  uint8_t color[MAX_GRAINS];
  uint8_t active[MAX_GRAINS];
  uint8_t tpcd[MAX_GRAINS];
  uint8_t still[MAX_GRAINS];
  int     count;
} GrainPool;
```

**The memory layout difference:**

AoS packs all fields of one grain together:
```
[x0 y0 vx0 vy0 color0 active0 ...] [x1 y1 vx1 vy1 color1 active1 ...] ...
```

SoA packs all values of one field together:
```
[x0 x1 x2 ... x4095] [y0 y1 y2 ... y4095] [vx0 vx1 ...] [vy0 vy1 ...]
```

### Step 2: Why SoA wins on cache

The gravity update loop reads only `x[]`, `y[]`, `vx[]`, `vy[]` — it never touches `color[]`, `active[]`, `tpcd[]`, or `still[]`.

**With AoS:** Loading grain 0's `x` and `y` also pulls `color` and `active` into the cache line (each cache line is 64 bytes = about 2–3 AoS grains). Those fields are wasted cache space.

**With SoA:** The four float arrays `x[]`, `y[]`, `vx[]`, `vy[]` each hold `4096 × 4 = 16 KB`. Together that is `4 × 16 KB = 64 KB`. A typical L2 cache is 256 KB — all four arrays fit, and no `color` or `active` bytes are ever loaded during the gravity pass.

```
4096 grains × 4 fields × 4 bytes = 65,536 bytes ≈ 64 KB
                   ↑ fits in 256 KB L2 cache
```

**With AoS:** Each grain is ~24 bytes. 4096 grains = ~98 KB — the full array fits in L2, but every iteration of the gravity loop loads 24 bytes (3 cache lines worth) to use only 16 bytes of `x/y/vx/vy`.

**JS analogy:**

```js
// AoS — natural but cache-unfriendly for tight loops
const pool = Array.from({length: 4096}, () => ({ x:0, y:0, vx:0, vy:0, color:0, active:false }));

// SoA — cache-friendly for the gravity loop
const pool = {
    x:      new Float32Array(4096),
    y:      new Float32Array(4096),
    vx:     new Float32Array(4096),
    vy:     new Float32Array(4096),
    color:  new Uint8Array(4096),
    active: new Uint8Array(4096),
};
```

The SoA JS version uses `TypedArray` — contiguous memory, no GC pressure. The SoA C version is structurally identical.

### Step 3: The `GrainPool` definition in full

From `src/game.h`:

```c
#define MAX_GRAINS 4096

typedef struct {
  float x[MAX_GRAINS];      /* horizontal position (pixels, sub-pixel float) */
  float y[MAX_GRAINS];      /* vertical   position                            */
  float vx[MAX_GRAINS];     /* horizontal velocity (pixels/second)            */
  float vy[MAX_GRAINS];     /* vertical   velocity (+down)                    */
  uint8_t color[MAX_GRAINS];  /* GRAIN_COLOR value                            */
  uint8_t active[MAX_GRAINS]; /* 1 = alive, 0 = free slot                    */
  uint8_t tpcd[MAX_GRAINS];   /* teleport cooldown (frames)                  */
  uint8_t still[MAX_GRAINS];  /* consecutive "nearly-stopped" frames         */
  int count;                  /* high-watermark: slots 0..count-1 are valid  */
} GrainPool;
```

`GrainPool` is a field inside `GameState`, which is declared `static` in `main()`:

```c
static GameState state;
```

This means `GrainPool`'s arrays live in the BSS segment (zero-initialised at program start), not on the stack.

### Step 4: The `active[]` flag — why not delete

When a grain exits the screen (falls past the floor) or settles into the `LineBitmap`, it is "freed" by setting:

```c
p->active[i] = 0;
```

The slot is now available for reuse. **Why not move the last grain into position `i` and shrink `count`?**

Moving would invalidate in-flight indices in `s_occ` (the per-frame occupancy bitmap) and would cause the currently-running `for (int i = 0; i < p->count; i++)` loop to either skip or double-process grains. The `active[]` flag avoids all these issues:

- The outer loop always iterates `0..count-1`.
- Inactive slots are skipped in O(1) with `if (!p->active[i]) continue;`.
- The slot is reused when `grain_alloc` finds it during the scan.

### Step 5: `grain_alloc` — high-watermark allocator

```c
static int grain_alloc(GrainPool *p) {
  /* Scan for a free slot.  We keep a "high watermark" (count) so the scan
   * stays short during the early frames when most slots are empty. */
  for (int i = 0; i < p->count; i++) {
    if (!p->active[i])
      return i;  /* reuse freed slot */
  }
  if (p->count < MAX_GRAINS) {
    return p->count++;  /* extend the pool by 1 */
  }
  return -1; /* pool full — caller must handle this */
}
```

**How the high-watermark works:**

- Initially `count = 0`. The first call returns slot 0 and increments `count` to 1.
- When slot 2 is freed (`active[2] = 0`) and a new grain is needed, the scan finds slot 2 at `i=2` immediately.
- `count` only grows, never shrinks. After running for a few seconds, `count` reaches some steady-state value (e.g., 300 active grains → `count` stabilises around 300–400).
- Once settled grains are baked and freed regularly, the scan almost always finds a free slot well before reaching `count`.

**Memory implication:** The arrays are `MAX_GRAINS = 4096` entries whether or not all slots are used. The `count` field just limits how far the scan goes, keeping iteration O(active_count) rather than O(MAX_GRAINS).

### Step 6: The `Emitter` struct

From `src/game.h`:

```c
typedef struct {
  int   x, y;              /* pixel position of the spout                  */
  int   grains_per_second; /* emission rate (e.g. 80)                      */
  float spawn_timer;       /* internal: seconds accumulated since last emit */
} Emitter;
```

The `spawn_timer` accumulates `dt` each frame. When it exceeds one grain interval (`1.0f / grains_per_second`), a grain is spawned and the interval is subtracted. This loop continues until the timer is below the interval again:

```c
em->spawn_timer += dt;
float interval = 1.0f / (float)em->grains_per_second;
while (em->spawn_timer >= interval) {
    em->spawn_timer -= interval;
    /* spawn one grain */
}
```

**Why subtract `interval` rather than reset `spawn_timer = 0`?**

If the timer overshoots by 0.003 s, that overshoot carries forward and is subtracted from the next interval. This ensures the long-term emission rate matches `grains_per_second` exactly, even when `dt` varies frame to frame. Resetting to zero would lose that overshoot and slightly undercount grains.

**Example at 80 grains/second, dt = 0.016 s:**

```
interval = 1/80 = 0.0125 s

Frame 1: timer = 0 + 0.016 = 0.016
  0.016 >= 0.0125 → spawn, timer = 0.016 - 0.0125 = 0.0035
  0.0035 < 0.0125 → stop
  → 1 grain spawned

Frame 2: timer = 0.0035 + 0.016 = 0.0195
  0.0195 >= 0.0125 → spawn, timer = 0.0195 - 0.0125 = 0.0070
  0.0070 < 0.0125 → stop
  → 1 grain spawned

Frame 3: timer = 0.0070 + 0.016 = 0.0230
  0.0230 >= 0.0125 → spawn, timer = 0.0230 - 0.0125 = 0.0105
  → 1 grain spawned

Frame 4: timer = 0.0105 + 0.016 = 0.0265
  0.0265 >= 0.0125 → spawn, timer = 0.0265 - 0.0125 = 0.0140
  0.0140 >= 0.0125 → spawn, timer = 0.0140 - 0.0125 = 0.0015
  → 2 grains spawned in one frame (catch-up)
```

Over 4 frames (0.064 s) the emitter spawned 5 grains: `5 / 0.064 = 78.1 grains/s`. The overshoot accumulation would drive this toward 80/s over many frames. This is the standard timer-accumulation pattern.

### Step 7: Position spread — eliminating discrete streams

From `src/game.c`:

```c
#define EMITTER_SPREAD  3   /* random x position offset at spawn (±3 px) */

/* ... inside spawn_grains: */
int x_spread = (rand() % (2 * EMITTER_SPREAD + 1)) - EMITTER_SPREAD;
p->x[slot] = (float)(em->x + x_spread);
```

`rand() % (2 * EMITTER_SPREAD + 1)` gives a uniform integer in `[0, 6]`. Subtracting `EMITTER_SPREAD = 3` maps that to `[-3, +3]`. Each grain starts at a random pixel within 3 pixels of the emitter's center column.

**Why is this necessary?**

Without spread, all grains spawn at exactly `(em->x, em->y)`. The very first grain that settles at that pixel occupies it in the collision bitmap. Every subsequent grain collides immediately on arrival and must slide diagonally — to `em->x - 1` or `em->x + 1`. Those positions fill up too. Now every grain slides to `em->x ± 2`. The result is three thin, discrete columns of sugar: one at `em->x`, one at `em->x - 2`, one at `em->x + 2`. The comment in the source describes this exactly:

```c
/* Without this, ALL grains begin at the exact same pixel.  The first grain
 * that stalls there occupies that pixel in the occupancy bitmap — every
 * subsequent grain collides immediately and slides to ±1 or ±2 pixels,
 * forming three discrete "satellite streams" rather than one smooth pour.
 * A small x spread distributes the impact across several pixels so the
 * pile grows uniformly from the start. */
```

### Step 8: Velocity jitter — keeping the stream alive

```c
#define EMITTER_JITTER  2.0f  /* random vx spread at spawn (±2 px/s velocity) */

float jitter =
    ((float)(rand() % 1000) / 1000.0f - 0.5f) * EMITTER_JITTER * 2.0f;
p->vx[slot] = jitter;
```

`rand() % 1000 / 1000.0f` gives a float in `[0, 1)`. Subtracting 0.5 maps it to `[-0.5, 0.5)`. Multiplying by `EMITTER_JITTER * 2.0f = 4.0f` gives a range of `[-2, +2)` px/s.

**How jitter differs from spread:**

| Parameter | What it controls | Effect without it |
|-----------|-----------------|-------------------|
| `EMITTER_SPREAD` (position ±3 px) | Starting pixel column | 3 discrete narrow streams |
| `EMITTER_JITTER` (velocity ±2 px/s) | Initial horizontal velocity | Grains all fall straight down, no lateral spread; piles form too steeply |

Jitter gives grains a tiny sideways nudge that fans them slightly wider as they fall, creating a more natural-looking pour. Because `EMITTER_JITTER = 2.0f` is much smaller than `GRAVITY = 280.0f`, grains barely deviate from a straight drop — they just don't all fall on exactly the same trajectory.

### Step 9: The full `spawn_grains` function

```c
static void spawn_grains(GameState *state, float dt) {
  LevelDef *lv = &state->level;
  GrainPool *p = &state->grains;

  for (int e = 0; e < lv->emitter_count; e++) {
    Emitter *em = &lv->emitters[e];
    em->spawn_timer += dt;
    float interval = 1.0f / (float)em->grains_per_second;
    while (em->spawn_timer >= interval) {
      em->spawn_timer -= interval;
      int slot = grain_alloc(&state->grains);
      if (slot < 0)
        break; /* pool full — stop spawning this frame */

      p->active[slot] = 1;
      p->color[slot]  = GRAIN_WHITE;
      p->tpcd[slot]   = 0;
      p->still[slot]  = 0;

      float jitter =
          ((float)(rand() % 1000) / 1000.0f - 0.5f) * EMITTER_JITTER * 2.0f;
      int x_spread = (rand() % (2 * EMITTER_SPREAD + 1)) - EMITTER_SPREAD;

      p->x[slot]  = (float)(em->x + x_spread);
      p->y[slot]  = (float)em->y;
      p->vx[slot] = jitter;
      p->vy[slot] = (state->gravity_sign > 0) ? 40.0f : -40.0f;
    }
  }
}
```

The initial `vy` is `+40.0f` in normal gravity (downward nudge) or `-40.0f` in flipped gravity. This gives each grain a slight initial velocity in the gravity direction, preventing them from appearing to "float" for a few frames at the start of their fall before gravity takes over.

### Step 10: Memory size sanity check

```
GrainPool arrays:
  float x[4096]      = 4096 × 4 = 16,384 bytes
  float y[4096]      = 16,384 bytes
  float vx[4096]     = 16,384 bytes
  float vy[4096]     = 16,384 bytes
  uint8_t color[4096]  = 4,096 bytes
  uint8_t active[4096] = 4,096 bytes
  uint8_t tpcd[4096]   = 4,096 bytes
  uint8_t still[4096]  = 4,096 bytes
  int count            = 4 bytes
  ---
  Total: 65,540 bytes ≈ 64 KB

Gravity pass reads only: x + y + vx + vy = 4 × 16,384 = 65,536 bytes = 64 KB
Typical L2 cache: 256 KB
64 KB / 256 KB = 25% — leaves 192 KB for LineBitmap (300 KB fits in L3 at worst)
```

The gravity pass fits entirely in L2. The SoA layout made this possible — with AoS the 24-byte-per-grain layout would interleave the floats with color/active bytes, tripling cache pressure for the inner loop.

---

## Common mistakes to prevent

1. **Resetting `spawn_timer = 0` instead of subtracting `interval`.**
   Resetting discards the overshoot, causing the long-term emission rate to be slightly lower than `grains_per_second`. At 80 grains/s over 30 seconds, that could mean missing dozens of grains. Always subtract `interval` in a `while` loop.

2. **Forgetting the `slot < 0` check after `grain_alloc`.**
   When the pool is full, `grain_alloc` returns `-1`. Writing to `p->x[-1]` is undefined behaviour — typically writes to unrelated memory or crashes. Always check `if (slot < 0) break;`.

3. **Skipping position spread (`EMITTER_SPREAD`).**
   All grains spawn at `em->x`. The first settled grain occupies that pixel. Every subsequent grain collides there and slides to `±2`. The result is three discrete narrow columns instead of a natural pour.

4. **Setting jitter too high.**
   `EMITTER_JITTER = 20.0f` would give grains `±20 px/s` initial vx. With `GRAVITY = 280 px/s²` and fall time of ~1 s, grains deviate `±20 px` horizontally — the stream spreads into a wide fan covering 40+ pixels, looking like sparse trickles rather than a focused pour. Keep jitter small (±2 px/s is the tuned value).

5. **Using AoS for a pool this large.**
   At 4096 grains, AoS is 98 KB. The gravity loop loads `color` and `active` bytes into cache on every iteration, wasting ~33% of cache bandwidth on data the gravity loop never reads. SoA keeps the relevant 64 KB tight and cache-hot.

6. **Declaring `GrainPool` on the stack.**
   `GrainPool` is 64 KB. The `update_grains` function also uses a `static uint8_t s_occ[CANVAS_W * CANVAS_H]` (307 KB). Both must be `static` or heap-allocated. The `s_occ` in `game.c` is already `static`:
   ```c
   static uint8_t s_occ[CANVAS_W * CANVAS_H];
   ```
   If you moved it to a local variable, you would blow the 8 MB default stack.

7. **Not checking `count < MAX_GRAINS` in `grain_alloc`.**
   If the pool is full and no free slot is found but `count == MAX_GRAINS`, extending `count` would write out of bounds. The guard `if (p->count < MAX_GRAINS)` prevents this.

---

## What to run

```bash
./build-dev.sh
./sugar_x11
```

Start level 1. Watch the emitter pour sugar from the nozzle. The stream should look like a natural, slightly fanning flow — not three discrete columns.

To observe the pool filling up, start a level and don't draw any lines. Grains pile up at the bottom. After several seconds, the pool approaches `MAX_GRAINS` and the emitter starts returning `slot < 0` — the flow visibly stops or slows.

To see the effect of position spread, temporarily set `EMITTER_SPREAD 0` in `game.c`, rebuild, and run. You will see exactly the three-discrete-streams artifact described above.

```bash
./build-dev.sh --backend=raylib -d
./sugar_raylib_dbg
```

The Raylib debug build with AddressSanitizer will catch any out-of-bounds pool access immediately.

---

## Summary

The `GrainPool` uses a Struct-of-Arrays layout — separate contiguous arrays for `x`, `y`, `vx`, `vy`, `color`, and `active` — so the gravity loop can load only the four float arrays it needs (64 KB total) without polluting cache with `color` or `active` bytes. The `active[]` flag allows O(1) grain deactivation without shifting array elements; `grain_alloc` finds free slots by scanning `0..count` (high-watermark), extending the pool only when no recycled slot is available. The `Emitter` fires grains using a `spawn_timer` accumulator: `timer += dt; while (timer >= interval) { spawn; timer -= interval; }` — this preserves overshoot across frames for an exact long-term emission rate. `EMITTER_SPREAD` gives each spawned grain a random ±3-pixel starting column to prevent three-stream pile-up artifacts; `EMITTER_JITTER` gives each grain a ±2 px/s horizontal nudge for a naturally fanning pour.
