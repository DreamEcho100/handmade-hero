/*
 * platform.h  —  Sugar, Sugar | Platform Contract
 *
 * The ONLY header the platform backends (main_x11.c, main_raylib.c) need to
 * implement.  The game (game.c) calls game_play_sound() and game_music_play()
 * directly — it never touches the platform for audio.
 *
 * Platform audio responsibility:
 *   Each frame, call platform_audio_update(&state).
 *   The platform asks game_get_audio_samples() to fill its hardware buffer.
 *   The game layer does all synthesis — the platform just moves bytes to hardware.
 *
 * Rule: if you add a new platform (SDL3, Win32, WebAssembly) you write a new
 * main_*.c that provides exactly these functions — zero changes to game.c.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "game.h"

/* -----------------------------------------------------------------------
 * MANDATORY — all backends must implement these four functions
 * ----------------------------------------------------------------------- */

/* Open a window with the given title and pixel dimensions. */
void   platform_init(const char *title, int width, int height);

/* Return monotonic time in seconds.
 * Called twice per frame; the difference is delta_time. */
double platform_get_time(void);

/* Read OS input events and fill *input with the current state.
 * Must call UPDATE_BUTTON() for every press/release seen this frame. */
void   platform_get_input(GameInput *input);

/* Upload bb->pixels (ARGB uint32_t array) to the display and flip buffers.
 * In X11: XPutImage.   In Raylib: UpdateTexture + DrawTexture. */
void   platform_display_backbuffer(const GameBackbuffer *backbuffer);

/* -----------------------------------------------------------------------
 * AUDIO — required by all backends (implement as stubs if no audio device)
 *
 * Lifecycle: platform_audio_init → [loop: platform_audio_update] →
 *            platform_audio_shutdown
 *
 * platform_audio_init: open the audio device, set sample rate, pre-fill
 *   silence to establish latency.  Must call game_audio_init() to wire
 *   the sample rate into GameState.audio.
 *
 * platform_audio_update: called every game frame.  If the hardware needs
 *   more data, call game_get_audio_samples() to generate samples, then
 *   push them to the device.
 *
 * platform_audio_shutdown: drain and close the audio device.
 * ----------------------------------------------------------------------- */
void   platform_audio_init(GameState *state, int samples_per_second);
void   platform_audio_update(GameState *state);
void   platform_audio_shutdown(void);

#endif /* PLATFORM_H */
