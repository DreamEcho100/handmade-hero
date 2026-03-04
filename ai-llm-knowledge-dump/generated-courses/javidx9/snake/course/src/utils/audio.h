/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio.h  —  Snake Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * WHAT IS THIS FILE?
 * ──────────────────
 * Defines every audio type the Snake game uses.
 *
 * The platform (main_x11.c / main_raylib.c) fills an AudioOutputBuffer each
 * frame.  The game writes raw PCM samples into it through
 * game_get_audio_samples().
 *
 * This is a simplified version of the Tetris audio system — no music
 * sequencer, no ToneGenerator.  Just sound effects: food-eaten (spatially
 * panned) and game-over (centred).  Compare with the Tetris utils/audio.h to
 * see how the same foundation scales up to a full music system.
 *
 * JS background note: if you've used the Web Audio API, think of this file as
 * implementing a small software synth that replaces AudioContext,
 * OscillatorNode, GainNode, and StereoPannerNode — except instead of the
 * browser managing these for you, we write every sample ourselves in a tight
 * loop.
 *
 * COURSE NOTE: The reference source has no audio at all.  This system is
 * added as a course enhancement.  See Lesson 11 and
 * COURSE-BUILDER-IMPROVEMENTS.md.
 */

#ifndef UTILS_AUDIO_H
#define UTILS_AUDIO_H

#include <math.h> /* (reserved for future use; kept for student experimentation) */
#include <stdint.h> /* int16_t, int64_t */

/* ══════ AudioOutputBuffer ══════════════════════════════════════════════════
 *
 * WHO FILLS WHAT
 * ──────────────
 *   Platform fills:   samples, samples_per_second, sample_count
 *   Game writes into: samples  (game_get_audio_samples writes PCM data here)
 *
 * MEMORY LAYOUT — Interleaved Stereo
 * ────────────────────────────────────
 *   Index:  0     1     2     3     4     5  …
 *   Value:  L[0]  R[0]  L[1]  R[1]  L[2]  R[2] …
 *
 *   To write sample pair N:
 *     samples[N * 2 + 0] = left_sample;
 *     samples[N * 2 + 1] = right_sample;
 *
 * JS equivalent: the Float32Array from AudioContext.createScriptProcessor()
 * but as raw int16 instead of float32.
 */
typedef struct {
  int16_t *samples;       /* Interleaved stereo PCM; platform allocates    */
  int samples_per_second; /* 44100 or 48000                            */
  int sample_count;       /* Stereo pairs to generate; buffer byte size =
                             sample_count × 2 × sizeof(int16_t)            */
} AudioOutputBuffer;

/* ══════ Sound Effect Identifiers ═══════════════════════════════════════════
 *
 * JS: `const enum SoundID` or a string-union type.
 * C:  `typedef enum` — each constant gets a unique integer starting at 0.
 *
 * SOUND_NONE = 0 also acts as a sentinel (uninitialized slot = no sound).
 * SOUND_COUNT is a trick: the last enum value equals the total count,
 * so you can size an array as [SOUND_COUNT] without a magic number.
 */
typedef enum {
  SOUND_NONE = 0,   /* No sound / empty slot                           */
  SOUND_FOOD_EATEN, /* Played when snake eats food — panned by food_x  */
  SOUND_GROW,       /* Short blip each step the snake extends          */
  SOUND_RESTART,    /* Rising sweep jingle when player restarts        */
  SOUND_GAME_OVER,  /* Played when game ends — centred                 */
  SOUND_COUNT       /* ALWAYS last — equals total number of sounds     */
} SOUND_ID;

/* ══════ SoundInstance — One Playing Sound Effect ═══════════════════════════
 *
 * THE PHASE ACCUMULATOR PATTERN
 * ──────────────────────────────
 * `phase` crawls from 0.0 toward 1.0 each sample and wraps back:
 *
 *   phase += frequency / sample_rate;   // advance one sample
 *   if (phase >= 1.0f) phase -= 1.0f;   // wrap (never reset!)
 *
 * One full cycle (0→1) = one full cycle of the waveform.
 * For a square wave: wave = (phase < 0.5f) ? 1.0f : -1.0f;
 *
 * At 440 Hz, 44100 Hz sample rate:
 *   phase advance per sample = 440 / 44100 ≈ 0.00997
 *   samples per full cycle   = 44100 / 440 ≈ 100.2 samples
 *
 * JS equivalent: Web Audio OscillatorNode — the browser manages phase
 * internally.  Here we implement the same math manually.
 *
 * ⚠ CRITICAL: NEVER reset phase between buffer fills!
 *   A phase discontinuity between fills creates a waveform jump → audible
 *   "click" or "pop".  phase must carry over from one fill to the next.
 *   Only reset phase when a NEW sound starts (in game_play_sound_at).
 */
#define MAX_SIMULTANEOUS_SOUNDS 4 /* Polyphony limit — tune if needed */

