# Lesson 14 --- Particle Laboratory

> **Advanced Module.** This lesson requires the `--particle-lab` build flag. All code is conditionally compiled under `#ifdef ENABLE_PARTICLE_LAB`.

## What we're building

A stress-test laboratory that spawns 1000 particles into the same things pool you built in lessons 02--13. Each particle follows one of 6 movement archetypes --- Bounce, Wander, Seek, Predictive Seek, Orbit, Drift Noise --- selected by a data field, not by an inheritance hierarchy. Particles collide with each other, and collision **mutates their archetype** rather than destroying them. A performance HUD overlay shows entity count, FPS, and update time in microseconds.

When you run `./build-dev.sh --backend=raylib --particle-lab -r` and press `0`, the screen fills with a swarm of colored particles. Some bounce off walls. Some wander drunkenly. Some chase a target. Some predict where the target will be and intercept. Some orbit a fixed point. Some drift on noise. When two particles collide, one of them changes color and movement pattern --- its archetype field got rewritten. The HUD in the corner tells you exactly how many entities are alive and how long the update loop takes.

This is the capstone. Everything from lessons 02--13 --- the flat array, the nil sentinel, the free list, generational IDs, the pool iterator, the kind enum --- is running under real load. If the pool works at 1000 entities and 60 FPS, it works.

## What you'll learn

- **Parallel arrays** --- extending pool entities with per-entity data (archetype, angular velocity, noise offset) without modifying `things.h`
- **Behavior as data** --- a `switch` on an integer archetype field inside an explicit `for` loop, replacing virtual dispatch and inheritance hierarchies
- **Data mutation over object replacement** --- changing an entity's behavior by rewriting a field, not by creating a new object
- **Predictive AI with controlled imperfection** --- `future_pos = current_pos + velocity * lookahead + random_error`
- **Performance observability** --- measuring real frame times and entity counts to prove the architecture scales

## Prerequisites

- Completed lessons 01--13 (the full things pool system and game demo)
- Familiarity with the `thing_pool` API: `thing_pool_add`, `thing_pool_remove`, `thing_pool_get`, `thing_is_valid`
- The game demo from lessons 11--12 running on at least one backend (Raylib recommended)

---

## Building and running (quick start)

Before diving into the code, here is how to build and run the Particle Lab so you can see what we are building:

```bash
cd course/
./build-dev.sh --backend=raylib --particle-lab -r
```

Once the window opens, press `0` to switch to the Particle Laboratory scene. You will see 1000 colored particles swarming, bouncing, seeking, orbiting, and mutating on collision. The HUD in the corner shows entity count, FPS, and update time. Press other number keys (`1`--`9`) to visit other scenes, then `0` to return.

If you are on X11 instead of Raylib:

```bash
./build-dev.sh --backend=x11 --particle-lab -r
```

> **JS bridge:** If you are coming from JavaScript, think of `./build-dev.sh` as your `npm run dev` --- it compiles the C source and launches the binary in one step. The `--particle-lab` flag is like an environment variable that enables a feature flag at compile time (similar to `NEXT_PUBLIC_FEATURE_X=true` in Next.js, except it happens at the compiler level via `#ifdef`, not at runtime).

---

## Step 1 --- Why a stress test matters

Lessons 11--12 proved the pool is *correct*. The player moves, trolls spawn, collision removes them, the free list recycles slots, generational IDs catch stale references. All verified. But correctness at 20 entities is a different claim than correctness at 1000 entities.

Here is what changes at scale:

1. **The free list gets a workout.** With 20 entities you might recycle a slot once or twice. With 1000 entities mutating on collision, slots churn constantly. If there is a bug in the free list linkage --- a cycle, a dangling index, an off-by-one --- it shows up under churn.

2. **The iterator hits real gaps.** At 20 entities in a 1024-slot array, almost every slot is alive. At 1000 entities with mutation-driven removal and re-addition, the `used[]` array develops genuine gaps. The iterator must skip them correctly every frame.

3. **Performance becomes measurable.** At 20 entities, the update loop takes microseconds. You cannot tell the difference between a good and bad data layout. At 1000 entities with per-entity behavior dispatch, you can actually see whether the flat array approach is keeping you inside your frame budget.

4. **Emergent behavior proves the system.** When 1000 particles are bouncing, seeking, orbiting, and mutating on collision, any systemic bug becomes visible. A nil sentinel corruption shows as a cluster of particles snapping to (0,0). A stale reference shows as a particle teleporting into another's slot. You do not need a debugger --- you can *see* the bugs.

> **Anton says:** "You could have 100,000 things and you're spending 150 megabytes. You're going to be completely bottlenecked by something else."

The Particle Lab proves that quote. 1000 entities at 48 bytes each is 48 KB. The bottleneck is not memory. The bottleneck is not the data structure. The bottleneck --- if there is one --- is whatever *you do* with the data. And the HUD will tell you exactly how much time that takes.

