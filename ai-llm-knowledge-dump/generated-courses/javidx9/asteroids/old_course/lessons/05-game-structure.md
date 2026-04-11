# Lesson 05 — Game Structure

## What you'll build
Split the monolithic `main.c` into three layers: `game.h` (types), `game.c`
(logic), and a backend `main_*.c` (platform).  Introduce `GameState`,
`GAME_PHASE`, and the `asteroids_init` / `asteroids_update` / `asteroids_render`
split.  A placeholder triangle renders at the player's position.

---

## Core Concepts

### 1. The Three-Layer Architecture

```
platform (main_x11.c or main_raylib.c)
    ↓ calls
game contract (platform.h / game.h)
    ↓ calls
game logic (game.c)
    ↓ uses
utilities (audio.c, draw-shapes.c, draw-text.c)
```

The platform **never** includes game-logic headers directly.  The game
**never** includes X11.h or raylib.h.  This allows running the exact same
`game.c` on both backends without any `#ifdef`.

### 2. `GameState` — All Game Data in One Struct

```c
typedef struct {
    SpaceObject player;
    SpaceObject asteroids[MAX_ASTEROIDS];
    int         asteroid_count;
    SpaceObject bullets[MAX_BULLETS];
    int         bullet_count;
    int         score;
    int         best_score;
    GAME_PHASE  phase;
    float       dead_timer;
    float       fire_timer;
    Vec2        ship_model[SHIP_VERTS];
    Vec2        asteroid_model[ASTEROID_VERTS];
    GameAudioState audio;
} GameState;
```

**No globals in game.c.**  Everything lives in `GameState`, which main()
allocates on the stack.  This makes the game state portable: you could
serialize it to a file, or run two independent instances side-by-side.

### 3. `GAME_PHASE` — State Machine

```c
typedef enum {
    PHASE_PLAYING = 0,
    PHASE_DEAD
} GAME_PHASE;

// In asteroids_update():
if (state->phase == PHASE_DEAD) {
    // Show game over screen, wait for FIRE to restart
    return;
}
// Normal gameplay...
```

JS equivalent: `enum GamePhase { Playing, Dead }` in TypeScript.

### 4. `asteroids_init` — The memset Wipe Pattern

```c
void asteroids_init(GameState *state) {
    // Save fields that must survive the wipe
    int   sps = state->audio.samples_per_second;
    float mv  = state->audio.master_volume;
    int   bs  = state->best_score;

    memset(state, 0, sizeof(*state));   // zero everything

    // Restore the preserved fields
    state->audio.samples_per_second = sps;
    state->audio.master_volume = (mv > 0.0f) ? mv : 0.8f;
    state->best_score = bs;

    // ... build models, reset_game() ...
}
```

`memset(ptr, 0, size)` zeros an entire struct instantly.  It's the C
equivalent of `state = {}` in JS, except you must be careful not to lose
fields that were set before the wipe (here: audio config + best score).

**Lesson from the Snake course:** forgetting the save/restore caused silent
audio on every restart.

### 5. Three Functions the Platform Calls

```c
// Declaration in game.h / platform.h:
void asteroids_init(GameState *state);
void asteroids_update(GameState *state, const GameInput *input, float dt);
void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb);
```

And the platform's loop:
```c
asteroids_init(&state);
while (running) {
    asteroids_update(&state, &inputs[1], dt);
    asteroids_render(&state, &bb);
    display_backbuffer();
}
```

---

## Implement It (Lesson 05 stage)

In `game.c` at this stage, both functions can be stubs:

```c
void asteroids_init(GameState *state) {
    memset(state, 0, sizeof(*state));
    state->player.x = SCREEN_W / 2.0f;
    state->player.y = SCREEN_H / 2.0f;
    state->player.active = 1;
    state->phase = PHASE_PLAYING;
}

void asteroids_update(GameState *state, const GameInput *input, float dt) {
    if (input->quit) { /* signal platform */ }
    (void)dt;  // not used yet
}

void asteroids_render(const GameState *state, AsteroidsBackbuffer *bb) {
    draw_rect(bb, 0, 0, SCREEN_W, SCREEN_H, COLOR_BLACK);  // clear
    // Draw placeholder: 3 pixels forming a triangle at player position
    int px = (int)state->player.x;
    int py = (int)state->player.y;
    draw_pixel(bb, px, py-5, COLOR_WHITE);    // nose
    draw_pixel(bb, px-3, py+3, COLOR_WHITE);  // left fin
    draw_pixel(bb, px+3, py+3, COLOR_WHITE);  // right fin
}
```

---

## Verify

```bash
./build-dev.sh -r
```

Three white dots appear at the screen centre.  The program responds to Q/Esc.
The three-file structure compiles cleanly.

---

## Summary

| Concept | C pattern | JS equivalent |
|---------|-----------|---------------|
| Game state | `GameState *state` parameter | Class instance / store |
| Zero a struct | `memset(state, 0, sizeof(*state))` | `state = {}` |
| Enum | `typedef enum { A, B } MyEnum;` | `enum MyEnum { A, B }` (TypeScript) |
| No globals | Pass `GameState *` everywhere | Function parameters |
