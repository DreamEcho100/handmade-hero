# Lesson 12 — Collision Detection

## What you'll learn
- Circle-circle intersection in game units
- Two-phase: mark-then-compact
- Asteroid splitting on bullet hit
- Ship-asteroid collision and the death phase

---

## Circle-circle intersection

Both ships and asteroids are represented as circles (radius = `size`).
Two circles overlap when the distance between centres is less than the
sum of their radii:

```c
// Overlap test (compare squared distance to avoid sqrt)
static int circles_overlap(float ax, float ay, float ar,
                            float bx, float by, float br)
{
    float dx   = ax - bx;
    float dy   = ay - by;
    float dist2 = dx*dx + dy*dy;
    float rsum  = ar + br;
    return dist2 < rsum * rsum;
}
```

Squaring both sides avoids `sqrtf()`.  Works because both sides are positive.

---

## Bullet-asteroid collisions

```c
// Two-phase: mark first, compact after
int new_asteroids[MAX_ASTEROIDS * 2];
int new_count = 0;
// (collect split fragments into a temporary list, add after loop)

for (int bi = 0; bi < state->bullet_count; bi++) {
    SpaceObject *b = &state->bullets[bi];
    if (!b->active) continue;

    for (int ai = 0; ai < state->asteroid_count; ai++) {
        SpaceObject *a = &state->asteroids[ai];
        if (!a->active) continue;

        if (!circles_overlap(b->x, b->y, 0.0f,
                             a->x, a->y, a->size))
            continue;

        // Hit! Mark both dead
        b->active = 0;
        a->active = 0;

        // Score
        if      (a->size >= ASTEROID_LARGE_SIZE)  state->score += 20;
        else if (a->size >= ASTEROID_MEDIUM_SIZE) state->score += 50;
        else                                       state->score += 100;

        // Sound with spatial pan
        float pan = (a->x / GAME_UNITS_W) * 2.0f - 1.0f;
        SOUND_ID snd =
            (a->size >= ASTEROID_LARGE_SIZE)  ? SOUND_EXPLODE_LARGE  :
            (a->size >= ASTEROID_MEDIUM_SIZE) ? SOUND_EXPLODE_MEDIUM :
                                                SOUND_EXPLODE_SMALL;
        game_play_sound_panned(&state->audio, snd, pan);

        // Spawn child asteroids
        float child_size =
            (a->size >= ASTEROID_LARGE_SIZE)  ? ASTEROID_MEDIUM_SIZE :
            (a->size >= ASTEROID_MEDIUM_SIZE) ? ASTEROID_SMALL_SIZE  :
                                                0.0f;
        if (child_size > 0.0f) {
            float speed = 0.5f + (float)(rand() % 10) * 0.1f;  // 0.5–1.4 u/s
            add_asteroid(state, a->x, a->y,
                          speed,  speed, child_size);
            add_asteroid(state, a->x, a->y,
                         -speed, -speed, child_size);
        }
        break;  // bullet can only hit one asteroid
    }
}
```

---

## Compaction (swap-with-tail)

After the collision pass, remove dead entries:

```c
// Compact bullets
int i = 0;
while (i < state->bullet_count) {
    if (!state->bullets[i].active) {
        state->bullets[i] = state->bullets[state->bullet_count - 1];
        state->bullet_count--;
    } else {
        i++;
    }
}

// Compact asteroids
i = 0;
while (i < state->asteroid_count) {
    if (!state->asteroids[i].active) {
        state->asteroids[i] = state->asteroids[state->asteroid_count - 1];
        state->asteroid_count--;
    } else {
        i++;
    }
}
```

**Why two phases?**
If you removed asteroids during the collision loop, a swap-with-tail
could place an unchecked asteroid at index `ai`, and the loop would skip it.
By marking dead and compacting after, all collisions in one frame are
processed correctly.

---

## Ship-asteroid collision

```c
if (state->phase == PHASE_PLAYING && state->player.active) {
    for (int ai = 0; ai < state->asteroid_count; ai++) {
        SpaceObject *a = &state->asteroids[ai];
        if (!a->active) continue;
        if (circles_overlap(state->player.x, state->player.y, SHIP_COLLISION_RADIUS,
                            a->x, a->y, a->size))
        {
            // Ship dies
            state->player.active = 0;
            state->phase         = PHASE_DEAD;
            state->dead_timer    = DEATH_DELAY;

            if (state->score > state->best_score)
                state->best_score = state->score;

            game_play_sound(&state->audio, SOUND_SHIP_DEATH);
            break;
        }
    }
}
```

`SHIP_COLLISION_RADIUS = 0.15625` units (5px ÷ 32) — tighter than the
visible ship model so near-misses feel fair.

---

## Death phase

```c
// In asteroids_update():
if (state->phase == PHASE_DEAD) {
    state->dead_timer -= dt;

    // Allow restart after DEATH_DELAY seconds
    if (state->dead_timer <= 0.0f &&
        input->fire.ended_down &&
        input->fire.half_transition_count > 0)
    {
        asteroids_init(state);  // full restart
    }
}
```

The `dead_timer` prevents immediately restarting the game when the player
happens to be holding Space at the moment of death.

---

## Key takeaways

1. Circle overlap: `(dx² + dy²) < (r1+r2)²` — no sqrt needed.
2. Two-phase collision: mark dead first, compact after the loop.
3. Swap-with-tail compaction: O(n), order doesn't matter.
4. Bullet hits one asteroid max (`break`); asteroid dies too.
5. Child asteroids spawn at parent position with perpendicular velocities.
6. Spatial pan: `(x / GAME_UNITS_W) * 2 - 1` maps world x to [-1, +1].
7. `SHIP_COLLISION_RADIUS` is tighter than the visual ship for fairness.
