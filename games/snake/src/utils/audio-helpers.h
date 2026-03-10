#ifndef AUDIO_HELPERS_H
#define AUDIO_HELPERS_H

#include <math.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Utility Functions
 *
 * Small, reusable helpers for procedural audio synthesis.
 * Used by game/audio.c for SFX and music generation.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── MIDI Conversion ─────────────────────────────────────────────────────
 */

/* Convert MIDI note number to frequency (A4 = 69 = 440Hz).
 * Note 0 = rest/silence. */
static inline float audio_midi_to_freq(int32_t note) {
  if (note == 0)
    return 0.0f;
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

/* ─── Waveform Generators ─────────────────────────────────────────────────
 */

static inline float audio_wave_square(float phase) {
  return (phase < 0.5f) ? 1.0f : -1.0f;
}

static inline float audio_wave_sine(float phase) {
  return sinf(phase * 2.0f * (float)M_PI);
}

static inline float audio_wave_triangle(float phase) {
  if (phase < 0.25f)
    return phase * 4.0f;
  if (phase < 0.75f)
    return 2.0f - phase * 4.0f;
  return phase * 4.0f - 4.0f;
}

static inline float audio_wave_sawtooth(float phase) {
  return 2.0f * phase - 1.0f;
}

/* ─── Sample Clamping ─────────────────────────────────────────────────────
 */

static inline int16_t audio_clamp_sample(float sample) {
  if (sample > 32767.0f)
    return 32767;
  if (sample < -32768.0f)
    return -32768;
  return (int16_t)sample;
}

/* ─── Stereo Panning ──────────────────────────────────────────────────────
 */

static inline void audio_calculate_pan(float pan, float *left_vol,
                                       float *right_vol) {
  if (pan <= 0.0f) {
    *left_vol = 1.0f;
    *right_vol = 1.0f + pan;
  } else {
    *left_vol = 1.0f - pan;
    *right_vol = 1.0f;
  }
}

#endif // AUDIO_HELPERS_H
