# Lesson 07 — Game State Structure

## What you'll learn
- `SpaceObject`: the single struct for ship, asteroids, and bullets
- `GameState`: the entire game in one struct (no globals)
- Fixed-size pools: why we pre-allocate instead of malloc
- `GamePhase` and game-over logic overview

---

## SpaceObject — one struct for all entities

```c
// game/game.h
typedef struct {
    float x, y;      // world-space position (game units, y-up)
    float dx, dy;    // velocity (game units/second)
    float angle;     // heading in radians (0 = up, + = clockwise)
    float size;      // collision radius (game units); also draw scale
    int   active;    // 1 = in-use, 0 = free slot
} SpaceObject;
```

Using one struct for ship, asteroids, and bullets:
- Keeps pool code uniform — no type-tagging complexity.
- `size` doubles as both collision radius and wireframe scale.
- `active` flag avoids shifting elements: mark dead, clean up later.

**Coordinate convention (LESSON 04 applied):**
- `x, y` are in game units, origin `COORD_ORIGIN_LEFT_BOTTOM` (y-up)
- `(0, 0)` = bottom-left, `(GAME_UNITS_W, GAME_UNITS_H)` = top-right
- `dx, dy` are game-units per second (not pixels per second)

---

## Fixed-size pools

```c
#define MAX_ASTEROIDS  32
#define MAX_BULLETS     8
```

```c
typedef struct {
    SpaceObject player;
    SpaceObject asteroids[MAX_ASTEROIDS];
    int         asteroid_count;
    SpaceObject bullets[MAX_BULLETS];
    int         bullet_count;

    int        score;
    int        best_score;     // survives asteroids_init() calls

    GAME_PHASE phase;
    float      dead_timer;
    float      fire_timer;

    Vec2 ship_model[SHIP_VERTS];        // local-space ship polygon
    Vec2 asteroid_model[ASTEROID_VERTS]; // local-space asteroid polygon

    GameAudioState audio;
} GameState;
```

**Why fixed pools (not malloc)?**
- Deterministic memory layout — no fragmentation, no allocation failures.
- All state in one struct → trivial to debug, trivially reset on restart.
- No malloc/free bugs.

JS equivalent: "preallocated object pool" pattern.

---

## No globals in game code

```c
// platforms/raylib/main.c
int main(void) {
    GameState state;
    memset(&state, 0, sizeof(state));
    state.audio.samples_per_second = 44100;
    game_audio_init(&state);
    asteroids_init(&state);
    // ...
}
```

`GameState` lives on the **stack** of `main()`.  It's roughly 3–4 KB —
well within typical stack limits (~1 MB).  The game never uses global
variables for game logic (only the platform uses globals for window/audio handles).

---

## GamePhase

```c
typedef enum {
    PHASE_PLAYING = 0,  // normal gameplay
    PHASE_DEAD,         // ship destroyed, waiting for restart
} GAME_PHASE;
```

`PHASE_PLAYING = 0` is the ZII (zero-is-initialisation) default —
`memset(&state, 0, ...)` sets the correct initial phase automatically.

---

## Gameplay constants (game units)

```c
// Asteroid collision radii
#define ASTEROID_LARGE_SIZE   0.625f    // 20px / 32
#define ASTEROID_MEDIUM_SIZE  0.3125f   // 10px / 32
#define ASTEROID_SMALL_SIZE   0.15625f  //  5px / 32

// Physics
#define BULLET_SPEED          6.25f     // 200 px/s ÷ 32
#define SHIP_TURN_SPEED       3.5f      // radians/s (unchanged)
#define SHIP_THRUST_ACCEL     3.125f    // 100 px/s² ÷ 32
#define SHIP_MAX_SPEED        6.875f    // 220 px/s ÷ 32
#define SHIP_FRICTION         0.97f     // multiplier per frame

// Render
#define SHIP_RENDER_SCALE     0.109375f // 3.5 px-scale ÷ 32

// Collision
#define SHIP_COLLISION_RADIUS 0.15625f  // 5px ÷ 32

// Timing
#define BULLET_LIFETIME       1.5f      // seconds
#define DEATH_DELAY           2.5f      // seconds before restart prompt
#define FIRE_COOLDOWN         0.15f     // seconds between shots
```

---

## asteroids_init()

Called once at startup and again on restart.  It:
1. Saves `best_score` and `audio` config (they must survive the reset).
2. `memset`s the entire `GameState` to zero.
3. Restores saved fields.
4. Builds the ship wireframe model (y-up vertices).
5. Builds a random jagged asteroid polygon.
6. Spawns the player at screen centre.
7. Spawns 4 large asteroids near the corners.

```c
void asteroids_init(GameState *state) {
    int saved_best  = state->best_score;
    GameAudioState saved_audio = state->audio;

    memset(state, 0, sizeof(*state));

    state->best_score = saved_best;
    state->audio      = saved_audio;

    // Ship model in local space (y-up: nose points up = +y)
    state->ship_model[0] = (Vec2){  0.0f, +5.0f };
    state->ship_model[1] = (Vec2){ -3.0f, -3.5f };
    state->ship_model[2] = (Vec2){  3.0f, -3.5f };

    // ... asteroid model randomisation ...
    // ... spawn player at (GAME_UNITS_W*0.5, GAME_UNITS_H*0.5) ...
    // ... spawn 4 corner asteroids ...
}
```

---

## Key takeaways

1. `SpaceObject` is one struct for ship, asteroids, bullets — uniform pools.
2. `size` doubles as collision radius and draw scale.
3. `GameState` on the stack: ~3–4 KB, no globals in game code.
4. Fixed pools: deterministic, no malloc/free, easy to debug.
5. `PHASE_PLAYING = 0` is the ZII default — `memset` gets it right.
6. All physics constants are ÷32 from their old pixel values.
