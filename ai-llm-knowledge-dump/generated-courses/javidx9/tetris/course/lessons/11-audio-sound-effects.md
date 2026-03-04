# Lesson 11: Audio — Sound Effects

## What we're building

Every game event now has a distinct sound: moving left/right (a short low click), rotating (a rising blip), locking a piece (a thud), clearing a line (an ascending sweep), a full Tetris (a triumphant rising tone), levelling up, game over, and restart. Sounds are **procedurally synthesized** — we generate raw PCM waveform data in code, with no `.wav` files.

Pieces near the left wall sound slightly left-panned; pieces near the right wall sound slightly right-panned. This gives a spatial feel that maps where the action is happening.

## What you'll learn

- `int16_t` — signed 16-bit PCM audio samples
- **Phase accumulator** pattern: the simplest oscillator
- **Square wave** synthesis: `(phase < 0.5f) ? 1.0f : -1.0f`
- `memset` to silence a buffer before mixing
- **Interleaved stereo**: L₀ R₀ L₁ R₁ L₂ R₂ ...
- Slot management: reusing `SoundInstance` slots when they expire
- Fade-in envelope: 96 samples ≈ 2ms to eliminate the attack click
- `calculate_pan_volumes`: linear stereo panning math
- `static inline` helper functions in a header

## Prerequisites

Lesson 10 complete: the HUD sidebar is rendering. The game loop in `main_raylib.c` exists but `platform_audio_init` and `platform_audio_update` are not yet wired up.

---

## Step 1: Why Procedural Audio?

### Zero dependencies

Web developers reach for the Web Audio API, Howler.js, or pre-recorded `.wav` files. Here we build our own mini-synth. The advantages:

- **No asset pipeline**: the sounds are just math in `audio.c`
- **Total control**: pitch, duration, volume, pan, frequency slide — all parameterized
- **Tiny binary**: a .wav file for each sound would add megabytes; our entire sound engine is < 200 lines

### The phase accumulator — the heart of all synthesis

A phase accumulator is a counter that advances from `0.0` to `1.0` and wraps back, once per waveform cycle:

```
phase += frequency * (1.0f / sample_rate);
if (phase >= 1.0f) phase -= 1.0f;
```

At 440 Hz (concert A) with a 48,000 Hz sample rate:
```
advance per sample = 440 / 48000 ≈ 0.00917
samples per cycle  = 48000 / 440 ≈ 109 samples
```

To generate a square wave from the phase:
```c
float wave = (phase < 0.5f) ? 1.0f : -1.0f;
```

```
phase: 0.0 ─────── 0.5 ─────── 1.0 (wraps)
wave:  +1.0 +1.0  │ -1.0 -1.0  │ +1.0 +1.0  ...
```

> **Why?** The Web Audio API's `OscillatorNode` implements exactly this pattern internally. We replicate it manually so we have direct access to the phase value — enabling pitch slides, envelope shaping, and continuous phase across buffer boundaries.

### ⚠ CRITICAL: Never reset `phase` between buffer fills

```
Buffer 1 fills:  phase goes 0.0 → 0.93
Buffer 2 fills:  phase MUST continue from 0.93 → ...

If you reset phase to 0.0 at buffer 2 start:
  End of buffer 1:  wave = -1.0  (phase was 0.93, past the 0.5 threshold)
  Start of buffer 2: wave = +1.0  (phase reset to 0.0)
  → Instant jump from -1.0 to +1.0 = a click/pop in the audio
```

ASCII diagram of the discontinuity:

```
Buffer 1            ┃ Buffer 2 (phase reset)
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄┃▄▄▄▄▄▄▄
                   ┃║
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀╛ ▀▀▀▀▀▀▀  ← discontinuity = audible click
```

`inst->phase` is never reset between fills. It is only reset when a **new sound starts** (`inst->phase = 0.0f` in `game_play_sound_at`).

> **Handmade Hero principle:** Audio phase is continuous. `inst->phase` ticks forward forever, never set to zero on slot reuse between buffer fills. This matches the behaviour of Web Audio API oscillator nodes, which maintain internal phase state independently of buffer boundaries.

---

## Step 2: Create `utils/audio.h`

This header defines every type the audio system uses. It lives in `utils/` alongside `draw-shapes.h` and `draw-text.h`.

**`src/utils/audio.h`** — complete new file:

