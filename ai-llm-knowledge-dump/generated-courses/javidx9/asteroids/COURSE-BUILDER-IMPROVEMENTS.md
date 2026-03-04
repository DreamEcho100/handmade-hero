# COURSE-BUILDER-IMPROVEMENTS.md
# Asteroids Course — Deviations, Upgrades, and Retrospective

This document has two parts:

1. **Part I — Engineering Deviations** — Every deliberate deviation from the
   reference source and why it makes the course better.

2. **Part II — Process Retrospective** — Every issue that surfaced during the
   build and post-build phases, analysed at a generic/meta level, with
   mitigations that apply to any future course built with this framework.

---

## Summary Table — Part I: Engineering Deviations from Reference Source

| # | Area | Reference Source | Course Upgrade | Lesson |
|---|------|-----------------|----------------|--------|
| 1 | Input model | Single `GameInput`, 1-arg prepare | Double-buffered `inputs[2]`; 2-arg `prepare_input_frame` copies `ended_down` | 04 |
| 2 | Input union | No union | `union { buttons[BUTTON_COUNT]; struct { up, left, right, fire, quit; }; }` | 04 |
| 3 | Rotation math | Hardcoded vertex transform | Explicit 2-D rotation derivation from nose vertex `(0, -r)` | 06 |
| 4 | Object pool | Fixed index loops + manual removal | `compact_pool` swap-with-tail; inactive objects collected after all loops | 09 |
| 5 | Bullet lifetime | No timer | `size` field repurposed as countdown; decremented by `dt` | 10 |
| 6 | Backbuffer | Implicit pitch | `{pixels, width, height, pitch}` — teaches stride | 01 |
| 7 | Font | Lookup by glyph index | `FONT_8X8[128][8]` indexed by ASCII code | 08 |
| 8 | Debug tools | None | `DEBUG_TRAP()` / `ASSERT(expr)` under `#ifdef DEBUG` | 01 |
| 9 | Build system | Two separate scripts | Unified `build-dev.sh --backend=x11\|raylib` | 01 |
| 10 | Audio | No audio | Full procedural mixer: square/noise, frequency slide, spatial pan, ENVELOPE_TRAPEZOID looping sounds, looped music, all sound events | 12 |
| 11 | Window resize | Fixed size | Letterbox centering for both backends from day 1 | 01 |

---

## Part I: Engineering Deviations

### 1. Double-Buffered Input

**Reference:** single `GameInput`; `prepare_input_frame` clears transitions only.

**Upgrade:** `inputs[2]` double-buffer; `prepare_input_frame(old, current)` copies
`ended_down` so the game sees clean `pressed`/`released` edges every frame.

**Why:** teaches the concept of "what was vs what is" — a pattern that
generalises to any per-frame state.

---

### 2. Explicit 2-D Rotation Derivation

**Reference:** nose position hardcoded or computed ad-hoc.

**Upgrade:** derive from first principles: nose local vertex `(0, -r)`, rotation
matrix applied → `(+r·sin θ, −r·cos θ)`.  Thrust direction = unit nose vector =
`(+sin θ, −cos θ)`.

**Why:** students see that the sign on `sin`/`cos` follows from the rotation matrix,
not intuition.  Prevents the "flip the sign until it works" instinct.

---

### 3. `compact_pool` — Swap-With-Tail Removal

**Reference:** inline `active = 0` during loops, potential double-processing.

**Upgrade:** mark inactive during loops; call `compact_pool` once after **all**
loops; swap-with-tail keeps indices dense.

**Why:** teaches safe pool mutation.  Removing during iteration causes skipped
or double-processed elements when the swapped element is at the current index.

---

### 4. Procedural Audio Engine

**Reference:** no audio.

**Upgrade:** software PCM mixer — `SoundInstance` pool, additive square/noise
synthesis, amplitude envelope, frequency slide, spatial panning, looped music
(gameplay drone + restart jingle), per-event SFX for all game events.

**Why:** audio requires no external assets, stays fully in C, and teaches
fundamental digital audio concepts applicable to any platform.

---

## Part II: Process Retrospective

These lessons are written in generic terms.  Each issue is stated as a category
of mistake, why it is easy to make, and how to prevent it in any future course.

---

### Issue 1 — Directional/Sign Bugs From Intuition, Not Derivation

