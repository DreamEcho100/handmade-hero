#include "audio.h"
#include "base.h"

#include <alloca.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Initialization
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(X11AudioConfig *config,
                        AudioOutputBuffer *audio_buffer) {
  int err;

  config->samples_per_frame = audio_buffer->samples_per_second / config->hz;
  config->latency_samples =
      config->samples_per_frame * config->frames_of_latency;
  config->safety_samples = config->samples_per_frame / 3;
  config->running_sample_index = 0;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 ALSA AUDIO\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:     %d Hz\n", audio_buffer->samples_per_second);
  printf("  Game update:     %d Hz\n", config->hz);
  printf("  Samples/frame:   %d\n", config->samples_per_frame);
  printf("  Target latency:  %d samples (%d frames)\n", config->latency_samples,
         config->frames_of_latency);

  err = snd_pcm_open(&g_x11.audio.pcm_handle, "default",
                     SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    fprintf(stderr, "❌ ALSA: Cannot open device: %s\n", snd_strerror(err));
    audio_buffer->is_initialized = false;
    return 1;
  }

  snd_pcm_hw_params_t *hw_params;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_hw_params_any(g_x11.audio.pcm_handle, hw_params);
  snd_pcm_hw_params_set_access(g_x11.audio.pcm_handle, hw_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(g_x11.audio.pcm_handle, hw_params,
                               SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(g_x11.audio.pcm_handle, hw_params, 2);

  unsigned int rate = audio_buffer->samples_per_second;
  snd_pcm_hw_params_set_rate_near(g_x11.audio.pcm_handle, hw_params, &rate, 0);
  audio_buffer->samples_per_second = rate;

  /* Use 4× the latency+safety window so that a frame that takes twice the
   * normal time cannot drain the ring buffer.  (The old ×2 multiplier gave
   * ~3 732 frames / 77 ms — tight enough for vsync jitter to cause underruns
   * that showed up as occasional clicks.) */
  snd_pcm_uframes_t buffer_frames =
      (config->latency_samples + config->safety_samples) * 4;
  snd_pcm_hw_params_set_buffer_size_near(g_x11.audio.pcm_handle, hw_params,
                                         &buffer_frames);

  snd_pcm_uframes_t period_frames = buffer_frames / 4;
  snd_pcm_hw_params_set_period_size_near(g_x11.audio.pcm_handle, hw_params,
                                         &period_frames, 0);

  err = snd_pcm_hw_params(g_x11.audio.pcm_handle, hw_params);
  if (err < 0) {
    fprintf(stderr, "❌ ALSA: Cannot set params: %s\n", snd_strerror(err));
    snd_pcm_close(g_x11.audio.pcm_handle);
    audio_buffer->is_initialized = false;
    return 1;
  }

  /* Start threshold = 1 for immediate playback */
  snd_pcm_sw_params_t *sw_params;
  snd_pcm_sw_params_alloca(&sw_params);
  snd_pcm_sw_params_current(g_x11.audio.pcm_handle, sw_params);
  snd_pcm_sw_params_set_start_threshold(g_x11.audio.pcm_handle, sw_params, 1);
  snd_pcm_sw_params(g_x11.audio.pcm_handle, sw_params);

  snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_frames);
  audio_buffer->sample_count = buffer_frames;

  snd_pcm_prepare(g_x11.audio.pcm_handle);
  audio_buffer->is_initialized = true;

  printf("✓ ALSA initialized (buffer: %lu frames)\n", buffer_frames);
  printf("═══════════════════════════════════════════════════════════\n\n");

  return 0;
}

void platform_audio_shutdown(void) {
  if (g_x11.audio.pcm_handle) {
    snd_pcm_drain(g_x11.audio.pcm_handle);
    snd_pcm_close(g_x11.audio.pcm_handle);
    g_x11.audio.pcm_handle = NULL;
  }
}

int platform_audio_get_samples_to_write(X11AudioConfig *config,
                                        AudioOutputBuffer *audio_buffer) {
  if (!audio_buffer->is_initialized || !g_x11.audio.pcm_handle)
    return 0;

  snd_pcm_sframes_t avail = snd_pcm_avail_update(g_x11.audio.pcm_handle);
  if (avail < 0) {
    if (avail == -EPIPE) {
      snd_pcm_prepare(g_x11.audio.pcm_handle);
      return config->samples_per_frame;
    }
    return 0;
  }

  int samples = (int)avail;
  /* Cap at 4 frames so we top-up the larger ring buffer steadily without
   * generating massive chunks that stress the game-audio mixer. */
  if (samples > config->samples_per_frame * 4)
    samples = config->samples_per_frame * 4;

  return samples;
}

void platform_audio_write(X11AudioConfig *config,
                          AudioOutputBuffer *audio_buffer,
                          int samples_to_write) {
  if (!audio_buffer->is_initialized || !g_x11.audio.pcm_handle ||
      samples_to_write <= 0)
    return;

  snd_pcm_sframes_t written = snd_pcm_writei(
      g_x11.audio.pcm_handle, audio_buffer->samples_buffer, samples_to_write);

  if (written < 0) {
    written = snd_pcm_recover(g_x11.audio.pcm_handle, written, 0);
  }

  if (written > 0)
    config->running_sample_index += written;
}
