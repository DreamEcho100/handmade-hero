#ifndef PLATFORMS_X11_AUDIO_H
#define PLATFORMS_X11_AUDIO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/audio.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — Minimal ALSA init (snd_pcm_open + snd_pcm_set_params).
 *             First audible output.
 *
 * LESSON 12 — Full ALSA init refactored to snd_pcm_hw_params /
 *             snd_pcm_sw_params for production quality.
 *
 * These prototypes match the platform contract declared in platform.h.
 * Include this header in platforms/x11/main.c (not in platform.h, which is
 * backend-agnostic).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../../utils/audio.h"

/* Initialize ALSA PCM device and fill `cfg->alsa_pcm`.
 * Returns 0 on success, -1 on failure. */
int platform_audio_init(PlatformAudioConfig *cfg);

/* Drain and close the ALSA PCM device. */
void platform_audio_shutdown(void);

/* Preferred form: pass cfg to close the handle stored inside it. */
void platform_audio_shutdown_cfg(PlatformAudioConfig *cfg);

/* Return how many sample-frames can be written without blocking.
 * Uses snd_pcm_avail_update(); capped at cfg->chunk_size. */
int platform_audio_get_samples_to_write(PlatformAudioConfig *cfg);

/* Write `num_frames` sample-frames from buf->samples to ALSA. */
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg);

#endif /* PLATFORMS_X11_AUDIO_H */