```c
#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

#include <math.h>
#include <stdint.h>

/* ══════ AudioOutputBuffer ══════════════════════════════════════════════════
 *
 * The platform allocates this, sets samples/samples_per_second/sample_count,
 * and passes it to game_get_audio_samples(). The game fills samples[].
 *
 * MEMORY LAYOUT — Interleaved Stereo:
 *   Index:  0     1     2     3     4     5   ...
 *   Value: L[0]  R[0]  L[1]  R[1]  L[2]  R[2] ...
 *
 *   To write stereo pair N:
 *     samples[N * 2 + 0] = left_sample;
 *     samples[N * 2 + 1] = right_sample;
 *
 * int16_t = signed 16-bit integer (-32768 to 32767).
 * JS: Float32Array but quantized to 16-bit.
 */
typedef struct {
  int16_t *samples;        /* Platform allocates; game writes PCM here        */
  int samples_per_second;  /* 44100 or 48000 Hz                               */
  int sample_count;        /* Stereo pairs to generate (buffer length)        */
} AudioOutputBuffer;

/* ══════ Sound IDs ══════════════════════════════════════════════════════════
 *
 * JS: type SoundID = 'none' | 'move' | 'rotate' | ...
 * C:  typedef enum { SOUND_NONE = 0, SOUND_MOVE, ... } SOUND_ID;
 *
 * SOUND_COUNT is the sentinel — last enum value = total count.
 * Use it to size arrays: SoundDef defs[SOUND_COUNT].
 */
typedef enum {
  SOUND_NONE = 0,
  SOUND_MOVE,
  SOUND_ROTATE,
  SOUND_DROP,
  SOUND_LINE_CLEAR,
  SOUND_TETRIS,
  SOUND_LEVEL_UP,
  SOUND_GAME_OVER,
  SOUND_RESTART,
  SOUND_COUNT
} SOUND_ID;

/* ══════ SoundInstance — one active sound effect ════════════════════════════
 *
 * Up to MAX_SIMULTANEOUS_SOUNDS instances can play at once.
 * Each holds its own phase accumulator, frequency, volume, and pan.
 *
 * PHASE ACCUMULATOR (see Step 1 for full explanation):
 *   phase crawls 0.0 → 1.0 each waveform cycle, then wraps.
 *   NEVER reset between buffer fills — only reset when a new sound starts.
 */
#define MAX_SIMULTANEOUS_SOUNDS 4

typedef struct {
  SOUND_ID sound_id;

  float phase;             /* 0.0 to <1.0; wraps. NEVER reset between fills.  */
  int samples_remaining;   /* Counts down; slot is free when this hits 0.      */
  float volume;            /* Amplitude: 0.0 = silent, 1.0 = full.            */
  float frequency;         /* Current pitch in Hz. Slides each sample.        */
  float frequency_slide;   /* Hz added to frequency per sample (pitch slide). */
  int total_samples;       /* Total duration in samples (set on sound start).  */
  int fade_in_samples;     /* Ramp up from 0 for this many samples (≈96).     */
  float pan_position;      /* -1.0 = full left, 0.0 = centre, 1.0 = right.   */
} SoundInstance;

/* ══════ ToneGenerator — music oscillator ═══════════════════════════════════
 *
 * Used by the music sequencer (lesson 12). One continuous oscillator.
 * current_volume ramps toward volume each sample to prevent note-change clicks.
 */
typedef struct {
  float phase;
  float frequency;
  float volume;
  float pan_position;
  float current_volume;   /* Smoothed volume — CSS-like transition: 2ms ramp  */
  int is_playing;         /* 1 = note scheduled, 0 = rest/silence             */
} ToneGenerator;

/* ══════ MusicSequencer ═════════════════════════════════════════════════════
 *
 * Cycles through 4 patterns of 16 MIDI note steps each.
 * step_duration = seconds per step (controls tempo).
 * 0 in pattern array = rest (silence).
 * Defined fully in lesson 12; declared here for GameAudioState.
 */
#define MUSIC_PATTERN_LENGTH 16
#define MUSIC_NUM_PATTERNS   4

typedef struct {
  uint8_t patterns[MUSIC_NUM_PATTERNS][MUSIC_PATTERN_LENGTH];
  int current_pattern;
  int current_step;
  float step_timer;
  float step_duration;
  ToneGenerator tone;
  int is_playing;
} MusicSequencer;

/* ══════ GameAudioState — all audio state embedded in GameState ═════════════
 *
 * Handmade Hero principle: no hidden global audio state.
 * Everything lives here — passed as &state->audio everywhere.
 */
typedef struct {
  SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  int active_sound_count;
  MusicSequencer music;
  float master_volume;
  float sfx_volume;
  float music_volume;
  int samples_per_second;   /* Set once by platform at startup; never changes  */
} GameAudioState;

/* ══════ Inline Audio Math Helpers ══════════════════════════════════════════
 *
 * `static inline` = private to each translation unit that includes this
 * header, compiled directly at call sites with zero overhead.
 * JS: small utility functions inside a module.
 */

/* midi_to_freq: convert MIDI note number to frequency in Hz.
 *
 * FORMULA: freq = 440 × 2^((note − 69) / 12)
 *   69 = MIDI A4 (440 Hz, tuning reference)
 *   12 = semitones per octave
 *   Each octave = 2× frequency; 12 semitones distribute that evenly.
 *
 * Examples:
 *   midi_to_freq(69) = 440 Hz (A4)
 *   midi_to_freq(81) = 880 Hz (A5, one octave up)
 *   midi_to_freq(60) ≈ 261.6 Hz (C4 / Middle C)
 *
 * JS: const midiToFreq = (note) => 440 * Math.pow(2, (note - 69) / 12);
 * C:  powf() is float-precision power from <math.h>. Link with -lm.
 */
static inline float midi_to_freq(int note) {
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* clamp_sample: clamp float to valid int16_t range before writing to buffer.
 *
 * When multiple sounds mix, their amplitudes add up and can exceed ±32767.
 * Clamping saturates (like a limiter) rather than wrapping (like clipping distortion).
 *
 * JS: Math.max(-32768, Math.min(32767, sample))
 */
static inline int16_t clamp_sample(float sample) {
  if (sample >  32767.0f) return  32767;
  if (sample < -32768.0f) return -32768;
  return (int16_t)sample;
}

/* calculate_pan_volumes: linear stereo panning.
 *
 * LINEAR PAN LAW:
 *   pan ≤ 0  → left = 1.0,       right = 1.0 + pan  (reduces right)
 *   pan > 0  → left = 1.0 - pan, right = 1.0         (reduces left)
 *
 *   pan = -0.5: left=1.0, right=0.5
 *   pan =  0.0: left=1.0, right=1.0 (both full at centre)
 *   pan = +0.5: left=0.5, right=1.0
 *
 * JS: Web Audio API StereoPannerNode does this automatically.
 *     Here we implement it manually for full transparency.
 */
static inline void calculate_pan_volumes(float pan, float *left_vol,
                                         float *right_vol) {
  if (pan <= 0.0f) {
    *left_vol  = 1.0f;
    *right_vol = 1.0f + pan;
  } else {
    *left_vol  = 1.0f - pan;
    *right_vol = 1.0f;
  }
}

/* FIELD_CENTER: horizontal centre of the playable field (columns 1..10).
 * Used by calculate_piece_pan() in game.c.
 * Playable cols: 1..(FIELD_WIDTH-2) → centre = (FIELD_WIDTH-2)/2 + 1
 * With FIELD_WIDTH=12: (12-2)/2 + 1 = 6.0
 */
#define FIELD_CENTER  ((FIELD_WIDTH - 2) / 2.0f + 1.0f)

#endif /* UTILS_AUDIO_H */
```

