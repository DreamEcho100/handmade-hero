# Asteroids Course — PLAN.md

Reference source analysed: `_ignore/asteroids/course/` (C port of `OneLoneCoder_Asteroids.cpp`)  
Course output: `asteroids/`

---

## What the program does

Classic Asteroids arcade game with **wireframe vector graphics** rendered entirely into a CPU pixel
backbuffer. The player pilots a triangular ship around an 800 × 600 toroidal arena (objects wrap
edge-to-edge). Left/Right arrows rotate the ship; Up thrusts by applying acceleration to velocity;
Space fires a bullet in the facing direction. Two large asteroids drift at startup; shooting one
splits it into two smaller ones (large→medium→small→gone). Clearing all asteroids earns a wave
bonus and spawns a new set 90° left/right of the player. If the ship touches any asteroid a 1.5-
second death flash plays and the game resets. Score accumulates; smaller asteroids score more per
hit. The game runs indefinitely until the player presses Q or Esc.

**What makes this game new vs tetris / snake:**

| Technique | Where first seen | Why it's new |
|-----------|-----------------|--------------|
| Bresenham's line algorithm | Lesson 03 | Integer pixel-exact line drawing — no sprites |
| 2D rotation matrix | Lesson 06 | `x'=x·cos θ−y·sin θ` applied per vertex each frame |
| Euler integration (physics) | Lesson 07 | `v += a*dt`, `p += v*dt` — Newtonian motion |
| Toroidal wrap via `draw_pixel_w` | Lesson 07 | Lines cross edges seamlessly at the pixel write level |
| Entity pools + `compact_pool` | Lesson 08 | O(1) removal via swap-with-last; no heap in game loop |
| Circle collision (no sqrt) | Lesson 10 | `dx²+dy² < r²` avoids sqrt in the hot path |
| Asteroid splitting | Lesson 10 | One object becomes two at random angles |
| Spatial audio pan | Lesson 12 | `pan = (x / SCREEN_W) * 2 - 1` — position-aware SFX |

---

## Lesson sequence

| # | Title | What gets built | What the student runs/sees |
|---|-------|-----------------|---------------------------|
| 01 | Window + backbuffer | `AsteroidsBackbuffer` with `pitch`, unified `build-dev.sh`, `platform_shutdown`, `DEBUG_TRAP`/`ASSERT` | Black 800×600 window; clean exit on Q/Esc/× button |
| 02 | Drawing primitives | `draw_pixel`, `draw_rect`, `draw_rect_blend`; `GAME_RGBA` cross-backend validation | Coloured rectangles on screen; pure-red rect confirms correct colours on both X11 and Raylib |
| 03 | Line drawing (Bresenham) | `draw_line` Bresenham integer algorithm; `draw_pixel_w` toroidal wrapper | Diagonal line across window; triangle from 3 `draw_line` calls; line wraps at screen edge |
| 04 | Input | `GameButtonState`, `UPDATE_BUTTON`, `inputs[2]` double-buffer, `GameInput` union (4 buttons: left/right/up/fire), "just pressed" check | Triangle moves and rotates in response to arrow keys; Space prints "FIRE" to stderr |
| 05 | Game structure | `GameState`, `GAME_PHASE` enum, `asteroids_init`/`update`/`render` split; placeholder triangle at ship position | Skeleton compiles and runs; triangle renders; PHASE dispatch in place |
| 06 | Vec2 + rotation matrix | `Vec2`, `draw_wireframe`, `cos`/`sin` pre-computed once per object, `ship_model[3]`, `srand(time(NULL))` | Ship wireframe rotates left/right at screen centre |
| 07 | Ship physics + toroidal wrap | `dx`/`dy` velocity, `dx += sin(angle)*THRUST*dt`, `draw_pixel_w` `% w`/`% h` wrap, `fmodf`-free position wrap | Ship thrusts, drifts, wraps at every screen edge seamlessly |
| 08 | Asteroids — entity pool | `SpaceObject asteroids[MAX_ASTEROIDS]`, `compact_pool` swap-with-last, spawn 3 large asteroids, rotate+drift | Three spinning asteroids float and wrap; removing one in code demonstrates `compact_pool` |
| 09 | Bullets | `SpaceObject bullets[MAX_BULLETS]`, fire on Space "just pressed", bullet `timer` lifetime, `compact_pool` bullets, `draw_pixel_w` | Ship fires white pixel bullets that travel in the facing direction and auto-expire |
| 10 | Collision detection | `is_point_inside_circle` (squared distance, no sqrt), bullet-asteroid hit, `add_asteroid` split, `compact_pool` after both loops, player-asteroid death, wave clear + bonus | Bullets destroy and split asteroids; ship dies on contact; wave clears and respawns |
| 11 | Font + UI | `FONT_8X8[128][8]` BIT7-left, `draw_char`, `draw_text`, `snprintf`, score panel, `PHASE_DEAD` overlay | "SCORE: 000" top-left; "GAME OVER" overlay on death; `draw_text` renders any ASCII |
| 12 | Audio | `AudioOutputBuffer`, `SoundInstance`, thrust-loop SFX, fire SFX, explode SFX (3 sizes) panned by `asteroid.x`, ship-death SFX; ALSA `start_threshold`; Raylib per-frame model | Sounds play for all game events; explosions pan left/right by screen position |
| 13 | Polish + `utils/` refactor | Move helpers to `utils/draw-shapes.c/h`, `utils/draw-text.c/h`, `utils/math.h`, `utils/audio.h`; level wave scaling; high-score persistence; `COURSE-BUILDER-IMPROVEMENTS.md` | Complete shippable game; `utils/` architecture explained and demonstrated |

