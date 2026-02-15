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
  uint32 target_refresh_rate_hz;
  uint32 max_allowed_refresh_rate_hz;

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

  char window_title[64];

} GameConfig;

/**
 * @brief Returns a default GameConfig with reasonable default values.
 *
 * These defaults are intended for rapid startup or debugging purposes.
 */
GameConfig get_default_game_config(void);

#endif /* DE100_GAME_CONFIG_H */
