# Lesson 08 — Asteroids Entity Pool

## What you'll build
`SpaceObject asteroids[MAX_ASTEROIDS]` — a fixed-size pool of asteroid
entities.  `compact_pool` to remove dead asteroids in O(n) without heap
allocation.  Three spinning asteroids that drift and wrap.

---

## Core Concepts

### 1. Fixed-Size Entity Pools

In JavaScript you'd naturally use an array and filter:
```js
asteroids = asteroids.filter(a => a.active);  // creates a new array!
```

In C (game loops), we avoid allocation inside the loop entirely.
Instead, we pre-allocate a fixed-size array and use an integer counter:

```c
SpaceObject asteroids[MAX_ASTEROIDS];   // pool storage (stack)
int         asteroid_count;             // how many are "live"

// All entries with index < asteroid_count are in use.
// To "add" an asteroid: asteroids[asteroid_count++] = new_asteroid;
// To "remove" an asteroid: mark it inactive, compact later.
```

This is called an **"array pool"** or **"packed array"** pattern.

### 2. `SpaceObject` — One Struct for Every Entity

```c
typedef struct {
    float x, y;     // position
    float dx, dy;   // velocity
    float angle;    // current heading / rotation
    float size;     // collision radius
    int   active;   // 0 = dead, 1 = live
} SpaceObject;
```

Using the same struct for the ship, asteroids, and bullets keeps the pool
code generic — `compact_pool(pool, count)` works on any of them.

### 3. `compact_pool` — Swap-with-Tail Removal

```c
static void compact_pool(SpaceObject *pool, int *count) {
    int i = 0;
    while (i < *count) {
        if (!pool[i].active) {
            // Replace dead slot with the LAST live element
            pool[i] = pool[*count - 1];
            (*count)--;
            // Do NOT increment i: the swapped-in element must be checked
        } else {
            i++;
        }
    }
}
```

**Why swap-with-tail instead of shift?**

| Method | Operation | Time |
|--------|-----------|------|
| Shift  | Move every element after the gap | O(n) copies per removal |
| Swap-with-tail | Copy one element from the end | O(1) per removal |

Total compaction is still O(n) (one pass over the array), but each individual
removal is O(1) instead of O(n).

**The critical rule:** call `compact_pool` AFTER all detection loops, not
during them.  If you swap during a loop, the newly-moved element might be
processed twice or skipped.

### 4. `add_asteroid`

```c
static int add_asteroid(GameState *state,
                        float x, float y, float dx, float dy, float size)
{
    if (state->asteroid_count >= MAX_ASTEROIDS) return 0;  // pool full

    SpaceObject *a = &state->asteroids[state->asteroid_count++];
    a->x = x;  a->y = y;
    a->dx = dx; a->dy = dy;
    a->size = size;
    a->angle = 0.0f;
    a->active = 1;
    return 1;
}
```

Returns 0 (failure) if the pool is full.  The caller decides whether to
ignore or log this — we silently ignore it in this game.

### 5. Asteroid Model — Jagged Circle

```c
// In asteroids_init():
for (int i = 0; i < ASTEROID_VERTS; i++) {
    float noise = 0.8f + ((float)(rand() % 100) / 100.0f) * 0.4f;
    // noise in [0.8, 1.2]: random 20% perturbation of a unit circle
    float t = ((float)i / ASTEROID_VERTS) * 2.0f * PI;
    state->asteroid_model[i].x = sinf(t) * noise;
    state->asteroid_model[i].y = cosf(t) * noise;
}
```

One model is built per game (in `asteroids_init`); all asteroids share it.
`draw_wireframe` scales by `a->size` — so large (r=20), medium (r=10), and
small (r=5) asteroids all use the same vertex data, just at different scales.

---

## Spawn 4 Starting Asteroids

In `reset_game`:
```c
add_asteroid(state, 60.0f, 60.0f, 
             (rand() % 40 - 20) * 0.5f, (rand() % 40 - 20) * 0.5f,
             ASTEROID_LARGE_SIZE);
// ... three more at other corners
```

Placing them at screen corners ensures they don't spawn on top of the
player (who starts at the centre).

---

## Verify

1. Four spinning asteroids drift around the screen.
2. Their shapes are slightly different each run (random noise).
3. They wrap at all screen edges.
4. Removing one from the pool in the debugger (set `active=0`) and watching
   `compact_pool` close the gap without crashing.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Fixed pool | `SpaceObject arr[MAX]` + `int count` | `Array` (dynamic) |
| O(1) removal | Swap with last element | `arr.splice(i, 1)` (O(n)) |
| No-alloc update loop | Iterate `count`, compact after | `arr.filter(...)` (allocates) |
