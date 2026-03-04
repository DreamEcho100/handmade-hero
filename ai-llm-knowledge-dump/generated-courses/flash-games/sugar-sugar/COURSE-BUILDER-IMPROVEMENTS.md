# COURSE-BUILDER-IMPROVEMENTS.md
# Sugar, Sugar — Suggested Improvements to the Course-Builder Framework

The course-builder prompt (`course-builder.md`) defines conventions that have
served the Tetris, Snake, Asteroids, and Frogger courses well.  Building
Sugar Sugar revealed a handful of new patterns that should be folded back
into the framework.

---

## 1. Cellular-automaton / falling-sand exception for Y-down coordinates

**The issue:**
`course-builder.md` mandates a Y-up, metres-based coordinate system for game
logic.  This works perfectly for Tetris, Snake, Asteroids, and Frogger because
those games are driven by entity positions measured in tiles or world units.

Falling-sand / cellular-automaton games are fundamentally different: each grain
IS a pixel.  Grain `x`/`y` are pixel indices, not world units.  Gravity is
`+500 px/s²` (positive = down, screen direction), and every collision check is
`IS_SOLID(ix, iy)` directly into a `uint8_t pixels[W*H]` array.

Converting to Y-up would require `screen_y = CANVAS_H - (int)grain.y` at
every array index — pure noise with zero conceptual benefit.