typedef struct {
  SOUND_ID sound_id; /* Which preset is playing                       */

  float phase; /* Oscillator phase [0.0, 1.0).
                  ⚠ NEVER reset between buffer fills!          */

  int samples_remaining; /* Countdown to 0 — slot is free when 0.
                            The game mix loop skips slots where this = 0. */

  float volume; /* Amplitude scalar: 0.0 = silent, 1.0 = full.  */

  float frequency; /* Current pitch in Hz — slides if freq_slide≠0. */

  float frequency_slide; /* Hz to add per sample (> 0 = rise, < 0 = fall).
                            Creates the "swooping" quality on game-over.  */

  int total_samples; /* Total duration in samples (set once at start).
                        Used to compute the fade-out envelope ratio.  */

  int fade_in_samples; /* Ramp-up samples (~10 ms) to prevent click at
                          attack. ≈ sample_rate / 100.                 */

  float pan_position; /* Stereo pan: −1.0 full left, 0.0 centre, 1.0
                         full right.  SOUND_FOOD_EATEN uses this for
                         spatial feedback — food on the left wall pans
                         left; food on the right wall pans right.      */
} SoundInstance;

/* ══════ GameAudioState ══════════════════════════════════════════════════════
 *
 * Embedded directly in GameState (see game.h).
 * No hidden globals — all audio state is visible through state->audio.
 *
 * Handmade Hero principle: "State is always visible."
 *   No static locals, no global singletons — everything lives in GameState.
 */
typedef struct {
  SoundInstance active_sounds[MAX_SIMULTANEOUS_SOUNDS];
  /* Pool of playing SFX.  Slots are reused when
     samples_remaining drops to 0.                 */
  int active_sound_count; /* How many slots are in use (informational).    */

  float master_volume; /* Overall mix scale [0.0 .. 1.0].              */
  float sfx_volume;    /* SFX sub-mix scale [0.0 .. 1.0].              */

  int samples_per_second; /* Set by main() before calling game_audio_init.
                             Never changes at runtime.                     */

  /* ── Music sequencer ─────────────────────────────────────────────────
   *
   * The music channel is SEPARATE from the SFX pool (active_sounds[]).
   * It continuously advances through a melody table stored in audio.c,
   * looping back to the start when the last note finishes.
   *
   * music_enabled         1 = playing; 0 = stopped (game over).
   * music_note_idx        Index of the currently playing note in MELODY[].
   * music_note_samples_left  Samples remaining for the current note.
   * music_fade_in         Counts down from MUSIC_FADE_IN_SAMPLES at each
   *                       note boundary — avoids click from phase reset.
   * music_phase           Oscillator phase [0.0, 1.0) — reset per note
   *                       boundary (not between fills) so each note starts
   *                       at a predictable position.
   * music_frequency       Current note's frequency in Hz (0 = rest).
   * music_volume          Per-channel gain for the music (set by
   *                       game_music_start; independent of sfx_volume). */
  int music_enabled;
  int music_note_idx;
  int music_note_samples_left;
  int music_fade_in;
  float music_phase;
  float music_frequency;
  float music_volume;

} GameAudioState;

/* ══════ Music Sequencer Constants ══════════════════════════════════════════
 *
 * MUSIC_FADE_IN_SAMPLES — number of samples over which a new note fades in.
 * A note that starts abruptly (phase reset to 0 at a non-zero level) would
 * produce an audible click.  Ramping the gain from 0→1 over ~2 ms (~88 samples
 * at 44100 Hz) eliminates that click without audible effect on the note itself.
 */
#define MUSIC_FADE_IN_SAMPLES 88 /* ~2 ms at 44100 Hz */

/*
 * `static inline` — private to each translation unit that includes this header.
 * Inlined at call sites — zero function-call overhead.
 * JS: like a small utility function unexported from a module.
 */

/* clamp_sample — clamp a float to the int16_t range [-32768, 32767].
 *
 * WHY CLAMP?
 *   Mixing multiple sounds adds their amplitudes — the sum can exceed ±32767.
 *   Without clamping, the cast to int16_t wraps around → harsh digital
 * clipping. Saturation (clamping) is gentler — it clips the peaks cleanly.
 *
 * JS equivalent: Math.max(-32768, Math.min(32767, sample))
 */
static inline int16_t clamp_sample(float sample) {
  if (sample > 32767.0f)
    return 32767;
  if (sample < -32768.0f)
    return -32768;
  return (int16_t)sample;
}

/* calculate_pan_volumes — compute per-channel gains from a pan position.
 *
 * LINEAR PANNING LAW:
 *   pan  <  0  → left = 1.0, right = 1.0 + pan   (pan is negative, reducing
 * right) pan  =  0  → left = 1.0, right = 1.0          (centre — equal) pan  >
 * 0  → left = 1.0 - pan, right = 1.0   (reducing left)
 *
 * Example pan = -0.5 (half left):  left_vol = 1.0, right_vol = 0.5
 * Example pan =  0.0 (centre):     left_vol = 1.0, right_vol = 1.0
 * Example pan = +0.5 (half right): left_vol = 0.5, right_vol = 1.0
 *
 * JS equivalent: Web Audio StereoPannerNode (handled by the browser).
 *   Here we implement it manually so you can see exactly how it works.
 */
static inline void calculate_pan_volumes(float pan, float *left_vol,
                                         float *right_vol) {
  if (pan <= 0.0f) {
    *left_vol = 1.0f;
    *right_vol = 1.0f + pan; /* pan is ≤ 0 so this reduces right volume */
  } else {
    *left_vol = 1.0f - pan; /* pan is > 0 so this reduces left volume  */
    *right_vol = 1.0f;
  }
}

#endif /* UTILS_AUDIO_H */
