#ifndef DE100_GAME_AUDIO_H
#define DE100_GAME_AUDIO_H

#include "../_common/base.h"
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * SOUND SOURCE (Individual audio generator)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * A simple oscillator with smooth volume transitions.
 * Used for continuous tones, music notes, etc.
 *
 * Games can use this directly or define their own structures.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  f32 phase;            /* Phase accumulator (0 to 1) */
  f32 frequency;        /* Current frequency (Hz) */
  f32 target_frequency; /* For smooth frequency transitions */
  f32 volume;           /* Target volume (0.0 to 1.0) */
  f32 current_volume;   /* Smoothed volume for click-free transitions */
  f32 pan_position;     /* -1.0 (left) to 1.0 (right) */
  bool is_playing;
} SoundSource;

/* ═══════════════════════════════════════════════════════════════════════════
 * GAME AUDIO STATE (Minimal - games extend this)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This is a MINIMAL audio state. Games with more complex audio should
 * define their own GameAudioState in their game code.
 *
 * Example: Snake game defines its own with MusicSequencer, SoundInstance[],
 * etc.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  SoundSource tone;  /* Simple oscillator for basic games */
  f32 master_volume; /* Global volume multiplier */
} GameAudioState;

/* ═══════════════════════════════════════════════════════════════════════════
 * AUDIO SAMPLE FORMAT
 *
 * Describes the PCM format that the platform backend expects the game to
 * write into samples_buffer.  The platform sets this once during init;
 * the game reads it and uses audio_write_sample() (see audio-helpers.h) so
 * that format conversion always happens at the platform boundary, never
 * inside game mixing code.
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef enum {
  AUDIO_FORMAT_I16 = 0, /* 16-bit signed — ALSA S16_LE, Raylib default      */
  AUDIO_FORMAT_I32,     /* 32-bit signed — professional interfaces            */
  AUDIO_FORMAT_F32,     /* 32-bit float  — CoreAudio, WASAPI, Web Audio       */
  AUDIO_FORMAT_F64,     /* 64-bit float  — DAWs, offline rendering            */
} AudioSampleFormat;

/* Bytes per *stereo frame* (L + R) for a given format. */
static inline int audio_format_bytes_per_sample(AudioSampleFormat fmt) {
  switch (fmt) {
  case AUDIO_FORMAT_I16:
    return (int)(sizeof(i16) * 2);
  case AUDIO_FORMAT_I32:
    return (int)(sizeof(i32) * 2);
  case AUDIO_FORMAT_F32:
    return (int)(sizeof(f32) * 2);
  case AUDIO_FORMAT_F64:
    return (int)(sizeof(f64) * 2);
  }
  return (int)(sizeof(i16) * 2); /* safe default */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SOUND OUTPUT BUFFER (Platform → Game → Platform)
 *
 * This is the ONLY shared audio contract between the platform layer and
 * game code. Platform fills the metadata and allocates `samples`; the game
 * writes raw PCM into it via game_get_audio_samples().
 *
 * The `format` field is set by the backend during init.  Game code must use
 * audio_write_sample() from audio-helpers.h — never cast samples_buffer to
 * i16* directly.  This keeps all format conversion at the platform boundary.
 *
 * Deliberately minimal — backends own all backend-specific state privately.
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  i32 samples_per_second; /* Sample rate (e.g., 48000) — set once at init */
  i32 sample_count;     /* Samples to generate THIS call (≤ max_sample_count) */
  i32 max_sample_count; /* Buffer capacity in frames — never exceed this */
  void *samples_buffer; /* Opaque PCM buffer — format described by `format`  */
  AudioSampleFormat
      format;          /* Set by backend; use audio_write_sample() to write */
  bool is_initialized; /* Platform audio subsystem is up; safe to generate */
} GameAudioOutputBuffer;

#endif /* DE100_GAME_AUDIO_H */
