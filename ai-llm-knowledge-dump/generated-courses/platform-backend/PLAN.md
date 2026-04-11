# Platform Foundation Course — PLAN.md

> Status: this file is now a legacy flat-course planning snapshot.
>
> Treat redesign-era planning material as historical archive context only.
> The active course surface is tracked by the top-level README and PLAN-TRACKER, not by older redesign notes.

> **Course-builder principles:** `ai-llm-knowledge-dump/prompt/course-builder.md`
> **Reference implementations:** `games/snake/` (Stage B layout), `games/tetris/` (Stage A)
> **Frame timing reference:** `engine/platforms/_common/frame-timing.{c,h}`, `engine/platforms/_common/frame-stats.{c,h}`, `engine/platforms/x11/backend.c`
> **Audio architecture reference:** `engine/docs/audio-architecture.md`
> **Purpose:** Teach the reusable platform/backend infrastructure once; game courses copy and adapt it.

---

## What This Course Builds

The Platform Foundation Course produces a complete, working **blank-canvas game template** — a window that opens, clears to a solid color, accepts keyboard input, and plays a test tone on demand. No game logic is included. The output is a set of source files that every subsequent game course copies as its starting point, adapting only the game-specific parts.

**Why this course exists:** Every game course previously rebuilt the same platform infrastructure in its first 4–6 lessons. The code was nearly identical each time. This course teaches it once, correctly, with full explanation. Game courses then copy the template and focus their lessons entirely on game-specific logic.

**What it is NOT:**

- Not a game course — no `GameState`, no game logic, no `game_update()`/`game_render()`
- Not a library — the output is source files to copy and own, not a `.a` or `.so` dependency

---

## Final Observable State

At the end of Lesson 14, the student has:

- A window that opens on both Raylib and X11 backends
- A solid-colored canvas rendered via the backbuffer pipeline
- A test rectangle and text string drawn using utilities
- FPS counter rendered as text (demonstrates `snprintf` for integer→string)
- Keyboard input detected and logged (Q = quit, space = play test tone)
- A procedural test tone played on keypress via the PCM mixer
- The complete template file tree ready to copy into any future game course

---

## Final File Structure (Stage B)

```
platform-backend/
├── PLAN.md                            ← this file
├── README.md
├── PLAN-TRACKER.md
└── course/
    ├── build-dev.sh                   ← --backend=x11|raylib, -r, -d/--debug, SOURCES var
    └── src/
        ├── utils/
        │   ├── math.h                 ← CLAMP, MIN, MAX, ABS macros
        │   ├── backbuffer.h           ← Backbuffer struct (with pitch) + GAME_RGB/RGBA + color constants
        │   ├── draw-shapes.h          ← draw_rect / draw_rect_blend declarations
        │   ├── draw-shapes.c          ← clipped fill + alpha blend implementation
        │   ├── draw-text.h            ← draw_char / draw_text declarations
        │   └── draw-text.c            ← 8×8 ASCII-indexed FONT_8X8[128][8] + renderers
        ├── game/
        │   ├── base.h                 ← GameButtonState, GameInput union, UPDATE_BUTTON, prepare_input_frame,
        │   │                            platform_swap_input_buffers, DEBUG_TRAP/ASSERT, BUTTON_COUNT
        │   ├── demo.c                 ← demo_render() for L03–L08 visual demos; REMOVED in L14
        │   └── audio_demo.c           ← PCM mixer, SoundDef table, MusicSequencer demo; REMOVED in L14
        ├── platforms/
        │   ├── x11/
        │   │   ├── base.h             ← X11State struct + extern g_x11
        │   │   ├── base.c             ← X11State g_x11 = {0} definition
        │   │   ├── main.c             ← platform_init(), main loop, X11/GLX, input, frame timing
        │   │   └── audio.c            ← ALSA full: hw_params, sw_params, latency model, underrun recovery
        │   └── raylib/
        │       └── main.c             ← platform_init(), main loop, Raylib audio stream, input, frame timing
        └── platform.h                 ← shared contract: platform_init, platform_get_input, platform_shutdown,
                                         platform_get_time; PlatformGameProps; GameProps; AudioOutputBuffer;
                                         PlatformAudioConfig; platform_game_props_init/free;
                                         STRINGIFY/TOSTRING/TITLE macros
```

**Why `game/demo.c`?** Lessons 03–08 produce visible output (canvas, rectangles, text, FPS counter) but this course has no `game_update`/`game_render`. Rather than writing demo rendering code twice (once per backend), `demo_render(Backbuffer *bb, GameInput *input)` lives in a single shared `game/demo.c` called from both backends' main loops. This file is stripped in Lesson 14 when the template is finalized — just as `audio_demo.c` is stripped at that point.

