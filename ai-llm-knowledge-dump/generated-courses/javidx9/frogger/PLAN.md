# Frogger Course — PLAN.md

## What we're building

A faithful C port of OneLoneCoder's (javidx9) Frogger game, running on both X11/OpenGL and Raylib backends. The player controls a frog that must cross a multi-lane road and river to reach five home cells at the top of the screen, while avoiding cars, buses, water, and screen edges. Lane objects scroll left or right at per-lane speeds; the frog rides logs in the river section and dies instantly on any unsafe cell. Each successful run through all five homes triggers a win state. The course teaches a JS developer modern C game architecture using the same patterns seen in the Tetris, Snake, and Asteroids courses — CPU backbuffer pipeline, Data-Oriented Design, a thin platform abstraction layer, and a procedural state machine — while introducing three new techniques unique to Frogger: pixel-accurate floor-mod scrolling (`lane_scroll`), a flat danger-buffer collision grid, and UV-cropped sprite rendering (`draw_sprite_partial`).

---

## Lesson sequence

| #  | Title                              | What gets built                                                     | What the student runs / sees                                           |
|----|------------------------------------|---------------------------------------------------------------------|------------------------------------------------------------------------|
| 01 | Window + Backbuffer                | Resizable window; `Backbuffer` with `pitch`; unified `build-dev.sh`; `PlatformGameProps`; letterbox; `platform_shutdown`; `DEBUG_TRAP`/`ASSERT` | Black window that resizes without stretching |
| 02 | Drawing Primitives                 | `draw_rect`, `draw_rect_blend`; `GAME_RGBA` (`0xAARRGGBB`); `CONSOLE_PALETTE`; named `COLOR_*` constants; pure-red cross-backend validation | Coloured rectangles; semi-transparent overlay |
| 03 | Input                              | `GameButtonState`; `UPDATE_BUTTON`; `inputs[2]` double-buffer; `GameInput` union; `prepare_input_frame`; X11 auto-repeat suppression | Arrow keys move a coloured rectangle one cell per press |
| 04 | Game Structure + `GAME_PHASE`      | `GameState`; `game_init` / `game_update` / `game_render` split; `GAME_PHASE` enum (`PHASE_PLAYING`, `PHASE_DEAD`, `PHASE_WIN`) | Frog at start position; phase shown as debug text |
| 05 | Lane Data (DOD)                    | `lane_speeds[NUM_LANES]`; `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]`; `tile_is_safe`; `tile_to_sprite`; DOD layout rationale | All 10 lanes drawn as coloured rectangles (no scrolling yet) |
| 06 | Scrolling Lanes (`lane_scroll`)    | `lane_scroll()` with floor-mod pixel arithmetic; bidirectional scroll; sub-tile-smooth rendering | Lanes scroll smoothly left and right; no 8-pixel jumps |
| 07 | Danger Buffer + Frog Collision     | `danger[SCREEN_CELLS_W × SCREEN_CELLS_H]` flat 2D bitmask; per-frame rebuild; frog position check; `PHASE_DEAD` flash and respawn | Frog dies on unsafe tiles; death flash; respawn at start |
| 08 | Sprites (`SpriteBank` + `draw_sprite_partial`) | `.spr` binary loader; `SpriteBank` fixed pool; `draw_sprite_partial` UV-crop; `CONSOLE_PALETTE` → `GAME_RGBA`; frog sprite | All lane tiles and frog rendered as pixel-art sprites |
| 09 | Homes + Win Condition              | Five home cells; `homes_reached` counter; home fill on arrival; `PHASE_WIN` when all five filled; win overlay | Frog fills homes; win screen appears after five successful runs |
| 10 | Font + UI                          | `FONT_8X8[128][8]` BIT7-left; `draw_char` / `draw_text`; `snprintf` HUD: score, timer, lives counter | `SCORE: 000`, time, `LIVES: 3` displayed each frame |
| 11 | Audio                              | `audio.c` + `utils/audio.h`; hop / splash / home-reached / lane-change SFX; spatial pan by `frog_x`; ALSA + Raylib buffer patterns | Sound effects fire on each frog event; panning matches screen position |
| 12 | Polish + `utils/` Refactor         | Refactor draw code → `utils/draw-shapes.c`; text → `utils/draw-text.c`; math → `utils/math.h`; audio helpers → `utils/audio.h`; respawn flicker; game-over overlay; restart on any key | Complete playable game; code split into utils/ modules |