---

## Step 3: Create `src/audio.c` (Sound Effects Only)

This lesson adds only the sound-effects machinery. Music sequencing arrives in lesson 12.

> **Why `int16_t` for samples?**
> CD audio uses 16-bit signed PCM: values from −32,768 to 32,767. Modern systems often use 32-bit float internally, but 16-bit is the most compatible format for hardware output and is what Raylib's `LoadAudioStream(sr, 16, 2)` expects. At the final mix step we convert our float math back to `int16_t` via `clamp_sample()`.

**`src/audio.c`** — complete file at end of lesson 11 (SFX only; music stubs in place):

```c
/* audio.c — Lesson 11: Sound Effects
 *
 * This file implements all audio: sound-effect synthesis and (lesson 12)
 * background music. It knows nothing about Raylib or the OS — it only
 * fills AudioOutputBuffer with raw int16_t PCM samples.
 */

#include "game.h"
#include <math.h>
#include <string.h>

/* ══════ Sound Definitions ══════════════════════════════════════════════════
 *
 * Each sound is defined by:
 *   frequency     : starting pitch in Hz
 *   frequency_end : ending pitch (for slide); 0 = no slide
 *   duration_ms   : how long the sound plays (milliseconds)
 *   volume        : amplitude [0.0..1.0]
 *
 * The frequency_slide per sample is computed from these two values:
 *   slide = (frequency_end - frequency) / total_samples
 *
 * SOUND_MOVE:      200 Hz → 150 Hz, 50ms  — short descending click
 * SOUND_ROTATE:    300 Hz → 400 Hz, 80ms  — ascending blip
 * SOUND_DROP:      150 Hz → 50 Hz,  100ms — low thud
 * SOUND_LINE_CLEAR:400 Hz → 800 Hz, 200ms — rising sweep
 * SOUND_TETRIS:    300 Hz → 1200Hz, 400ms — triumphant rising tone
 * SOUND_LEVEL_UP:  440 Hz → 880 Hz, 300ms — octave jump
 * SOUND_GAME_OVER: 400 Hz → 100 Hz, 500ms — falling doom tone
 * SOUND_RESTART:   600 Hz → 1200Hz, 200ms — quick ascending fanfare
 */
typedef struct {
  float frequency;
  float frequency_end;
  float duration_ms;
  float volume;
} SoundDef;

static const SoundDef SOUND_DEFS[SOUND_COUNT] = {
    [SOUND_NONE]       = {0,   0,    0,   0.0f},
    [SOUND_MOVE]       = {200, 150,  50,  0.3f},
    [SOUND_ROTATE]     = {300, 400,  80,  0.3f},
    [SOUND_DROP]       = {150, 50,   100, 0.5f},
    [SOUND_LINE_CLEAR] = {400, 800,  200, 0.6f},
    [SOUND_TETRIS]     = {300, 1200, 400, 0.8f},
    [SOUND_LEVEL_UP]   = {440, 880,  300, 0.5f},
    [SOUND_GAME_OVER]  = {400, 100,  500, 0.7f},
    [SOUND_RESTART]    = {600, 1200, 200, 0.5f},
};
/* Note: [SOUND_NONE] = {0, ...} uses designated initializer syntax.
 * C99 allows initializing array elements by index: [index] = value.
 * JS: Object.fromEntries / enum mapping would do the same thing.
 * This is safer than positional init — if we reorder SOUND_ID enum values,
 * each sound still maps to the right slot. */

/* ══════ Audio Initialization ═══════════════════════════════════════════════ */

void game_audio_init(GameAudioState *audio, int samples_per_second) {
  /* memset zeros the entire struct — equivalent to `audio = {}` in JS.
   * We then fill only the fields that need non-zero defaults.
   *
   * JS: Object.assign(audio, { master_volume: 0.8, sfx_volume: 1.0, ... })
   * C:  memset clears all bytes to 0 first; then we set explicit values.
   *
   * memset signature: memset(ptr, byte_value, byte_count)
   *   - ptr: start of memory region
   *   - byte_value: the byte to fill with (0 = zero all bytes)
   *   - byte_count: how many bytes to fill
   * JS: new ArrayBuffer(size) filled with zeros automatically.
   */
  memset(audio, 0, sizeof(GameAudioState));

  audio->samples_per_second = samples_per_second;
  audio->master_volume      = 0.8f;
  audio->sfx_volume         = 1.0f;
  audio->music_volume       = 0.5f;

  /* Music patterns are loaded in lesson 12 */
  audio->music.step_duration = 0.15f;
  audio->music.tone.volume   = 0.3f;
}

/* ══════ Play Sound Effect ═══════════════════════════════════════════════════
 *
 * SLOT MANAGEMENT
 * ────────────────
 * We have MAX_SIMULTANEOUS_SOUNDS (4) slots. To play a new sound:
 *   1. Scan for a slot where samples_remaining <= 0 (finished / never used).
 *   2. If none found, steal slot 0 (oldest by convention — simple, good enough).
 *   3. Write the new sound's parameters into that slot.
 *
 * JS analogy: an audio channel pool in a game engine —
 *   if all channels are busy, interrupt the oldest one.
 *
 * Note: inst->phase = 0.0f only here — at sound START.
 * Between buffer fills, phase is NEVER reset (see lesson notes on continuity).
 */
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position) {
  if (sound == SOUND_NONE || sound >= SOUND_COUNT) return;

  /* Find a free slot */
  int slot = -1;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) slot = 0; /* All slots busy: steal slot 0 */

  const SoundDef    *def  = &SOUND_DEFS[sound];
  SoundInstance     *inst = &audio->active_sounds[slot];

  inst->sound_id          = sound;
  inst->phase             = 0.0f;  /* reset ONLY on new sound start */
  inst->frequency         = def->frequency;
  inst->volume            = def->volume;
  inst->samples_remaining =
      (int)(def->duration_ms * audio->samples_per_second / 1000.0f);
  inst->total_samples     = inst->samples_remaining;
  inst->fade_in_samples   = 96;  /* ~2ms at 48kHz — prevents attack click */

  /* Clamp pan to [-1.0, 1.0] */
  inst->pan_position = (pan_position < -1.0f) ? -1.0f
                     : (pan_position >  1.0f) ?  1.0f
                     : pan_position;

  /* Compute frequency slide per sample:
   *   slide = (end_freq - start_freq) / total_samples
   * Each sample, frequency += frequency_slide.
   * At the last sample, frequency ≈ frequency_end.
   */
  if (def->frequency_end > 0 && inst->samples_remaining > 0) {
    inst->frequency_slide =
        (def->frequency_end - def->frequency) / inst->samples_remaining;
  } else {
    inst->frequency_slide = 0.0f;
  }
}

/* Convenience: play at centre pan (pan=0.0) */
void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  game_play_sound_at(audio, sound, 0.0f);
}

/* ══════ Music Stubs (implemented in lesson 12) ══════════════════════════════ */

void game_music_play(GameAudioState *audio) {
  audio->music.is_playing = 0;  /* No-op until lesson 12 loads the patterns */
}

void game_music_stop(GameAudioState *audio) {
  audio->music.is_playing    = 0;
  audio->music.tone.is_playing = 0;
}

void game_audio_update(GameAudioState *audio, float delta_time) {
  (void)audio; (void)delta_time;  /* No-op until lesson 12 */
}

/* ══════ Generate PCM Samples ════════════════════════════════════════════════
 *
 * Called by the platform once per audio buffer period.
 * Fills buffer->samples with interleaved stereo int16_t PCM.
 *
 * ALGORITHM per sample pair:
 *   1. Clear mixed_left / mixed_right floats to 0.
 *   2. For each active SoundInstance:
 *      a. Generate a square wave: (phase < 0.5) ? +1 : -1
 *      b. Apply fade-out envelope (samples_remaining / total_samples)
 *      c. Apply fade-in envelope (first 96 samples ramp up from 0)
 *      d. Scale by volume and sfx_volume
 *      e. Apply pan: multiply left/right gains from calculate_pan_volumes()
 *      f. Advance phase; apply frequency slide; decrement samples_remaining
 *   3. Scale by master_volume * 16000  (maps [0..1] float to int16 range)
 *   4. Clamp and write left/right samples to interleaved buffer
 *
 * JS equivalent:
 *   AudioWorkletProcessor.process() — called every ~128 samples by the browser.
 *   Here we do the same math but in a manually-called C function.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio   = &state->audio;
  int16_t        *out     = buffer->samples;
  int             n       = buffer->sample_count;
  float inv_sr            = 1.0f / (float)buffer->samples_per_second;

  /* Zero the entire output buffer first.
   * memset(ptr, 0, bytes) fills bytes bytes starting at ptr with zero.
   * JS: buffer.fill(0)
   * We zero BEFORE mixing so any inactive slot contributes 0 (silence).
   * n stereo pairs = n * 2 int16_t values = n * 2 * sizeof(int16_t) bytes.
   */
  memset(out, 0, (size_t)n * 2 * sizeof(int16_t));

  for (int s = 0; s < n; s++) {
    float mixed_left  = 0.0f;
    float mixed_right = 0.0f;

    /* ── Mix all active sound effects ── */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0) continue;

      /* Square wave from phase accumulator */
      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;

      /* Fade-out envelope: amplitude decreases linearly as sound expires.
       * At start: env = 1.0. At end: env = 0.0.
       * JS: const env = samplesRemaining / totalSamples */
      float env = (float)inst->samples_remaining / (float)inst->total_samples;

      /* Fade-in envelope: first `fade_in_samples` samples ramp from 0.
       * This eliminates the click you'd get if the waveform starts mid-cycle.
       * 96 samples ≈ 2ms at 48kHz — imperceptible as a delay but kills clicks.
       *
       * JS: const fadein = samplesPlayed < fadeInSamples
       *       ? samplesPlayed / fadeInSamples : 1.0;
       */
      int samples_played = inst->total_samples - inst->samples_remaining;
      if (samples_played < inst->fade_in_samples) {
        float fade_in = (float)samples_played / (float)inst->fade_in_samples;
        env *= fade_in;
      }

      float sample = wave * inst->volume * env * audio->sfx_volume;

      /* Stereo panning */
      float left_vol, right_vol;
      calculate_pan_volumes(inst->pan_position, &left_vol, &right_vol);
      mixed_left  += sample * left_vol;
      mixed_right += sample * right_vol;

      /* Advance phase and apply frequency slide */
      inst->phase += inst->frequency * inv_sr;
      if (inst->phase >= 1.0f) inst->phase -= 1.0f;
      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* ── Final scale and write to interleaved buffer ──
     * Scale factor 16000.0f maps normalized float to ~half of int16_t range,
     * leaving headroom for multiple simultaneous sounds to mix without clipping.
     *
     * Interleaved layout:
     *   out[s*2 + 0] = left  channel
     *   out[s*2 + 1] = right channel
     *
     * The `out++` idiom advances the pointer by 1 element (2 bytes for int16_t)
     * after each write — equivalent to out[s*2] then out[s*2+1] by index.
     */
    mixed_left  *= audio->master_volume * 16000.0f;
    mixed_right *= audio->master_volume * 16000.0f;
    *out++ = clamp_sample(mixed_left);
    *out++ = clamp_sample(mixed_right);
  }
}
```