**Why `platform_swap_input_buffers` in `game/base.h`?** The function swaps two `GameInput` structs — it is purely game-layer logic with no OS dependencies. Placing it next to `prepare_input_frame` (which it is always paired with) keeps the architecture boundary clean: `platform.h` declares only functions that backends _implement_, while `game/base.h` declares helpers that backends _call_.

**Why Stage B?** The X11 backend needs two separate files (windowing/input in `main.c`, ALSA audio in `audio.c`) that share internal state via `X11State g_x11`. Stage B's subdirectory layout makes this split clean without `base.h` being visible to the game layer.

---

## Topic Inventory

Every item below must appear in exactly one lesson's "New concepts" column.

### `build-dev.sh`

- `clang` compiler invocation, include paths, linker flags
- `SOURCES` variable for common source files
- `--backend=x11|raylib` flag with `case` statement
- `-r` / `--run` flag (build then run)
- `-d` / `--debug` flag (`-O0 -g -DDEBUG -fsanitize=address,undefined`)
- `mkdir -p build` before compile
- Output always `./build/game`

### `src/utils/math.h`

- `MIN`, `MAX`, `CLAMP`, `ABS` macros

### `src/utils/backbuffer.h`

- `Backbuffer` struct: `pixels`, `width`, `height`, `pitch`, `bytes_per_pixel`
- `GAME_RGB(r,g,b)` and `GAME_RGBA(r,g,b,a)` macros (0xAARRGGBB)
- Pre-defined named color constants (`COLOR_BLACK`, `COLOR_WHITE`, etc.)
- Why `pitch` ≠ `width × bpp` (row alignment)
- `bb->pixels[py * (pitch/4) + px]` pixel addressing
- R↔B channel swap: `0xAARRGGBB` stores `[B,G,R,A]` on little-endian; X11 needs `GL_BGRA`; Raylib's `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` reads `[R,G,B,A]` so R and B must be swapped

### `src/utils/draw-shapes.c/.h`

- `draw_rect()`: clip to bounds, row pointer via `pitch/4`, inner pixel loop
- `draw_rect_blend()`: per-channel alpha composite formula
- Why clip before loop (out-of-bounds write prevention)

### `src/utils/draw-text.c/.h`

- `FONT_8X8[128][8]` — ASCII-indexed 8×8 bitmap glyphs
- BIT7-left bit-mask `(0x80 >> col)` — why direction matters (test with 'N')
- `draw_char()`: glyph row loop, scale factor, bounds check
- `draw_text()`: cursor advance, `8 * scale`

### `src/game/demo.c`

- `demo_render(Backbuffer *bb, GameInput *input)` — visual demo called from both backends (L03–L08)
- `snprintf` for integer→string conversion (used in FPS counter display, L08)

### `src/game/base.h`

- `GameButtonState`: `ended_down`, `half_transition_count`
- `BUTTON_COUNT` macro
- `GameInput` struct with union (buttons array + named fields)
- `UPDATE_BUTTON(button, is_down)` macro — why do/while(0)
- `prepare_input_frame(old, current)` — copy `ended_down`, reset transition count
- `platform_swap_input_buffers(old, current)` — swap contents, not pointers (game-layer logic; placed here, not in `platform.h`)
- `DEBUG_TRAP()` / `ASSERT(expr)` / `ASSERT_MSG(expr, msg)` — platform-specific breakpoints

### `src/platform.h`

- `PlatformGameProps`: `title`, `width`, `height`, `game` (`GameProps`), `is_running`
- `GameProps`: `backbuffer` (`Backbuffer`), `audio` (`AudioOutputBuffer`)
- `platform_game_props_init()` — allocates backbuffer and audio buffers with `malloc`
- `platform_game_props_free()` — frees both buffers
- `STRINGIFY` / `TOSTRING` / `TITLE` macros — why two levels of macro expansion are needed
- `platform_init(props)` — backend returns non-zero on failure
- `platform_get_input(input, props)` — fills `GameInput` this frame
- `platform_shutdown()` — release OS handles, close audio device
- `platform_get_time()` → `double` seconds (monotonic)
- `PlatformAudioConfig` integration (stub in L05, full in L12)

### `src/utils/audio.h`

