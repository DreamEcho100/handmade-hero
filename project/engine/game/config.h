#ifndef DE100_GAME_CONFIG_H
#define DE100_GAME_CONFIG_H

#include "../_common/base.h"

/**
 * @brief Describes the configuration and preferences for a specific game.
 *
 * GameConfig contains **game-defined intent**: what the game would like to
 * request from the platform. This is **not authoritative**; the platform may
 * clamp or override values based on actual hardware or OS capabilities.
 */
typedef struct {

  /* =========================
     MEMORY CONFIGURATION
     ========================= */

  /** Size of memory reserved for persistent storage (game state, resources) in
   * bytes */
  uint64 permanent_storage_size;

  /** Size of memory reserved for transient storage (temporary working memory)
   * in bytes */
  uint64 transient_storage_size;

  /* =========================
     GAME / BUILD FLAGS
     ========================= */

  /** Flags describing game-specific behavior or policies.
   * Example: developer mode, fixed timestep, headless mode.
   */
  uint32 game_flags;

  /* =========================
     WINDOW / DISPLAY PREFERENCES
     ========================= */

  /** Desired initial window width in pixels */
  uint32 window_width;

  /** Desired initial window height in pixels */
  uint32 window_height;

  /** Desired target refresh rate in Hz (used to compute default
   * target_seconds_per_frame)
   *
   * @note This is a **hint only**, not a guarantee of actual monitor refresh
   * rate.
   */
  uint32 refresh_rate_hz;

  /** Request VSync be enabled if possible */
  bool prefer_vsync;

  /** Request borderless window if possible */
  bool prefer_borderless;

  /** Request resizable window if possible */
  bool prefer_resizable;

  /** Request adaptive frame pacing if possible */
  bool prefer_adaptive_fps;

  /* =========================
     INPUT REQUIREMENTS
     ========================= */

  /** Maximum number of controllers the game intends to support */
  uint32 max_controllers;

  /* =========================
     AUDIO REQUIREMENTS
     ========================= */

  /** Desired audio sample rate (Hz) */
  uint32 initial_audio_sample_rate;

  /** Desired audio buffer size in frames */
  uint32 audio_buffer_size_frames;

  /** Game update rate for audio calculations (Hz) */
  uint32 audio_game_update_hz;

  /* =========================
     TIMING INTENT
     ========================= */

  /** Desired simulation timestep in seconds per frame */
  float target_seconds_per_frame;

} GameConfig;

/**
 * @brief Returns a default GameConfig with reasonable default values.
 *
 * These defaults are intended for rapid startup or debugging purposes.
 */
GameConfig get_default_game_config();

/**
 * @brief Describes the actual capabilities and state of the platform.
 *
 * PlatformConfig contains **authoritative, platform-determined values**. This
 * is what the game must observe at runtime. It is read-only and is typically
 * initialized once at startup.
 */
typedef struct {

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
     AUDIO BACKEND (ACTUAL)
     ========================= */

  /** Actual audio sample rate being used */
  uint32 audio_sample_rate;

  /** Actual audio buffer size in frames */
  uint32 audio_buffer_size_frames;

  /** Latency between audio write and playback in frames */
  uint32 audio_latency_frames;

  /* =========================
     FRAME PACING
     ========================= */

  /** Actual seconds per frame used by the platform for rendering and timing */
  float seconds_per_frame;

} PlatformConfig;

/**
 * @brief Returns the authoritative, read-only PlatformConfig.
 *
 * The returned pointer should be treated as immutable.
 * PlatformConfig is built once at startup by the platform layer.
 *
 * @note Do not modify the returned struct.
 */
const PlatformConfig *PlatformGetConfig(void);

#endif /* DE100_GAME_CONFIG_H */