---

## Step 2 --- Parallel arrays: extending pool entities without struct changes

Here is the problem. Each particle needs data that does not belong in the `thing` struct: an archetype ID, an angular velocity for orbiting, a noise offset for drifting, a wander angle for wandering. Stuffing all of this into the `thing` struct would bloat every entity in the pool --- even the player and trolls that never use these fields.

The solution is **parallel arrays**. You create separate arrays, indexed by the same `thing_idx`, that hold the particle-specific data:

```
thing_pool.things[]       ← position, velocity, size, color, kind (shared by ALL entities)
particle_archetype[]      ← which movement pattern (parallel, indexed by thing_idx)
particle_angular_vel[]    ← orbital angular velocity (parallel, indexed by thing_idx)
particle_noise_offset[]   ← drift noise seed (parallel, indexed by thing_idx)
particle_wander_angle[]   ← current wander direction (parallel, indexed by thing_idx)
```

In memory, it looks like this:

```
things[]:          [NIL] [PLAYER] [PARTICLE] [PARTICLE] [PARTICLE] ...
                     0       1        2          3          4

particle_archetype[]:  [0]    [0]      [BOUNCE]   [SEEK]     [ORBIT]  ...
                        0      1         2          3          4
                   (unused) (unused)  (matches   (matches   (matches
                                      things[2]) things[3]) things[4])
```

Here is a wider view showing how all parallel arrays line up by `thing_idx`:

```
Parallel arrays (indexed by thing_idx):
thing_idx:              [0]  [1]  [2]  [3]  [4]  ...
pool.things[]:          NIL  PLYR PTCL PTCL PTCL ...  (in the thing_pool)
particle_archetype[]:   ---  ---  SEEK BNCE ORBT ...  (parallel array)
particle_angular_vel[]: ---  ---  0.0  0.0  2.1  ...  (parallel array)
particle_noise_offset[]:---  ---  0.0  0.0  0.0  ...  (parallel array)
particle_wander_angle[]:---  ---  1.2  0.0  3.4  ...  (parallel array)
                         ^    ^
                     (unused)(unused -- player, not a particle)
```

Slots 0 and 1 hold the nil sentinel and the player. Their parallel array entries are garbage, but that is fine -- the update loop checks `kind == THING_KIND_PARTICLE` before reading the parallel arrays, so non-particle slots are never touched.

The key insight: `particle_archetype[i]` describes the particle at `things[i]`. The index is the link. No pointers. No hash maps. No separate "particle" struct that references back to a "thing." Just parallel arrays that share an index.

> **Why?** In JavaScript, you might add properties to an object: `entity.archetype = 'bounce'`. In C, adding a field to the `thing` struct affects every entity. Parallel arrays let you extend *specific* entities without touching the shared struct. This is the C equivalent of a mixin --- data bolted on from the outside.

> **JS bridge:** Parallel arrays are like having multiple `Map` objects that all use the same key. Imagine `const archetypes = new Map()`, `const angularVels = new Map()`, `const noiseOffsets = new Map()` --- all keyed by entity ID. When you want entity 47's archetype, you look up `archetypes.get(47)`. In C, the "key" is just the array index, so `particle_archetype[47]` is equivalent. The difference is that array lookup is a single pointer offset (one CPU instruction), while `Map.get()` involves hashing, bucket lookup, and key comparison. Parallel arrays are the fastest possible key-value store --- when your keys are dense integers starting from 0.

### Where the arrays live

These parallel arrays are declared inside the game state, **not** inside `things.h`. The pool library does not know particles exist. The pool library does not know archetypes exist. It manages slots. The particle-specific data is the game's concern:

```c
/* In game.h, inside game_state */
#ifdef ENABLE_PARTICLE_LAB
#define MAX_PARTICLES MAX_THINGS
#define PARTICLE_ARCHETYPE_COUNT 6

typedef enum {
    ARCHETYPE_BOUNCE = 0,
    ARCHETYPE_WANDER,
    ARCHETYPE_SEEK,
    ARCHETYPE_PREDICTIVE_SEEK,
    ARCHETYPE_ORBIT,
    ARCHETYPE_DRIFT_NOISE,
} particle_archetype_kind;

/* Parallel arrays indexed by thing_idx */
int    particle_archetype[MAX_PARTICLES];
float  particle_angular_vel[MAX_PARTICLES];
float  particle_noise_offset[MAX_PARTICLES];
float  particle_wander_angle[MAX_PARTICLES];
#endif
```

When `thing_pool_add` returns index 47, you write `particle_archetype[47] = ARCHETYPE_ORBIT`. When `thing_pool_remove` frees index 47, the parallel array entry at 47 becomes garbage --- but that is fine, because `used[47]` is false, so the iterator will never visit it.