**Suggested addition to course-builder.md:**
> **Y-down exception:** courses whose primary simulation domain is a 2-D
> pixel grid (falling sand, cellular automata, tilemaps) may use Y-down
> screen-pixel coordinates throughout.  Document the exception prominently
> in `PLAN.md` (see Sugar Sugar's "Intentional exceptions" table) and in the
> first relevant lesson (Lesson 4 in this course).

---

## 2. Struct-of-Arrays (SoA) as a named pattern

**The issue:**
The course-builder encourages the `GameState` monolith and platform/game
separation, but says nothing about internal data layout.  Every previous course
used a simple single-struct particle or entity.  Sugar Sugar's `GrainPool` is
the first SoA, and it introduces a real-world performance trade-off that the
student should understand explicitly.

**Suggested addition:**
> **Data layout note:** When a simulation iterates N ≥ 1024 particles and only
> a subset of fields are accessed per loop, consider a Struct-of-Arrays (SoA)
> layout:
>
>   AoS: `struct Grain { float x, y, vx, vy; uint8_t color, active; };`
>   SoA: `struct GrainPool { float x[N], y[N], vx[N], vy[N]; uint8_t color[N], active[N]; };`
>
> SoA improves cache utilisation when tight inner loops only touch a few
> fields.  Always document the trade-off in the relevant lesson.

---

## 3. `platform_play_sound` / `platform_play_music` callback pattern

**The issue:**
The previous courses (Tetris through Frogger) used a custom software audio
mixer built on ALSA or Raylib's raw `AudioStream` API.  That mixer is
powerful for continuously-generated waveforms (Asteroids thrust engine),
but it is substantial code to introduce in a course focused on something else.

Sugar Sugar fires audio on discrete events only (cup fill, level complete,
background music loop).  For this use case, Raylib's higher-level
`LoadSound`/`PlaySound`/`LoadMusicStream`/`UpdateMusicStream` API is simpler,
safer, and more idiomatic.

**Suggested addition to course-builder.md:**
> **Audio: choose the right abstraction tier.**
>
> | Use case | Recommended API |
> |----------|----------------|
> | Continuously-generated waveforms (engine hum, synth) | Custom ALSA/Raylib `AudioStream` mixer |
> | Discrete events + music loop | `LoadSound` / `PlaySound` / `LoadMusicStream` |
>
> Document the chosen tier in Lesson 15 (or the audio lesson) and explain
> why you didn't use the other.

---

## 4. Letterbox from Lesson 1 — both backends

**The issue:**
In the Asteroids and Frogger courses, letterbox centering was added only after
window-resize bugs were reported.  Starting a course without letterbox means
the student writes code that breaks on first resize.

**What we did in Sugar Sugar:**
- X11 backend: `XSizeHints` lock the window to `CANVAS_W × CANVAS_H` (non-resizable)
- Raylib backend: `DrawTextureEx` with `fminf(win_w/CANVAS_W, win_h/CANVAS_H)` scale and `(win_w-dst_w)/2` offset

**Suggested rule for course-builder.md:**
> **Letterbox from Day 1.**  Every course backend MUST handle window size ≠
> canvas size from the first lesson.
>
> - X11: lock the window to `CANVAS_W × CANVAS_H` via `XSizeHints` (simplest
>   correct approach for single-canvas games).
> - Raylib: `DrawTextureEx(tex, offset, 0, scale, WHITE)` where
>   `scale = fminf(win_w/W, win_h/H)` and `offset = ((win-dst)/2, (win-dst)/2)`.

---

## 5. Pixel-format bug guard for Raylib

**The issue:**
Our `0xAARRGGBB` pixel format is stored little-endian as bytes `[B, G, R, A]`.
Raylib's `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` reads bytes as `[R, G, B, A]`,
so red and blue channels are swapped.  This is a silent, hard-to-spot bug:
the game runs but all colours are wrong.

The reference `main_raylib.c` had a TODO comment about this.  Our course
version fixed it in `platform_display_backbuffer` with a double-pass
in-place channel swap before/after `UpdateTexture`.

**Suggested addition to course-builder.md:**
> **Raylib pixel-format guard.**  Before `UpdateTexture`, verify that the
> channel order matches.  If using `0xAARRGGBB` (ARGB), add a R↔B channel
> swap:
> ```c
> /* Swap R and B: 0xAARRGGBB → Raylib RGBA 0xAABBGGRR */
> for (int i = 0; i < W*H; i++) {
>     uint32_t c = px[i];
>     px[i] = (c & 0xFF00FF00u) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
> }
> UpdateTexture(tex, px);
> /* Swap back so game state is unchanged */
> for (int i = 0; i < W*H; i++) { /* same swap */ }
> ```
> Alternatively, invert the `GAME_RGB` macro to emit `0xAABBGGRR` and drop
> the swap entirely.  Document which approach the course uses.

---

## 6. Designated initialisers in level data files

**The issue:**
Levels 1–30 are defined in `levels.c` using C99 designated initialisers:
```c
LevelDef g_levels[TOTAL_LEVELS] = {
    [0] = { .emitter_count = 1, .emitters = { EMITTER(320, 20, 240) }, ... },
    [1] = { ... },
};
```
This pattern is excellent but was not mentioned in the course-builder prompt.
Unspecified fields are automatically zero-initialised, which eliminates
hundreds of lines of boilerplate `.filter_count = 0, .obstacle_count = 0`.

**Suggested addition to course-builder.md:**
> **Data files and designated initialisers.**  Separate level/map/config data
> into their own `.c` file (e.g. `levels.c`).  Use C99 designated initialisers
> so unspecified fields are zero-initialised automatically.  Declare the array
> `extern` in the shared header.  This approach:
> - Keeps data readable (only non-default fields appear)
> - Speeds incremental compilation (changing data doesn't rebuild game logic)
> - Works with helper macros (`EMITTER(...)`, `CUP(...)`, `OBS(...)`) for
>   terseness

---

## 7. `build-dev.sh` must always output to `./build/game` and `-r` means run

**Problem found:** The initial build script used a variable output path and
mixed up the `-r` (run) flag semantics. The dev script should:
- Always produce `./build/game` (never `./build/sugar_sugar_dbg` etc.)
- Always build with `-g -O0 -DDEBUG -fsanitize=address,undefined`
- Accept `--backend=x11|raylib` (default: raylib)
- Accept `-r` to run the binary immediately after a successful build

**Mitigation for future courses:**
- [ ] `build-dev.sh` template: hardcode output as `./build/game`, always debug flags
- [ ] `-r` flag sets `RUN_AFTER_BUILD=1`; add ` && ./build/game` at the end if set
- [ ] Test both `bash build-dev.sh` and `bash build-dev.sh --backend=x11 -r` before shipping

---

## 8. Audio must be included from Day 1 — not retrofitted

**Problem found:** The initial course had `platform_play_sound()` stubs in
`platform.h` with no implementation. After user feedback, the entire audio
architecture was redesigned three times. Every redesign broke existing code.

**Root cause:** Audio was treated as optional/deferred. Retrofitting a
procedural audio mixer into existing platform code is significantly harder
than building it correctly from the start.

**Mitigation for future courses:**
- [ ] Audio section of course-builder.md is mandatory — never mark "TODO later"
- [ ] Create `src/utils/audio.h`, `src/audio.c`, and wire `GameAudioState audio` in `GameState` BEFORE writing game logic
- [ ] Confirm both backends produce audible output on first compile (even with a simple test tone)
- [ ] Do NOT use `platform_play_sound()` callbacks — game layer calls `game_play_sound()` directly

---

## 9. Raylib: `PlaySound()` bypasses the PCM mixer entirely

**Problem found:** Initial Raylib backend used `PlaySound(LoadSoundFromWave(...))`
which hands audio to Raylib's internal scheduler. This bypasses
`game_get_audio_samples()` entirely and produces unreliable timing.

**Mitigation for future courses:**
- [ ] Never call `PlaySound`, `LoadSound`, `LoadSoundFromWave` in the platform backend
- [ ] Use `AudioStream` + `IsAudioStreamProcessed` loop exclusively
- [ ] Search for `PlaySound(` at build time (grep in CI or build script) as a guard

---

## 10. Raylib `AudioStream` buffer size — `SetAudioStreamBufferSizeDefault` and `UpdateAudioStream` MUST use the same count

**Problem found:** After adding `SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME * 2)`
but keeping `UpdateAudioStream(..., SAMPLES_PER_FRAME)`, Raylib's ring buffer
write pointer drifted — causing stuttering every ~1 second.

**Rule:**
```c
#define AUDIO_CHUNK_SIZE 2048   /* the ONE constant for everything */
SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);          /* before LoadAudioStream */
UpdateAudioStream(stream, buf.samples, AUDIO_CHUNK_SIZE);   /* in the fill loop */
AudioOutputBuffer.sample_count = AUDIO_CHUNK_SIZE;          /* passed to game */
```

**Mitigation for future courses:**
- [ ] Define a single `AUDIO_CHUNK_SIZE` constant ≥ 2048 at 44100 Hz
- [ ] Use it in exactly three places: `SetAudioStreamBufferSizeDefault`, `UpdateAudioStream`, and `AudioOutputBuffer.sample_count`
- [ ] Never derive buffer size from `samples_per_frame * 2` separately in different places

---

## 11. X11/ALSA: `snd_pcm_set_params` is unreliable — use `snd_pcm_hw_params`

**Problem found:** `snd_pcm_set_params(46ms_latency)` created a ~2028-sample
hardware buffer. Pre-filling 2048 frames then checking `avail < 2048` before
every write means `avail` is almost always < 2048 → no audio ever written
after startup → permanent silence.

**Fix:** Use `snd_pcm_hw_params` (explicit hw parameter API) and calculate the
write amount with `snd_pcm_delay` (Casey's model):
```c
snd_pcm_delay(g_pcm, &delay);
to_write = (LATENCY_TARGET + SAFETY_MARGIN) - (int)delay;
to_write = clamp(to_write, 0, avail);
```

**Mitigation for future courses:**
- [ ] Always use `snd_pcm_hw_params` for ALSA init (never `snd_pcm_set_params`)
- [ ] Calculate write amount per-frame with `snd_pcm_delay`, not `snd_pcm_avail_update`
- [ ] Set `LATENCY_TARGET ≥ FRAMES_PER_FRAME * 4` (≥66ms at 60Hz/44100Hz)
- [ ] Follow `games/tetris/src/main_x11.c` — `platform_audio_get_samples_to_write()` as canonical reference

---

## 12. X11/ALSA: `#include <alloca.h>` must precede `<alsa/asoundlib.h>`

**Problem found:** `snd_pcm_hw_params_alloca` and `snd_pcm_sw_params_alloca`
expand to `alloca()`. Clang with `-Wall` produces "call to undeclared library
function" unless `<alloca.h>` is included explicitly.

**Mitigation for future courses:**
- [ ] Always add `#include <alloca.h>` immediately before `#include <alsa/asoundlib.h>`

---

## 13. X11/ALSA: `start_threshold` must be set to `1` or startup is silent

**Problem found:** ALSA's default `start_threshold` equals the hardware buffer
capacity — ALSA waits until the entire buffer is full before starting playback.
At ~4× latency target this causes >0.5 s of silence at startup.

**Fix:**
```c
snd_pcm_sw_params_t *sw;
snd_pcm_sw_params_alloca(&sw);
snd_pcm_sw_params_current(g_pcm, sw);    /* MUST call current() first */
snd_pcm_sw_params_set_start_threshold(g_pcm, sw, 1);
snd_pcm_sw_params(g_pcm, sw);
```

**Mitigation for future courses:**
- [ ] Always set `start_threshold = 1` after `snd_pcm_hw_params()`
- [ ] Never skip `snd_pcm_sw_params_current()` — without it, unloaded fields retain undefined values

---

## 14. X11/ALSA: underrun recovery must re-pre-fill or clicks occur

**Problem found:** After an ALSA underrun (`-EPIPE`), calling `snd_pcm_recover`
alone leaves the hardware buffer empty. When the next write arrives there is a
brief silence gap at the recovery boundary → audible click.

**Fix:** After `snd_pcm_recover`, immediately pre-fill with silence up to the
latency target before the next audio write:
```c
if (written < 0) {
    int rerr = snd_pcm_recover(g_pcm, (int)written, 0);
    if (rerr >= 0) {
        memset(buf, 0, LATENCY_TARGET * 2 * sizeof(int16_t));
        snd_pcm_writei(g_pcm, buf, LATENCY_TARGET);
    }
}
```

**Mitigation for future courses:**
- [ ] Always re-pre-fill silence after `snd_pcm_recover` in the write path

---

## 15. Utility headers belong in `utils/` — not at `src/` root

**Problem found:** `font.h` and `sounds.h` were placed at the same level as
`game.c/game.h`. This mixes reusable helpers with game-specific logic and
contradicts course-builder.md's stated directory structure.

**Additional problem:** `sounds.h` was a redundant re-export of `utils/audio.h`.
Any file that included `game.h` already had `utils/audio.h` transitively —
`sounds.h` added no value and created confusion.

**Mitigation for future courses:**
- [ ] Put ALL utility/helper headers in `utils/`: `font.h`, `math.h`, `draw-shapes.h`, etc.
- [ ] `game.h` includes `utils/audio.h` directly — never create re-export shim headers
- [ ] Validate `src/` root only contains: `game.c`, `game.h`, `audio.c`, `levels.c`, `platform.h`, `main_*.c`

---

## 16. `game_audio_init` resets all audio state — triggers after `game_init` are wiped

**Problem found:** `game_init` calls `change_phase(PHASE_TITLE)` which calls
`game_music_play_title()`, setting `title_seq.is_playing = 1`.  But
`platform_audio_init` (called after `game_init`) calls `game_audio_init` which
begins with `memset(audio, 0, sizeof(*audio))` — erasing `is_playing`.

Result: title music never plays on launch. Only after navigating away and back
does the sequencer retrigger correctly.

**Fix:**
```c
game_init(&state, &bb);
platform_audio_init(&state, SAMPLE_RATE);   /* resets audio via game_audio_init */
/* Retrigger initial phase music AFTER audio is fully initialised */
game_music_play_title(&state.audio);
```

**Rule:** After `platform_audio_init`, always explicitly start the music for
the initial game phase. Do not rely on `game_init` to trigger audio (it runs
before the platform audio device is open).

**Mitigation for future courses:**
- [ ] In every `main()`: call `game_music_play_XXX` after `platform_audio_init`
- [ ] Or: move `game_audio_init` into `game_init` directly and have `platform_audio_init` skip it, but only after `game_init` has run
- [ ] Document this ordering rule in `platform.h` comments

---

## 17. Procedural audio sound design process

**Problem found:** Sound definitions were tuned iteratively across 6+ user
feedback rounds:
1. Moving/rotate sounds too spammy → added debounce threshold
2. Moving/rotate too quiet → raised volume
3. Music too quiet (below 0.50 music_volume floor)
4. SFX volumes too low initially (started at 0.45, needed 0.75+)
5. Sounds not fitting the game's mood (candy/sugar theme)

**Lessons:**
- Start ALL volumes at 1.0 and tune DOWN after confirming audibility
- Design SFX with the game's theme in mind before writing code (note frequencies, envelope character)
- Use a pentatonic scale for background music — avoids dissonance without music theory expertise
- For a "candy" game: higher frequencies (660–1320 Hz), short attack, medium decay feel sweet

**Mitigation for future courses:**
- [ ] Set `master_volume = 1.0`, `sfx_volume = 1.0`, `music_volume = 0.6` in `game_audio_init`
- [ ] Set every `SoundDef.volume = 1.0` initially
- [ ] Match SFX frequency range to the game's visual style (puzzle/candy = high; action = low-mid)
- [ ] Document the musical scale choice in the audio lesson