- `AudioOutputBuffer`: `samples_buffer`, `samples_per_second`, `sample_count`, `max_sample_count`, `is_initialized`
- `max_sample_count` safety cap — defense-in-depth against buffer overflow
- Interleaved stereo layout: `[L0, R0, L1, R1, ...]`, `samples_buffer[s*2+0]` = left
- `audio_write_sample(buf, frame_index, left_f32, right_f32)` — format-agnostic write helper; converts normalized `f32` to `int16_t` at output boundary
- `SoundDef`: `frequency`, `frequency_end`, `duration_ms`, `volume`
- `SOUND_ID` enum template (game adds its own entries)
- `SoundInstance`: `phase`, `samples_remaining`, `frequency`, `frequency_slide`, `fade_in_samples`, `pan_position`, `total_samples`, `volume`
- `GameAudioState`: `active_sounds[MAX_SIMULTANEOUS_SOUNDS]`, `music`, `samples_per_second`, `master_volume`, `sfx_volume`, `music_volume`
- `ToneGenerator`: `frequency`, `current_volume`, `target_volume`, `is_playing`
- `MusicSequencer`: `patterns`, `current_pattern`, `current_step`, `step_timer`, `step_duration`, `tone`, `is_playing`
- `MAX_SIMULTANEOUS_SOUNDS = 16`
- `PlatformAudioConfig`: `samples_per_second`, `buffer_size_samples`, `samples_per_frame`, `latency_samples`, `safety_samples`, `running_sample_index`, `frames_of_latency`, `hz`

### `src/platforms/x11/base.h` and `base.c`

- `X11State` struct: `display`, `window`, `gl_context`, `wm_delete_window`, `width`, `height`, `vsync_enabled`, `texture_id`, `is_running`, `audio` (`X11Audio`)
- `extern X11State g_x11` in header, defined once in `base.c`
- Why file-scope global (vs pointer parameter)

### `src/platforms/x11/main.c` (windowing, input, frame timing)

- `XOpenDisplay`, `XCreateWindow`, `XMapWindow`
- `XSizeHints` to lock window size — **temporary** fixed-size mechanism introduced in L02 for simplicity; removed in L08 when real letterbox is added
- `XPending` event drain loop (introduced in L02)
- `glXChooseFBConfig`, `glXCreateNewContext`, `glXMakeContextCurrent`
- `glGenTextures`, `glBindTexture`, `GL_TEXTURE_2D`
- `glTexImage2D` with `GL_BGRA` (byte-order for `0xAARRGGBB` on little-endian)
- `glTexSubImage2D` per-frame (update, not re-create)
- Fullscreen quad with `glBegin(GL_QUADS)` / UV coordinates
- VSync extension detection (`GLX_EXT_swap_control` / `GLX_MESA_swap_control`), `g_x11.vsync_enabled`
- Letterbox math: compute scale + offset to maintain aspect ratio on resize (L08; replaces `XSizeHints`)
- `XkbLookupKeySym` (not `XLookupKeysym`) — why CapsLock breaks it
- `XInternAtom("WM_DELETE_WINDOW")` + `XSetWMProtocols` + `ClientMessage` handler
- `platform_get_time()` via `CLOCK_MONOTONIC`
- **Frame timing (DE100 pattern):**
  - `FrameTiming` struct: `frame_start`, `work_end`, `frame_end`; derived `work_seconds`, `total_seconds`, `sleep_seconds`
  - `frame_timing_begin()` — capture `frame_start` (+ `__rdtsc()` in `DEBUG`)
  - `frame_timing_mark_work_done()` — capture `work_end`, compute `work_seconds`
  - `frame_timing_sleep_until_target(target_seconds)` — Phase 1: coarse `nanosleep(1ms)` loop until 3ms before target; Phase 2: spin-wait to exact target
  - `frame_timing_end()` — capture `frame_end`, compute `total_seconds` and `sleep_seconds`
  - `FrameStats` (`#ifdef DEBUG`): `frame_count`, `missed_frames`, `min_frame_time_ms`, `max_frame_time_ms`, `total_frame_time_ms`; printed on shutdown

### `src/platforms/x11/audio.c` (ALSA — full implementation, L12)

- `snd_pcm_open`, `snd_pcm_hw_params` (not `snd_pcm_set_params`)
- Hardware buffer parameters: format (`SND_PCM_FORMAT_S16_LE`), channels (2), rate, period
- `snd_pcm_hw_params_get_buffer_size` → `hw_buffer_size`
- `snd_pcm_sw_params_set_start_threshold(..., 1)` — eliminate silent startup
- Why `alloca.h` must come before `asoundlib.h` with clang
- Per-frame write: `platform_audio_get_samples_to_write()` via `snd_pcm_avail_update`
- `snd_pcm_writei` + `snd_pcm_recover` on underrun
- Re-fill silence after underrun recovery (prevent click)
- Pre-fill silence on init
- `snd_pcm_close` on shutdown

### `src/platforms/raylib/main.c` (Raylib — full audio, L11)