---

## Final file structure

```
asteroids/
├── PLAN.md
├── README.md
├── PLAN-TRACKER.md
├── COURSE-BUILDER-IMPROVEMENTS.md
└── course/
    ├── build-dev.sh
    └── src/
        ├── utils/
        │   ├── backbuffer.h        ← AsteroidsBackbuffer, GAME_RGBA, pitch, COLOR_*
        │   ├── draw-shapes.c       ← draw_pixel_w, draw_pixel, draw_rect, draw_rect_blend, draw_line
        │   ├── draw-shapes.h
        │   ├── draw-text.c         ← FONT_8X8[128][8], draw_char, draw_text
        │   ├── draw-text.h
        │   ├── math.h              ← MIN, MAX, CLAMP, ABS, Vec2, rotate_vec2 helper
        │   └── audio.h             ← AudioOutputBuffer, SOUND_ID, SoundInstance, GameAudioState
        ├── game.h                  ← SpaceObject, GameState, GameInput union, GAME_PHASE, public API
        ├── game.c                  ← asteroids_init, asteroids_update, asteroids_render,
        │                              prepare_input_frame, compact_pool, add_asteroid, draw_wireframe
        ├── audio.c                 ← game_audio_init, game_get_audio_samples, game_play_sound_at, SFX defs
        ├── platform.h              ← 4-function contract + platform_swap_input_buffers
        ├── main_x11.c              ← X11 + GLX + ALSA backend
        └── main_raylib.c           ← Raylib backend (per-frame audio model)
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives.md
        ├── 03-line-drawing-bresenham.md
        ├── 04-input.md
        ├── 05-game-structure.md
        ├── 06-vec2-and-rotation-matrix.md
        ├── 07-ship-physics-and-toroidal-wrap.md
        ├── 08-asteroids-entity-pool.md
        ├── 09-bullets.md
        ├── 10-collision-detection.md
        ├── 11-font-and-ui.md
        ├── 12-audio.md
        └── 13-polish-and-utils-refactor.md
```

---

## JS → C concept mapping

