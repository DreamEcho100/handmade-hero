# Lesson 13 — Polish + `utils/` Refactor

## What you'll build
Reorganise all helper code into the `utils/` folder, review the final file
architecture, and discuss polish items.  This is the "refactor" lesson where
you see how the lessons-01-12 monolithic `game.c` becomes the final modular
layout.

---

## Core Concepts

### 1. Why Refactor into `utils/`?

Through lessons 1–12, all drawing helpers lived directly in `game.c`:
- `draw_pixel`, `draw_pixel_w`, `draw_line`, `draw_rect`, `draw_rect_blend`
- `draw_char`, `draw_text`, `FONT_8X8`

This worked fine for learning — one file, no include management.  But for
a real project (or a second game), you'd want:
- Drawing code reusable without copying it
- Game logic readable without scrolling past hundreds of lines of font data
- Easier to test drawing in isolation

The `utils/` split is the minimal refactor that achieves this.

### 2. What Moves to `utils/` vs Stays in `game.c`

| Item | Where it lives | Why |
|------|----------------|-----|
| `draw_pixel`, `draw_pixel_w` | `utils/draw-shapes.c` | Platform-independent primitive |
| `draw_line`, `draw_rect`, `draw_rect_blend` | `utils/draw-shapes.c` | Same |
| `draw_char`, `draw_text`, `FONT_8X8` | `utils/draw-text.c` | Same |
| `Vec2`, `MIN/MAX/CLAMP` | `utils/math.h` | Generic types, no game state |
| `AsteroidsBackbuffer`, `GAME_RGBA` | `utils/backbuffer.h` | Type definition |
| `GameAudioState`, `SoundInstance` | `utils/audio.h` | Audio types |
| **`draw_wireframe`** | **`game.c`** | Uses `ship_model` and `asteroid_model` from `GameState` — game-specific |
| `compact_pool`, `add_asteroid` | `game.c` | Game-specific logic |
| `asteroids_init/update/render` | `game.c` | Game logic |

### 3. Include Order

Each file only includes what it directly uses:

```
utils/backbuffer.h   (depends on: <stdint.h>)
utils/math.h         (depends on: nothing)
utils/audio.h        (depends on: <stdint.h>)
utils/draw-shapes.h  (depends on: backbuffer.h)
utils/draw-text.h    (depends on: backbuffer.h)
game.h               (depends on: backbuffer.h, math.h, audio.h)
game.c               (depends on: game.h, draw-shapes.h, draw-text.h)
audio.c              (depends on: game.h)
main_x11.c           (depends on: platform.h)
main_raylib.c        (depends on: platform.h)
```

### 4. Build Script — Explicit Source List

```bash
SOURCES="src/game.c src/audio.c src/utils/draw-shapes.c src/utils/draw-text.c"
```

We list `.c` files explicitly.  The header files are found automatically by
`#include`.  Listing only the translation units (`.c` files) that need to be
compiled avoids recompiling headers multiple times.

### 5. Final File Structure

```
course/
├── build-dev.sh
├── src/
│   ├── platform.h          ← 4-function contract + input helpers
│   ├── game.h              ← all types + public API
│   ├── game.c              ← game logic + draw_wireframe
│   ├── audio.c             ← sound mixer
│   ├── main_x11.c          ← X11 + GLX + ALSA backend
│   ├── main_raylib.c       ← Raylib backend
│   └── utils/
│       ├── backbuffer.h    ← AsteroidsBackbuffer, GAME_RGBA, colours
│       ├── math.h          ← Vec2, MIN, MAX, CLAMP
│       ├── audio.h         ← GameAudioState, SoundInstance, SOUND_ID
│       ├── draw-shapes.h   ← pixel/line/rect declarations
│       ├── draw-shapes.c   ← pixel/line/rect implementations
│       ├── draw-text.h     ← font declarations
│       └── draw-text.c     ← FONT_8X8 + draw_char/draw_text
└── lessons/
    └── 01-window-backbuffer.md … 13-refactor.md
```

### 6. Polish Items

**Wave progression:** Spawn more asteroids each wave:
```c
int wave_count = 4 + (wave_number - 1);   // wave 1→4, wave 2→5, etc.
for (int i = 0; i < wave_count; i++)
    add_asteroid(state, rand_edge_x(), rand_edge_y(), ...);
```

**Best score persistence across sessions:**
```c
// Save to a file on death:
FILE *f = fopen("highscore.dat", "wb");
if (f) { fwrite(&state->best_score, sizeof(int), 1, f); fclose(f); }

// Load on init:
FILE *f = fopen("highscore.dat", "rb");
if (f) { fread(&state->best_score, sizeof(int), 1, f); fclose(f); }
```

---

## Build and Verify

```bash
# Both backends, zero warnings
./build-dev.sh --backend=raylib 2>&1
./build-dev.sh --backend=x11 2>&1
```

Play through one full game session:
- [ ] Ship rotates and thrusts correctly
- [ ] Bullets fire with cooldown
- [ ] Asteroids split and are scored correctly
- [ ] Death shows overlay and waits for FIRE
- [ ] Restart preserves best score
- [ ] All SFX play on both backends
- [ ] Explosions pan correctly by screen position

---

## Summary

This lesson shows the principle that **correctness first, structure second**.
You built a working game incrementally, lesson by lesson, in a single file.
The final refactor into `utils/` is a mechanical transformation — moving code
to separate files without changing behaviour.

Refactoring becomes safe and fast when:
1. The code already works (lessons 1–12 proved it)
2. The file boundaries are clear (game logic vs. utilities)
3. You can verify each step with a quick build + run

---

## Complete Summary of C → JS Lessons Learned

| C concept | JS equivalent |
|-----------|---------------|
| `uint32_t` pixel | `Uint32Array` element |
| `pitch = width * 4` | `imageData.data.byteLength / height` |
| Bresenham line | Canvas `lineTo` |
| `draw_pixel_w` toroidal | `((x % w) + w) % w` |
| Fixed-size pool | `Array` with manual index |
| `compact_pool` swap-tail | `filter` (allocates) |
| Euler integration | Game loop `+=` |
| `sinf/cosf` rotation | `Math.sin/cos` |
| `fmodf` wrap | `((x % w) + w) % w` |
| `snprintf` format | Template literals |
| `memset` zero | `state = {}` |
| `ASSERT` + `DEBUG_TRAP` | `console.assert` |
| PCM square wave | `OscillatorNode` |
| Stereo interleaved | Web Audio API buffers |