**What happened:** the thrust acceleration and bullet velocity were given the
wrong sign.  The ship moved in the wrong direction after rotation, and bullets
flew in the opposite direction.

**Root cause:** the sign was guessed from "feels right" intuition rather than
derived from the coordinate system and rotation matrix.  `dx = -sin(a) * speed`
vs `dx = +sin(a) * speed` looks plausible either way without a derivation.

**Category:** *speculative physics / sign-based bugs*.

**Mitigation:**
- For any vector derived from a rotation angle, always write out the 2×2
  rotation matrix explicitly in a comment or diagram first.
- State the coordinate system convention (Y-up or Y-down, clockwise or
  counter-clockwise positive angle) at the top of the relevant source file.
- Add a trivial manual test: angle=0 should move the object in the expected
  "forward" direction; verify this before building anything else on top.
- Never write `dx = ±ca * speed` or `dx = ±sa * speed` from memory.

---

### Issue 2 — Audio Volumes Inaudible by Default

**What happened:** all sound volumes were set to low "safe" values
(0.10–0.18 × a master of 0.8).  The result was effectively silence.  The user
reported no audio on three separate occasions before root-cause was identified.

**Root cause:** the developer defaulted to "quiet" to avoid annoyance, without
verifying audibility on a fresh run with headphones removed.

**Category:** *silent-default calibration error*.

**Mitigation:**
- Set every new sound to **maximum audible volume** (0.7–1.0) when first
  writing its `SoundDef`.  Tune down later after confirming it is heard.
- Set `master_volume` to 1.0 by default.  It is a multiplier on top of
  individual volumes; starting below 1.0 quietly loses headroom for no reason.
- As a build-time sanity check: run the game for 5 seconds immediately after
  adding any sound; confirm the sound is audible before committing.
- Document the effective loudness of each sound in `SOUND_DEFS` comments:
  `// ≈ −10 dB (loud)` or `// ≈ −25 dB (quiet fx)`.

---

### Issue 3 — Sound Pool Overflow From Undersized `MAX_SIMULTANEOUS_SOUNDS`

**What happened:** music + thrust + rotate + multiple explosions exceeded 8 slots.
Sounds were silently dropped; the pool filled up and new instances were rejected.

**Root cause:** `MAX_SIMULTANEOUS_SOUNDS = 8` was set from a per-event estimate
without accounting for simultaneous looping sounds (music, continuous thrust,
continuous rotate) layered on top of event sounds.

**Category:** *resource-pool undersizing*.

**Mitigation:**
- When defining the pool size, enumerate ALL sounds that can be active at the
  same time at the game's busiest moment.  For any looping sound, count it as
  always occupying a slot.
- Add an assertion or diagnostic counter: `ASSERT(pool_count < MAX)` at
  insertion time.  Silent rejection masks bugs for too long.
- Default to 16 or higher; the cost is negligible (each slot is ~32 bytes).

---

### Issue 4 — Raylib Audio Ring-Buffer Overflow (Unconditional Push)

**What happened (first attempt):** audio was gated behind `IsAudioStreamProcessed`
but `game_get_audio_samples` was not called when the gate was closed.
`samples_remaining` stalled; looping sounds re-triggered when the gate re-opened
→ "chopped echo" artifact.

**What happened (second attempt, over-corrected):** removed the gate entirely;
called `UpdateAudioStream` unconditionally every game frame with a 735-frame
buffer.  Raylib's `UpdateAudioStream` silently skips the write when both
internal sub-buffers are still full (`"Buffer not processed"` warning).
Skipped writes drifted the ring-buffer write-pointer ahead of the read-pointer
→ **wavy/echoing audio with a slight pitch shift**.

**Root cause:** the two symptoms (stale echo, wavy echo) look superficially
similar but have opposite causes.  Over-correcting from one to the other
produced the second bug.

**Category:** *platform streaming API misuse; iterative misdiagnosis*.

**Correct pattern:**
```c
// SAMPLES_PER_FRAME = 2048  (≈ 46 ms — large enough that timing jitter
//                             never causes back-to-back gate closures)
if (IsAudioStreamProcessed(stream)) {
    game_get_audio_samples(&state, &buf);   // advance state + generate PCM
    UpdateAudioStream(stream, buf.samples, SAMPLES_PER_FRAME);
}
```

**Rules:**
1. **Only generate PCM when you are about to push it.**  Generation and push
   are one atomic operation from the platform's perspective.