> **Handmade Hero connection:** Casey's entity system also uses flat arrays with behavior dispatched via type tags. The `EntityType` enum in HH plays the same role as `thing_kind` here, and per-type data lives in parallel or in the fat struct. The pattern is the same: flat array, integer index, data-driven dispatch.

### Zero-initialization safety

When you `memset` the game state to zero, every `particle_archetype[i]` becomes 0, which is `ARCHETYPE_BOUNCE`. This is deliberate. The zero value must always be a safe, valid default --- the same principle as the nil sentinel at `things[0]`. If a particle somehow gets its archetype field corrupted to zero, it bounces. It does not crash. It does not access uninitialized memory. It just bounces.

---

## Step 3 --- Behavior as data: the archetype switch

Here is the core pattern. Every frame, you iterate the pool and update each particle based on its archetype:

```c
void particle_lab_update(game_state *state, float dt)
{
    thing_pool *pool = &state->pool;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        if (pool->things[i].kind != THING_KIND_PARTICLE) continue;

        thing *p = &pool->things[i];

        switch (state->particle_archetype[i]) {
            case ARCHETYPE_BOUNCE:
                particle_update_bounce(p, dt);
                break;
            case ARCHETYPE_WANDER:
                particle_update_wander(p, &state->particle_wander_angle[i], dt);
                break;
            case ARCHETYPE_SEEK:
                particle_update_seek(p, state->seek_target_x,
                                     state->seek_target_y, dt);
                break;
            case ARCHETYPE_PREDICTIVE_SEEK:
                particle_update_predictive_seek(p, state->seek_target_x,
                                                state->seek_target_y,
                                                state->seek_target_dx,
                                                state->seek_target_dy, dt);
                break;
            case ARCHETYPE_ORBIT:
                particle_update_orbit(p, &state->particle_angular_vel[i],
                                     state->orbit_center_x,
                                     state->orbit_center_y, dt);
                break;
            case ARCHETYPE_DRIFT_NOISE:
                particle_update_drift(p, state->particle_noise_offset[i], dt);
                break;
        }
    }
}
```

Read that loop carefully. There is no vtable. No virtual function call. No `particle->update()` method. No inheritance hierarchy. There is a `for` loop, a `switch`, and six concrete functions. The `switch` is the dispatch mechanism, and the `particle_archetype[i]` integer is the selector.

> **JS bridge:** The `switch` on `particle_archetype[i]` is the C equivalent of the Strategy pattern in JavaScript. In JS, you might write `const strategies = { bounce: updateBounce, wander: updateWander, seek: updateSeek }` and then call `strategies[entity.archetype](entity, dt)`. The `switch` does the same dispatch, but the compiler turns it into a **jump table** --- a flat array of function addresses indexed by the integer value. Jump tables are faster than object property lookups because there is no hash, no string comparison, no prototype chain traversal. The CPU loads one address from a fixed offset and jumps to it.

> **Why not use the thing_iter abstraction from Lesson 10?** Because this is a teaching laboratory. The raw `for (int i = 1; i < pool->next_empty; i++)` loop with explicit `used[i]` and `kind` checks makes the iteration mechanics visible. You can see exactly what the iterator does internally. In production code, you would use the iterator. In a lesson about understanding the pool under load, the raw loop is clearer.

### What this replaces in OOP

In a class-based system, you would have:

```
class Particle { virtual void update(float dt) = 0; };
class BouncingParticle : public Particle { void update(float dt) override { ... } };
class WanderingParticle : public Particle { void update(float dt) override { ... } };
class SeekingParticle  : public Particle { void update(float dt) override { ... } };
// ...6 classes, each with a vtable entry
```

With 1000 particles, that is 1000 virtual function calls per frame, each jumping through a vtable pointer to a function address that the CPU branch predictor cannot anticipate (because the pointer depends on the runtime type). The data for different particle types is scattered across the heap because each `new BouncingParticle` allocates separately.

With the archetype switch, the data is contiguous in the `things[]` array. The `switch` compiles to a jump table that the CPU can predict. All 1000 particles are touched in a linear sweep through memory. This is not premature optimization --- this is the natural consequence of the data layout we already built.

> **Alternative approach:** You could use an array of function pointers: `update_fn[archetype](particle, dt)`. That is closer to a vtable, but still data-driven --- the function pointer is selected by the archetype integer, not by an object's hidden vptr. For teaching clarity, the explicit `switch` is better because the student can see every code path without indirection.

---

## Step 4 --- The 6 archetypes

Each archetype is a pure function of the particle's current state. No archetype reads another particle's state (except Seek/Predictive Seek, which read a shared target position). No archetype allocates memory. No archetype modifies the pool. They just update position and velocity.

### Archetype 0: Bounce

The default. The particle moves in a straight line and reverses direction when it hits a screen edge.