- `InitWindow`, `SetWindowState(FLAG_WINDOW_RESIZABLE)`, `SetTargetFPS`
- `LoadTexture` → `UpdateTexture` per frame
- Letterbox with `DrawTexturePro` / `DrawTextureEx`
- `IsKeyDown`, `IsKeyPressed`, `WindowShouldClose`
- `InitAudioDevice`, `SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE)`
- `LoadAudioStream(rate, 16, 2)`, `PlayAudioStream`
- Pre-fill 2 silent buffers on init (prevent startup click)
- `IsAudioStreamProcessed` + `UpdateAudioStream` per frame (use `while`, not `if`)
- `AUDIO_CHUNK_SIZE = 2048` — single constant for all Raylib audio sizing
- `GetTime()` for `platform_get_time()`
- `CloseAudioDevice`, `CloseWindow`

### Minimal audio init (L09 only — simplified, replaced in L11/L12)

- **Raylib**: `InitAudioDevice()` + `LoadAudioStream(sps, 16, 2)` + `PlayAudioStream()` + simple `if (IsAudioStreamProcessed)` check — sufficient to hear output immediately
- **X11**: `snd_pcm_open()` + `snd_pcm_set_params()` (convenience one-liner) + `snd_pcm_writei()` — sufficient to hear output immediately
- L11 and L12 refactor these to production-quality implementations; the contrast is a teaching moment

### PCM mixer (`game_get_audio_samples`)

- Float mixing — never `int16_t` directly (precision, no overflow)
- `memset(out, 0, ...)` clear before mix
- Phase accumulator per `SoundInstance`: `phase += freq * inv_sr; if (phase >= 1.0f) phase -= 1.0f`
- Square wave: `(phase < 0.5f) ? 1.0f : -1.0f`
- `is_initialized` guard — return silence if backend has not yet set up the audio device
- `audio_write_sample(buf, i, L, R)` — write via format-agnostic helper

### `game_play_sound_at` / `game_play_sound`

- Find free slot by `samples_remaining <= 0`; steal oldest if full
- `ASSERT(0 && "Sound pool full")` at insertion
- `duration_samples = duration_ms * sps / 1000`
- `frequency_slide = (freq_end - freq_start) / duration_samples`
- Click prevention: `fade_in` ramp + `fade_out` ramp
- Stereo panning (linear law): `vol_left = (pan <= 0) ? 1 : 1 - pan`

### Music: `ToneGenerator` + volume ramping

- `current_volume` → `target_volume` per sample at fixed `ramp_speed = 0.001f`
- Why ramp (discontinuous volume = click)

### Music: `MusicSequencer` + `midi_to_freq`

- `step_timer += delta_time`; advance `current_step` when `step_timer >= step_duration`
- `midi_to_freq(note) = 440.0f * powf(2.0f, (note - 69) / 12.0f)`
- Two-loop architecture: `game_audio_update` (game time) ↔ `game_get_audio_samples` (audio time)

### Delta-time game loop

- `platform_get_time()` before and after each frame
- `delta_time = (float)(curr - prev); prev = curr`
- Cap: `if (delta_time > 0.1f) delta_time = 0.1f`
- `prepare_input_frame` → `platform_get_input` → `game_update` → `demo_render` → upload backbuffer → audio
- `platform_swap_input_buffers` + double-buffer index pattern

---

## Proposed Lesson Sequence

> **Pedagogical note:** L03, L09, and L12 are the densest lessons (7 sub-concepts each, grouped into 3 teaching units). Each can be split into two lessons if students need more breathing room, at the cost of extending the course to 17 lessons.