---

## Final file structure

```
ai-llm-knowledge-dump/generated-courses/javidx9/frogger/
├── PLAN.md
├── README.md
├── PLAN-TRACKER.md
├── COURSE-BUILDER-IMPROVEMENTS.md      ← written last (Phase 4)
└── course/
    ├── build-dev.sh                    ← unified --backend=x11|raylib -r -d
    ├── assets/
    │   ├── frog.spr
    │   ├── water.spr
    │   ├── pavement.spr
    │   ├── wall.spr
    │   ├── home.spr
    │   ├── log.spr
    │   ├── car1.spr
    │   ├── car2.spr
    │   └── bus.spr
    ├── src/
    │   ├── utils/
    │   │   ├── backbuffer.h            ← Backbuffer type, GAME_RGBA, pitch
    │   │   ├── draw-shapes.c           ← draw_rect, draw_rect_blend, draw_sprite_partial
    │   │   ├── draw-shapes.h
    │   │   ├── draw-text.c             ← draw_char, draw_text using FONT_8X8
    │   │   ├── draw-text.h
    │   │   ├── math.h                  ← MIN, MAX, CLAMP macros
    │   │   └── audio.h                 ← SoundInstance, AudioChannel, fill_audio_buffer
    │   ├── game.h                      ← Types, constants, GameState, public API
    │   ├── game.c                      ← game_init, game_update, game_render
    │   ├── audio.c                     ← fill_audio_buffer, sound mixing, SFX helpers
    │   ├── platform.h                  ← 4-function platform contract
    │   ├── main_x11.c                  ← X11/OpenGL backend
    │   └── main_raylib.c               ← Raylib backend
    └── lessons/
        ├── 01-window-and-backbuffer.md
        ├── 02-drawing-primitives.md
        ├── 03-input.md
        ├── 04-game-structure-and-phase.md
        ├── 05-lane-data-dod.md
        ├── 06-scrolling-lanes.md
        ├── 07-danger-buffer-and-collision.md
        ├── 08-sprites.md
        ├── 09-homes-and-win-condition.md
        ├── 10-font-and-ui.md
        ├── 11-audio.md
        └── 12-polish-and-utils-refactor.md
```

---

## JS → C concept mapping per lesson

| Lesson | New C concept(s)                                        | Nearest JS equivalent                                                                        |
|--------|---------------------------------------------------------|----------------------------------------------------------------------------------------------|
| 01     | `uint32_t *pixels` CPU pixel buffer; `pitch`; `#ifdef DEBUG` macros; unified build script | `canvas.getContext('2d')` + `ImageData.data`; conditional code in `webpack.mode`             |
| 02     | `GAME_RGBA` bit-pack; static `const uint8_t` palette; `uint8_t` alpha blend math | CSS `rgba()`; `CanvasRenderingContext2D.globalAlpha`                                         |
| 03     | `GameButtonState` half-transition-count; `UPDATE_BUTTON` macro; `inputs[2]` double-buffer; C unions | `addEventListener('keydown')` / `keyup`; storing `pressed` vs `justPressed` flags            |
| 04     | `typedef enum`; `switch` dispatch; `memset(&state, 0)`; function pointer–free state machine | JS `class` with a `phase` string field; `switch(this.phase)` in `update()`                  |
| 05     | Parallel arrays (DOD); `static const` 2D char array; `static const float` array | `{ids: [], speeds: []}` vs `[{id, speed}]` struct-of-arrays vs array-of-structs              |
| 06     | `static inline` floor-mod; why C `(int)` truncates toward zero; sub-pixel scroll math | `ctx.translate()` → manual sub-pixel offset; `%` operator negative-modulo gotcha in JS      |
| 07     | Flat 2D array as collision grid; `memset` to clear; danger–render consistency invariant | `new Uint8Array(W * H)` as collision grid; write in `update`, read in same `update`          |
| 08     | Binary file `fread`; `int32_t` wire format; `int16_t` color/glyph arrays; fixed-pool allocation | `fetch(url).arrayBuffer()` then `DataView`; `ArrayBuffer` as raw bytes; pool allocator       |
| 09     | `%` for wrap-around pattern lookup; multiple win conditions checked in same `switch` arm | Array `.indexOf()` for win detection; wrapping with `% length`                               |
| 10     | `FONT_8X8[128][8]` bit-field; BIT7-left mask `0x80 >> col`; `snprintf` int-to-string | Canvas `fillText()`; string formatting with template literals                                |
| 11     | PCM audio synthesis; `float phase` accumulation; stereo pan L/R gain; ALSA vs Raylib buffer model | `AudioContext` + `ScriptProcessorNode`; stereo panning via `StereoPannerNode`                |
| 12     | Multi-file C project; `#include` guards; header/source split; `extern` declarations | ES module `import`/`export`; separating render helpers into `utils/canvas.js`                |