```c
static void particle_update_bounce(thing *p, float dt)
{
    p->x += p->dx * dt;
    p->y += p->dy * dt;

    if (p->x < 0.0f || p->x > SCREEN_W) p->dx = -p->dx;
    if (p->y < 0.0f || p->y > SCREEN_H) p->dy = -p->dy;

    /* Clamp inside bounds */
    if (p->x < 0.0f) p->x = 0.0f;
    if (p->x > SCREEN_W) p->x = SCREEN_W;
    if (p->y < 0.0f) p->y = 0.0f;
    if (p->y > SCREEN_H) p->y = SCREEN_H;
}
```

This is the simplest possible movement. It is also the zero-value default: if a particle's archetype field is corrupted to 0, it bounces. Safe fallback.

### Archetype 1: Wander

The particle walks in a direction that drifts randomly each frame. Think of a drunk person walking --- they have momentum, but the direction wobbles.

```c
static void particle_update_wander(thing *p, float *wander_angle, float dt)
{
    /* Drift the angle by a random amount each frame */
    *wander_angle += ((float)rand() / (float)RAND_MAX - 0.5f) * 3.0f * dt;

    float speed = 80.0f;
    p->dx = cosf(*wander_angle) * speed;
    p->dy = sinf(*wander_angle) * speed;

    p->x += p->dx * dt;
    p->y += p->dy * dt;

    /* Wrap around screen edges */
    if (p->x < 0.0f) p->x += SCREEN_W;
    if (p->x > SCREEN_W) p->x -= SCREEN_W;
    if (p->y < 0.0f) p->y += SCREEN_H;
    if (p->y > SCREEN_H) p->y -= SCREEN_H;
}
```

The `wander_angle` is stored in the parallel array `particle_wander_angle[i]`. Each frame, a random perturbation is added. The result is smooth, organic-looking movement. Notice: the wander angle is *per particle*, stored in a parallel array, mutated in place. No object. No state machine. Just a float being nudged each frame.

> **Why `cosf` and `sinf`?** These are the single-precision (float) versions of `cos` and `sin`. In JavaScript, `Math.cos()` and `Math.sin()` always use double-precision (64-bit) floats. C gives you a choice: `cos()` for `double`, `cosf()` for `float`. Since our positions are `float`, we use `cosf`/`sinf` to avoid an unnecessary double-to-float conversion. The math is the same --- `cosf(angle)` gives the x-component of a unit vector at that angle, `sinf(angle)` gives the y-component. Together they convert an angle into a direction vector. If you have ever done `{ x: Math.cos(angle), y: Math.sin(angle) }` in a canvas game, this is identical.

> **Why `rand()` and not something better?** The C standard `rand()` function is a simple pseudo-random number generator. It is NOT suitable for cryptography (do not use it for passwords or tokens), but for scattering particle behavior it is more than good enough. In a production engine, you might use a **xorshift RNG** instead --- a faster PRNG that produces better statistical distribution in about 4 CPU instructions. Xorshift works by repeatedly XOR-ing a state value with bit-shifted copies of itself: `state ^= state << 13; state ^= state >> 7; state ^= state << 17`. It is deterministic (same seed = same sequence), which is useful for replays and networking. For this lab, `rand()` keeps the code simple.

### Archetype 2: Seek

The particle accelerates toward a target position. Simple steering: compute the direction to the target, accelerate in that direction.

```c
static void particle_update_seek(thing *p, float target_x, float target_y,
                                 float dt)
{
    float to_x = target_x - p->x;
    float to_y = target_y - p->y;
    float dist = sqrtf(to_x * to_x + to_y * to_y);

    if (dist > 1.0f) {
        float nx = to_x / dist;
        float ny = to_y / dist;
        float accel = 120.0f;
        p->dx += nx * accel * dt;
        p->dy += ny * accel * dt;
    }

    /* Speed cap */
    float speed = sqrtf(p->dx * p->dx + p->dy * p->dy);
    if (speed > 200.0f) {
        p->dx = (p->dx / speed) * 200.0f;
        p->dy = (p->dy / speed) * 200.0f;
    }

    p->x += p->dx * dt;
    p->y += p->dy * dt;
}
```

Seek is deliberately dumb. It aims directly at where the target *is*, not where the target *will be*. This makes it visibly lag behind a moving target --- particles cluster behind the target instead of intercepting it. That lag is the motivation for Archetype 3.

### Archetype 3: Predictive Seek

The upgrade to Seek. Instead of aiming at the target's current position, aim at where the target will be in `lookahead` seconds --- plus some random error.