| C concept | JS equivalent | First lesson | Notes |
|-----------|--------------|-------------|-------|
| `uint32_t *pixels` flat array | `new Uint32Array(w * h)` | 01 | C needs explicit bit-width; JS numbers are always 64-bit float |
| `pitch = width * 4` (row stride) | `ImageData` is always contiguous | 01 | Teaches stride awareness; needed for aligned/sub-texture buffers |
| `#define GAME_RGBA(r,g,b,a)` | CSS `rgba(r,g,b,a)` packed into a number | 02 | Compile-time bit shift; zero runtime overhead |
| `draw_rect(bb, x, y, w, h, color)` | `ctx.fillRect(x, y, w, h)` | 02 | We write each pixel manually; no GPU draw call |
| `draw_line` Bresenham | `ctx.lineTo()` | 03 | Integer algorithm; no floating-point; wraps toroidally |
| `draw_pixel_w` modulo wrap | `((x % w) + w) % w` on canvas coord | 03/07 | Wrap happens at write time; all higher-level draws get it free |
| `GameButtonState` `half_transition_count` | `keydown` + `event.repeat === false` | 04 | Browser tracks this; in C we implement it ourselves in the struct |
| `union { buttons[N]; struct { left, right, up, fire } }` | JS object with array backing | 04 | C lets you alias the same memory two ways; no JS equivalent |
| `inputs[2]` double-buffer | `[a, b] = [b, a]` swap | 04 | X11 is event-based; without copying `ended_down`, held keys appear released |
| `typedef enum GAME_PHASE` | `Object.freeze({ PLAYING:0, DEAD:1 })` | 05 | Compiler warns on missing switch case; no magic integers |
| `Vec2` struct `{ float x, y; }` | `{ x: number, y: number }` | 06 | Fixed memory layout; no GC; 8 bytes exactly |
| `cosf`/`sinf` pre-computed once | `Math.cos`/`Math.sin` | 06 | Two trig calls per object per frame (not per vertex); expensive |
| `SpaceObject arr[MAX]; int count` | `let arr = []` | 08 | Fixed pool; no realloc; `count` is the "length" |
| `compact_pool` swap-with-last | `arr.splice(i, 1)` — O(n) | 08 | Swap with last entry: O(1); order not preserved (acceptable for unordered pools) |
| `dx*dx + dy*dy < r*r` | `Math.hypot(dx,dy) < r` | 10 | Avoids `sqrt`; squaring both sides gives identical result |
| `srand(time(NULL))` once | `Math.random()` auto-seeded | 06 | C `rand()` needs manual seeding; seed once — never inside the game loop |
| `FONT_8X8[128][8]` bitmap | `ctx.font = '8px monospace'` | 11 | No font engine; we render pixels from a 1-KB lookup table |
| `snprintf(buf, sizeof(buf), "%d", n)` | `String(n)` or template literal | 11 | `sizeof(buf)` prevents buffer overrun; `sprintf` is unsafe |
| `int16_t samples[]` interleaved stereo | `Float32Array` from `AudioContext` | 12 | We generate every sample manually; no Web Audio API |
| `pan = (x / SCREEN_W) * 2 - 1` | `StereoPannerNode.pan.value` | 12 | We compute left/right gain ourselves; no browser node |

---

## C++ original vs C course comparison

| C++ (olcConsoleGameEngine) | C course equivalent | Key difference |
|---------------------------|--------------------|-|
| `class OneLoneCoder_Asteroids : public olcConsoleGameEngine` | `game.c` + `platform.h` + `main_*.c` | No OOP; explicit platform layering |
| `vector<sSpaceObject> vecAsteroids` | `SpaceObject asteroids[MAX_ASTEROIDS]` + `int asteroid_count` | Fixed pool; no heap realloc |
| `vector<sSpaceObject> vecBullets` | `SpaceObject bullets[MAX_BULLETS]` + `int bullet_count` | Same pool pattern |
| `vector<pair<float,float>> vecModelShip` | `Vec2 ship_model[SHIP_VERTS]` static array | Known size at compile time |
| `vector<pair<float,float>> vecModelAsteroid` | `Vec2 asteroid_model[ASTEROID_VERTS]` | Same |
| `remove_if` + `erase` — O(n), invalidates iterators | `compact_pool` swap-with-last — O(1) | No iterator invalidation; no shifting |
| `OnUserCreate()` — OLC virtual | `asteroids_init(GameState *)` | Plain function; no vtable |
| `OnUserUpdate(float)` — OLC virtual | `asteroids_update(state, input, dt)` | delta_time passed explicitly |
| `DrawWireFrameModel(vector<pair>, x, y, r, s, col)` | `draw_wireframe(bb, model, n, x, y, angle, scale, color)` | Array + count; no vector |
| `Draw(x, y, wchar_t, short)` — console character | `draw_pixel_w(bb, x, y, uint32_t)` — real pixel | Actual pixel-level graphics |
| `DrawLine` — OLC built-in | `draw_line` — we implement Bresenham ourselves | Course teaches the algorithm |
| `DrawString` — OLC built-in | `draw_text` — we implement from FONT_8X8 | Course teaches bitmap font rendering |
| `WrapCoordinates(float,float,float&,float&)` | `draw_pixel_w` wrap + position wrap in update | Wrap at pixel write AND position update |
| `IsPointInsideCircle` — uses `sqrt` | `is_point_inside_circle` — uses `dx²+dy²<r²` | Course eliminates sqrt from hot path |
| `bDead` — plain bool | `GAME_PHASE phase` enum | Scales; compiler warns on missing switch case |
| `ScreenWidth()` / `ScreenHeight()` — OLC method | `SCREEN_W` / `SCREEN_H` — compile-time constants | No virtual dispatch for a constant |
| `m_keys[VK_...]` — OLC key array | `GameInput` union: `buttons[4]` + named fields | Platform-independent; no VK_ codes |
| No audio | `audio.c` + `utils/audio.h` — procedural SFX | Added as course enhancement |
| No font upgrade needed (OLC handled text) | `FONT_8X8[128][8]` ASCII-indexed, BIT7-left | We own the font renderer |
| Two build scripts (`build_x11.sh`, `build_raylib.sh`) | Single `build-dev.sh --backend=x11\|raylib` | Avoids duplicating 90% of the same build logic |

