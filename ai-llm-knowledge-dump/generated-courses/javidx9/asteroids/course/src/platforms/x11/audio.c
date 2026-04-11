
#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/audio.c — ALSA Audio Backend for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Full hw_params/sw_params init path for production-quality audio.
 *
 * LATENCY MODEL
 * ─────────────
 *   samples_per_frame  = sample_rate / TARGET_FPS       (~735 @ 44100/60)
 *   latency_samples    = samples_per_frame × 2          (~1470, ~33 ms)
 *   hw ring buffer     = (latency + safety) × 4         (~9680 frames)
 *
 * FLOAT32 FORMAT
 * ──────────────
 *   SND_PCM_FORMAT_FLOAT_LE matches AudioOutputBuffer.samples (float*).
 *   No int16 conversion needed.
 *
 * UNDERRUN RECOVERY
 * ─────────────────
 *   snd_pcm_recover() handles EPIPE (underrun) and ESTRPIPE (suspend).
 *   After recovery a silence pre-fill prevents click artifacts.
 *
 * LESSON NOTE
 * ──────────
 *   alloca.h MUST precede asoundlib.h when using clang — asoundlib uses
 *   alloca() internally and clang requires a visible declaration first.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <alloca.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>

#include "./audio.h"
#include "../../platform.h"   /* TARGET_FPS */

/* ── Module-level ALSA latency state ────────────────────────────────────── */
static int      g_samples_per_frame        = 0;
static int      g_latency_samples          = 0;
static int      g_safety_samples           = 0;
static int64_t  g_running_sample_index     = 0;

/* ── platform_audio_init ─────────────────────────────────────────────────
 *
 * Opens the ALSA default PCM device and configures it for float32
 * interleaved stereo at cfg->sample_rate.
 * ─────────────────────────────────────────────────────────────────────────
 */
int platform_audio_init(PlatformAudioConfig *cfg) {
  int err;
  snd_pcm_t *pcm = NULL;

  g_samples_per_frame    = cfg->sample_rate / TARGET_FPS;
  g_latency_samples      = g_samples_per_frame * 2;
  g_safety_samples       = g_samples_per_frame / 3;
  g_running_sample_index = 0;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 ALSA AUDIO INIT\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:    %d Hz\n",  cfg->sample_rate);
  printf("  Samples/frame:  %d\n",     g_samples_per_frame);
  printf("  Target latency: %d samples (~%d ms)\n",
         g_latency_samples,
         g_latency_samples * 1000 / cfg->sample_rate);

  err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    fprintf(stderr, "❌ ALSA: Cannot open device: %s\n", snd_strerror(err));
    return -1;
  }
  cfg->alsa_pcm = pcm;

  /* ── hw_params ────────────────────────────────────────────────────────
   * snd_pcm_hw_params_alloca() allocates on the stack via alloca.
   * We set format, access, channels, rate, buffer/period sizes.         */
  snd_pcm_hw_params_t *hw;
  snd_pcm_hw_params_alloca(&hw);
  snd_pcm_hw_params_any(pcm, hw);
  snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
  snd_pcm_hw_params_set_channels(pcm, hw, (unsigned)cfg->channels);

  unsigned int rate = (unsigned)cfg->sample_rate;
  snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
  cfg->sample_rate = (int)rate;   /* ALSA may adjust to nearest supported rate. */

  /* Ring buffer = (latency + safety) × 4.
   * The driver rounds up to nearest power-of-two; ×4 is a deliberate
   * over-allocation so two-frame hiccups can't drain the buffer.       */
  snd_pcm_uframes_t hw_buf =
      (snd_pcm_uframes_t)(g_latency_samples + g_safety_samples) * 4;
  snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &hw_buf);

  snd_pcm_uframes_t period = hw_buf / 4;
  snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);

  err = snd_pcm_hw_params(pcm, hw);
  if (err < 0) {
    fprintf(stderr, "❌ ALSA: snd_pcm_hw_params: %s\n", snd_strerror(err));
    snd_pcm_close(pcm);
    cfg->alsa_pcm = NULL;
    return -1;
  }

  snd_pcm_hw_params_get_buffer_size(hw, &hw_buf);
  cfg->alsa_buffer_size = (int)hw_buf;

  /* ── sw_params ────────────────────────────────────────────────────────
   * start_threshold = 1: playback begins as soon as the first sample
   * arrives.  Default = full buffer, causing a silent startup gap.     */
  snd_pcm_sw_params_t *sw;
  snd_pcm_sw_params_alloca(&sw);
  snd_pcm_sw_params_current(pcm, sw);
  snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
  snd_pcm_sw_params(pcm, sw);

  /* Pre-fill one buffer of silence to avoid click on first real write. */
  snd_pcm_prepare(pcm);
  {
    int silence_frames = (int)hw_buf;
    float *silence = (float *)alloca((size_t)silence_frames * 2 * sizeof(float));
    memset(silence, 0, (size_t)silence_frames * 2 * sizeof(float));
    snd_pcm_writei(pcm, silence, (snd_pcm_uframes_t)silence_frames);
  }

  printf("✓ ALSA initialized (hw ring-buffer: %d frames)\n",
         cfg->alsa_buffer_size);
  printf("═══════════════════════════════════════════════════════════\n\n");
  return 0;
}