---

## Step 4: Update `src/game.h` to Include Audio

Add the audio header include and the `GameAudioState audio` field to `GameState`, plus audio function declarations.

**`src/game.h`** — complete file at end of lesson 11 (showing additions):

```c
#ifndef GAME_H
#define GAME_H

#include "utils/audio.h"      /* NEW: AudioOutputBuffer, GameAudioState, SOUND_ID */
#include "utils/backbuffer.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Constants and types (identical to lesson 09/10) ── */
#define FIELD_WIDTH  12
#define FIELD_HEIGHT 18
#define CELL_SIZE    30
#define SIDEBAR_WIDTH 200

#define TETROMINO_SPAN        '.'
#define TETROMINO_BLOCK       'X'
#define TETROMINO_LAYER_COUNT 4
#define TETROMINO_SIZE        16
#define TETROMINOS_COUNT      7

#define COLOR_BLACK      GAME_RGB(0,   0,   0  )
#define COLOR_WHITE      GAME_RGB(255, 255, 255)
#define COLOR_GRAY       GAME_RGB(128, 128, 128)
#define COLOR_DARK_GRAY  GAME_RGB(64,  64,  64 )
#define COLOR_CYAN       GAME_RGB(0,   255, 255)
#define COLOR_BLUE       GAME_RGB(0,   0,   255)
#define COLOR_ORANGE     GAME_RGB(255, 165, 0  )
#define COLOR_YELLOW     GAME_RGB(255, 255, 0  )
#define COLOR_GREEN      GAME_RGB(0,   255, 0  )
#define COLOR_MAGENTA    GAME_RGB(255, 0,   255)
#define COLOR_RED        GAME_RGB(255, 0,   0  )

typedef enum { TETROMINO_I_IDX, TETROMINO_J_IDX, TETROMINO_L_IDX,
               TETROMINO_O_IDX, TETROMINO_S_IDX, TETROMINO_T_IDX,
               TETROMINO_Z_IDX } TETROMINO_BY_IDX;

typedef enum { TETRIS_FIELD_EMPTY, TETRIS_FIELD_I, TETRIS_FIELD_J,
               TETRIS_FIELD_L, TETRIS_FIELD_O, TETRIS_FIELD_S,
               TETRIS_FIELD_T, TETRIS_FIELD_Z, TETRIS_FIELD_WALL,
               TETRIS_FIELD_TMP_FLASH } TETRIS_FIELD_CELL;

typedef enum { TETROMINO_R_0, TETROMINO_R_90,
               TETROMINO_R_180, TETROMINO_R_270 } TETROMINO_R_DIR;

typedef enum { TETROMINO_ROTATE_X_NONE, TETROMINO_ROTATE_X_GO_LEFT,
               TETROMINO_ROTATE_X_GO_RIGHT } TETROMINO_ROTATE_X_VALUE;

typedef struct {
  float timer;
  float initial_delay;
  float interval;
  bool  is_active;
  bool  passed_initial_delay;
} RepeatInterval;

typedef struct {
  int x, y;
  TETROMINO_BY_IDX index;
  TETROMINO_BY_IDX next_index;
  TETROMINO_R_DIR rotate_x_value;
  TETROMINO_ROTATE_X_VALUE next_rotate_x_value;
} CurrentPiece;

typedef struct { int half_transition_count; int ended_down; } GameButtonState;

#define BUTTON_COUNT 4
typedef struct {
  union {
    GameButtonState buttons[BUTTON_COUNT];
    struct {
      GameButtonState move_left;
      GameButtonState move_right;
      GameButtonState move_down;
      GameButtonState rotate_x;
    };
  };
} GameInput;

#define UPDATE_BUTTON(button, is_down)                        \
  do {                                                        \
    if ((button).ended_down != (is_down)) {                   \
      (button).half_transition_count++;                       \
      (button).ended_down = (is_down);                        \
    }                                                         \
  } while (0)

/* ═══════════════════════════════════════════════════════════════════════════
 * GameState
 * NEW THIS LESSON: GameAudioState audio
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct {
  unsigned char field[FIELD_WIDTH * FIELD_HEIGHT];
  CurrentPiece current_piece;
  int score;
  int pieces_count;
  int level;
  struct {
    int indexes[TETROMINO_LAYER_COUNT];
    int count;
    RepeatInterval flash_timer;
  } completed_lines;
  struct {
    RepeatInterval move_left;
    RepeatInterval move_right;
    RepeatInterval move_down;
  } input_repeat;
  struct {
    RepeatInterval rotate_direction;
  } input_values;

  /* NEW: All audio state — sound effects and (lesson 12) music */
  GameAudioState audio;

  int should_quit;
  int should_restart;
} GameState;

extern const char *TETROMINOES[7];

/* ── Game Functions ── */
void game_init(GameState *state);
void prepare_input_frame(GameInput *old_input, GameInput *new_input);
int  tetromino_pos_value(int px, int py, TETROMINO_R_DIR r);
int  tetromino_does_piece_fit(GameState *state, int piece, int rotation,
                               int pos_x, int pos_y);
void game_update(GameState *state, GameInput *input, float delta_time);
void game_render(Backbuffer *backbuffer, GameState *state);

/* NEW: Audio Functions (implemented in audio.c) */
void game_audio_init(GameAudioState *audio, int samples_per_second);
void game_play_sound(GameAudioState *audio, SOUND_ID sound);
void game_play_sound_at(GameAudioState *audio, SOUND_ID sound, float pan_position);
void game_music_play(GameAudioState *audio);
void game_music_stop(GameAudioState *audio);
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer);
void game_audio_update(GameAudioState *audio, float delta_time);

#endif /* GAME_H */
```