| #   | Title                                      | What gets built                                                                                                       | Observable outcome                                          | New concepts (≤ 3 groups)                                                                                                                                                                                                                                                                                                                                                              | Files created/modified                                                                                |
| --- | ------------------------------------------ | --------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| 01  | Toolchain + Colored Window (Raylib)        | `build-dev.sh`; `platforms/raylib/main.c`; blank Raylib window                                                        | Solid-color window at 60 fps                                | `clang` invocation + include paths + linker flags; `build-dev.sh` (`SOURCES` var, `--backend`, `-r`, `-d`); `InitWindow`/`BeginDrawing`/`EndDrawing`/`CloseWindow`                                                                                                                                                                                                                     | `build-dev.sh`, `platforms/raylib/main.c`                                                             |
| 02  | X11/GLX Window                             | `platforms/x11/base.h/.c`; `platforms/x11/main.c`; same window via X11                                                | Same window via X11/GLX                                     | `XCreateWindow` + `XMapWindow` + `XSizeHints` (fixed size, temporary); GLX context (`glXChooseFBConfig`, `glXCreateNewContext`, `glXMakeContextCurrent`) + `glXSwapBuffers`; `XPending` event drain loop + `XInternAtom("WM_DELETE_WINDOW")` + `ClientMessage` handler                                                                                                                 | `platforms/x11/base.h`, `platforms/x11/base.c`, `platforms/x11/main.c`                                |
| 03  | Backbuffer + GAME_RGB                      | `utils/backbuffer.h`; pixel array allocated; uploaded each frame; `game/demo.c` with `demo_render()`                  | Solid-color canvas rendered via pixel buffer                | `Backbuffer` struct + `pitch` (stride ≠ width×bpp) + pixel addressing; `GAME_RGB/RGBA` macro (`0xAARRGGBB`) + named color constants; `glTexImage2D`/`glTexSubImage2D` (`GL_BGRA`) + `UpdateTexture` + R↔B channel swap                                                                                                                                                                 | `utils/backbuffer.h`, `game/demo.c` (new), both backends updated                                      |
| 04  | Drawing Primitives                         | `utils/math.h`; `utils/draw-shapes.c/.h`; rectangle drawn in `demo_render`                                            | Colored rectangle visible in window                         | `MIN`/`MAX`/`CLAMP`/`ABS` macros; `draw_rect` clip-before-loop + `pitch/4` row pointer; `draw_rect_blend` per-channel alpha composite formula                                                                                                                                                                                                                                          | `utils/math.h`, `utils/draw-shapes.h`, `utils/draw-shapes.c`, `game/demo.c` updated                   |
| 05  | Platform Contract                          | `platform.h` with `PlatformGameProps`, 4-function API, `platform_game_props_init/free`, `STRINGIFY/TOSTRING/TITLE`    | Same visual; backends now satisfy contract                  | `PlatformGameProps` + `GameProps` + `platform_game_props_init/free` (malloc); 4-function platform API (`platform_init`, `platform_get_input`, `platform_shutdown`, `platform_get_time`); `STRINGIFY`/`TOSTRING`/`TITLE` macros (why two levels of macro expansion)                                                                                                                     | `platform.h`, both backends refactored                                                                |
| 06  | Bitmap Font                                | `utils/draw-text.c/.h`; "PLATFORM READY" string rendered in `demo_render`                                             | Text visible in window                                      | `FONT_8X8[128][8]` ASCII-indexed bitmap glyphs; BIT7-left mask `(0x80>>col)` (why direction matters); `draw_char` glyph loop + scale + bounds + `draw_text` cursor advance                                                                                                                                                                                                             | `utils/draw-text.h`, `utils/draw-text.c`, `game/demo.c` updated                                       |
| 07  | Double-Buffered Input                      | `game/base.h`; Q key quits; key state logged; `platform_swap_input_buffers`                                           | Q key quits; key transitions logged to console              | `GameButtonState` (`ended_down` + `half_transition_count`) + `UPDATE_BUTTON` do/while(0); `GameInput` union + `prepare_input_frame` + `platform_swap_input_buffers` (in `game/base.h`); `DEBUG_TRAP`/`ASSERT`/`ASSERT_MSG`                                                                                                                                                             | `game/base.h`, both backends updated                                                                  |
| 08  | Delta-time Loop + Frame Timing + Letterbox | Full game loop wired; `FrameTiming`; VSync; letterbox (`XSizeHints` removed); FPS counter via `snprintf`              | Resizable window; letterboxed canvas; FPS counter displayed | delta-time pattern + `platform_get_time` + cap; `FrameTiming` (coarse `nanosleep` + spin-wait) + `FrameStats` (`#ifdef DEBUG`) + `snprintf` for FPS counter; VSync extension detection (`GLX_EXT_swap_control`) + letterbox math (scale + offset) + `XSizeHints` removed                                                                                                               | both backends updated, `game/demo.c` updated (FPS counter), `platform.h` (`PlatformAudioConfig` stub) |
| 09  | Audio Foundations + Minimal Output         | `utils/audio.h`; PCM mixer skeleton; phase accumulator; `audio_write_sample`; **minimal audio init on both backends** | Audible test tone on spacebar (both backends)               | `AudioOutputBuffer` + interleaved stereo layout + `max_sample_count` + `is_initialized` guard; phase accumulator + square wave + float mixing + `audio_write_sample` (format-agnostic helper) + `GameAudioState`; minimal Raylib audio init (`InitAudioDevice` + `LoadAudioStream` + simple `if (IsAudioStreamProcessed)`) + minimal ALSA init (`snd_pcm_open` + `snd_pcm_set_params`) | `utils/audio.h`, `game/audio_demo.c` (new), both backends updated (minimal audio)                     |
| 10  | Sound Effects                              | `SoundDef` table + `game_play_sound_at` + pool; click prevention; stereo panning; frequency slide                     | SFX with frequency sweep plays on spacebar                  | `SoundDef` + `SoundInstance` + `game_play_sound_at` (free-slot + steal-oldest); fade-in/fade-out click prevention (`elapsed / fade_in_samples`); stereo panning linear law + frequency slide per-sample                                                                                                                                                                                | `game/audio_demo.c` updated                                                                           |
| 11  | Audio — Raylib Full Integration            | `platforms/raylib/main.c` audio stream production-quality; replaces L09 minimal init                                  | Raylib audio: robust, pre-filled, no startup click          | `SetAudioStreamBufferSizeDefault` + `AUDIO_CHUNK_SIZE = 2048` (single constant); pre-fill 2 silent buffers on init (prevent startup click); `IsAudioStreamProcessed` `while`-loop (not `if`) + `UpdateAudioStream`                                                                                                                                                                     | `platforms/raylib/main.c` updated                                                                     |
| 12  | Audio — X11/ALSA Full Integration          | `platforms/x11/audio.c` production-quality; replaces L09 minimal init                                                 | X11 audio: robust, latency-modelled, recovers from underrun | `snd_pcm_hw_params` (not `snd_pcm_set_params`) + `hw_buffer_size` + `snd_pcm_sw_params` (`start_threshold=1`) + `alloca.h` ordering; `PlatformAudioConfig` latency model + `platform_audio_get_samples_to_write` via `snd_pcm_avail_update`; `snd_pcm_writei` + `snd_pcm_recover` + re-fill silence after underrun + pre-fill silence on init                                          | `platforms/x11/audio.c` (new), `platform.h` (`PlatformAudioConfig` full)                              |
| 13  | Music Sequencer                            | `ToneGenerator` + volume ramping; `MusicSequencer`; `midi_to_freq`; two-loop architecture                             | Background chiptune melody on both backends                 | `ToneGenerator` + `current_volume` ramp (`ramp_speed = 0.001f`, why ramp prevents click); `MusicSequencer` + `step_timer` advancement + `midi_to_freq(note) = 440 × 2^((note−69)/12)`; two-loop architecture: `game_audio_update` (game time) ↔ `game_get_audio_samples` (audio time)                                                                                                  | `game/audio_demo.c` updated, `utils/audio.h` finalized                                                |
| 14  | Platform Template Complete                 | Strip `game/demo.c` and `audio_demo.c`; ASSERT coverage audit; Stage B deviation notes; final build                   | Clean build on both backends; template ready to copy        | Template review + `game/demo.c` removal + `audio_demo.c` removal from build; ASSERT coverage audit; Stage B deviation notes for game courses                                                                                                                                                                                                                                           | All files finalized; `PLAN-TRACKER.md` updated                                                        |