2. **Gate BOTH generate and push** behind `IsAudioStreamProcessed`.
   Never generate-always and push-sometimes, or vice versa.
3. **Use a larger chunk size** (2048 frames, not 735).  735 frames ≈ 1 render
   frame at 60 fps; any timing jitter causes consecutive pushes before
   consumption.  2048 frames ≈ 46 ms gives comfortable headroom.
4. **Test immediately with `SetAudioStreamBufferSizeDefault`** before
   `LoadAudioStream`.  Without it, the ring buffer uses Raylib's large default,
   making `IsAudioStreamProcessed` return false for many frames in a row.

**Mitigation:**
- Write the audio update once as a well-commented canonical block and copy it
  verbatim into every Raylib course.  Do not rederive it from scratch each time.
- Test audio on Raylib with a repeating 440 Hz tone for 10 seconds.  Any wavy
  pitch or echo indicates ring-buffer overflow.

---

### Issue 5 — ALSA Silent Startup (`start_threshold`)

**What happened:** the game started without sound for 0.5 seconds.  ALSA waited
for its default start threshold (= the ring buffer capacity) to fill before
beginning playback.

**Root cause:** ALSA's default `start_threshold` is the size of the hardware
buffer.  The software mixer writes one small period at a time; the buffer fills
slowly, causing an initial silence.

**Category:** *platform-specific default that contradicts intended behaviour*.

**Mitigation:**
- Always set `start_threshold = 1` for any ALSA PCM opened for playback.
  Never skip the `snd_pcm_sw_params` block.
- Copy this block verbatim from a tested template:
  ```c
  snd_pcm_sw_params_t *sw;
  snd_pcm_sw_params_alloca(&sw);
  snd_pcm_sw_params_current(pcm, sw);   // load defaults FIRST
  snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
  snd_pcm_sw_params(pcm, sw);
  ```
  Note: `snd_pcm_sw_params_current` must be called before overriding any field.

---

### Issue 6 — Window Resize Not Supported From Day 1

**What happened:** initial builds used a fixed window; the letterbox viewport
code was added as a later fix.  This required touching three files across two
backends and updating several lessons.

**Root cause:** "we can add resize support later" is a common deferral that
becomes expensive when the rendering path is already established.

**Category:** *deferred polish that becomes a retrofit*.

**Mitigation:**
- For any game course using a fixed-resolution logical canvas, implement
  letterbox centering in the very first rendering lesson.
- Canonical snippet for X11:
  ```c
  // ConfigureNotify handler updates g_win_w / g_win_h.
  // display_backbuffer: scale = min(win_w/W, win_h/H);
  //   glViewport(0,0,win_w,win_h); glClear; glViewport(vx,vy,vw,vh);
  ```
- Canonical snippet for Raylib:
  ```c
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  // Each frame: compute sc = min(sw/W, sh/H); DrawTexturePro with dst rect.
  ```
- These are short, self-contained, and prevent the need for a later retrofit.

---

### Issue 7 — Lessons Written for One Backend Only

**What happened:** the first draft of every lesson described only the Raylib
implementation.  X11-specific setup (GLX, ALSA, `clock_gettime`, `XkbKeycodeToKeysym`,
`WM_DELETE_WINDOW`, `XkbSetDetectableAutoRepeat`) was entirely absent.  Three
lessons required significant rewrites to add X11 coverage.

**Root cause:** the Raylib backend is simpler and faster to write; X11 coverage
was deferred as "an exercise for the reader".

**Category:** *platform asymmetry in documentation*.

**Mitigation:**
- For any course with two backends, treat each lesson as having a required
  **Platform Coverage Checklist**:
  - [ ] Window creation / teardown
  - [ ] Frame timing
  - [ ] Input reading
  - [ ] Pixel/texture rendering
  - [ ] Audio push
  - [ ] Shutdown / cleanup
- Write at least one code example per section for each backend, or an explicit
  "Same as Raylib" note when they are identical.
- Draft the X11 section immediately after the Raylib section, before moving to
  the next lesson.  Never leave it for the end.

---

### Issue 8 — `memset(state, 0)` Destroying Audio Config on Restart

**What happened:** `asteroids_init` calls `memset(state, 0, sizeof(*state))` to
reset all game state on restart.  This also zeroed `state.audio` —
`samples_per_second`, `master_volume`, `sfx_volume` — causing silent audio after
the first restart.

