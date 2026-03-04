# Frogger Course — Plan Tracker

Progress log. Updated after every file created or phase completed.

---

## Phase 1 — Planning

| Task          | Status  | Notes                                              |
|---------------|---------|----------------------------------------------------|
| PLAN.md       | ✅ Done | Lesson sequence, file tree, JS→C mapping, arch notes |
| README.md     | ✅ Done | Overview, prerequisites, build/run, lesson list    |
| PLAN-TRACKER.md | ✅ Done | This file                                        |

---

## Phase 2 — Source files

| Task                          | Status   | Notes |
|-------------------------------|----------|-------|
| `course/build-dev.sh`         | ✅ Done  | Unified `--backend=x11\|raylib -r -d` |
| `course/assets/` (copy .spr)  | ✅ Done  | All 9 `.spr` files copied |
| `course/src/platform.h`       | ✅ Done  | 4-function contract + `PlatformGameProps` + `platform_swap_input_buffers` |
| `course/src/game.h`           | ✅ Done  | All types, `GAME_PHASE`, `SpriteBank`, `lane_scroll`, public API |
| `course/src/game.c`           | ✅ Done  | `game_init`, `game_update`, `game_render` |
| `course/src/audio.c`          | ✅ Done  | 4 sounds, mixer, spatial pan |
| `course/src/main_x11.c`       | ✅ Done  | X11+GLX+ALSA; letterbox; `XkbKeycodeToKeysym` |
| `course/src/main_raylib.c`    | ✅ Done  | Raylib; `FLAG_WINDOW_RESIZABLE`; `SAMPLES_PER_FRAME=2048` |
| `course/src/utils/backbuffer.h`  | ✅ Done | `Backbuffer`, `GAME_RGBA`, screen constants, `COLOR_*` |
| `course/src/utils/draw-shapes.h` | ✅ Done | Declarations |
| `course/src/utils/draw-shapes.c` | ✅ Done | `draw_rect`, `draw_rect_blend`, `draw_sprite_partial` |
| `course/src/utils/draw-text.h`   | ✅ Done | Declarations |
| `course/src/utils/draw-text.c`   | ✅ Done | `FONT_8X8[128][8]` + `draw_char`/`draw_text` |
| `course/src/utils/math.h`        | ✅ Done | `MIN`, `MAX`, `CLAMP`, `ABS` |
| `course/src/utils/audio.h`       | ✅ Done | `SOUND_ID`, `SoundInstance`, `GameAudioState`, `AudioOutputBuffer` |

---

## Phase 3 — Lesson files

| Task                                          | Status   | Notes |
|-----------------------------------------------|----------|-------|
| `lessons/01-window-and-backbuffer.md`         | ✅ Done  | 819 lines |
| `lessons/02-drawing-primitives.md`            | ✅ Done  | 715 lines |
| `lessons/03-input.md`                         | ✅ Done  | 721 lines |
| `lessons/04-game-structure-and-phase.md`      | ✅ Done  | 700 lines |
| `lessons/05-lane-data-dod.md`                 | ✅ Done  | 631 lines |
| `lessons/06-scrolling-lanes.md`               | ✅ Done  | 679 lines |
| `lessons/07-danger-buffer-and-collision.md`   | ✅ Done  | ghost-collision demo included |
| `lessons/08-sprites.md`                       | ✅ Done  | `.spr` loader, UV-crop, CONSOLE_PALETTE |
| `lessons/09-homes-and-win-condition.md`       | ✅ Done  | win overlay, game-over, best_score preserve |
| `lessons/10-font-and-hud.md`                  | ✅ Done  | `FONT_8X8[128][8]` BIT7-left, `snprintf` HUD |
| `lessons/11-audio.md`                         | ✅ Done  | ALSA + Raylib patterns, SAMPLES_PER_FRAME=2048 |
| `lessons/12-polish-and-utils-refactor.md`     | ✅ Done  | complete gap table, respawn flicker, overlays |

---

## Phase 4 — Retrospective

| Task                              | Status   | Notes |
|-----------------------------------|----------|-------|
| `COURSE-BUILDER-IMPROVEMENTS.md` | ✅ Done  | 15 gaps + 5 process issues documented |

---

## Build verification

| Check                                    | Status   | Notes |
|------------------------------------------|----------|-------|
| X11 backend builds with zero warnings    | ✅ Done  | `clang -Wall -Wextra` clean |
| Raylib backend builds with zero warnings | ✅ Done  | `clang -Wall -Wextra` clean |
| Input testing checklist passed           | ⬜ Manual | Run `./build/frogger` and test arrow keys |
| Rendering testing checklist passed       | ⬜ Manual | Verify sprites, scrolling, letterbox |
| Game logic testing checklist passed      | ⬜ Manual | Verify death/win/restart flows |
| Audio testing checklist passed           | ⬜ Manual | Verify SFX on hop/splash/home/gameover |
