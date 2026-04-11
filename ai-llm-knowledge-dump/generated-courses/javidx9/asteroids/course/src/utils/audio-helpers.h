#ifndef UTILS_AUDIO_HELPERS_H
#define UTILS_AUDIO_HELPERS_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio-helpers.h — Waveform generators, MIDI conversion, panning
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Pure, stateless helper functions — no dependencies other than <math.h>.
 *
 * Waveform convention
 * ───────────────────
 *   • `phase` is in [0.0, 1.0) — the fractional part of one oscillator cycle.
 *   • Return value is in [-1.0, +1.0].
 *   • Advance phase each sample: `phase = fmodf(phase + freq * inv_sr, 1.0f)`
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── MIDI conversion ────────────────────────────────────────────────────
 * Convert a MIDI note number to frequency in Hz.
 *
 *   A4  = MIDI 69 = 440 Hz  (concert pitch reference)
 *   C4  = MIDI 60 = 261.63 Hz
 *   REST = 0       → 0 Hz (silence)
 *
 * Formula: freq = 440 × 2^((note − 69) / 12)                             */
static inline float audio_midi_to_freq(int note) {
  if (note == 0) return 0.0f;
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* ─── Waveform generators ──────────────────────────────────────────────── */

/* audio_wave_square — +1 for first half of cycle, -1 for second.
 * Bright, buzzy timbre. Classic chiptune / Asteroids arcade sound.       */
static inline float audio_wave_square(float phase) {
  return (phase < 0.5f) ? 1.0f : -1.0f;
}

/* audio_wave_sine — pure sinusoidal. Soft, smooth timbre.                */
static inline float audio_wave_sine(float phase) {
  return sinf(phase * 2.0f * (float)M_PI);
}

/* audio_wave_triangle — linear ramp up then down. Hollow, flute-like.   */
static inline float audio_wave_triangle(float phase) {
  if (phase < 0.25f) return phase * 4.0f;
  if (phase < 0.75f) return 2.0f - phase * 4.0f;
  return phase * 4.0f - 4.0f;
}

/* audio_wave_sawtooth — linear ramp from -1 to +1. Bright, harsh.       */
static inline float audio_wave_sawtooth(float phase) {
  return 2.0f * phase - 1.0f;
}

/* ─── Sample clamping ────────────────────────────────────────────────────
 * Clamp a float mix value to [-1.0, +1.0] (float PCM range).
 * Call this on the final mix when multiple voices may sum above ±1.0.   */
static inline float audio_clamp_sample(float sample) {
  if (sample >  1.0f) return  1.0f;
  if (sample < -1.0f) return -1.0f;
  return sample;
}

/* ─── Stereo panning ─────────────────────────────────────────────────────
 * Linear equal-amplitude pan.
 *
 *   pan = -1.0 → left=1.0, right=0.0   (full left)
 *   pan =  0.0 → left=1.0, right=1.0   (centre, both channels full)
 *   pan = +1.0 → left=0.0, right=1.0   (full right)
 *
 * Asteroids uses this to pan asteroid explosions based on screen X:
 *   pan = (asteroid.x / GAME_UNITS_W) * 2.0f - 1.0f                     */
static inline void audio_calculate_pan(float pan,
                                       float *left_vol, float *right_vol) {
  if (pan <= 0.0f) {
    *left_vol  = 1.0f;
    *right_vol = 1.0f + pan;
  } else {
    *left_vol  = 1.0f - pan;
    *right_vol = 1.0f;
  }
}

#endif /* UTILS_AUDIO_HELPERS_H */