---

## Concept Dependency Map

```
clang invocation → SOURCES var → build-dev.sh → --backend flag → -r / -d flags

InitWindow → BeginDrawing/EndDrawing → CloseWindow
XCreateWindow + XSizeHints (fixed, L02) → removed in L08 → letterbox math replaces it
XOpenDisplay → XCreateWindow → XPending drain → GLX context → glXSwapBuffers → XCloseDisplay
XInternAtom("WM_DELETE_WINDOW") → XSetWMProtocols → ClientMessage handler

Backbuffer struct → pitch (stride ≠ width×bpp) → GAME_RGB/RGBA → pixel addressing → draw_rect
glTexImage2D (GL_BGRA) + UpdateTexture → R↔B channel swap (L03)
draw_rect → clip-before-loop → pitch/4 indexing → draw_rect_blend
FONT_8X8 → BIT7-left mask → draw_char → draw_text → (snprintf used with draw_text in L08)

PlatformGameProps → platform_game_props_init → platform_game_props_free
STRINGIFY → TOSTRING → TITLE macro
platform_init → main loop → platform_get_time → delta_time → delta_time cap
platform.h contract → platform_get_input → platform_shutdown
demo_render (L03) → updated each lesson L03–L08 → removed L14

FrameTiming.frame_start → frame_timing_mark_work_done → work_seconds
work_seconds → frame_timing_sleep_until_target (coarse nanosleep + spin-wait)
frame_timing_sleep_until_target → frame_timing_end → total_seconds + sleep_seconds
FrameStats (DEBUG) → frame_count + missed_frames + min/max/avg

GameButtonState → ended_down + half_transition_count → UPDATE_BUTTON → prepare_input_frame
GameInput union → platform_swap_input_buffers (game/base.h, not platform.h)
DEBUG_TRAP → ASSERT → ASSERT_MSG

AudioOutputBuffer → samples_per_second + max_sample_count + is_initialized guard
interleaved stereo layout → audio_write_sample (format-agnostic helper)
phase accumulator → square wave → float mixing → audio_write_sample → int16_t clamped output
minimal Raylib init (L09) → full Raylib audio (L11, refactor)
minimal ALSA init / snd_pcm_set_params (L09) → full ALSA / snd_pcm_hw_params (L12, refactor)
SoundDef → SoundInstance → game_play_sound_at → fade_in/fade_out → pan_position
game_play_sound_at → ASSERT sound pool full
SetAudioStreamBufferSizeDefault → AUDIO_CHUNK_SIZE → LoadAudioStream → pre-fill → IsAudioStreamProcessed while-loop → UpdateAudioStream
snd_pcm_hw_params → hw_buffer_size → snd_pcm_sw_params (start_threshold=1) → snd_pcm_writei → snd_pcm_recover
PlatformAudioConfig → samples_per_frame → latency_samples → platform_audio_get_samples_to_write

ToneGenerator → current_volume ramp → target_volume
MusicSequencer → step_timer → current_step → midi_to_freq → ToneGenerator
game_audio_update (game time) → ToneGenerator.target_volume
game_get_audio_samples (audio time) → ToneGenerator.current_volume ramp
```

