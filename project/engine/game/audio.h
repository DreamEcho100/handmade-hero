#ifndef ENGINE_GAME_AUDIO_H
#define ENGINE_GAME_AUDIO_H

#include <stdint.h>
#include "../_common/base.h"

typedef struct {
  // ═════════════════════════════════════════════════════════
  // HARDWARE PARAMETERS (Platform Layer Owns)
  // ═════════════════════════════════════════════════════════
  bool is_initialized;

  int32_t samples_per_second; // 48000 Hz (hardware config)
  int32_t bytes_per_sample;   // 4 (16-bit stereo)

  // ═════════════════════════════════════════════════════════
  // AUDIO GENERATION STATE (Platform Layer Uses)
  // ═════════════════════════════════════════════════════════
  int64_t running_sample_index; // Sample counter (for waveform)
  int wave_period;              // Samples per wave (cached calculation)
  real32 t_sine;                // Phase accumulator (0 to 2π)
  int latency_sample_count;     // Target latency in samples

  // ═════════════════════════════════════════════════════════
  // GAME-SET PARAMETERS (Game Layer Sets)
  // ═════════════════════════════════════════════════════════
  int tone_hz;         // Frequency of tone to generate
  int16_t tone_volume; // Volume of tone to generate
  int pan_position;    // -100 (left) to +100 (right)
  int32_t game_update_hz;

  // Day 20 safety margin
  int32_t safety_sample_count; // Samples of safety buffer (~1/3 frame)
} GameSoundOutput;


#endif // ENGINE_GAME_AUDIO_H
