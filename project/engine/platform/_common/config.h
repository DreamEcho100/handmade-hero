#ifndef DE100_GAME__COMMON_CONFIG_H
#define DE100_GAME__COMMON_CONFIG_H

#include "../../_common/base.h"


// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM AUDIO CONFIG
// ═══════════════════════════════════════════════════════════════════════════
//
// Hardware and timing configuration for audio subsystem.
// Filled by platform, read by audio code.
//
// Casey's equivalent: win32_sound_output
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // Hardware (set once at init)
  int32 samples_per_second; // e.g., 48000 Hz
  int32 bytes_per_sample;   // e.g., 4 (16-bit stereo)
  int32 buffer_size_bytes;  // Total buffer size _(secondary_buffer_size)_


  /** Maximum number of samples to process per audio callback */
  uint32 max_samples_per_call;

  // Timing (updated each frame)
  int64 running_sample_index; // Total samples written (ever)

  // Latency
  int32 latency_samples; // Buffer size in samples
  int32 safety_samples;  // Safety margin (~1/3 frame)

  // Game timing
  int32 game_update_hz; // Game logic rate (e.g., 30 Hz)

  bool is_initialized;
} PlatformAudioConfig;

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM CONFIG
// ═══════════════════════════════════════════════════════════════════════════
//
// Authoritative platform capabilities and state.
// Read-only from game's perspective.
//
// Casey's equivalent: Scattered across win32_* structs and globals
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // Audio
  PlatformAudioConfig audio;

  // Display
  uint32 window_width;
  uint32 window_height;
  uint32 monitor_refresh_hz;

  // Capabilities
  uint32 cpu_core_count;
  uint64 page_size;
  uint32 max_controllers;

  // Feature flags
  bool vsync_enabled;
  bool has_keyboard;
  bool has_mouse;
  bool has_gamepads;

  // Timing
  real32 seconds_per_frame;
} PlatformConfig;

#endif /* DE100_GAME__COMMON_CONFIG_H */
