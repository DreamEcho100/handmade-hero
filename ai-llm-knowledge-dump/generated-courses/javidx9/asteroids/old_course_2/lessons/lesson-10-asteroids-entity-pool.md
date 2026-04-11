# Lesson 10 — Asteroids Entity Pool

## What you'll learn
- `add_asteroid()` and the active-flag pattern
- Three size tiers and their game-unit radii
- Corner spawn positions in game units
- Jagged polygon model generation

---

## add_asteroid()

```c
static void add_asteroid(GameState *state,
                         float x, float y,
                         float dx, float dy,
                         float size)
{
    if (state->asteroid_count >= MAX_ASTEROIDS) return;

    SpaceObject *a = &state->asteroids[state->asteroid_count++];
    a->x      = x;
    a->y      = y;
    a->dx     = dx;
    a->dy     = dy;
    a->size   = size;
    a->angle  = 0.0f;
    a->active = 1;
}
```

`asteroid_count` is a "fill pointer" — we always add to the end.
When asteroids die (collision), we mark `active = 0` and compact later.

---

## Three size tiers

```c
#define ASTEROID_LARGE_SIZE   0.625f    // 20px ÷ 32
#define ASTEROID_MEDIUM_SIZE  0.3125f   // 10px ÷ 32
#define ASTEROID_SMALL_SIZE   0.15625f  //  5px ÷ 32
```

The `size` field of `SpaceObject` serves double duty:
1. **Collision radius** — used in circle-circle intersection tests.
2. **Wireframe scale** — passed to `draw_wireframe` as the scale factor.

One large asteroid splits into 2 medium; one medium into 2 small; small vanishes.

---

## Corner spawns

```c
// asteroids_init — spawn 4 asteroids near corners
float corner = 1.875f;   // 60px ÷ 32
float vm     = 0.015625f; // velocity multiplier: 0.5/32

float wR = GAME_UNITS_W - corner;  // right  spawn band
float hT = GAME_UNITS_H - corner;  // top    spawn band

add_asteroid(state, corner, corner, // bottom-left
             vm * (50 + rand() % 50), vm * (50 + rand() % 50),
             ASTEROID_LARGE_SIZE);
add_asteroid(state, wR, corner,     // bottom-right
             -vm * (50 + rand() % 50), vm * (50 + rand() % 50),
             ASTEROID_LARGE_SIZE);
add_asteroid(state, corner, hT,     // top-left
             vm * (50 + rand() % 50), -vm * (50 + rand() % 50),
             ASTEROID_LARGE_SIZE);
add_asteroid(state, wR, hT,         // top-right
             -vm * (50 + rand() % 50), -vm * (50 + rand() % 50),
             ASTEROID_LARGE_SIZE);
```

The velocity multiplier `vm = 0.015625 = 0.5/32` converts old pixel-based
random ranges to game-unit velocities.  `rand() % 50 + 50` gives 50–100 px/s
→ multiplied by `vm` = 0.78–1.56 units/s.

---

## Jagged polygon model

```c
#define ASTEROID_VERTS 20
```

Built once in `asteroids_init()` for visual variety:

```c
for (int i = 0; i < ASTEROID_VERTS; i++) {
    float angle = ((float)i / (float)ASTEROID_VERTS) * 6.2832f;
    float r = 0.75f + ((float)(rand() % 5) * 0.05f);  // 0.75–0.95 of base size
    state->asteroid_model[i].x = r * sinf(angle);
    state->asteroid_model[i].y = r * cosf(angle);
}
```

All vertices use `COORD_ORIGIN_LEFT_BOTTOM` convention: `angle=0` points
up (+y), same as the ship model.  `draw_wireframe` applies the object's
`size` field as the scale, so a large asteroid uses the same polygon as
a small one — just rendered larger.

---

## Active flag vs compacting

The pool uses an active flag pattern:

```c
// Skip inactive asteroids in update:
for (int i = 0; i < state->asteroid_count; i++) {
    if (!state->asteroids[i].active) continue;
    // update position...
}

// Compact pool after collision pass (swap-with-tail):
int i = 0;
while (i < state->asteroid_count) {
    if (!state->asteroids[i].active) {
        state->asteroids[i] = state->asteroids[state->asteroid_count - 1];
        state->asteroid_count--;
    } else {
        i++;
    }
}
```

Swap-with-tail (O(n)) avoids shifting the entire array.
Order of elements changes, but that's fine — asteroids are interchangeable.

---

## Win condition

```c
// In asteroids_update(), after all updates:
if (state->asteroid_count == 0 && state->phase == PHASE_PLAYING) {
    // Player cleared all asteroids — respawn new wave
    asteroids_init(state);  // resets game (also replays music jingle)
}
```

For a real game you'd increment a wave counter and increase difficulty,
but for this course we restart the whole game.

---

## Key takeaways

1. `asteroid_count` is a fill pointer — always add to end, mark inactive on death.
2. `size` doubles as collision radius and wireframe draw scale.
3. Three tiers: large (0.625) → 2 medium (0.3125) → 2 small (0.15625) → gone.
4. Corner spawns at 1.875 units from edge (60px ÷ 32).
5. 20-vertex jagged polygon built once; `size` scales it per instance.
6. Swap-with-tail compaction: O(n), order irrelevant for interchangeable objects.