/* ── platform_audio_shutdown ─────────────────────────────────────────────
 * Bare version (no cfg access).  Callers with a cfg pointer should prefer
 * platform_audio_shutdown_cfg.                                           */
void platform_audio_shutdown(void) { /* use platform_audio_shutdown_cfg */ }

/* ── platform_audio_shutdown_cfg ─────────────────────────────────────── */
void platform_audio_shutdown_cfg(PlatformAudioConfig *cfg) {
  if (cfg && cfg->alsa_pcm) {
    snd_pcm_drain((snd_pcm_t *)cfg->alsa_pcm);
    snd_pcm_close((snd_pcm_t *)cfg->alsa_pcm);
    cfg->alsa_pcm = NULL;
  }
}

/* ── platform_audio_get_samples_to_write ─────────────────────────────────
 *
 * snd_pcm_avail_update() returns how many frames the ring buffer can
 * accept without blocking.  We cap the result to avoid huge mixer chunks:
 *   - cap at 4 × samples_per_frame
 *   - cap at cfg->chunk_size (AudioOutputBuffer capacity)               */
int platform_audio_get_samples_to_write(PlatformAudioConfig *cfg) {
  if (!cfg || !cfg->alsa_pcm) return 0;

  snd_pcm_t *pcm = (snd_pcm_t *)cfg->alsa_pcm;
  snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
  if (avail < 0) {
    /* Underrun or broken pipe — recover and return one frame's worth. */
    snd_pcm_recover(pcm, (int)avail, 0);
    if (g_samples_per_frame > 0) {
      float *silence =
          (float *)alloca((size_t)g_samples_per_frame * 2 * sizeof(float));
      memset(silence, 0, (size_t)g_samples_per_frame * 2 * sizeof(float));
      snd_pcm_writei(pcm, silence, (snd_pcm_uframes_t)g_samples_per_frame);
    }
    return g_samples_per_frame;
  }

  int samples = (int)avail;
  int cap4 = g_samples_per_frame * 4;
  if (samples > cap4) samples = cap4;
  if (cfg->chunk_size > 0 && samples > cfg->chunk_size)
    samples = cfg->chunk_size;
  return samples;
}

/* ── platform_audio_write ───────────────────────────────────────────────
 *
 * snd_pcm_writei writes num_frames interleaved frames from buf->samples.
 * snd_pcm_recover handles EPIPE (underrun) and ESTRPIPE (suspend).      */
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg) {
  if (!cfg || !cfg->alsa_pcm || !buf || !buf->samples || num_frames <= 0)
    return;

  snd_pcm_t *pcm = (snd_pcm_t *)cfg->alsa_pcm;
  snd_pcm_sframes_t written =
      snd_pcm_writei(pcm, buf->samples, (snd_pcm_uframes_t)num_frames);
  if (written < 0) {
    written = snd_pcm_recover(pcm, (int)written, 0);
  }
  if (written > 0) {
    g_running_sample_index += written;
  }
}