---

## Gaps vs reference source (`_ignore/asteroids/course/`) — course fixes

| # | Gap | Reference has | Course adds | Lesson |
|---|-----|--------------|-------------|--------|
| 1 | No `pitch` field | `y * width + x` (implicit stride) | `pitch = width * 4`; `py*(pitch/4)+px` everywhere | 01 |
| 2 | Single `GameInput` buffer | `prepare_input_frame(input)` — 1 arg; only resets `htc` | `inputs[2]` + `prepare_input_frame(old, current)` that copies `ended_down` | 04 |
| 3 | No `GameInput` union | Named fields only: `.left`, `.right`, `.up`, `.fire` | `union { buttons[BUTTON_COUNT]; struct { left,right,up,fire }; }` | 04 |
| 4 | No `platform_shutdown` | Contract has 3 functions | Add 4th function; both backends free GL/ALSA/Raylib resources | 01 |
| 5 | Two build scripts | `build_x11.sh` + `build_raylib.sh` | Unified `build-dev.sh --backend=x11\|raylib -r -d` | 01 |
| 6 | No `utils/` split | All code inline in `asteroids.c` | Lessons 01–12 inline; refactor to `utils/` in Lesson 13 | 13 |
| 7 | No audio | Silence | `audio.c` + `utils/audio.h`: thrust/fire/explode(3)/death SFX with spatial pan | 12 |
| 8 | 5×7 column-major font | `font_glyphs[96][5]` BIT0-left, `ch - 32` offset | `FONT_8X8[128][8]` BIT7-left, direct ASCII index | 11 |
| 9 | `ASTEROIDS_RGBA` naming | Same byte layout, different macro name | Rename to `GAME_RGBA` for cross-game consistency | 02 |
| 10 | No `DEBUG_TRAP`/`ASSERT` | No debug macros | `#ifdef DEBUG` + `__builtin_trap()` + `ASSERT(expr)` | 01 |
| 11 | `prepare_input_frame` single-arg | Only resets `half_transition_count` | Two-arg version copies `ended_down` from previous frame | 04 |
| 12 | `is_point_inside_circle` comment says avoids sqrt | Reference already avoids sqrt (correct!) | ✅ No fix needed — document it as a course positive | 10 |

---

## Important implementation notes

- **`srand(time(NULL))` in `asteroids_init`** — seed once at startup (not in `reset_game` — reset does not re-seed)
- **Bullet lifetime as `timer` field** — the reference uses `b->angle` as the lifetime counter; the course uses `b->timer` for clarity
- **`compact_pool` after both loops** — bullet-asteroid collision marks both inactive, then two `compact_pool` calls clean up; never remove during iteration
- **Wave clear**: spawn 2 large asteroids at `±90°` from player heading, at radius 150px from player centre — matches original
- **`DEAD_ANIM_TIME = 1.5f`** — death flash blinks at 10 Hz (`(int)(dead_timer / 0.05f) % 2`)
- **Asteroid model** generated once in `asteroids_init` with 20 vertices, noise `[0.8, 1.2]` on a unit circle; scaled by `size` (16/32/64 px) when drawn
- **Audio `memset` trap** — `asteroids_init` uses `memset` to reset state; audio config (`samples_per_second`, volumes) MUST be saved and restored, not zeroed (lesson learned from Snake)
- **No music sequencer in this course** — audio is SFX-only (6 sound types); music can be added as a student exercise

---

_Phase 0 analysis complete. Phase 1 planning files created. Awaiting confirmation to begin Phase 2 (source files)._