```c
static void particle_update_predictive_seek(thing *p,
                                            float target_x, float target_y,
                                            float target_dx, float target_dy,
                                            float dt)
{
    float lookahead = 0.5f; /* seconds into the future */

    /* Predict future target position */
    float future_x = target_x + target_dx * lookahead;
    float future_y = target_y + target_dy * lookahead;

    /* Add controlled imperfection */
    float error_radius = 30.0f;
    future_x += ((float)rand() / (float)RAND_MAX - 0.5f) * error_radius;
    future_y += ((float)rand() / (float)RAND_MAX - 0.5f) * error_radius;

    /* Seek toward predicted position */
    float to_x = future_x - p->x;
    float to_y = future_y - p->y;
    float dist = sqrtf(to_x * to_x + to_y * to_y);

    if (dist > 1.0f) {
        float nx = to_x / dist;
        float ny = to_y / dist;
        float accel = 150.0f;
        p->dx += nx * accel * dt;
        p->dy += ny * accel * dt;
    }

    /* Speed cap */
    float speed = sqrtf(p->dx * p->dx + p->dy * p->dy);
    if (speed > 250.0f) {
        p->dx = (p->dx / speed) * 250.0f;
        p->dy = (p->dy / speed) * 250.0f;
    }

    p->x += p->dx * dt;
    p->y += p->dy * dt;
}
```

The formula is: `future_pos = current_pos + velocity * lookahead + random_error`.

Three things to notice:

1. **Lookahead.** `0.5f` seconds is half a second into the future. The particle aims at where the target will be, not where it is. Visually, predictive seekers converge *ahead* of the target, while basic seekers trail behind.

2. **Error radius.** `30.0f` pixels of random scatter. Without this, all predictive seekers converge to the same point and stack on top of each other. The error spreads them into a cloud around the predicted intercept point. This looks more natural and demonstrates that imperfect prediction is often better than perfect prediction --- a useful lesson for game AI.

3. **No special state.** Predictive seek reads the target's velocity (`target_dx`, `target_dy`) from the shared game state. It does not store "previous target position" or "interception trajectory" --- it recomputes from scratch each frame. Stateless update, computed from current data. Simple.

> **Common mistake:** Trying to build a "proper" interception algorithm with quadratic equations for time-to-intercept. For a particle lab, the linear prediction with noise is better: simpler to understand, visually interesting, and demonstrates the principle without the math overhead.

### Archetype 4: Orbit

The particle orbits a fixed center point. The angular velocity is stored in the parallel array.

```c
static void particle_update_orbit(thing *p, float *angular_vel,
                                  float center_x, float center_y, float dt)
{
    float rel_x = p->x - center_x;
    float rel_y = p->y - center_y;
    float radius = sqrtf(rel_x * rel_x + rel_y * rel_y);

    if (radius < 1.0f) radius = 1.0f; /* Prevent division by zero */

    float angle = atan2f(rel_y, rel_x);
    angle += (*angular_vel) * dt;

    p->x = center_x + cosf(angle) * radius;
    p->y = center_y + sinf(angle) * radius;
    p->dx = -sinf(angle) * (*angular_vel) * radius;
    p->dy =  cosf(angle) * (*angular_vel) * radius;
}
```

Each orbiting particle has a different `angular_vel` (set randomly on spawn), so they orbit at different speeds and directions. Some orbit clockwise, some counterclockwise. The resulting visual is a loose ring of particles circling the center point.

Notice: `dx`/`dy` are set to the tangential velocity. This is important for the collision system --- if an orbiting particle gets its archetype mutated to Seek, its current velocity provides a smooth transition instead of a dead stop.

### Archetype 5: Drift Noise

The particle's velocity is driven by a simple noise function. The `noise_offset` ensures each particle samples a different part of the noise space.

```c
static float simple_noise(float x, float y)
{
    /* Fast, deterministic pseudo-noise. Not Perlin, not simplex ---
     * just something that varies smoothly enough for visual drift. */
    float val = sinf(x * 1.3f + y * 0.7f) * cosf(y * 1.1f - x * 0.9f);
    return val;
}

static void particle_update_drift(thing *p, float noise_offset, float dt)
{
    float time_scale = 0.3f;
    float nx = simple_noise(p->x * 0.01f + noise_offset,
                            p->y * 0.01f + noise_offset);
    float ny = simple_noise(p->y * 0.01f + noise_offset + 100.0f,
                            p->x * 0.01f + noise_offset + 100.0f);

    float speed = 60.0f;
    p->dx = nx * speed;
    p->dy = ny * speed;
    p->x += p->dx * dt;
    p->y += p->dy * dt;

    /* Wrap */
    if (p->x < 0.0f) p->x += SCREEN_W;
    if (p->x > SCREEN_W) p->x -= SCREEN_W;
    if (p->y < 0.0f) p->y += SCREEN_H;
    if (p->y > SCREEN_H) p->y -= SCREEN_H;
}
```

The `noise_offset` per particle is stored in `particle_noise_offset[i]`, set randomly on spawn. This ensures each drifting particle follows a different flow pattern. The visual effect is particles drifting in smooth, lazy curves that slowly evolve.

> **Why not use proper Perlin noise?** Because this is a particle laboratory, not a terrain generator. The `simple_noise` function produces smooth-enough variation for visible drift. Adding a Perlin noise library would add a dependency, increase code complexity, and teach nothing about the pool system. Simplicity over fidelity.

