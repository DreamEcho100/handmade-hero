#ifndef PLATFORMS_X11_AUDIO_H
#define PLATFORMS_X11_AUDIO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/audio.h — ALSA Audio Backend for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Prototypes matching the platform contract declared in platform.h.
 * Include this header only in platforms/x11/main.c.
 *
 * ALSA uses SND_PCM_FORMAT_FLOAT_LE (32-bit float, little-endian) to match
 * the float32 samples produced by game_get_audio_samples().
 *
 * Init sequence:
 *   snd_pcm_hw_params — explicit buffer/period sizing (production quality)
 *   snd_pcm_sw_params — start_threshold=1 (no silent startup gap)
 *   snd_pcm_prepare   — pre-fill silence to prevent initial click
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../../utils/audio.h"

/* Initialize ALSA PCM device and fill cfg->alsa_pcm.
 * Returns 0 on success, -1 on failure. */
int platform_audio_init(PlatformAudioConfig *cfg);

/* Drain and close the ALSA PCM device. */
void platform_audio_shutdown(void);

/* Preferred form: pass cfg to close the handle stored inside it. */
void platform_audio_shutdown_cfg(PlatformAudioConfig *cfg);

/* Return how many sample-frames can be written without blocking.
 * Uses snd_pcm_avail_update(); capped at cfg->chunk_size. */
int platform_audio_get_samples_to_write(PlatformAudioConfig *cfg);

/* Write num_frames sample-frames from buf->samples to ALSA. */
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg);

#endif /* PLATFORMS_X11_AUDIO_H */
