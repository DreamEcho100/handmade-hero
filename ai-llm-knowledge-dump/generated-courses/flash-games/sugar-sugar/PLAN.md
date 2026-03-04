# Sugar, Sugar — Course Plan

## What the final game does

Sugar, Sugar is a falling-sand puzzle game originally released on Newgrounds (2009)
by Bart Bonte.  The player draws lines with the mouse to guide falling sugar grains
into cups.  Levels introduce increasingly complex mechanics: color filters that tint
grains, multi-cup split routing, named obstacles, paired teleporters that instantly
warp grains from one portal to another, and a gravity-flip button that reverses the
direction of fall.  A sandbox "Freeplay" mode is unlocked after completing all 30
levels.  The game is mouse-driven; keyboard shortcuts (R = reset, G = flip gravity,
Escape = menu) supplement but do not replace the mouse.

---

## Final file structure

```
sugar-sugar/
├── PLAN.md
├── README.md
├── PLAN-TRACKER.md
├── COURSE-BUILDER-IMPROVEMENTS.md
└── course/
    ├── build-dev.sh
    ├── src/
    │   ├── font.h
    │   ├── sounds.h
    │   ├── platform.h
    │   ├── game.h
    │   ├── game.c
    │   ├── levels.c
    │   ├── main_x11.c
    │   └── main_raylib.c
    └── lessons/
        ├── lesson-01-window-and-canvas.md
        ├── lesson-02-pixel-rendering.md
        ├── lesson-03-mouse-input-and-drawing.md
        ├── lesson-04-one-falling-grain.md
        ├── lesson-05-grain-pool.md
        ├── lesson-06-grain-collision.md
        ├── lesson-07-slide-settle-bake.md
        ├── lesson-08-state-machine.md
        ├── lesson-09-level-system.md
        ├── lesson-10-win-detection.md
        ├── lesson-11-color-filters.md
        ├── lesson-12-obstacles-and-level-variety.md
        ├── lesson-13-font-system-and-hud.md
        ├── lesson-14-teleporters-and-gravity-flip.md
        └── lesson-15-audio-and-final-polish.md
```

---

## Lesson sequence

| # | Title | What gets built | What the student runs/sees |
|---|-------|-----------------|----------------------------|
| 01 | Window and canvas | `build-dev.sh`, both backends, blank canvas | Sky-blue 640×480 window on X11 and Raylib |
| 02 | Pixel rendering | `draw_rect`, `draw_circle`, `GAME_RGB` macros, color constants | Rectangles and circles drawn from `game_render()` |
| 03 | Mouse input and drawing | `MouseInput`, brush press/drag, `LineBitmap` write, Bresenham line | Player can draw dark lines on the canvas |
| 04 | One falling grain | `vx`, `vy`, gravity, bounce; no collision yet | A single grain falls and bounces off the bottom edge |
| 05 | Grain pool | SoA `GrainPool`, `MAX_GRAINS`, active flag, spawning | Hundreds of grains falling at once |
| 06 | Grain collision | `LineBitmap` solid check; grains land on player lines | Grains pile up on drawn lines |
| 07 | Slide, settle, bake | Diagonal slide, angle of repose, `GRAIN_SETTLE_FRAMES`, baking | Grains form realistic sand piles; baked grains free the pool |
| 08 | State machine | `GAME_PHASE`, `change_phase()`, title screen | Title screen with keyboard/mouse navigation |
| 09 | Level system | `LevelDef`, designated initialisers, emitters, cups, `level_load()` | Level 1 playable: grains pour and fill a cup |
| 10 | Win detection | `check_win()`, `PHASE_LEVEL_COMPLETE`, level advance | Completing a level shows a brief celebration screen and loads the next |
| 11 | Color filters | `GRAIN_COLOR`, `ColorFilter`, per-grain color field | Grains passing through a zone change color; colored cups require matching grains |
| 12 | Obstacles and level variety | Pre-set `Obstacle` rectangles stamped at load; levels 1–15 | More complex routing puzzles |
| 13 | Font system and HUD | `font.h` (8×8 bitmap), `draw_text`, level number, grain counts | On-screen level indicator, cup fill percentage |
| 14 | Teleporters and gravity flip | `Teleporter` with cooldown, `gravity_sign` toggle | Grains warp between portals; G key flips gravity |
| 15 | Audio and final polish | Raylib audio implementation, `sounds.h`, level-complete fanfare, all 30 levels | Complete Sugar Sugar with sound effects |

---

## JS→C concept mapping

