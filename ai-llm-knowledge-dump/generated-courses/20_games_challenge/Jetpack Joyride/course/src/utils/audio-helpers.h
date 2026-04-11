#ifndef UTILS_AUDIO_HELPERS_H
#define UTILS_AUDIO_HELPERS_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/audio-helpers.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 10 — Waveform generators (square, sine, triangle, sawtooth),
 *             MIDI-to-frequency conversion, stereo panning, sample clamping.
 *
 * These are pure, stateless helper functions with no dependencies other than
 * <math.h> and <stdint.h>.  Copy them verbatim into any game course; they
 * never need modification.
 *
 * Waveform convention
 * ───────────────────
 *   • `phase` is in [0.0, 1.0) — the fractional part of one oscillator cycle.
 *   • Return value is in [-1.0, +1.0].
 *   • Advance phase each sample: `phase = fmodf(phase + freq * inv_sr, 1.0f)`
 *
 * Volume scale convention
 * ───────────────────────
 *   • These helpers return [-1, +1].  Multiply by `volume * master_volume`
 *     before passing to audio_write_sample().  Use audio_clamp_sample() on
 *     the final mix if multiple voices may sum above ±1.0.
 *
 * References
 * ──────────
 *   games/snake/src/utils/audio-helpers.h   — original source
 *   engine/docs/audio-architecture.md        — SoundSource oscillator design
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── MIDI conversion ───────────────────────────────────────────────────────
 * LESSON 13 — Convert a MIDI note number to frequency in Hz.
 *
 *   A4  = MIDI 69 = 440 Hz  (concert pitch reference)
 *   C4  = MIDI 60 = 261.63 Hz
 *   C5  = MIDI 72 = 523.25 Hz
 *   REST = 0       → 0 Hz (use this to encode a silence step)
 *
 * Formula: freq = 440 × 2^((note − 69) / 12)
 *
 * JS analogy: AudioContext.createOscillator().frequency.value =
 *             440 * Math.pow(2, (note - 69) / 12)                           */
static inline float audio_midi_to_freq(int note) {
  if (note == 0) return 0.0f;
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* ─── Waveform generators ───────────────────────────────────────────────────
 * LESSON 09/10 — All take `phase` in [0, 1) and return sample in [-1, +1].
 *
 * audio_wave_square   — Hard clip: +1 for first half, -1 for second.
 *                       Bright, buzzy timbre. Classic chiptune sound.
 *
 * audio_wave_sine     — Pure sinusoidal. Soft, smooth timbre. Good for
 *                       beeps and pure tones.
 *
 * audio_wave_triangle — Linear ramp up then down. Softer than square;
 *                       hollow, flute-like quality.
 *
 * audio_wave_sawtooth — Linear ramp from -1 to +1. Bright, harsh timbre.
 *                       Good for synth bass and leads.                      */

static inline float audio_wave_square(float phase) {
  return (phase < 0.5f) ? 1.0f : -1.0f;
}

static inline float audio_wave_sine(float phase) {
  return sinf(phase * 2.0f * (float)M_PI);
}

static inline float audio_wave_triangle(float phase) {
  if (phase < 0.25f) return phase * 4.0f;
  if (phase < 0.75f) return 2.0f - phase * 4.0f;
  return phase * 4.0f - 4.0f;
}

static inline float audio_wave_sawtooth(float phase) {
  return 2.0f * phase - 1.0f;
}

/* ─── Sample clamping ───────────────────────────────────────────────────────
 * LESSON 16 — Clamp a float mix value to [-1.0, +1.0] (float PCM range).
 *
 * Call this on the final mix when multiple voices may sum above ±1.0.
 * The driver (Raylib/ALSA) clips gracefully, but an explicit clamp here
 * makes the saturation audibly soft rather than a hard DAC clip.
 *
 * Usage:
 *   float s = wave * volume * master_vol;
 *   audio_write_sample(buf, i, audio_clamp_sample(mix_l),
 *                               audio_clamp_sample(mix_r));                 */
static inline float audio_clamp_sample(float sample) {
  if (sample >  1.0f) return  1.0f;
  if (sample < -1.0f) return -1.0f;
  return sample;
}

/* ─── Stereo panning ────────────────────────────────────────────────────────
 * LESSON 10 — Linear equal-amplitude pan.
 *
 *   pan = -1.0 → left=1.0, right=0.0   (full left)
 *   pan =  0.0 → left=1.0, right=1.0   (centre, both channels full)
 *   pan = +1.0 → left=0.0, right=1.0   (full right)
 *
 * This is a simple linear pan law.  For more accurate perceived-loudness
 * panning, use a constant-power (√2) pan law instead.  Linear is correct
 * for educational purposes and sounds fine for game effects.               */
static inline void audio_calculate_pan(float pan,
                                       float *left_vol, float *right_vol) {
  if (pan <= 0.0f) {
    *left_vol  = 1.0f;
    *right_vol = 1.0f + pan;   /* pan is negative, so right fades */
  } else {
    *left_vol  = 1.0f - pan;   /* pan is positive, so left fades */
    *right_vol = 1.0f;
  }
}

#endif /* UTILS_AUDIO_HELPERS_H */
