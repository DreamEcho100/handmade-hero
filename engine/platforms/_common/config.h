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
  i32 samples_per_second; // e.g., 48000 Hz
  i32 bytes_per_sample;   // e.g., 4 (16-bit stereo)
  i32 buffer_size_bytes;  // Total buffer size _(secondary_buffer_size)_

  /** Maximum number of samples to process per audio callback */
  u32 max_samples_per_call;

  // Timing (updated each frame)
  i64 running_sample_index; // Total samples written (ever)

  // Latency
  i32 latency_samples; // Buffer size in samples
  i32 safety_samples;  // Safety margin (~1/3 frame)

  // Game timing
  i32 game_update_hz; // Game logic rate (e.g., 30 Hz)

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
  u32 window_width;
  u32 window_height;

  // Capabilities
  u32 cpu_core_count;
  u64 page_size;
  u32 max_controllers;

  // Feature flags
  bool vsync_enabled;
  bool has_keyboard;
  bool has_mouse;
  bool has_gamepads;

  // Timing
  f32 seconds_per_frame;
} PlatformConfig;

#endif /* DE100_GAME__COMMON_CONFIG_H */
