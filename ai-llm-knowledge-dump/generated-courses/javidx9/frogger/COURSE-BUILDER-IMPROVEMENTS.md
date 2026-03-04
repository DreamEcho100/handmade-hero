# COURSE-BUILDER-IMPROVEMENTS.md
# Frogger Course — Deviations, Upgrades, and Retrospective

This document has two parts:

1. **Part I — Engineering Deviations** — Every deliberate deviation from the
   reference source and why it makes the course better.

2. **Part II — Process Retrospective** — Issues that surfaced during the build,
   with generic mitigations applicable to any future course.

---

## Summary Table — Part I: Engineering Deviations

| # | Area | Reference Source | Course Upgrade | Lesson |
|---|------|-----------------|----------------|--------|
| 1  | Build scripts | `build_x11.sh` + `build_raylib.sh` | Unified `build-dev.sh --backend=x11\|raylib -r -d` | 01 |
| 2  | Backbuffer | No `pitch` field | `{pixels, width, height, pitch}` — teaches stride | 01 |
| 3  | Debug tools | None | `DEBUG_TRAP()` / `ASSERT(expr)` under `#ifdef DEBUG` | 01 |
| 4  | Platform API | 3-function contract | 4-function: added `platform_shutdown` | 01 |
| 5  | Pixel format | `FROGGER_RGBA` = `0xAABBGGRR` | `GAME_RGBA` = `0xAARRGGBB` (Tetris convention) | 02 |
| 6  | Color constants | Hardcoded literals | Named `COLOR_BLACK`, `COLOR_WHITE`, etc. | 02 |
| 7  | Input model | Single `GameInput` | Double-buffered `inputs[2]` + `prepare_input_frame` | 03 |
| 8  | Input union | No union | `union { buttons[BUTTON_COUNT]; struct { move_up, … }; }` | 03 |
| 9  | Platform init | Raw `(title, w, h)` args | `PlatformGameProps` wrapper struct | 01 |
| 10 | Window resize | Fixed non-resizable window | Letterbox centering from Lesson 01 | 01 |
| 11 | Debug output | Bare `fprintf` | Guarded by `#ifdef DEBUG` | 01 |
| 12 | Code organisation | All draw code inline in `frogger.c` | `utils/draw-shapes.c`, `draw-text.c`, `math.h` | 12 |
| 13 | Font | 5×7 column-major `font_glyphs[96][5]` | `FONT_8X8[128][8]` BIT7-left | 10 |
| 14 | Audio | None | `audio.c` + `utils/audio.h`; 4 SFX; spatial pan | 11 |
| 15 | Naming | `frogger_init`, `frogger_tick`, `frogger_render` | `game_init`, `game_update`, `game_render` | 04 |

---

## Part I: Engineering Deviations

### 1. Unified Build Script

**Reference:** two separate scripts `build_x11.sh` and `build_raylib.sh` with ~90%
duplicated content (compiler flags, source list, output path).

**Upgrade:** single `build-dev.sh --backend=x11|raylib -r -d` with a `case`
statement for the 10% that differs.

**Trade-off:** slightly more complex argument parsing in exchange for:
- Single source of truth for compiler flags
- `--debug` flag adds AddressSanitizer + UBSan to both backends
- Consistent workflow matching Tetris/Snake/Asteroids

**Mitigation note:** the `ASSETS_DIR` define is baked in at compile time using
`-DASSETS_DIR=\"$(pwd)/assets\"`.  This means the binary must be run from the
`course/` directory, or the define overridden at build time.  A production
build would use `$XDG_DATA_DIR` or a config file.

---

### 2. Backbuffer `pitch` Field

**Reference:** backbuffer width assumed to equal stride.

**Upgrade:** `Backbuffer.pitch = width * 4` explicitly.  Pixel address is:
```c
&bb->pixels[y * (bb->pitch / 4) + x]
```

**Why:** teaches the concept of stride — essential for understanding GPU texture
uploads, sub-region blitting, and misaligned-width image formats.

---

### 3. Debug Tools: `DEBUG_TRAP` / `ASSERT`

**Reference:** no debug assertions.

**Upgrade:**
```c
#ifdef DEBUG
  #define DEBUG_TRAP() __builtin_trap()
  #define ASSERT(expr) do { if (!(expr)) DEBUG_TRAP(); } while(0)
#else
  #define DEBUG_TRAP() ((void)0)
  #define ASSERT(expr) ((void)0)
#endif
```

Used to catch out-of-bounds pool accesses, invalid sound IDs, and null pointers
at development time.  Zero cost in release builds.

---

### 4. `platform_shutdown`

