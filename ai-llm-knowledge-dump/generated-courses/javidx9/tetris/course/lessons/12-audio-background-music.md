# Lesson 12: Audio — Background Music

## What we're building

The Korobeiniki — the classic Tetris theme — plays as background music. It's driven by a **MIDI-style step sequencer**: four patterns of 16 notes each, cycling in order. Each note is a MIDI note number that we convert to a frequency using the equal-temperament formula. A single `ToneGenerator` (square-wave oscillator) plays the melody, with smooth **volume ramping** between notes to prevent clicks.

## What you'll learn

- `uint8_t` arrays for compact MIDI note storage
- `memcpy` to copy pattern arrays into the sequencer
- `powf()` from `<math.h>` for MIDI-to-Hz conversion
- Volume ramping: why you must never jump a volume discontinuously
- How the sequencer (game logic time) and sample generation (audio time) advance independently
- The two-loop architecture: `game_audio_update` (game thread) vs `game_get_audio_samples` (audio thread)

## Prerequisites

Lesson 11 complete: `audio.c` exists with SFX synthesis. `game_audio_update` and `game_music_play` are currently stubs.

---

## Step 1: MIDI Notes and Equal Temperament

Each step in the Korobeiniki is a **MIDI note number** — an integer from 0 to 127. We convert it to Hz with the formula already declared in `utils/audio.h`:

```c
float midi_to_freq(int note) {
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}
```

### Why 69? Why 12?

MIDI note **69 is A4** — the standard tuning reference (440 Hz). Subtracting 69 makes A4 the zero-point of the exponent. There are **12 semitones per octave** in equal temperament, so each octave doubles the frequency and each semitone multiplies by `2^(1/12) ≈ 1.059`.

```
midi_to_freq(69) = 440 × 2^(0/12)  = 440.0 Hz  (A4)
midi_to_freq(81) = 440 × 2^(12/12) = 880.0 Hz  (A5, one octave up)
midi_to_freq(57) = 440 × 2^(-12/12)= 220.0 Hz  (A3, one octave down)
midi_to_freq(60) = 440 × 2^(-9/12) ≈ 261.6 Hz  (C4 / Middle C)
```

> **Why?** In JavaScript this is exactly: `const midiToFreq = (note) => 440 * Math.pow(2, (note - 69) / 12)`. The formula is identical — equal temperament is universal. The only difference is that we call `powf()` (float precision) instead of `Math.pow()` (double precision) to avoid unnecessary precision and stay consistent with our float audio pipeline.

### The Korobeiniki melody as MIDI note numbers

```c
/* Pattern A — main melody */
{ 76, 71, 72, 74, 72, 71, 69, 69, 72, 76, 74, 72, 71, 71, 72, 74 }
/*  E5  B4  C5  D5  C5  B4  A4  A4  C5  E5  D5  C5  B4  B4  C5  D5 */

/* Pattern B */
{ 76,  0, 72,  0, 69, 69, 72, 74, 76,  0, 72,  0, 69,  0,  0,  0 }
/* 0 = rest (silence) */
```

Each `uint8_t` is a MIDI note (0 = rest, 1-127 = note). We use `uint8_t` because MIDI notes never exceed 127, and packing them in one byte keeps the pattern arrays compact.