---

## Per-Lesson Skill Inventory Table

| Lesson | New concepts                                                                                                                                                                                                                                                              | Concepts re-used from prior lessons                                                   |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| 01     | `clang` invocation + linker flags; `build-dev.sh` + `SOURCES` var + `--backend` + `-r` + `-d`; `InitWindow` + `BeginDrawing/EndDrawing`                                                                                                                                   | —                                                                                     |
| 02     | `XCreateWindow` + `XMapWindow` + `XSizeHints` (fixed, temporary); GLX context init (`glXChooseFBConfig`, `glXCreateNewContext`, `glXMakeContextCurrent`) + `glXSwapBuffers`; `XPending` drain loop + `XInternAtom("WM_DELETE_WINDOW")` + `ClientMessage`                  | `build-dev.sh`, `SOURCES`                                                             |
| 03     | `Backbuffer` struct + `pitch` + pixel addressing; `GAME_RGB/RGBA` + color constants + R↔B channel swap (X11: `GL_BGRA`; Raylib: swap R and B); `glTexImage2D`/`glTexSubImage2D` + `UpdateTexture` + `game/demo.c` intro (`demo_render`)                                   | both backends                                                                         |
| 04     | `MIN`/`MAX`/`CLAMP`/`ABS`; `draw_rect` clip-before-loop + `pitch/4` row pointer; `draw_rect_blend` alpha composite formula                                                                                                                                                | `Backbuffer`, pixel addressing                                                        |
| 05     | `PlatformGameProps` + `GameProps` + `platform_game_props_init/free`; 4-function platform API; `STRINGIFY`/`TOSTRING`/`TITLE`                                                                                                                                              | both backends, `Backbuffer`, `build-dev.sh`                                           |
| 06     | `FONT_8X8[128][8]`; BIT7-left mask `(0x80>>col)`; `draw_char` + `draw_text`                                                                                                                                                                                               | `Backbuffer`, `draw_rect`, `PlatformGameProps`                                        |
| 07     | `GameButtonState` + `UPDATE_BUTTON`; `GameInput` union + `prepare_input_frame` + `platform_swap_input_buffers` (in `game/base.h`); `DEBUG_TRAP`/`ASSERT`/`ASSERT_MSG`                                                                                                     | platform contract, `PlatformGameProps`                                                |
| 08     | delta-time pattern + `platform_get_time` + cap; `FrameTiming` (coarse `nanosleep` + spin-wait) + `FrameStats` + `snprintf` for FPS counter; VSync extension detection + letterbox math + `XSizeHints` removed                                                             | `PlatformGameProps`, both backends, `GameInput`, `demo_render`, `draw_text`           |
| 09     | `AudioOutputBuffer` + interleaved stereo + `max_sample_count` + `is_initialized` guard; phase accumulator + square wave + float mixing + `audio_write_sample` + `GameAudioState`; minimal Raylib audio init + minimal ALSA init (`snd_pcm_set_params`)                    | `platform.h`, `math.h`                                                                |
| 10     | `SoundDef` + `SoundInstance` + `game_play_sound_at` (free-slot + steal-oldest); fade-in/fade-out click prevention; stereo panning linear law + frequency slide per-sample                                                                                                 | `AudioOutputBuffer`, `GameAudioState`, `ASSERT`, `audio_write_sample`                 |
| 11     | `SetAudioStreamBufferSizeDefault` + `AUDIO_CHUNK_SIZE`; pre-fill 2 silent buffers on init; `IsAudioStreamProcessed` `while`-loop + `UpdateAudioStream`                                                                                                                    | `AudioOutputBuffer`, Raylib backend, `PlatformAudioConfig`                            |
| 12     | `snd_pcm_hw_params` + `hw_buffer_size` + `snd_pcm_sw_params` (`start_threshold=1`) + `alloca.h` ordering; `PlatformAudioConfig` latency model + `platform_audio_get_samples_to_write` via `snd_pcm_avail_update`; `snd_pcm_writei` + `snd_pcm_recover` + pre-fill silence | `AudioOutputBuffer`, X11 backend, `PlatformAudioConfig`                               |
| 13     | `ToneGenerator` + `current_volume` ramp; `MusicSequencer` + `step_timer` + `midi_to_freq`; two-loop architecture (`game_audio_update` game time ↔ `game_get_audio_samples` audio time)                                                                                    | `game_get_audio_samples`, `game_audio_update`, `GameAudioState`, `audio_write_sample` |
| 14     | Template review: `game/demo.c` + `audio_demo.c` removal from build; ASSERT coverage audit; Stage B deviation notes for game courses                                                                                                                                       | All prior concepts                                                                    |