---

## Step 5: Add Sound Calls to `game.c`

Two changes in `game.c`:

1. Add `calculate_piece_pan()` helper (maps piece X to stereo pan)
2. Add `game_play_sound_at()` calls in `tetris_apply_input` and the piece-lock section

### `calculate_piece_pan`

```c
/* Map piece X column to stereo pan [-0.8, +0.8].
 * Cap at ±0.8 so audio is never completely absent from one ear.
 * FIELD_CENTER = 6.0 (see utils/audio.h)
 */
static float calculate_piece_pan(int piece_x) {
  float center     = (FIELD_WIDTH - 2) / 2.0f + 1.0f;  /* 6.0 */
  float max_offset = (FIELD_WIDTH - 2) / 2.0f;           /* 5.0 */
  float offset     = (float)piece_x - center;
  float pan        = (offset / max_offset) * 0.8f;
  if (pan < -1.0f) pan = -1.0f;
  if (pan >  1.0f) pan =  1.0f;
  return pan;
}
```

Sound calls added to `tetris_apply_input` (see `src/game.c:636-679`):

```c
/* On successful rotation: */
game_play_sound_at(&state->audio, SOUND_ROTATE, pan);

/* On successful move left or right: */
game_play_sound_at(&state->audio, SOUND_MOVE, pan);

/* On soft drop: */
game_play_sound_at(&state->audio, SOUND_MOVE, pan);
```