**Root cause:** `memset` is a blunt instrument.  The game state and the audio
config live in the same struct, but have different lifetimes: game state resets
on every restart; audio config is set once at startup and must persist.

**Category:** *lifetime mismatch: reset vs persist*.

**Mitigation:**
- Before any `memset(state, 0, ...)` that spans a large struct, explicitly list
  what must NOT be zeroed.  Save those fields before the memset and restore after.
  ```c
  int   saved_sps    = state->audio.samples_per_second;
  float saved_master = state->audio.master_volume;
  int   saved_best   = state->best_score;
  memset(state, 0, sizeof(*state));
  state->audio.samples_per_second = saved_sps;
  state->audio.master_volume      = saved_master;
  state->best_score                = saved_best;
  ```
- Alternatively, separate persistent config from mutable game state:
  ```c
  typedef struct { GameState game; AudioConfig audio; int best_score; } AppState;
  memset(&app->game, 0, sizeof(app->game));  // only reset game sub-struct
  ```
- Add a comment above every `memset(state)` call listing the fields intentionally
  excluded.

---

### Issue 9 — `GL_BGRA` vs `GL_RGBA` Pixel Format Mismatch

**What happened (documented, avoided in this course):** the GAME_RGBA macro packs
bytes as `[R][G][B][A]` in memory.  OpenGL `GL_RGBA` reads them correctly.  Using
`GL_BGRA` swaps the red and blue channels → colours shift (e.g., cyan ↔ orange).

**Category:** *byte-order / pixel-format mismatch*.

**Mitigation:**
- Write a unit-test pixel: `GAME_RGBA(255, 0, 0, 255)` (pure red) into pixel [0].
  If the top-left pixel of the window appears red, the format is correct.
  If it appears blue, swap `GL_RGBA` ↔ `GL_BGRA`.
- Document the GAME_RGBA layout in the backbuffer lesson, including the byte
  order and the OpenGL constant used:
  ```
  GAME_RGBA: uint32 = 0xAABBGGRR (little-endian bytes: [R][G][B][A])
  OpenGL: GL_RGBA  (not GL_BGRA) — reads bytes in memory order
  ```

---

### Issue 10 — Looping SFX "Throbbing / Spammy / Buzzy" Due to Wrong Envelope and Wave Type

**What happened (first report — throbbing):** the thrust and rotate sounds were
described as "annoying / spammy".  Both used `ENVELOPE_FADEOUT` while being
re-triggered every ~100–300 ms.  The result:

| Sound | Problem |
|-------|---------|
| SOUND_THRUST (90 Hz, 300ms, FADEOUT) | Amplitude pulsed loud→quiet→loud at ~3 Hz — perceived as throbbing |
| SOUND_ROTATE (400 Hz, 100ms, FADEOUT) | 400 Hz square wave is harsh; 10 Hz loop rate felt like a dentist drill |

**Fix — `ENVELOPE_TRAPEZOID`:** 10% attack + 80% sustain + 10% decay; retrigger at
decay zone start → crossfade sums to 1.0, eliminating the amplitude pulse.

**What happened (second report — still not smooth):** after switching to
ENVELOPE_TRAPEZOID + lower frequencies, the sounds were still described as
"not smooth/linear".  The remaining issue: **WAVE_SQUARE** produces an abrupt
±1 polarity flip every half-period (at 80 Hz: every ~6 ms), perceived as a
buzzing texture even at low volume.  The crossfade also has a slight (~30%)
amplitude dip due to chunk-based audio timing.

**Root cause (second issue):** the wave type was still square.  Square waves
are "retro/digital" sounding — correct for short SFX, but always perceived as
harsh/buzzy when looped as a background sound.

**Category:** *wrong wave type for smooth looping sounds*.

**Final fix — `WAVE_TRIANGLE`:**

```c
// -1 at phase=0, +1 at phase=0.5, -1 at phase=1.0
s = 1.0f - fabsf(4.0f * inst->phase - 2.0f);
```

Continuous linear ramps — no abrupt discontinuities.  Produces a smooth, gentle
hum rather than a buzz.  Also reduce volumes to "smallish" (ambient feedback,
not prominent):

| Sound | Final params |
|-------|-------------|
| SOUND_THRUST | 120 Hz triangle, 1.0s, vol 0.75, TRAPEZOID |
| SOUND_ROTATE | 200 Hz triangle, 0.6s, vol 0.50, TRAPEZOID |