**Reference:** no shutdown function — the OS reclaims resources on exit.

**Upgrade:** `platform_shutdown()` drains ALSA, destroys the GL context, and
closes the X11 display / Raylib window cleanly.

**Why:** without `snd_pcm_drain`, ALSA leaves audio hardware in an undefined
state that can cause the next process to fail to open the device.  In Raylib,
`CloseWindow()` is needed to prevent a segfault in miniaudio's cleanup thread.

---

### 5. `GAME_RGBA` Pixel Format

**Reference:** `FROGGER_RGBA(r,g,b,a) = (a<<24)|(b<<16)|(g<<8)|r`
→ memory layout `[R][G][B][A]` on little-endian ✓

The reference's name (`0xAABBGGRR`) suggests the byte order, but the actual
formula is identical to the Tetris convention.  Both produce `[R][G][B][A]`
in memory — an exact match for `GL_RGBA` and
`PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`.

**Upgrade:** rename to `GAME_RGBA` for cross-game consistency.  Add a
pure-red validation test in Lesson 02 to catch `GL_BGRA` / byte-order bugs
on both backends.

---

### 6. Named Color Constants

**Reference:** raw `0xFF00FF00`-style literals scattered through drawing code.

**Upgrade:** `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_RED`, `COLOR_GREEN`,
`COLOR_YELLOW`, `COLOR_CYAN` defined once in `utils/backbuffer.h`.

**Why:** a single definition point; refactoring a color means one change.
Matches the pattern in Tetris/Snake/Asteroids.

---

### 7 & 8. Double-Buffered Input + `GameInput` Union

**Reference:** single `GameInput`; no union; `prepare_input_frame` only clears.

**Upgrade:** `inputs[2]` double-buffer with `prepare_input_frame(old, current)`.
The union allows loop-based clearing (`buttons[i]`) while preserving named access
(`input->move_up`).

**Why Frogger specifically benefits:** Frogger uses "just pressed" detection
(one hop per keypress, no auto-repeat).  A single buffer cannot distinguish
"key held this frame" from "key just pressed this frame" — the double buffer
solves this cleanly.

---

### 9. `PlatformGameProps` Wrapper

**Reference:** `platform_init(const char *title, int width, int height)` — raw args.

**Upgrade:** `PlatformGameProps { title, width, height, backbuffer, is_running }`.

**Why:** adding a new platform property (e.g. `fullscreen`, `vsync`) only
changes the struct, not every call site.  The `is_running` field allows the
platform to signal quit without a global variable visible to game code.

---

### 10. Letterbox Centering from Lesson 01

**Reference:** fixed-size window; no resize handling.

**Upgrade:** both backends support resizable windows with letterbox centering
from Lesson 01 (not retrofitted in a later lesson).

**X11:** `ConfigureNotify` event updates `g_win_w/h`; `display_backbuffer()`
uses two `glViewport` calls — clear with the full window viewport, draw with
the centred viewport.

**Raylib:** `FLAG_WINDOW_RESIZABLE` + `DrawTexturePro` with a scaled destination
rectangle.

---

### 11. `#ifdef DEBUG`-guarded Debug Output

**Reference:** `fprintf(stderr, …)` calls left active in release code.

**Upgrade:** all debug prints wrapped in `#ifdef DEBUG`.  `build-dev.sh --debug`
enables `-DDEBUG` + AddressSanitizer + UBSan.  Release builds have zero debug
output.

---

### 12. `utils/` Split

**Reference:** `draw_sprite_partial`, bitmap font, and tile drawing all inline
in `frogger.c`.

**Upgrade:** extract into `utils/`:
- `draw-shapes.c` — `draw_rect`, `draw_rect_blend`, `draw_sprite_partial`
- `draw-text.c`   — `FONT_8X8[128][8]` + `draw_char` / `draw_text`
- `math.h`        — `MIN`, `MAX`, `CLAMP`, `ABS` (header-only macros)

**Why:** `game.c` stays focused on game logic.  `utils/` files are reusable
across games (the same `draw-shapes.c` and `draw-text.c` could be dropped
into a future course unchanged).

**Trade-off:** `CONSOLE_PALETTE` is `static const` in `game.h` — each TU
gets its own copy (~48 bytes).  A production codebase would declare it `extern`
in the header and define it once in `game.c`.  For a course this is acceptable
(zero linker ambiguity, simpler teaching).

---

### 13. `FONT_8X8[128][8]` BIT7-left

**Reference:** `font_glyphs[96][5]` — 5-column-wide, 96-char subset (starting
at space), column-major (each byte encodes one column).

