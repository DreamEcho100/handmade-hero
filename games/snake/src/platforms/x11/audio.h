#ifndef PLATFORMS_X11_AUDIO_H
#define PLATFORMS_X11_AUDIO_H

#include "../../utils/audio.h"
#include "base.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Audio API  (X11 / ALSA)
 *
 * All three functions take X11AudioConfig* — this struct lives inside
 * g_x11.audio.config and is invisible to the shared platform.h layer.
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(X11AudioConfig *config,
                        AudioOutputBuffer *audio_buffer);
void platform_audio_shutdown(void);
int platform_audio_get_samples_to_write(X11AudioConfig *config,
                                        AudioOutputBuffer *audio_buffer);
void platform_audio_write(X11AudioConfig *config,
                          AudioOutputBuffer *audio_buffer,
                          int samples_to_write);

#endif // PLATFORMS_X11_AUDIO_H