**What happened (third report — volume inaudible):** lowering volumes to
0.20/0.08 made them completely inaudible.  Additionally, the re-trigger loop
design (play short clip → retrigger when nearly expired) still felt unnatural
— the user wanted an engine-like sound: starts when key pressed, continues
while held, fades when released.

**Root cause (third issue):** the retrigger-loop pattern is inherently the
wrong abstraction for input-held sounds.  It produces N separate sound
instances over time, each starting/ending — not one continuous sound.
Also, very low volumes (< 0.10) are lost in the background drone noise floor.

**Final fix — `game_sustain_sound` (sustain-while-held):**

```c
// While key is down, call every game frame:
game_sustain_sound(&state->audio, SOUND_THRUST);
// Key released → stop calling → natural TRAPEZOID decay
```

The function:
1. If not playing → `game_play_sound` (triggers attack ramp = "spool up")
2. If playing → pins `samples_remaining ≥ total/4` (25% floor, keeps amplitude at 1.0)
3. Key released (no call) → remaining drains → decay zone fades 1→0 ("engine stop")

```
Key held:  [100ms attack] ──── [sustained @ full vol] ──── [pinned at 25%]
Released:  ← 150ms full → ← 100ms fade → silence
```

**Mitigation:**
- Any sound tied to a held key (movement, rotation, charge-up) should use
  `game_sustain_sound`, not `game_play_sound` in a retrigger loop.
- The retrigger-loop pattern (`game_is_X_playing` + re-fire) is only correct
  for sounds that should repeat regardless of whether the key is still down
  (e.g., automatic fire rate, background music).
- Use `WAVE_TRIANGLE` for ALL continuously looping sounds.  `WAVE_SQUARE` for
  short SFX (duration < 0.3s) only.
- **Minimum audible frequency: 100 Hz.**  Below ~80 Hz most laptop/small
  desktop speakers produce vibration, not tone — the sound appears silent even
  at full volume.  Always use ≥ 100 Hz for any sustained looping tone.
- Volume: 0.50–0.80 for looping sounds (≥ 0.50 or they disappear under
  the music drone).  Tune down after confirming audibility.

---

## Quick-Reference Checklist for Any Future Course

Before declaring Phase 2 (source files) complete, verify:

- [ ] All directional vectors derived from the rotation matrix, not guessed
- [ ] All sound volumes ≥ 0.5 × master_volume 1.0; play-tested with headphones off
- [ ] All continuous/looping sounds use `WAVE_TRIANGLE` + `ENVELOPE_TRAPEZOID`
      + `game_sustain_sound`; play-test by holding key 30s — smooth start,
      sustained at constant vol, natural fade on release, no buzz/throb