---

## Frogger-specific architecture notes

### DOD lane data

The `lane_speeds[NUM_LANES]` (float array, 40 bytes, fits one cache line) and `lane_patterns[NUM_LANES][LANE_PATTERN_LEN]` (char 2D array) are kept in **separate arrays**, not a `Lane` struct. This matches Casey Muratori's Data-Oriented Design principle:

- `game_update` reads **only** `lane_speeds` when computing river drift and building the `danger[]` buffer — the pattern chars are not needed.
- `game_render` reads **both** arrays — but they are touched in distinct inner loops.

Keeping the data separate avoids cache pollution: the speeds and patterns have different access patterns and should not compete for cache lines.

### Danger buffer

`state.danger[SCREEN_CELLS_W * SCREEN_CELLS_H]` (uint8, 128×80 = 10 240 bytes) is a **flat 2D bitmask** rebuilt every frame in `game_update`:

1. `memset(state->danger, 1, sizeof(state->danger))` — mark everything unsafe.
2. For each lane: call `lane_scroll()` to get `tile_start` and `px_offset`. Convert `px_offset` to `cell_offset = px_offset / CELL_PX`. For each tile in view, look up `tile_is_safe(c)` and write `0` (safe) into all cells covered by that tile.
3. Frog center-cell lookup: `cx = (frog_px_x + TILE_PX/2) / CELL_PX; cy = …`; if `danger[cy * W + cx]` → death.

Critical invariant: **the same `lane_scroll()` call with the same `state->time` is used for both the danger write (tick) and the sprite draw (render)**. If they diverge, the collision grid silently mismatches what is on screen — a ghost-collision bug that is nearly impossible to debug. Lesson 07 demonstrates this bug deliberately before introducing the shared `lane_scroll` call.

### SpriteBank fixed pool

`SpriteBank` is a flat struct embedded directly in `GameState` — no heap allocation after `game_init`:

```c
typedef struct {
    int16_t colors[SPR_POOL_CELLS];   /* all sprites' color indices, packed */
    int16_t glyphs[SPR_POOL_CELLS];   /* 0x2588 = solid, 0x0020 = transparent */
    int     widths [NUM_SPRITES];
    int     heights[NUM_SPRITES];
    int     offsets[NUM_SPRITES];     /* starting index into colors[]/glyphs[] */
} SpriteBank;
```

`sprite_load()` reads `.spr` binary files during init and appends directly into the pool using pre-computed `offsets[]`. After init, `game_render` indexes into the pool with `bank->offsets[spr_id]` and renders via `draw_sprite_partial`.

### `lane_scroll` floor-mod

The reference fixed a subtle C truncation bug. The naive pattern `(int)(time * speed) % PATTERN_LEN` fails for negative `speed` because `(int)` truncates toward zero, not toward negative infinity. This causes left-moving lanes to jump one extra tile each time the fractional part crosses zero.

The fix works entirely in pixels and uses a positive-modulo clamp:

