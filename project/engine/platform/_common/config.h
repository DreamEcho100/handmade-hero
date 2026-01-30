#ifndef DE100_GAME__COMMON_CONFIG_H
#define DE100_GAME__COMMON_CONFIG_H

#include "../../_common/base.h"

// ═══════════════════════════════════════════════════════════════════════════
// PLATFORM AUDIO CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════
// This structure is shared across all platform backends.
// Each backend fills in the values during initialization.
//
// Casey's DirectSound equivalent: win32_audio_output
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // ─────────────────────────────────────────────────────────────────────
  // INITIALIZATION STATE
  // ─────────────────────────────────────────────────────────────────────
  bool is_initialized;

  // ─────────────────────────────────────────────────────────────────────
  // HARDWARE CONFIGURATION (set once at init)
  // ─────────────────────────────────────────────────────────────────────
  int32 samples_per_second;    // e.g., 48000 Hz // audio_buffer_size_frames
  int32 bytes_per_sample;      // e.g., 4 (16-bit stereo = 2 * 2) //
  int32 secondary_buffer_size; // Total buffer size in bytes

  // ─────────────────────────────────────────────────────────────────────
  // TIMING STATE (updated each frame)
  // ─────────────────────────────────────────────────────────────────────
  int64 running_sample_index; // Total samples written (ever) //
                              // audio_latency_frames

  // ─────────────────────────────────────────────────────────────────────
  // LATENCY CONFIGURATION
  // ─────────────────────────────────────────────────────────────────────
  int32 latency_sample_count; // Buffer size in samples
  int32 safety_sample_count;  // Safety margin (1/3 frame)

  // ─────────────────────────────────────────────────────────────────────
  // GAME TIMING
  // ─────────────────────────────────────────────────────────────────────
  int32 game_update_hz; // Game logic rate (e.g., 30 Hz)

} PlatformAudioConfig;

/**
 * @brief Describes the actual capabilities and state of the platform.
 *
 * PlatformConfig contains **authoritative, platform-determined values**. This
 * is what the game must observe at runtime. It is read-only and is typically
 * initialized once at startup.
 */
typedef struct {

  /* =========================
     AUDIO BACKEND (ACTUAL)
     ========================= */
  PlatformAudioConfig audio;

  /* =========================
     PLATFORM IDENTITY
     ========================= */

  /** Flags describing platform capabilities or properties */
  uint32 platform_flags;

  /** Number of CPU cores available */
  uint32 cpu_core_count;

  /** OS page size in bytes */
  uint64 page_size;

  /* =========================
     WINDOW / DISPLAY (ACTUAL)
     ========================= */

  /** Actual window width in pixels */
  uint32 window_width;

  /** Actual window height in pixels */
  uint32 window_height;

  /** Actual monitor refresh rate in Hz */
  uint32 monitor_refresh_hz;

  /** True if VSync was successfully enabled */
  bool vsync_enabled;

  /** True if borderless window was successfully enabled */
  bool borderless_enabled;

  /** True if window resizing is supported */
  bool resizable_enabled;

  /* =========================
     INPUT CAPABILITIES
     ========================= */

  /** Maximum number of controllers the platform can support */
  uint32 max_controllers;

  /** True if gamepads are available */
  bool has_gamepads;

  /** True if mouse input is available */
  bool has_mouse;

  /** True if keyboard input is available */
  bool has_keyboard;
  /* =========================
     FRAME PACING
     ========================= */

  /** Actual seconds per frame used by the platform for rendering and timing */
  float seconds_per_frame;

} PlatformConfig;

// /**
//  * @brief Returns the authoritative, read-only PlatformConfig.
//  *
//  * The returned pointer should be treated as immutable.
//  * PlatformConfig is built once at startup by the platform layer.
//  *
//  * @note Do not modify the returned struct.
//  */
// const PlatformConfig *PlatformGetConfig(void);

#endif /* DE100_GAME__COMMON_CONFIG_H */
