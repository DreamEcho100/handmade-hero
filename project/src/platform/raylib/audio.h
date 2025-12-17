#ifndef RAYLIB_AUDIO_H
#define RAYLIB_AUDIO_H

#include "../base.h"
#include <raylib.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════
// 🔊 AUDIO STATE (Day 7-9)
// ═══════════════════════════════════════════════════════════════
// This mirrors your LinuxSoundOutput but simplified for Raylib
// ═══════════════════════════════════════════════════════════════

typedef struct {
  // Raylib audio stream handle
  AudioStream stream;
  bool is_initialized;

  // Audio format parameters (Casey's Day 7)
  int32_t samples_per_second; // 48000 Hz
  int32_t bytes_per_sample;   // 4 (16-bit stereo)

  // Sound generation state (Casey's Day 8)
  uint32_t running_sample_index;
  int tone_hz;
  int16_t tone_volume;
  int wave_period;

  // Day 9: Sine wave
  real32 t_sine;            // Phase accumulator
  int latency_sample_count; // Buffer ahead amount

  // Your panning extension
  int pan_position; // -100 (left) to +100 (right)
} RaylibSoundOutput;

extern RaylibSoundOutput g_sound_output;
void raylib_audio_callback(void *buffer, unsigned int frames);
void raylib_init_audio(void);
void set_tone_frequency(int hz);
void handle_update_tone_frequency(int hz_to_add);
void handle_increase_pan(int num);

void set_tone_frequency(int hz);
void handle_update_tone_frequency(int hz_to_add);
void handle_increase_volume(int num);
void handle_musical_keypress(int key);
void handle_increase_pan(int num);
void raylib_debug_audio(void);

#endif // RAYLIB_AUDIO_H
#define RAYLIB_AUDIO_H