- [ ] Looping sound volumes ≥ 0.15 (don't disappear under music drone)
- [ ] `MAX_SIMULTANEOUS_SOUNDS` ≥ (looping sounds) + (max simultaneous events) + 4 headroom
- [ ] Raylib audio: `SAMPLES_PER_FRAME = 2048`, gated behind `IsAudioStreamProcessed`
- [ ] ALSA audio: `start_threshold = 1` set via sw_params
- [ ] Window resize / letterbox centering implemented on both backends
- [ ] `memset`-on-restart: persistent fields saved and restored
- [ ] OpenGL pixel format verified with a pure-red test pixel
- [ ] `platform_shutdown()` prevents terminal hang on window close

Before declaring Phase 3 (lessons) complete, verify:

- [ ] Every lesson section has code examples for BOTH X11 and Raylib
  (or an explicit "identical on both backends" note)
- [ ] Platform comparison table in at least: window, input, audio lessons
- [ ] Lesson 12 (audio) covers: ALSA setup, `start_threshold`, Raylib stream
  setup, correct gating pattern, chunk-size rationale

---

## Gaps Found During Sugar Sugar Course (Fixes Applied)

### Issue 7 — `build-dev.sh` Convention Mismatches

**Problem:** The initial Sugar Sugar `build-dev.sh` used wrong conventions:
- Output was `sugar_x11` / `sugar_raylib` / `sugar_x11_dbg` etc. instead of
  always `./build/game`
- `-r` flag was wired to "release mode" instead of "run after build"
- Default backend was X11 instead of Raylib
- The dev script had a release mode at all (it should always be debug)

**Fix applied:** Rewrote `build-dev.sh`:
- Output: always `./build/game` — no per-backend naming
- Always debug: `-O0 -g -DDEBUG -fsanitize=address,undefined` — no release mode
- `-r` / `--run`: run after build
- Default backend: raylib (matches `course-builder.md`)
- `mkdir -p build` before building

**Mitigation for future courses:** Add to Phase 1 checklist:
- [ ] `build-dev.sh` output is always `./build/game`
- [ ] Script is always-debug (no `-r` = release mode)
- [ ] `-r` flag means "run after build"
- [ ] `mkdir -p build` present
- [ ] Default backend is raylib

---

### Issue 8 — Missing `src/utils/audio.h` + `src/audio.c` Structure

**Problem:** The initial Sugar Sugar audio used `platform_play_sound()` —
a fire-and-forget call from the game to the platform. This violates the
`course-builder.md` architecture: the platform should never receive audio
intent from the game; instead the game synthesizes samples via
`game_get_audio_samples()` and the platform pulls them.

The result: no `GameAudioState`, no `SoundInstance` pool, no procedural
synthesis, no music sequencer — the entire audio subsystem was missing.

**Fix applied:**
- Created `src/utils/audio.h` with `GameAudioState`, `SoundInstance`,
  `ToneGenerator`, `MusicSequencer`, `AudioOutputBuffer`
- Created `src/audio.c` with `game_get_audio_samples()`, `game_play_sound()`,
  `game_audio_update()`, Sugar Sugar `SOUND_DEFS[]`, pentatonic music patterns
- Added `GameAudioState audio` to `GameState` in `game.h`
- Removed `platform_play_sound/music/stop_music` from `platform.h`
- Added `platform_audio_init/update/shutdown` to `platform.h` contract
- Wired audio calls in `change_phase()` and cup-fill event

**Mitigation for future courses:** Add to Phase 2 checklist:
- [ ] `src/utils/audio.h` created with canonical types
- [ ] `src/audio.c` created with `game_get_audio_samples()` sample loop
- [ ] `GameAudioState` embedded in `GameState` (NOT a global)
- [ ] `platform.h` has `platform_audio_init/update/shutdown`, NOT `platform_play_sound`
- [ ] `change_phase()` has audio switch statement at end
- [ ] Cup/item collection events trigger `game_play_sound()`
- [ ] `game_audio_update()` called once per game frame from `game_update()`

---

### Issue 9 — Raylib AudioStream Pattern Not Applied

**Problem:** The initial Raylib backend used `LoadSoundFromWave` + `PlaySound`
— Raylib takes ownership and plays when it wants. This causes wavy/echoing
artifacts because Raylib internally resamples and may double-buffer.

**Fix applied:** Full `AudioStream` rewrite:
- `SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME)` before `LoadAudioStream`
- `SAMPLES_PER_FRAME = 2048` (NOT 735) — prevents underruns from OS jitter
- Pre-fill 2 buffers with silence at startup
- Per-frame: `while (IsAudioStreamProcessed(stream)) { game_get_audio_samples; UpdateAudioStream }`

**Mitigation for future courses:** Add to Raylib backend checklist:
- [ ] `SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME)` called BEFORE `LoadAudioStream`
- [ ] `SAMPLES_PER_FRAME = 2048` (not 735)
- [ ] 2 silence pre-fills at startup
- [ ] `while (IsAudioStreamProcessed)` loop (not `if`) per frame
- [ ] Verify: no `PlaySound()` calls anywhere in codebase

---

### Issue 10 — Mouse Coordinates Not Transformed for Raylib Letterbox

**Problem:** `platform_get_input` used `GetMousePosition()` raw window coords.
When the window is resized, the canvas is letterboxed (scaled + centered).
Mouse clicks are off by the border amount and don't scale correctly.

**Fix applied:**
- `platform_display_backbuffer` computes and stores `g_scale`, `g_offset_x`,
  `g_offset_y` as globals each frame
- `platform_get_input` transforms: `cx = (pos.x - g_offset_x) / g_scale`
- Result clamped to `[0, CANVAS_W-1]` and `[0, CANVAS_H-1]`

**Mitigation for future courses:** Add to Raylib backend checklist:
- [ ] `g_scale`, `g_offset_x`, `g_offset_y` globals set in `platform_display_backbuffer`
- [ ] `platform_get_input` reads those globals to transform mouse coords
- [ ] Mouse coords clamped to canvas bounds

---

### Issue 11 — Missing `DEBUG_TRAP` / `ASSERT` Macros

**Problem:** `game.h` had no `ASSERT()` macro. The `course-builder.md`
explicitly requires these for catching bugs in debug builds.

**Fix applied:**
```c
#ifdef DEBUG
  #define DEBUG_TRAP() __builtin_trap()
  #define ASSERT(expr) do { if (!(expr)) { DEBUG_TRAP(); } } while (0)
#else
  #define DEBUG_TRAP() ((void)0)
  #define ASSERT(expr) ((void)(expr))
#endif
```
Added to `game.h` immediately after includes. `build-dev.sh` passes `-DDEBUG`.

**Mitigation for future courses:**
- [ ] `DEBUG_TRAP` and `ASSERT` defined in `game.h` right after includes
- [ ] `build-dev.sh` passes `-DDEBUG`
- [ ] Use `ASSERT` to validate function arguments and invariants

---

### Issue 12 — Raylib AudioStream Buffer Size Mismatch Causes Stuttering

**Problem:** `SetAudioStreamBufferSizeDefault(N)` and `UpdateAudioStream(..., N)`
MUST use the same `N`. Using different values (e.g., `*2` for one but not the
other) misaligns Raylib's internal ring-buffer write pointer, producing
stuttering/repeating audio every ~1 second.

**Failure pattern:**
```c
SetAudioStreamBufferSizeDefault(SAMPLES_PER_FRAME * 2);  /* registers 4096 */
UpdateAudioStream(stream, buf, SAMPLES_PER_FRAME);        /* writes 2048 — MISMATCH */
```

**Fix applied:**
```c
#define AUDIO_CHUNK_SIZE 2048  /* the ONE value used everywhere */
SetAudioStreamBufferSizeDefault(AUDIO_CHUNK_SIZE);
UpdateAudioStream(stream, buf, AUDIO_CHUNK_SIZE);
```
Pre-fill twice for double-buffering — the count is still AUDIO_CHUNK_SIZE each time.

**Mitigation for future courses:**
- [ ] Define a single constant `AUDIO_CHUNK_SIZE` (≥ 2048 at 44100 Hz)
- [ ] Use it for `SetAudioStreamBufferSizeDefault`, `UpdateAudioStream`, and `AudioOutputBuffer.sample_count`
- [ ] Never use different values for registration vs. fill

---

### Issue 13 — ALSA `snd_pcm_set_params` with Fixed Chunk Check

**Problem:** `snd_pcm_set_params(50ms_latency)` creates a ~2200-sample
hardware buffer. Pre-filling 2048 frames then checking `avail < 2048`
before every write guarantees `avail` is almost always < 2048 → audio
NEVER writes after the pre-fill → permanent silence.

**Fix applied:** Replaced `snd_pcm_set_params` with `snd_pcm_hw_params` (the
explicit API) plus Casey's latency model using `snd_pcm_delay`:
```c
snd_pcm_delay(g_pcm, &delay);       /* how many samples are queued */
to_write = (latency_target - delay); /* write only what's needed */
```
This keeps the buffer filled exactly to the latency target each frame.

**Mitigation for future courses:**
- [ ] Always use `snd_pcm_hw_params` for ALSA (not `snd_pcm_set_params`)
- [ ] Use `snd_pcm_delay` for Casey's fill calculation, not `snd_pcm_avail_update`
- [ ] Set `latency_target = samples_per_frame * frames_of_latency + safety_frames`
- [ ] Follow `games/tetris/src/main_x11.c` — `platform_audio_get_samples_to_write()`

---

### Issue 14 — Utility Headers Should Live in `utils/`

**Problem:** `font.h` and `sounds.h` were at the `src/` root alongside
`game.c/game.h`, mixing reusable utilities with game-specific logic.
`sounds.h` was a redundant re-export of `utils/audio.h`.

**Fix applied:**
- Moved `font.h` → `utils/font.h`; updated `game.c` include
- Deleted `sounds.h` (game.h already includes `utils/audio.h` directly)

**Mitigation for future courses:**
- [ ] At course creation: put ALL reusable helpers in `utils/` (font, math, draw, audio)
- [ ] Never put utility-only headers at `src/` root
- [ ] Avoid re-export shim headers — include the canonical header directly