---

## Step 5 --- Prediction with controlled imperfection

Let's zoom in on the predictive seek, because it demonstrates a principle that extends far beyond particles.

The naive approach to game AI is perfect information: the enemy knows exactly where the player is and moves directly toward them. This produces robotic, boring behavior. Real intelligence --- even artificial intelligence --- involves uncertainty.

The predictive seek formula introduces uncertainty in two places:

```
future_x = target_x + target_dx * lookahead + random_error
```

1. **The prediction itself.** `target_dx * lookahead` assumes the target will maintain its current velocity for the next 0.5 seconds. This is wrong most of the time --- the player changes direction constantly. But the prediction is *close enough* to produce visibly smarter behavior than basic seek.

2. **The random error.** `random_error` up to 30 pixels in each axis. This scatters the interception points, preventing all seekers from converging to a single pixel. It also simulates sensor noise or reaction-time variance.

Here is a concrete example of the prediction math:

```
Player at (400, 300), moving right at 200px/s

Prediction with 0.5s lookahead:
  predicted_x = 400 + 200 * 0.5 = 500
  predicted_y = 300 + 0   * 0.5 = 300

With error (error_radius=30):
  actual_target_x = 500 + random(-30, +30)
  actual_target_y = 300 + random(-30, +30)

  ┌─── screen ────────────────────────┐
  │              o particle            │
  │               \                    │
  │                → predicted (500,300)│
  │              *  actual target      │
  │          [PLAYER]→→→               │
  │          (400,300)  moving right   │
  └───────────────────────────────────┘
```

The particle aims at the predicted-plus-error point, not at the player's current position. This is why predictive seekers converge *ahead* of the target rather than trailing behind it.

The result: predictive seekers appear to *anticipate* the target. They cluster ahead of it rather than behind it. Some overshoot, some undershoot. The swarm looks alive.

```
Basic Seek swarm:                    Predictive Seek swarm:

         Target ──►                           Target ──►
              ·····                       ·  ·
            ·····                        · ·  ·
          ·····                         ·  · ·
        (all trailing behind)         (scattered ahead of target)
```

This is "emergent intelligence" from simple rules. No pathfinding graph. No behavior tree. No state machine. Just `position + velocity * time + noise`.

> **JS bridge:** Prediction is the same concept as estimating element positions in JavaScript animations. If you know a `div` is moving at 200px/sec to the right, you can predict where it will be in 500ms: `futureLeft = currentLeft + 200 * 0.5`. Game AI uses this same arithmetic to intercept moving targets. The `setTimeout + estimated position` pattern in JS (schedule an action based on where something WILL be, not where it IS) is the same mental model. The random error term is like adding `Math.random() * jitter` to your estimate --- it prevents all predictors from converging on exactly the same pixel, which would look robotic.

---

## Step 6 --- Data mutation: collision changes behavior, not objects

Here is where the Particle Lab demonstrates the anti-OOP principle most directly.

In a class-based system, if a `BouncingParticle` needs to become a `SeekingParticle` after collision, you have a problem. You cannot change an object's class at runtime. You have to:

1. Create a new `SeekingParticle` object
2. Copy all relevant state from the old `BouncingParticle`
3. Destroy the old `BouncingParticle`
4. Update all references to point to the new object

That is four operations, a heap allocation, a deallocation, and a reference-update pass. For 1000 particles colliding every frame, this is a disaster.

In the things pool with parallel arrays, mutation is:

```c
state->particle_archetype[i] = ARCHETYPE_SEEK;
```

One line. One integer write. The particle is now a seeker. Its position, velocity, size, and color remain exactly where they are in the `things[]` array. Its slot index does not change. Its generational ID does not change. Every reference to it remains valid. Next frame, the `switch` in the update loop dispatches to `particle_update_seek` instead of `particle_update_bounce`.

### The collision-mutation loop

```c
void particle_lab_collisions(game_state *state)
{
    thing_pool *pool = &state->pool;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        if (pool->things[i].kind != THING_KIND_PARTICLE) continue;

        for (int j = i + 1; j < pool->next_empty; j++) {
            if (!pool->used[j]) continue;
            if (pool->things[j].kind != THING_KIND_PARTICLE) continue;

            thing *a = &pool->things[i];
            thing *b = &pool->things[j];

            float dx = a->x - b->x;
            float dy = a->y - b->y;
            float dist_sq = dx * dx + dy * dy;
            float min_dist = a->size + b->size;

            if (dist_sq < min_dist * min_dist) {
                /* Mutate: the "loser" changes archetype */
                int new_archetype = (state->particle_archetype[i] + 1)
                                    % PARTICLE_ARCHETYPE_COUNT;
                state->particle_archetype[j] = new_archetype;

                /* Visual feedback: change color */
                pool->things[j].color = archetype_color(new_archetype);

                /* Separate the particles to prevent repeated collision */
                float dist = sqrtf(dist_sq);
                if (dist > 0.01f) {
                    float overlap = min_dist - dist;
                    float nx = dx / dist;
                    float ny = dy / dist;
                    b->x -= nx * overlap * 0.5f;
                    b->y -= ny * overlap * 0.5f;
                    a->x += nx * overlap * 0.5f;
                    a->y += ny * overlap * 0.5f;
                }
            }
        }
    }
}
```