| Lesson | New C concept | Nearest JS equivalent |
|--------|--------------|----------------------|
| 01 | `uint32_t *pixels` flat array, `pitch` row stride | `Uint32Array` typed array; `y * width + x` index |
| 01 | Two `.c` translation units compiled together | Two ES module files bundled by Webpack |
| 01 | `build-dev.sh --backend=x11\|raylib` flag parsing | `npm run start:x11` vs `npm run start:raylib` |
| 02 | `uint32_t` pixel as packed ARGB integer | `ctx.fillStyle = '#RRGGBB'` CSS color |
| 02 | `GAME_RGB(r,g,b)` macro | `const rgb = (r,g,b) => \`rgba(${r},${g},${b})\`` |
| 03 | `GameButtonState` half-transition model | `{ isDown, pressedThisFrame, releasedThisFrame }` |
| 03 | `MouseInput.prev_x / prev_y` | `event.movementX` accumulated over the frame |
| 03 | Bresenham line algorithm | Canvas `ctx.lineTo()` — but we implement it manually |
| 04 | Euler integration `y += vy * dt; vy += g * dt` | `y += vy * dt` in a `requestAnimationFrame` loop |
| 04 | `dt` delta-time capping | `const dt = Math.min(elapsed, 100)` |
| 05 | SoA `GrainPool` (Struct of Arrays) | Array of `{x, y, vx, vy}` objects = AoS; SoA = separate `x[], y[], vx[], vy[]` typed arrays |
| 05 | High-watermark `count` field | `.length` on a sparse array |
| 06 | `LineBitmap uint8_t pixels[]` packed byte encoding | `Uint8Array` with enum values |
| 06 | `IS_SOLID` macro for bounds-guarded lookup | `const isSolid = (x,y) => x>=0 && x<W && bitmap[y*W+x]` |
| 07 | `GRAIN_SLIDE_REACH` diagonal check | Physics: angle of repose; closest JS = `if (Math.abs(dx) <= reach && free)` |
| 07 | `still[]` settle counter + bake | Particles library "sleep" state |
| 08 | `GAME_PHASE` enum + `switch(state->phase)` | Redux reducer with action types |
| 08 | `change_phase()` — single entry point for transitions | `dispatch(action)` in a state machine |
| 09 | `LevelDef` with designated initialisers `[0] = { .field = val }` | `const levels = [{ emitters:[...], cups:[...] }]` |
| 09 | `extern LevelDef g_levels[]` across translation units | `export const levels = [...]` |
| 10 | `check_win()` iterating cups | `cups.every(c => c.collected >= c.required)` |
| 11 | `GRAIN_COLOR` enum + per-grain `color` byte | `grain.color = 'red'` property |
| 12 | Obstacles stamped into `LineBitmap` at load | Drawing obstacle rects onto an offscreen canvas at init |
| 13 | 8×8 bitmap font, bit-test per pixel | Drawing text onto a canvas with a custom bitmap atlas |
| 14 | Teleporter `tpcd` (cooldown byte) prevents re-entry | `if (grain.teleportCooldown > 0) skip;` |
| 14 | `gravity_sign = ±1` | `const gravityDir = -1` multiplied against gravity constant |
| 15 | `platform_play_sound(id)` callback pattern | `audioCtx.playSound(soundId)` event-driven |
| 15 | `LoadSound` / `PlaySound` (Raylib) | `const audio = new Audio('sound.mp3'); audio.play()` |

---

## Course-builder.md rules: followed vs. exceptions

### Followed ✅

- CPU backbuffer pipeline (`GameBackbuffer`, `uint32_t *pixels`, GPU upload per frame)
- `GAME_RGB` / `GAME_RGBA` macros, 0xAARRGGBB byte order
- 3-layer split: game logic / `platform.h` contract / `main_*.c` backends
- `GameButtonState` (`half_transition_count`, `ended_down`); `UPDATE_BUTTON` macro
- `BUTTON_PRESSED` / `BUTTON_RELEASED` convenience macros
- `prepare_input_frame()` called each frame before `platform_get_input()`
- `GAME_PHASE` enum state machine with `change_phase()` helper
- 8×8 ASCII-indexed bitmap font (`font.h`, BIT7-left convention)
- Platform contract minimal and stable (4 mandatory functions)
- Data-driven level definitions isolated in `levels.c`
- `= {0}` aggregate init; `memset` for full reset
- `should_quit` flag on `GameState`
- `render_*` functions are pure reads of `GameState`
- `pitch` field on `GameBackbuffer` (never hardcoded `width × 4`)
- `#define CANVAS_W 640` / `CANVAS_H 480` fixed logical canvas

### Intentional exceptions ⚠️

| Rule | Exception | Reason |
|------|-----------|--------|
| Unified `build-dev.sh` | Reference had two separate scripts | Fixed: course uses unified script |
| Audio ≥ 6–8 sounds | Reference had empty stubs | Fixed: Raylib backend gets real implementation |
| VSync + letterbox from Lesson 1 | Reference had no letterbox | Fixed: add letterbox + `SetTargetFPS(60)` |
| Y-up meters coordinate system | **Y-down pixels** | Cellular automaton / falling-sand: physics grid maps to screen pixels directly. Using meters adds indirection with zero benefit. This is a documented exception. See Lesson 04. |
| `GAME_PHASE_*` enum prefix | Reference uses `PHASE_*` | Keep reference naming (internally consistent); note difference in comments |
| Audio thread (ALSA, low-level) | Callback pattern (`platform_play_sound`) | Sugar Sugar fires audio on discrete events (cup fill, level complete), not every frame. `PlaySound()` is correct here. See Lesson 15. |