Sound calls in piece-lock section of `game_update` (see `src/game.c:834-918`):

```c
game_play_sound_at(&state->audio, SOUND_DROP, pan);          /* after lock stamp */
/* After collapse: */
if (completed_lines.count == 4)
    game_play_sound(&state->audio, SOUND_TETRIS);
else if (completed_lines.count > 0)
    game_play_sound(&state->audio, SOUND_LINE_CLEAR);
/* After level-up: */
game_play_sound(&state->audio, SOUND_LEVEL_UP);
```

---

## Step 6: Add Audio to `main_raylib.c`

The platform layer needs to initialize, update, and shut down the audio stream. This is fully explained in `src/main_raylib.c:90-303`. The key points:

- `InitAudioDevice()` + `LoadAudioStream(48000, 16, 2)` — 48 kHz, 16-bit, stereo
- Pre-fill two Raylib buffers with silence before `PlayAudioStream()` — prevents startup click
- `IsAudioStreamProcessed()` — returns `true` when a buffer has been consumed
- Fill consumed buffers by calling `game_get_audio_samples()` and `UpdateAudioStream()`

```c
/* In main(), after game_init(): */
game_audio_init(&game_state.audio, platform_game_props.audio.samples_per_second);

/* In the game loop, after game_update(): */
platform_audio_update(&game_state, &platform_game_props.audio);
```

