# Sugar, Sugar — Plan Tracker

## Status legend
- `[ ]` pending
- `[~]` in progress
- `[x]` complete
- `[!]` blocked

---

## Phase 1 — Planning

- [x] `PLAN.md` — course outline, lesson table, JS→C mapping, rule exceptions
- [x] `README.md` — overview, prerequisites, build instructions
- [x] `PLAN-TRACKER.md` — this file

---

## Phase 2 — Source files

- [x] `course/build-dev.sh` — unified `--backend=x11|raylib -r -d` build script
- [x] `course/src/font.h` — 8×8 bitmap font table (verbatim from reference)
- [x] `course/src/sounds.h` — sound ID enum (verbatim from reference)
- [x] `course/src/platform.h` — platform contract header (enhanced comments)
- [x] `course/src/game.h` — all type definitions (enhanced JS-analogy comments)
- [x] `course/src/game.c` — full game logic (Raylib color-fix + enhanced comments)
- [x] `course/src/levels.c` — 30 level definitions (verbatim)
- [x] `course/src/main_x11.c` — X11 backend (non-resizable window, frame-cap)
- [x] `course/src/main_raylib.c` — Raylib backend (real audio + letterbox + R↔B fix)

---

## Phase 3 — Lessons

- [x] `lessons/lesson-01-window-and-canvas.md`
- [x] `lessons/lesson-02-pixel-rendering.md`
- [x] `lessons/lesson-03-mouse-input-and-drawing.md`
- [x] `lessons/lesson-04-one-falling-grain.md`
- [x] `lessons/lesson-05-grain-pool.md`
- [x] `lessons/lesson-06-grain-collision.md`
- [x] `lessons/lesson-07-slide-settle-bake.md`
- [x] `lessons/lesson-08-state-machine.md`
- [x] `lessons/lesson-09-level-system.md`
- [x] `lessons/lesson-10-win-detection.md`
- [x] `lessons/lesson-11-color-filters.md`
- [x] `lessons/lesson-12-obstacles-and-level-variety.md`
- [x] `lessons/lesson-13-font-system-and-hud.md`
- [x] `lessons/lesson-14-teleporters-and-gravity-flip.md`
- [x] `lessons/lesson-15-audio-and-final-polish.md`

---

## Phase 4 — Meta

- [x] `COURSE-BUILDER-IMPROVEMENTS.md`

---

## Notes

- Y-down pixel coordinates are an intentional exception (see PLAN.md)
- `PHASE_*` enum naming follows reference; differs from `GAME_PHASE_*` convention in other courses
- Raylib backend must implement `platform_play_sound()` / `platform_play_music()` for real (reference had stubs)
- Color channel swap (R↔B) must be fixed in Raylib backend (`0xAARRGGBB` → Raylib RGBA)