**Upgrade:** `FONT_8X8[128][8]` — 8×8, full 128-char ASCII, row-major,
BIT7-left (`0x80 >> col` to test a pixel).

**Why:**
1. Direct ASCII indexing — no `ch - 32` offset, no bounds check for common chars.
2. Larger glyph set — all 128 ASCII codes usable.
3. Row-major storage matches screen scan order for the inner render loop.
4. Consistent with Tetris/Snake/Asteroids.

**Memory trade-off:** 1024 bytes vs 480 bytes.  Negligible at game scale.

---

### 14. Audio System (Added from Scratch)

**Reference:** no audio.

**Upgrade:** `audio.c` + `utils/audio.h`:
- `SOUND_ID` enum: `SOUND_HOP`, `SOUND_SPLASH`, `SOUND_HOME_REACHED`, `SOUND_GAME_OVER`
- `SoundDef` table: 4 sounds with `base_freq`, `freq_slide`, `duration`, `volume`
- Mixer: square / noise / triangle waves + `ENVELOPE_FADEOUT`
- `game_play_sound(state, id, pan)` — spatial panning
- X11: ALSA `snd_pcm_writei` with underrun recovery
- Raylib: `IsAudioStreamProcessed` gate + `SAMPLES_PER_FRAME = 2048`

**Why SAMPLES_PER_FRAME = 2048 (not 735):** At 735 frames/push, Raylib's
miniaudio ring buffer can be both sub-buffers full when the game loop runs
slightly fast.  Silently dropped pushes cause the write-pointer to drift,
mixing stale and new data → "wavy/echoing" artifact with a pitch shift.
2048 frames with the `IsAudioStreamProcessed` gate eliminates this.

---

### 15. `game_init` / `game_update` / `game_render` Naming

**Reference:** `frogger_init`, `frogger_tick`, `frogger_render`.

**Upgrade:** `game_init`, `game_update`, `game_render` — consistent with all
other courses.  `game.h` / `game.c` instead of `frogger.h` / `frogger.c`.

**Why:** a student moving from Tetris → Snake → Asteroids → Frogger should
see the same API shape each time.  Only the game logic inside changes.

---

## Part II: Process Retrospective

### Issue 1 — `GAME_RGBA` naming vs documentation

The reference uses `FROGGER_RGBA` and the docs say `0xAABBGGRR`, but the
*actual byte layout* `[R][G][B][A]` is identical to the Tetris convention.
This naming inconsistency caused confusion during analysis.

**Mitigation:** add a cross-backend validation test (pure-red `draw_rect`)
in Lesson 02 of every course.  If the bytes are wrong the red square turns
cyan — instantly visible.

---

### Issue 2 — `lane_scroll` consistency invariant is subtle

The requirement that `game_update` and `game_render` call `lane_scroll` with
the *same* `state->time` is easy to violate accidentally (e.g. by adding a
`dt` accumulation in `game_render`).  The resulting bug (ghost collision) is
nearly impossible to diagnose without the invariant explanation.

**Mitigation:** document the invariant prominently in `game.h` above the
`lane_scroll` inline, in `game.c` above both call sites, and in Lesson 07
with a deliberate bug demonstration before the fix.

---

### Issue 3 — `alloca` requires explicit `#include <alloca.h>` with clang

`snd_pcm_hw_params_alloca` expands to `alloca()`.  On clang with `-Wall`,
this produces "call to undeclared library function" even though glibc includes
`<stdlib.h>` in `<alsa/asoundlib.h>`.  Clang is more strict.

**Mitigation:** add `#include <alloca.h>` explicitly before `<alsa/asoundlib.h>`
in `main_x11.c`.  Document in a comment.

---

### Issue 4 — `GameInput` field names differ from asteroids course

The asteroids course uses `input->up`, `input->left`, etc.  Frogger uses
`input->move_up`, `input->move_left` (to avoid ambiguity with the concept of
"direction" vs "action").  This caused copy-paste errors in the initial
backend drafts.

**Mitigation:** always read `game.h` `GameInput` struct before writing a new
backend.  The field names are the canonical source of truth.

---

### Issue 5 — `static const` palette in header included in multiple TUs

`CONSOLE_PALETTE` declared `static const` in `game.h` is included by both
`game.c` and `draw-shapes.c` — each TU gets its own 48-byte copy.  The linker
sees no duplicate symbol (because `static`), so no error, but the pattern is
unusual.

**Mitigation (future courses):** declare as `extern const` in the header,
define once in `game.c`.  Access via a thin getter if needed by utils code.
For this course `static const` is kept to simplify the lesson on multi-file
C (students don't need to understand `extern` linkage yet in Lesson 02).
