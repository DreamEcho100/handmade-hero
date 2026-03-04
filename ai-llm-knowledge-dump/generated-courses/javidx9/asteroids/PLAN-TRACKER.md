# Asteroids Course — PLAN-TRACKER.md

Progress log. Update status after every file created or major task completed.

**Status legend:** ⬜ Pending | 🔄 In progress | ✅ Done | ❌ Blocked

---

## Phase 1 — Planning files

| Task | Status | Notes |
|------|--------|-------|
| `PLAN.md` | ✅ Done | Lesson sequence, file structure, JS→C mapping, C++ vs C comparison, gap table |
| `README.md` | ✅ Done | Course overview, prerequisites, controls, lesson list, build instructions |
| `PLAN-TRACKER.md` | ✅ Done | This file |

---

## Phase 2 — Source files

| Task | Status | Notes |
|------|--------|-------|
| `course/build-dev.sh` | ✅ Done | Unified `--backend=x11\|raylib -r -d`; replaces reference `build_x11.sh` + `build_raylib.sh` |
| `course/src/platform.h` | ✅ Done | 4-function contract + `platform_swap_input_buffers`; adds `platform_shutdown` vs reference |
| `course/src/utils/backbuffer.h` | ✅ Done | `AsteroidsBackbuffer` with `pitch`; `GAME_RGBA` (renamed from `ASTEROIDS_RGBA`); `COLOR_*` |
| `course/src/utils/math.h` | ✅ Done | `MIN`, `MAX`, `CLAMP`, `ABS`, `Vec2` struct + inline helpers |
| `course/src/utils/audio.h` | ✅ Done | `AudioOutputBuffer`, `SOUND_ID` enum (6+NONE+COUNT), `SoundInstance`, `GameAudioState`, pan helpers |
| `course/src/utils/draw-shapes.h` | ✅ Done | Declarations for `draw_pixel_w`, `draw_pixel`, `draw_rect`, `draw_rect_blend`, `draw_line` |
| `course/src/utils/draw-shapes.c` | ✅ Done | Implementations; toroidal `draw_pixel_w`; Bresenham line; correct GAME_RGBA channel extraction |
| `course/src/utils/draw-text.h` | ✅ Done | Declarations for `draw_char`, `draw_text`, `GLYPH_W/H` constants |
| `course/src/utils/draw-text.c` | ✅ Done | Full `FONT_8X8[128][8]` BIT7-left; `draw_char` clipped; `draw_text` |
| `course/src/game.h` | ✅ Done | `SpaceObject`, `GameState`, `GameInput` union (4 buttons), `GAME_PHASE`, `DEBUG_TRAP/ASSERT`, full public API |
| `course/src/game.c` | ✅ Done | `asteroids_init` (memset+save/restore), `asteroids_update`, `asteroids_render`, `prepare_input_frame`, `compact_pool`, `add_asteroid`, `add_bullet`, `draw_wireframe`, `reset_game` |
| `course/src/audio.c` | ✅ Done | `game_audio_init`, `game_get_audio_samples`, `game_play_sound`, `game_play_sound_panned`, `game_is_thrust_playing`; SFX defs for 6 sound types; linear envelope |
| `course/src/main_x11.c` | ✅ Done | X11 + GLX + ALSA; `WM_DELETE_WINDOW`, `XkbKeycodeToKeysym`, `XkbSetDetectableAutoRepeat`, ALSA `sw_params`/`start_threshold=1`, `GL_RGBA` |
| `course/src/main_raylib.c` | ✅ Done | Raylib; per-frame `IsAudioStreamProcessed`+`UpdateAudioStream`; `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` |

---

## Phase 3 — Lesson files

| Task | Status | Notes |
|------|--------|-------|
| `course/lessons/01-window-backbuffer.md` | ✅ Done | GAME_RGBA byte layout, pitch, DEBUG_TRAP/ASSERT, build-dev.sh |
| `course/lessons/02-drawing-primitives.md` | ✅ Done | draw_pixel, draw_rect, draw_rect_blend, pure-red test |
| `course/lessons/03-line-drawing.md` | ✅ Done | Bresenham trace table, draw_pixel_w toroidal, triangle example |
| `course/lessons/04-input.md` | ✅ Done | GameButtonState, anonymous union, double-buffer, "just pressed" |
| `course/lessons/05-game-structure.md` | ✅ Done | GameState, GAME_PHASE, memset save/restore pattern, 3-layer arch |
| `course/lessons/06-vec2-rotation.md` | ✅ Done | Vec2, 2×2 rotation matrix, draw_wireframe, ship_model, srand |
| `course/lessons/07-physics-wrap.md` | ✅ Done | Euler integration, speed cap, fmodf toroidal, asteroid drift |
| `course/lessons/08-entity-pool.md` | ✅ Done | Fixed pool, compact_pool swap-tail, add_asteroid, jagged model |
| `course/lessons/09-bullets.md` | ✅ Done | add_bullet, FIRE cooldown, lifetime decay, compact after loop |
| `course/lessons/10-collision.md` | ✅ Done | Circle collision, two-loop pattern, asteroid split, wave clear |
| `course/lessons/11-font-hud.md` | ✅ Done | FONT_8X8 BIT7-left, draw_char/draw_text, snprintf, centring |
| `course/lessons/12-audio.md` | ✅ Done | SFX synthesis, spatial pan, thrust loop, ALSA fix, Raylib model |
| `course/lessons/13-refactor.md` | ✅ Done | utils/ split rationale, what moves vs stays, build verification |

---

## Phase 4 — Retrospective

| Task | Status | Notes |
|------|--------|-------|
| `COURSE-BUILDER-IMPROVEMENTS.md` | ✅ Done | Written in Snake course retrospective; referenced here |

---

## Build verification checkpoints

| Checkpoint | Status | Notes |
|------------|--------|-------|
| Phase 2 complete: both backends build clean (`-Wall -Wextra`, zero warnings) | ✅ Done | Verified with clang |
| Phase 3 complete: all 13 lesson markdown files written | ✅ Done | |
| Gameplay verified: asteroids split, score increments, wave clears, audio plays | ⬜ Pending | Play-test required |
| Colours correct on both backends (pure-red `draw_rect` test) | ⬜ Pending | Visual verification |
| Audio plays on both backends (not silent) | ⬜ Pending | Requires running game |
| Window × button exits cleanly (no terminal hang) | ⬜ Pending | Requires running game |
| Q / Esc exits cleanly on X11 | ⬜ Pending | Requires running game |