> **Why `uint8_t`?** MIDI note range is 0-127, which fits in 7 bits. `uint8_t` (unsigned 8-bit) holds 0-255 — a perfect fit. Using `int` (typically 32-bit) would waste 3-4× the memory. More importantly, `uint8_t` communicates the data's semantic range to future readers: "this is a byte-sized MIDI value, not a general integer."
>
> JS equivalent: a plain `number[]` — JavaScript has no 8-bit integer type (you'd use `Uint8Array` for compact storage, exactly like `uint8_t[]` in C).

---

## Step 2: The Two-Loop Architecture

The music system has two separate advancement loops:

```
GAME THREAD (each frame)              AUDIO THREAD (each buffer fill)
─────────────────────────             ────────────────────────────────
game_update()                         game_get_audio_samples()
  → game_audio_update()                 for each sample:
      seq->step_timer += delta_time       tone->phase += freq * inv_sr
      if step complete:                   tone->current_volume approaches target
        → set tone->frequency             → write to buffer
        → set tone->is_playing
```

- `game_audio_update` runs on **game logic time** (once per frame, delta_time ≈ 16ms at 60fps). It advances the note sequencer: checks if the current step duration has elapsed, and if so moves to the next note.
- `game_get_audio_samples` runs on **audio time** (every buffer fill, ≈ 10ms at 48kHz/480 samples). It synthesizes actual PCM samples at the sample rate.

They communicate through `ToneGenerator.frequency`, `is_playing`, and `current_volume`. The sequencer writes; the synthesizer reads.

> **Handmade Hero principle:** One frame = one call chain. `game_audio_update` is called from `game_update`, which is called from the platform loop. Nothing fires outside this chain — no callbacks, no threads, no event queues triggered by the audio side. The audio fill loop is also deterministic: it only reads from state set during `game_update`.

### Why does the sequencer run in game time, not audio time?

The sequencer needs `delta_time` measured in seconds of wall-clock time. In `game_get_audio_samples`, we could advance a timer by `1.0f / sample_rate` per sample — but that means doing 48,000 comparisons per second inside the sample loop, which is wasteful. The melody step rate (0.15s ≈ 6.67 steps/second) is far coarser than the sample rate. Game-time updates are the natural place for it.

---

## Step 3: Volume Ramping — Preventing Note Clicks

When a note ends and a rest starts (or the frequency changes), the waveform must not jump instantaneously. If `current_volume` snapped from 0.3 to 0.0 between two adjacent samples, the audio would hear a discontinuity (a click).

The `ToneGenerator` uses **linear volume ramping**:

```c
const float VOLUME_RAMP_SPEED = 0.002f;  /* per sample */

float target_volume = tone->is_playing ? tone->volume : 0.0f;

if (tone->current_volume < target_volume) {
    tone->current_volume += VOLUME_RAMP_SPEED;
    if (tone->current_volume > target_volume) tone->current_volume = target_volume;
} else if (tone->current_volume > target_volume) {
    tone->current_volume -= VOLUME_RAMP_SPEED;
    if (tone->current_volume < 0.0f) tone->current_volume = 0.0f;
}
```

Each sample, `current_volume` moves 0.002 toward `target_volume`. To go from 0.0 to 0.3:
```
samples needed = 0.3 / 0.002 = 150 samples ≈ 3ms at 48kHz
```

3ms is below the threshold of human perception for a click (≈5-20ms), so the transition sounds clean.

> **JS equivalent:** Web Audio API `gainNode.gain.linearRampToValueAtTime(targetValue, time)`. We implement the same concept manually: instead of scheduling a ramp event on a timeline, we just step toward the target each sample. The result is identical — a short linear fade.

The `current_volume` approach also means the sequencer and synthesizer are **decoupled**: the sequencer says "note ends now" by setting `is_playing = 0`, and the synthesizer smoothly fades out at its own pace over the next 150 samples.

---

## Step 4: Updated `src/audio.c` — Complete with Music

This is the **complete final** `audio.c` — all SFX functions from lesson 11 plus the music sequencer.

**`src/audio.c`** — complete final file (see also `games/tetris/src/audio.c`):

```c
/* audio.c — Lesson 12: Background Music
 *
 * Complete audio implementation: sound effects (lesson 11) + music sequencer.
 */

#include "game.h"
#include <math.h>
#include <string.h>

/* ══════ Sound Definitions (from lesson 11, unchanged) ══════════════════════ */
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

/* ══════ Korobeiniki (Tetris Theme) Note Patterns ═══════════════════════════
 *
 * Four patterns of 16 MIDI notes each. The sequencer cycles A → B → C → D → A...
 * 0 = rest (silence for that step).
 *
 * uint8_t: MIDI notes 0-127 fit in one unsigned byte.
 *   Storage: 4 patterns × 16 notes × 1 byte = 64 bytes total.
 *   With int: 4 × 16 × 4 = 256 bytes — no benefit for values ≤ 127.
 *
 * MIDI note reference:
 *   60 = C4 (Middle C),  69 = A4 (440 Hz),  76 = E5,  71 = B4
 */
static const uint8_t TETRIS_PATTERN_A[MUSIC_PATTERN_LENGTH] = {
    76, 71, 72, 74, 72, 71, 69, 69, 72, 76, 74, 72, 71, 71, 72, 74,
    /* E5 B4 C5 D5 C5 B4 A4 A4 C5 E5 D5 C5 B4 B4 C5 D5 */
};
static const uint8_t TETRIS_PATTERN_B[MUSIC_PATTERN_LENGTH] = {
    76,  0, 72,  0, 69, 69, 72, 74, 76,  0, 72,  0, 69,  0,  0,  0,
};
static const uint8_t TETRIS_PATTERN_C[MUSIC_PATTERN_LENGTH] = {
    74,  0, 74, 76, 78,  0, 76, 74, 72,  0, 72, 74, 76,  0, 74, 72,
};
static const uint8_t TETRIS_PATTERN_D[MUSIC_PATTERN_LENGTH] = {
    71,  0,  0, 72, 74,  0, 72, 71, 69,  0,  0,  0,  0,  0,  0,  0,
};

/* ══════ Initialization ══════════════════════════════════════════════════════
 *
 * NEW: Copy the four Korobeiniki patterns into audio->music.patterns[][].
 *
 * memcpy(dest, src, bytes):
 *   Copies `bytes` bytes from `src` to `dest`. No overlap allowed.
 *   JS: dest.set(src) on a TypedArray, or structuredClone for objects.
 *
 *   We copy each pattern (16 bytes) into the corresponding row of the
 *   2D patterns array: audio->music.patterns[0], [1], [2], [3].
 *
 *   MUSIC_PATTERN_LENGTH = 16 bytes (one uint8_t per step).
 */
void game_audio_init(GameAudioState *audio, int samples_per_second) {
  memset(audio, 0, sizeof(GameAudioState));

  audio->samples_per_second = samples_per_second;
  audio->master_volume      = 0.8f;
  audio->sfx_volume         = 1.0f;
  audio->music_volume       = 0.5f;

  audio->music.step_duration = 0.15f;  /* seconds per step: 1/0.15 ≈ 6.67 steps/s */
  audio->music.tone.volume          = 0.3f;
  audio->music.tone.current_volume  = 0.0f;  /* start silent, ramp up on first note */
  audio->music.tone.pan_position    = 0.0f;  /* music is always centred */

  /* Copy the four patterns into the sequencer's 2D array.
   * patterns[0] = row 0 = TETRIS_PATTERN_A, etc.
   * sizeof reference: MUSIC_PATTERN_LENGTH = 16 (bytes, since uint8_t).
   */
  memcpy(audio->music.patterns[0], TETRIS_PATTERN_A, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[1], TETRIS_PATTERN_B, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[2], TETRIS_PATTERN_C, MUSIC_PATTERN_LENGTH);
  memcpy(audio->music.patterns[3], TETRIS_PATTERN_D, MUSIC_PATTERN_LENGTH);
}

/* ══════ Sound Effect Functions (from lesson 11, unchanged) ═════════════════ */

void game_play_sound_at(GameAudioState *audio, SOUND_ID sound,
                        float pan_position) {
  if (sound == SOUND_NONE || sound >= SOUND_COUNT) return;
  int slot = -1;
  for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
    if (audio->active_sounds[i].samples_remaining <= 0) { slot = i; break; }
  }
  if (slot < 0) slot = 0;
  const SoundDef *def  = &SOUND_DEFS[sound];
  SoundInstance  *inst = &audio->active_sounds[slot];
  inst->sound_id          = sound;
  inst->phase             = 0.0f;
  inst->frequency         = def->frequency;
  inst->volume            = def->volume;
  inst->samples_remaining =
      (int)(def->duration_ms * audio->samples_per_second / 1000.0f);
  inst->total_samples     = inst->samples_remaining;
  inst->fade_in_samples   = 96;
  inst->pan_position      = (pan_position < -1.0f) ? -1.0f
                           : (pan_position >  1.0f) ?  1.0f : pan_position;
  inst->frequency_slide   = (def->frequency_end > 0 && inst->samples_remaining > 0)
      ? (def->frequency_end - def->frequency) / inst->samples_remaining : 0.0f;
}

void game_play_sound(GameAudioState *audio, SOUND_ID sound) {
  game_play_sound_at(audio, sound, 0.0f);
}

/* ══════ Music Control ═══════════════════════════════════════════════════════
 *
 * game_music_play: (re)start the sequencer from pattern 0, step 0.
 *   Resets current_volume to 0.0 so the music fades in smoothly.
 *
 * game_music_stop: freeze the sequencer and the tone.
 *   Called on game over.  Music will also restart with the game (R key).
 */
void game_music_play(GameAudioState *audio) {
  audio->music.is_playing       = 1;
  audio->music.current_pattern  = 0;
  audio->music.current_step     = 0;
  audio->music.step_timer       = 0.0f;
  audio->music.tone.current_volume = 0.0f; /* smooth fade-in from silence */
  audio->music.tone.pan_position   = 0.0f;
}

void game_music_stop(GameAudioState *audio) {
  audio->music.is_playing        = 0;
  audio->music.tone.is_playing   = 0;
}

/* ══════ Music Sequencer Update (game logic time) ════════════════════════════
 *
 * Called from game_update() once per frame with delta_time seconds.
 *
 * STATE MACHINE per call:
 *   seq->step_timer accumulates delta_time.
 *   When it reaches step_duration (0.15s):
 *     Subtract step_duration (keep fractional remainder for timing precision).
 *     Read the MIDI note at patterns[current_pattern][current_step].
 *     If note > 0: set tone->frequency = midi_to_freq(note), is_playing = 1
 *     If note == 0: set is_playing = 0  (rest)
 *     Advance current_step; wrap to 0 at MUSIC_PATTERN_LENGTH (16).
 *     Advance current_pattern at end of each 16-step pattern.
 *
 * WHY step_timer -= step_duration and not = 0?
 *   Same reason as the gravity timer in game_update: subtracting keeps the
 *   fractional overflow. If delta_time = 0.017s and step_duration = 0.15s,
 *   the step fires at 0.153s (3ms late). With reset:  timer = 0.003 is lost.
 *   With subtract: timer = 0.003, next step fires 3ms sooner — tight timing.
 *
 * The tone's frequency and is_playing are set here. The synthesizer in
 * game_get_audio_samples reads them, independent of when this function ran.
 */
void game_audio_update(GameAudioState *audio, float delta_time) {
  if (!audio->music.is_playing) return;

  MusicSequencer *seq = &audio->music;
  seq->step_timer += delta_time;

  if (seq->step_timer >= seq->step_duration) {
    seq->step_timer -= seq->step_duration;  /* keep fractional remainder */

    /* Read the MIDI note for this step.
     * uint8_t note: 0 = rest, 1-127 = MIDI note number.
     * midi_to_freq converts note to Hz: 440 * 2^((note-69)/12)
     */
    uint8_t note = seq->patterns[seq->current_pattern][seq->current_step];

    if (note > 0) {
      seq->tone.frequency  = midi_to_freq(note);
      seq->tone.is_playing = 1;
    } else {
      seq->tone.is_playing = 0;  /* rest: volume will ramp to 0 in synthesizer */
    }

    /* Advance step counter; wrap at MUSIC_PATTERN_LENGTH (16) */
    seq->current_step++;
    if (seq->current_step >= MUSIC_PATTERN_LENGTH) {
      seq->current_step    = 0;
      /* Advance to next pattern; wrap cyclically: A→B→C→D→A→... */
      seq->current_pattern = (seq->current_pattern + 1) % MUSIC_NUM_PATTERNS;
    }
  }
}

/* ══════ Generate PCM Samples — SFX + Music Mix ══════════════════════════════
 *
 * Extended from lesson 11 to include the music ToneGenerator.
 *
 * VOLUME RAMP ALGORITHM (see Step 3 explanation):
 *   target_volume = tone->is_playing ? tone->volume : 0.0f
 *   current_volume steps toward target_volume by VOLUME_RAMP_SPEED each sample.
 *   VOLUME_RAMP_SPEED = 0.002 → ~150 samples = ~3ms to ramp: inaudible click.
 *
 * Music is only synthesized when current_volume > 0.0001 or is_playing.
 * This ensures we don't waste CPU on a truly silent music channel.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *buffer) {
  GameAudioState *audio = &state->audio;
  int16_t        *out   = buffer->samples;
  int             n     = buffer->sample_count;
  float inv_sr          = 1.0f / (float)buffer->samples_per_second;

  memset(out, 0, (size_t)n * 2 * sizeof(int16_t));

  /* Volume ramp speed: 0.002 per sample ≈ 150 samples = ~3ms to reach target */
  const float VOLUME_RAMP_SPEED = 0.002f;

  for (int s = 0; s < n; s++) {
    float mixed_left  = 0.0f;
    float mixed_right = 0.0f;

    /* ── Sound Effects (unchanged from lesson 11) ── */
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; i++) {
      SoundInstance *inst = &audio->active_sounds[i];
      if (inst->samples_remaining <= 0) continue;
      float wave = (inst->phase < 0.5f) ? 1.0f : -1.0f;
      float env  = (float)inst->samples_remaining / (float)inst->total_samples;
      int sp = inst->total_samples - inst->samples_remaining;
      if (sp < inst->fade_in_samples)
        env *= (float)sp / (float)inst->fade_in_samples;
      float sample = wave * inst->volume * env * audio->sfx_volume;
      float lv, rv;
      calculate_pan_volumes(inst->pan_position, &lv, &rv);
      mixed_left  += sample * lv;
      mixed_right += sample * rv;
      inst->phase += inst->frequency * inv_sr;
      if (inst->phase >= 1.0f) inst->phase -= 1.0f;
      inst->frequency += inst->frequency_slide;
      inst->samples_remaining--;
    }

    /* ── NEW: Music Tone Generator ──
     *
     * Step 1: advance current_volume toward target.
     * Step 2: synthesize a square wave at tone->frequency.
     * Step 3: apply pan (music stays centred, pan_position = 0.0).
     * Step 4: advance the tone's phase (same as SFX oscillator).
     *
     * Critically: tone->phase is never reset here. It ticks continuously
     * so there's no click when the oscillator's volume rises or falls.
     * The only state written here is: current_volume and phase.
     * The sequencer (game_audio_update) writes: frequency, is_playing.
     */
    if (audio->music.is_playing) {
      ToneGenerator *tone = &audio->music.tone;

      /* Volume ramping toward target */
      float target_volume = tone->is_playing ? tone->volume : 0.0f;
      if (tone->current_volume < target_volume) {
        tone->current_volume += VOLUME_RAMP_SPEED;
        if (tone->current_volume > target_volume)
          tone->current_volume = target_volume;
      } else if (tone->current_volume > target_volume) {
        tone->current_volume -= VOLUME_RAMP_SPEED;
        if (tone->current_volume < 0.0f)
          tone->current_volume = 0.0f;
      }

      /* Synthesize only if there's audible volume */
      if (tone->current_volume > 0.0001f || tone->is_playing) {
        float wave   = (tone->phase < 0.5f) ? 1.0f : -1.0f;
        float sample = wave * tone->current_volume * audio->music_volume;
        float lv, rv;
        calculate_pan_volumes(tone->pan_position, &lv, &rv);
        mixed_left  += sample * lv;
        mixed_right += sample * rv;

        /* Advance phase: never reset between fills */
        tone->phase += tone->frequency * inv_sr;
        if (tone->phase >= 1.0f) tone->phase -= 1.0f;
      }
    }

    /* ── Final mix: scale float to int16 range ── */
    mixed_left  *= audio->master_volume * 16000.0f;
    mixed_right *= audio->master_volume * 16000.0f;
    *out++ = clamp_sample(mixed_left);
    *out++ = clamp_sample(mixed_right);
  }
}
```

---

## Step 5: Add `game_audio_update` Call to `game_update`

One line added at the top of `game_update` — BEFORE the game-over and flash-timer checks, so music always advances even when game logic is frozen:

```c
void game_update(GameState *state, GameInput *input, float delta_time) {
  /* Always advance music even when flash animation is frozen */
  game_audio_update(&state->audio, delta_time);  /* NEW */

  /* Handle restart */
  if (state->should_restart) {
    game_init(state);
    if (state->audio.samples_per_second > 0) {
      game_music_play(&state->audio);
      game_play_sound(&state->audio, SOUND_RESTART);
    }
    return;
  }
  /* ... rest unchanged ... */
```

The `game_audio_update` call is intentionally placed at the very top of `game_update`. This ensures the sequencer advances at game-logic rate regardless of whether the game is in a flash-animation pause, or even waiting for the game-over screen. The music should never stutter because the game logic is frozen — those are two independent rhythms.

---

## Step 6: Start Music from `main_raylib.c`

In `main()`, after `game_audio_init`:

```c
game_audio_init(&game_state.audio, platform_game_props.audio.samples_per_second);
game_music_play(&game_state.audio);  /* NEW: start Korobeiniki immediately */
```

The full `main_raylib.c` is at `src/main_raylib.c` — only this one additional call is needed.

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

Build command is identical to lesson 11 — no new source files.

---

## Expected Result

The Korobeiniki plays in the background:
- Music fades in on the first note (no startup click)
- Melody cycles through all four patterns, repeating indefinitely
- Rests in the pattern are silent gaps
- Note transitions are click-free (volume ramping)
- Sound effects play on top of the music simultaneously
- On game over, music stops
- On restart (R), music restarts from the beginning

---

## Common Mistakes

> **Common mistake:** Calling `game_audio_update` INSIDE the flash-timer check (`if (completed_lines.flash_timer.timer > 0) { game_audio_update(...); return; }`). The music would then stop advancing during line-clear animations — you'd hear a rhythmic hitch every time you clear lines. Always call `game_audio_update` unconditionally at the very top of `game_update`, before any early-return logic.

> **Common mistake:** Resetting `tone->phase = 0.0f` when starting a new note in `game_audio_update`. This causes a click at every note boundary — exactly the problem we solved for SFX with `fade_in_samples`. The `ToneGenerator` phase must never be reset. The volume ramp handles the transition cleanly.

> **Common mistake:** Using `step_timer = 0` instead of `step_timer -= step_duration`. Over time, rounding errors accumulate and the tempo drifts. With subtraction, each step fires at exactly `step_duration` intervals regardless of frame-rate variance.

---

## Exercises

1. Change `step_duration` from `0.15f` to `0.12f`. Listen to how the tempo increases. What BPM does each value correspond to? (Hint: steps per beat depends on how you count the melody.)
2. Add a second pattern sequence that plays a simple bass line simultaneously. What fields would you need to add to `MusicSequencer`?
3. Make `music_volume` decrease by `0.05f` each level (the background music gets quieter as the game gets harder, so SFX are more prominent). Where is the cleanest place to do this?
4. The Korobeiniki has 4 patterns. Change `MUSIC_NUM_PATTERNS` to 2 and only fill patterns 0 and 1. Verify the melody loops correctly after pattern 1.

## What's Next

**Lesson 13** adds the finishing touches: a **ghost piece** (translucent preview of where the piece lands), a **game-over overlay**, and a clean restart flow. After lesson 13, the game is complete.