The critical line is:

```c
state->particle_archetype[j] = new_archetype;
```

That is the mutation. Particle `j` now follows a different movement pattern. Its slot in the `things[]` array did not change. Its index did not change. No allocation. No deallocation. No reference update. The next frame, the `switch` sends it down a different code path.

The color change is visual feedback so you can *see* mutations happening in real time. Bouncing particles are red, wanderers are green, seekers are yellow, predictive seekers are cyan, orbiters are magenta, drifters are blue. When a particle collides and its color changes, you are watching data mutation in action.

> **Common mistake:** Trying to implement this with an OOP "Strategy pattern" --- creating a `MovementStrategy` interface and swapping strategy objects on collision. That is still heap allocation and indirection. The parallel array rewrite is zero-cost.

### Why the N-squared collision loop is acceptable here

Yes, the collision check is O(n^2). With 1000 particles, that is up to 500,000 pair checks per frame. In a real game, you would use spatial partitioning (grid, quadtree). In this laboratory, the O(n^2) loop is deliberate:

1. It stresses the pool iteration path maximally.
2. It keeps the code simple --- no spatial data structure to explain.
3. On modern hardware, 500K simple distance checks is still fast enough for 60 FPS. The HUD will show you.

If the HUD shows the update exceeding 16ms, that is a useful data point too: it tells you when you need spatial partitioning, based on real measurement rather than premature optimization.

---

## Step 7 --- Performance observability

The Particle Lab HUD shows four metrics:

```
┌──────────────────────────────────┐
│ Entities: 1000/1024              │
│ FPS: 60                          │
│ Update: 2.3ms                    │
│ Archetype: B:167 W:168 S:166    │
│            PS:167 O:166 DN:166   │
└──────────────────────────────────┘
```

### How to measure update time

```c
#include <time.h>

void particle_lab_scene(game_state *state, const game_input *input, float dt)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    particle_lab_update(state, dt);
    particle_lab_collisions(state);

    clock_gettime(CLOCK_MONOTONIC, &end);

    long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L
                    + (end.tv_nsec - start.tv_nsec);
    state->particle_update_us = (float)elapsed_ns / 1000.0f;
}
```

`clock_gettime(CLOCK_MONOTONIC, ...)` gives nanosecond-resolution wall-clock time. We convert to microseconds for readability. At 60 FPS, your entire frame budget is 16,667 microseconds (16.6ms). If the particle update takes 2,000 microseconds (2ms), you are using 12% of the frame budget for entity logic. That leaves 88% for rendering, input, audio, and everything else. Comfortable.

### Entity count

```c
int alive = 0;
for (int i = 1; i < pool->next_empty; i++) {
    if (pool->used[i]) alive++;
}
```

At startup, 1000 particles are spawned. After that, the count should stay at 1000 unless you add a mechanism to remove particles. The count on the HUD confirms no particles are leaking (count drops unexpectedly) or being double-added (count exceeds expected).

### Archetype distribution

Count how many particles are in each archetype:

```c
int archetype_counts[PARTICLE_ARCHETYPE_COUNT] = {0};
for (int i = 1; i < pool->next_empty; i++) {
    if (!pool->used[i]) continue;
    if (pool->things[i].kind != THING_KIND_PARTICLE) continue;
    archetype_counts[state->particle_archetype[i]]++;
}
```

Initially all particles are evenly distributed. Over time, collisions shift the distribution. Watching the archetype counts evolve tells you the collision-mutation system is working. If one archetype count goes to zero, particles are being mutated away from it faster than into it --- an interesting emergent behavior to observe.

### What the numbers tell you

| Metric | Healthy | Problem |
|---|---|---|
| Entity count | Stable at expected value (1000) | Dropping = leak; Rising above 1024 = impossible (pool caps it) |
| FPS | 60 (vsync) or uncapped target | Below 30 = update or render too expensive |
| Update time | < 5ms for 1000 entities | > 10ms = collision loop needs spatial optimization |
| Archetype distribution | Roughly even, slowly shifting | One archetype at 0 = mutation cycle is one-directional |

The HUD is not decoration. It is your instrumentation. Professional game engines have metrics overlays for the same reason: you cannot optimize what you cannot measure.

---

## Step 8 --- Building and running

### The --particle-lab build flag

In `build-dev.sh`, the `--particle-lab` flag adds `-DENABLE_PARTICLE_LAB` to the compiler flags:

```bash
PARTICLE_LAB=0
for arg in "$@"; do
    case "$arg" in
        --particle-lab) PARTICLE_LAB=1 ;;
    esac
done

EXTRA_FLAGS=""
if [ "$PARTICLE_LAB" = "1" ]; then
    EXTRA_FLAGS="$EXTRA_FLAGS -DENABLE_PARTICLE_LAB"
fi
```

Without the flag, every `#ifdef ENABLE_PARTICLE_LAB` block is stripped by the preprocessor. The compiled binary contains zero particle lab code. No dead code. No unused data. No binary bloat.

### Running the lab

```bash
# Raylib backend (recommended --- better visual + built-in FPS display)
./build-dev.sh --backend=raylib --particle-lab -r

# X11 backend
./build-dev.sh --backend=x11 --particle-lab -r
```

Once running, press `0` to switch to the Particle Laboratory scene. The scene spawns 1000 particles with evenly distributed archetypes and starts the simulation immediately.

### What to observe

1. **First 5 seconds:** Watch the initial distribution. Bouncers (red) bounce off walls. Wanderers (green) meander. Seekers (yellow) and predictive seekers (cyan) converge on the target. Orbiters (magenta) circle the center. Drifters (blue) float on noise.

2. **After 10 seconds:** Collisions start changing colors. The swarm becomes multicolored. Watch a specific particle --- if it changes from red to green, it went from bouncing to wandering. The mutation is visible.

3. **Watch the HUD:** Entity count stable at 1000. FPS holding at 60. Update time under 5ms. Archetype counts slowly shifting.

4. **Press number keys to switch scenes.** The particle lab is Scene 0. Pressing 1-9 switches to the original 9 scenes from lessons 11-12. Pressing 0 returns to the particle lab. The pool is re-initialized on each scene switch.

### The standalone lesson binary

For students who want to study the particle lab without the full game demo:

```bash
./build-dev.sh --lesson=14 --particle-lab -r
```

This compiles `lesson_14.c` as a standalone binary that only runs the particle lab. It includes a minimal Raylib integration (no scene selector, no other scenes) and the complete parallel array / archetype / collision / HUD system.

---

## Connection to your work

This lesson connects backward and forward:

| Concept | Where you learned it | How the Particle Lab extends it |
|---|---|---|
| Flat array pool | Lesson 02 (fat struct) | 1000 entities in the same flat array --- proves it scales |
| Integer indices | Lesson 03 (thing_idx) | Parallel arrays indexed by thing_idx --- extends entities without struct changes |
| Nil sentinel | Lesson 06 | Archetype 0 (Bounce) is the safe zero-value default --- same principle |
| Free list | Lesson 08 | If you add particle removal, the free list handles churn at 1000 entities |
| Generational IDs | Lesson 09 | References to mutated particles remain valid --- the slot never changes |
| Pool iterator | Lesson 10 | Raw `for` loop in the lab is what the iterator does internally |
| Kind enum dispatch | Lesson 02 (thing_kind) | Archetype dispatch is the same pattern at a finer granularity |
| Collision removal | Lesson 12 | Collision mutation is the upgrade: change behavior instead of destroying |

The pattern you have learned --- flat arrays, integer indices, data-driven dispatch, mutation over allocation --- is how professional entity systems work at scale. Anton's "things pool" and Casey's Handmade Hero entity system both use this approach. The Particle Lab proves it is not just theory. It runs. It scales. You measured it.

> **Handmade Hero connection:** Casey's entity system also uses flat arrays with behavior dispatched via type tags. When an entity changes type in HH, Casey reassigns the type enum and reinitializes the relevant fields. No new object. No allocation. Same pattern as archetype mutation here.

## What's next

The Particle Lab is the end of the "Large Arrays of Things" course. You now have:

1. A **reusable pool library** (`things.h`/`things.c`) with nil sentinel, free list, generational IDs, and iterator.
2. A **game demo** (9 scenes) that exercises every pool feature visually.
3. A **stress-test laboratory** (1000 particles) that proves the system scales.
4. A **mental model** for data-driven entity systems: flat arrays, integer indices, behavior as data, mutation over inheritance.

Where this goes next:

- **Game Programming Patterns**: The Object Pool pattern (Chapter 19) is exactly what you built. Data Locality (Chapter 17) is why it performs well. Component (Chapter 14) is the next evolution --- splitting the fat struct into focused components.
- **Game Engine Architecture**: Entity systems (Chapters 15-16) formalize handles, component stores, and system iteration. You have already built the foundation.
- **Handmade Hero**: When you resume at day 33+, Casey's entity system will feel familiar. The vocabulary --- handles, generation, slot reuse, type dispatch --- is the same vocabulary you used in this course.
- **Your DE100 Engine**: The things pool is the entity subsystem. The parallel array pattern is how you will add physics data, render data, and AI data without monolithic structs.

You started this course with a question: "How do AAA games manage thousands of entities without modern C++ features?" Now you have the answer. You built it. You measured it. It works.