**Coverage check:** Every item in the topic inventory maps to exactly one "New concepts" cell above. Items introduced in the lesson table match items in the skill inventory. The `R↔B channel swap` is taught once in L03 and not re-listed in L14 (L14 only reviews and audits; it introduces nothing new).

---

## Deviations from `course-builder.md` Standard Layout

| Deviation                                                              | Reason                                                                                                                                                                                                                                                                                                                                     |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| No `game.c` / `game_update` / `game_render`                            | This course teaches only the platform layer; game logic is the first thing each game course adds                                                                                                                                                                                                                                           |
| `game/base.h` only (no `game/main.h`)                                  | Platform course provides the `GameInput` template; game courses add `GameState` in their own `main.h`                                                                                                                                                                                                                                      |
| `game/demo.c` (visual demos, L03–L08)                                  | Provides a single `demo_render()` called by both backends, avoiding duplicating demo rendering in each backend's `main.c`. Removed in L14.                                                                                                                                                                                                 |
| `game/audio_demo.c` (audio demos, L09–L13)                             | Provides an observable audio test without introducing `GameState`; removed when game courses copy the template                                                                                                                                                                                                                             |
| `platform_swap_input_buffers` lives in `game/base.h`, not `platform.h` | The function is pure game-layer logic (swaps two `GameInput` structs); placing it in `platform.h` would blur the architecture boundary. It is always paired with `prepare_input_frame` and belongs alongside it.                                                                                                                           |
| Minimal audio init in L09 (replaced in L11/L12)                        | Students need audible output in L09 to verify the PCM mixer works. A simplified init (`snd_pcm_set_params`, simple `if (IsAudioStreamProcessed)`) achieves this. L11/L12 refactor to production quality — the contrast is a teaching moment about latency, underrun recovery, and chunked writes.                                          |
| Frame timing as inline code (not a separate `frame-timing.h`)          | The DE100 engine separates `frame-timing.{c,h}` and `frame-stats.{c,h}` for reuse across backends. The platform course inlines this logic directly in each backend's `main.c` to keep the file count minimal for students. The structure (`FrameTiming` struct, coarse sleep + spin-wait, `FrameStats`) matches the DE100 pattern exactly. |
| Lesson 14 is a review/strip/audit lesson                               | Platform courses need an explicit "template finalization" pass; game courses skip this because they copy an already-finalized template                                                                                                                                                                                                     |

---

## How Game Courses Use This Template

When starting a new game course, copy `course/src/` from this directory and make these adaptations:

| File                      | What to change                                                                                                                |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `build-dev.sh`            | Game name in binary output path and title                                                                                     |
| `platform.h`              | `TITLE` macro (game name + backend identifier)                                                                                |
| `platforms/x11/main.c`    | `GAME_W`, `GAME_H` dimensions; key bindings in `platform_get_input`                                                           |
| `platforms/raylib/main.c` | Same dimensions; same key bindings                                                                                            |
| `game/base.h`             | `BUTTON_COUNT`; rename `GameInput` named button fields for this game                                                          |
| `game/demo.c`             | **Delete** — replace with `game/main.c` containing `game_init`, `game_update`, `game_render`                                  |
| `game/audio_demo.c`       | **Delete** — replace with `game/audio.c` containing game-specific `game_get_audio_samples`, `game_audio_update`, `SOUND_DEFS` |

Game course lessons start **after** copying the template. Lesson 1 of a game course is the first game-specific topic (e.g., "Game Structure + GameState" or "Grid Layout"). When a lesson requires a platform adaptation (e.g., adding a new key), the lesson shows only the changed lines and notes:

> _This is a platform-layer change. See Platform Foundation Course — Lesson 07 for the full `GameInput` and `UPDATE_BUTTON` explanation._