The `main_raylib.c` file at this stage is identical to the final `src/main_raylib.c` — refer to that file directly.

---

## Build and Run

```bash
clang -Wall -Wextra -g -O0 \
  -o build/game \
  src/game.c src/audio.c src/utils/draw-shapes.c src/utils/draw-text.c \
  src/main_raylib.c \
  -lraylib -lpthread -ldl -lm

./build/game
```

Note: `audio.c` is a new translation unit. The `-lm` flag links `libm` for `powf()` in `midi_to_freq`.

---

## Expected Result

Every game event now has sound:
- Moving left/right: short low click
- Rotating: ascending blip
- Locking: thud
- Line clear: sweeping ascending tone
- Tetris (4 lines): triumphant fanfare
- Level up: octave jump
- Pieces near the left wall sound slightly left; near the right wall, slightly right

---

## Common Mistakes

> **Common mistake:** Resetting `inst->phase = 0.0f` in the buffer-fill loop instead of only at sound start. The audio will sound fine for a single-buffer test but produce periodic clicks as each new buffer begins. Only set `phase = 0.0f` in `game_play_sound_at()`, never in `game_get_audio_samples()`.

> **Common mistake:** Forgetting `-lm` in the compile command. `powf()` (used in `midi_to_freq`) lives in libm. Without the flag you'll get `undefined reference to 'powf'`.

> **Common mistake:** Writing `n * 2 * sizeof(int16_t)` in `memset` but only passing `n` as `sample_count` to the stereo buffer. `sample_count` is the number of stereo *pairs*, so the buffer is `sample_count * 2` int16_t values wide.

---

## Exercises

1. Add a `SOUND_SOFT_DROP` that plays when the piece moves down via the soft-drop key, distinct from `SOUND_MOVE`. What would you change in the `SoundDef` table?
2. Increase `MAX_SIMULTANEOUS_SOUNDS` from 4 to 8. What changes in memory usage? (Hint: `sizeof(GameAudioState)`)
3. Modify the fade-in to 192 samples (≈4ms) and compare the click behaviour. At what fade-in length does it become noticeably too slow as an attack?
4. Try changing the square wave to a sawtooth: `wave = 2.0f * phase - 1.0f`. How does it sound compared to the square wave?

## What's Next

The game has sound effects. **Lesson 12** adds the **Korobeiniki** (Tetris theme) as background music using a MIDI-style step sequencer, note-to-frequency conversion, and smooth volume ramping between notes.