```c
int sc = (int)(time * speed * (float)TILE_PX) % PATTERN_PX;
if (sc < 0) sc += PATTERN_PX;          /* always [0, PATTERN_PX) */
*out_tile_start = sc / TILE_PX;
*out_px_offset  = sc % TILE_PX;        /* 0..63 — sub-tile smooth */
```

Lesson 06 steps through a concrete numerical example (speed = −3, time = 1.33 s, TILE_PX = 64) showing both the broken and fixed values.

### `draw_sprite_partial` UV crop

`draw_sprite_partial(bb, bank, spr_id, src_x, src_y, src_w, src_h, dest_px_x, dest_px_y)` renders a rectangular slice of a sprite sheet into the backbuffer. The key use case is tiling: a log sprite is 24 cells wide (start + 8 middle + end), and each tile in a lane renders one 8-cell segment of it using `src_x = 0 | 8 | 16`. The source clip is applied **before** writing to the backbuffer — only the selected region's pixels are written. Transparent cells (`glyph == 0x0020`) are skipped, preserving whatever the backbuffer had underneath (background sprite or scrolled lane).

### GAME_PHASE state machine

The reference used a single `int dead` flag. The course replaces this with a `typedef enum GAME_PHASE { PHASE_PLAYING, PHASE_DEAD, PHASE_WIN }` and dispatches through a `switch` in both `game_update` and `game_render`. This prevents category errors (e.g., checking `if (dead && win)` which is logically impossible) and makes adding new phases (e.g., `PHASE_START_SCREEN`) trivial.

### Audio spatial panning

Frogger is the first course to use stereo panning. The hop and death sounds are panned based on the frog's horizontal position:

```c
float pan = (state->frog_x / (float)(SCREEN_CELLS_W - TILE_CELLS)) * 2.0f - 1.0f;
/* -1.0 = full left, 0.0 = centre, +1.0 = full right */
float gain_l = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
float gain_r = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
```

This gives a natural positional cue: a frog on the left of the screen sounds left, and on the right it sounds right.

---

## Known deviations from reference source

| # | Reference                           | Course                                          | Documented in |
|---|-------------------------------------|-------------------------------------------------|---------------|
| 1 | `build_x11.sh` + `build_raylib.sh`  | Unified `build-dev.sh --backend=x11\|raylib`     | Lesson 01     |
| 2 | No `pitch` in backbuffer            | `pitch = width * 4`                             | Lesson 01     |
| 3 | No `DEBUG_TRAP`/`ASSERT`            | Added under `#ifdef DEBUG`                      | Lesson 01     |
| 4 | `platform.h` only 3 functions       | Added `platform_shutdown`                       | Lesson 01     |
| 5 | `FROGGER_RGBA` = `0xAABBGGRR`        | `GAME_RGBA` = `0xAARRGGBB` (Tetris convention)   | Lesson 02     |
| 6 | No named `COLOR_*` constants        | `COLOR_BLACK`, `COLOR_WHITE`, …                 | Lesson 02     |
| 7 | Single `GameInput` buffer           | `inputs[2]` + `prepare_input_frame(old, curr)`  | Lesson 03     |
| 8 | No `GameInput` union                | `union { buttons[]; struct { … }; }`            | Lesson 03     |
| 9 | Raw `platform_init(title, w, h)`    | `PlatformGameProps` wrapper                     | Lesson 01     |
| 10 | Fixed non-resizable window          | Letterbox from Lesson 01                        | Lesson 01     |
| 11 | Bare `fprintf` debug calls          | Guarded by `#ifdef DEBUG`                       | Lesson 01     |
| 12 | All draw code inline in `frogger.c` | Start inline; refactor to `utils/` in Lesson 12 | Lessons 02, 12 |
| 13 | 5×7 column-major `font_glyphs[96][5]` | `FONT_8X8[128][8]` BIT7-left                  | Lesson 10     |
| 14 | No audio                            | `audio.c` + `utils/audio.h` with spatial SFX   | Lesson 11     |
| 15 | `frogger_init/tick/render` naming   | `game_init/update/render`                       | Lesson 04     |
