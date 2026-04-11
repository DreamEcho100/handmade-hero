# Lesson 10 — Collision Detection

## What you'll build
Bullet-vs-asteroid collision (destroys bullet, splits asteroid), player-vs-
asteroid collision (death + game over), asteroid splitting into smaller pieces,
score tracking, and wave clear.  The game becomes fully playable.

---

## Core Concepts

### 1. Circle Collision — No `sqrt` Required

Two circles overlap when the distance between their centres is less than the
sum of their radii.  The naive approach:

```c
float dist = sqrtf(dx*dx + dy*dy);
if (dist < ra + rb) { /* hit */ }
```

But we can avoid the expensive `sqrtf` by comparing **squared distances**:

```c
float dx = b->x - a->x;
float dy = b->y - a->y;
float dist2 = dx*dx + dy*dy;         // squared distance (no sqrt!)
float r_sum = a->size + 1.0f;        // 1.0f = approximate bullet radius
if (dist2 < r_sum * r_sum) { /* hit */ }
```

`dist² < (ra + rb)²` is mathematically equivalent to `dist < ra + rb`
when both sides are non-negative.  `sqrtf` is one of the slower math
functions — skipping it in a tight collision loop is a meaningful saving.

**COURSE NOTE:** The reference source uses `sqrtf` directly.  For a game
with ≤32 asteroids and ≤8 bullets, it doesn't matter in practice.  The
course shows the optimised form as a teachable moment.  This version uses
`sqrtf` for readability; Lesson 10 challenge: replace it yourself.

### 2. The Two-Loop, Compact-After Pattern

```c
// Loop 1: bullets vs asteroids (mark both inactive on hit)
for (int bi = 0; bi < state->bullet_count; bi++) {
    if (!state->bullets[bi].active) continue;
    for (int ai = 0; ai < state->asteroid_count; ai++) {
        if (!state->asteroids[ai].active) continue;
        // ... collision check ...
        if (hit) {
            state->bullets[bi].active = 0;
            state->asteroids[ai].active = 0;
            // spawn children
            break;  // one bullet hits one asteroid
        }
    }
}

// Loop 2: asteroids vs ship
for (int ai = 0; ai < state->asteroid_count; ai++) {
    // ...
}

// AFTER both loops: compact
compact_pool(state->bullets,   &state->bullet_count);
compact_pool(state->asteroids, &state->asteroid_count);
```

**Why compact after both loops?**  
If we removed during the first loop, `compact_pool`'s swap-with-tail might
move an asteroid that was already processed by the first loop to an index
that will be processed again by the second loop — double-processing.

### 3. Asteroid Splitting

When a large or medium asteroid is destroyed it splits into two smaller ones:

```c
if (a->size >= ASTEROID_LARGE_SIZE) {
    state->score += 20;
    // Two medium asteroids, deflected 90° from parent's velocity
    add_asteroid(state, a->x, a->y,
                  a->dy * 0.6f + a->dx * 0.4f,
                 -a->dx * 0.6f + a->dy * 0.4f,
                  ASTEROID_MEDIUM_SIZE);
    add_asteroid(state, a->x, a->y,
                 -a->dy * 0.6f + a->dx * 0.4f,
                  a->dx * 0.6f + a->dy * 0.4f,
                  ASTEROID_MEDIUM_SIZE);
} else if (a->size >= ASTEROID_MEDIUM_SIZE) {
    state->score += 50;
    // Two small asteroids
    // ... similar pattern ...
} else {
    state->score += 100;  // small asteroid — gone completely
}
```

The velocity calculation rotates the parent's velocity 90° in both directions
using the rotation matrix with angle=90°:
- cos(90°) = 0, sin(90°) = 1
- `x' = x*0 − y*1 = -y`
- `y' = x*1 + y*0 =  x`

That's why children get `(dy, -dx)` and `(-dy, dx)` as velocities (scaled).

### 4. Player Death

```c
// In the asteroid loop:
if (dist < a->size + state->player.size) {
    state->player.active = 0;
    state->phase = PHASE_DEAD;
    state->dead_timer = DEATH_DELAY;   // 2.5 seconds before restart
    break;
}
```

### 5. Wave Clear

```c
// After compact:
if (state->asteroid_count == 0 && state->phase == PHASE_PLAYING) {
    // All asteroids cleared — new wave (re-init preserving score)
    int saved_score = state->score;
    asteroids_init(state);
    state->score = saved_score;
}
```

---

## Score Values

| Event | Points |
|-------|--------|
| Large asteroid | 20 |
| Medium asteroid | 50 |
| Small asteroid | 100 |

---

## Verify

1. Bullets destroy asteroids; large ones split into mediums, mediums into smalls.
2. Score increments correctly.
3. Ship death activates `PHASE_DEAD` and stops movement.
4. Clearing all asteroids spawns a new wave.

---

## Summary

| Concept | C | JS equivalent |
|---------|---|---------------|
| Circle collision | `dx²+dy² < r_sum²` | Same formula |
| Split into children | `add_asteroid` ×2 | `pool.push(...)` ×2 |
| Mark → compact | Set `active=0`; compact after loops | `splice` / `filter` after loops |
