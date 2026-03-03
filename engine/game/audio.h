#ifndef DE100_GAME_AUDIO_H
#define DE100_GAME_AUDIO_H

#include "../_common/base.h"
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// SOUND SOURCE (Individual audio generator)
// ═══════════════════════════════════════════════════════════════════════════
//
// A simple oscillator with smooth volume transitions.
// Used for continuous tones, music notes, etc.
//
// KEY FEATURE: current_volume provides click-free volume changes.
//
// Usage:
//   SoundSource tone = {0};
//   tone.frequency = 440.0f;
//   tone.volume = 0.5f;        // Target volume
//   tone.current_volume = 0.0f; // Start silent (ramps up)
//   tone.is_playing = true;
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  f32 phase;            // Phase accumulator (0 to 1) - PERSISTS between calls!
  f32 frequency;        // Current frequency (Hz)
  f32 target_frequency; // For smooth frequency transitions (future)
  f32 volume;           // Target volume (0.0 to 1.0)
  f32 current_volume;   // Smoothed volume for click-free transitions
  f32 pan_position;     // -1.0 (left) to 1.0 (right)
  bool is_playing;
} SoundSource;

// ═══════════════════════════════════════════════════════════════════════════
// GAME AUDIO STATE (Lives in game memory - MINIMAL)
// ═══════════════════════════════════════════════════════════════════════════
//
// This is a MINIMAL audio state for simple games.
// Games with more complex audio (SFX, music) should define their own
// GameAudioState in their game code, using the helpers from audio-helpers.h
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  SoundSource tone;  // Simple oscillator
  f32 master_volume; // Global volume multiplier
} GameAudioState;

// ═══════════════════════════════════════════════════════════════════════════
// SOUND OUTPUT BUFFER (Platform → Game → Platform)
// ═══════════════════════════════════════════════════════════════════════════
//
// Platform allocates memory, tells game how many samples to generate,
// game fills buffer, platform sends to hardware.
//
// Casey's equivalent: game_audio_output_buffer
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  i32 samples_per_second; // Sample rate (e.g., 48000)
  i32 sample_count;       // How many samples to generate THIS call
  void *samples;          // Pointer to sample buffer (i16 interleaved stereo)
} GameAudioOutputBuffer;

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO HELPER MACROS
// ═══════════════════════════════════════════════════════════════════════════

// Default volume ramp speed (~2ms at 48kHz = ~96 samples)
// 1.0 / 96 ≈ 0.01, but we use 0.002 for smoother transitions
#define DE100_AUDIO_VOLUME_RAMP_SPEED 0.002f

// Standard fade times in samples (at 48kHz)
#define DE100_AUDIO_FADE_MS_TO_SAMPLES(ms, sample_rate)                        \
  ((i32)((f32)(ms) * (f32)(sample_rate) / 1000.0f))

#endif // DE100_GAME_AUDIO_H
