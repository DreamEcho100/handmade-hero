#ifndef ENGINE_PLATFORM_RAYLIB_AUDIO_H
#define ENGINE_PLATFORM_RAYLIB_AUDIO_H

#include "../../_common/base.h"
#include "../../_common/memory.h"
#include "../../game/audio.h"
#include "../../game/game-loader.h"
#include "../../game/memory.h"
#include "../_common/audio.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 RAYLIB SOUND OUTPUT STATE
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  AudioStream stream;
  bool stream_valid;
  bool stream_playing;

  // Buffer configuration
  uint32_t buffer_size_frames;

  // Sample buffer for game to fill
  PlatformMemoryBlock sample_buffer;
  uint32_t sample_buffer_size;

  // Track write statistics
  int64_t total_samples_written;
  int32_t writes_this_period;
  double last_stats_time;

} RaylibSoundOutput;

extern RaylibSoundOutput g_raylib_audio_output;

// ═══════════════════════════════════════════════════════════════════════════
// 🔊 PUBLIC FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

bool raylib_init_audio(GameAudioOutputBuffer *audio_output,
                       PlatformAudioConfig *audio_config,
                       int32_t samples_per_second, int32_t buffer_size_frames,
                       int32_t game_update_hz);

void raylib_shutdown_audio(GameAudioOutputBuffer *audio_output,
                           PlatformAudioConfig *audio_config);

/**
 * Check if we should generate audio this frame.
 * Returns number of samples to generate, or 0 if buffer is full.
 */
int32_t raylib_get_samples_to_write(PlatformAudioConfig *audio_config,
                                    GameAudioOutputBuffer *audio_output);

/**
 * Send samples to Raylib audio stream.
 */
void raylib_send_samples(PlatformAudioConfig *audio_config,
                         GameAudioOutputBuffer *source);

void raylib_clear_audio_buffer(PlatformAudioConfig *audio_config);

void raylib_debug_audio_latency(PlatformAudioConfig *audio_config);

void raylib_audio_fps_change_handling(GameAudioOutputBuffer *audio_output,
                                      PlatformAudioConfig *audio_config);
void raylib_debug_audio_overlay(void);

#endif // ENGINE_PLATFORM_RAYLIB_AUDIO_H
