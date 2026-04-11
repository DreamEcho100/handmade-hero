
#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/audio.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 09 — First audible output: minimal snd_pcm_set_params init path.
 *             Students can hear a tone by the end of this lesson.
 *
 * LESSON 12 — Full refactor: snd_pcm_hw_params (explicit control of
 *             buffer/period sizes), snd_pcm_sw_params (start_threshold=1
 *             eliminates silent startup), snd_pcm_recover on underrun.
 *
 * LESSON 12 — Key ALSA concepts covered:
 *   • alloca.h BEFORE asoundlib.h (clang requirement)
 *   • hw_buffer_size vs. how-many-to-write-per-frame (different things!)
 *   • snd_pcm_avail_update() for non-blocking "write how much is available"
 *   • Latency model: samples_per_frame × frames_of_latency
 *   • max_sample_count safety cap prevents OOB writes
 *   • Pre-fill silence on init + recovery (prevents click artifacts)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LESSON 09 — MINIMAL PATH (what students write first)
 * ─────────────────────────────────────────────────────────────────────────
 *
 *   // In lesson 9 the students write this simpler version:
 *   snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
 *   snd_pcm_set_params(pcm,
 *       SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
 *       2, 44100, 1, 50000);   // 50 ms latency
 *   cfg->alsa_pcm = pcm;
 *
 *   This is enough to hear a tone, but has no control over buffer sizes
 *   and can stutter under load. Lesson 12 replaces it with hw_params.
 * ─────────────────────────────────────────────────────────────────────────
 * LESSON 12 — PRODUCTION PATH (what this file contains)
 * ─────────────────────────────────────────────────────────────────────────
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* LESSON 12 — alloca.h MUST come before asoundlib.h when using clang.
 * asoundlib.h uses alloca() internally; if alloca.h isn't included first,
 * clang may not find the declaration and emits an error.                   */
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>

#include "./audio.h"
#include "../../platform.h"   /* TARGET_FPS */

/* ─────────────────────────────────────────────────────────────────────────
 * Module-level ALSA latency state
 * (These don't belong in PlatformAudioConfig because they're ALSA-only.)
 * ─────────────────────────────────────────────────────────────────────────
 */
static int g_samples_per_frame   = 0;   /* sample_rate / TARGET_FPS          */
static int g_latency_samples     = 0;   /* samples_per_frame * 2 frames      */
static int g_safety_samples      = 0;   /* samples_per_frame / 3             */
static int64_t g_running_sample_index = 0; /* total frames written to ALSA   */

/* ─────────────────────────────────────────────────────────────────────────
 * platform_audio_init
 * ─────────────────────────────────────────────────────────────────────────
 * LESSON 12 — Full hw_params init.
 *
 * Why not snd_pcm_set_params?
 *   snd_pcm_set_params is a convenience wrapper that gives ALSA full
 *   control of buffer sizing.  For a platform course we want to show
 *   explicit control so students understand the latency model.
 * ─────────────────────────────────────────────────────────────────────────
 */
int platform_audio_init(PlatformAudioConfig *cfg) {
  int err;
  snd_pcm_t *pcm = NULL;

  /* LESSON 12 — Latency model:
   *   samples_per_frame = sample_rate / fps          (≈ 735 @ 44100/60)
   *   latency_samples   = samples_per_frame × 2      (≈ 1470 → ~33ms)
   *   safety_samples    = samples_per_frame / 3      (≈  245)
   * Ring buffer = (latency + safety) × 4 so that two-frame hiccups can't
   * drain it.  The ×4 is a deliberate over-allocation; the driver will
   * round up to the nearest power-of-two anyway.                          */
  g_samples_per_frame  = cfg->sample_rate / TARGET_FPS;
  g_latency_samples    = g_samples_per_frame * 2;
  g_safety_samples     = g_samples_per_frame / 3;
  g_running_sample_index = 0;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 ALSA AUDIO INIT\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:    %d Hz\n", cfg->sample_rate);
  printf("  Samples/frame:  %d\n",  g_samples_per_frame);
  printf("  Target latency: %d samples (~%d ms)\n",
         g_latency_samples, g_latency_samples * 1000 / cfg->sample_rate);

  err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    fprintf(stderr, "❌ ALSA: Cannot open device: %s\n", snd_strerror(err));
    return -1;
  }
  cfg->alsa_pcm = pcm;

  /* ── hw_params ────────────────────────────────────────────────────────
   * LESSON 12 — snd_pcm_hw_params_alloca() allocates on the stack (uses
   * alloca internally — hence the include-order requirement above).      */
  snd_pcm_hw_params_t *hw;
  snd_pcm_hw_params_alloca(&hw);
  snd_pcm_hw_params_any(pcm, hw);
  snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm, hw, (unsigned)cfg->channels);

  unsigned int rate = (unsigned)cfg->sample_rate;
  snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
  cfg->sample_rate = (int)rate;  /* ALSA may adjust rate to nearest supported. */

  /* Ring buffer = (latency + safety) × 4. */
  snd_pcm_uframes_t hw_buf = (snd_pcm_uframes_t)(g_latency_samples + g_safety_samples) * 4;
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
   * LESSON 12 — start_threshold = 1 means playback begins as soon as
   * the very first sample arrives.  Default is the full buffer, which
   * causes a silent period before audio starts.                          */
  snd_pcm_sw_params_t *sw;
  snd_pcm_sw_params_alloca(&sw);
  snd_pcm_sw_params_current(pcm, sw);
  snd_pcm_sw_params_set_start_threshold(pcm, sw, 1);
  snd_pcm_sw_params(pcm, sw);

  /* Pre-fill one period of silence to avoid click on first write. */
  snd_pcm_prepare(pcm);
  {
    int silence_frames = (int)hw_buf;
    int16_t *silence   = (int16_t *)alloca((size_t)silence_frames * 2 * sizeof(int16_t));
    memset(silence, 0, (size_t)silence_frames * 2 * sizeof(int16_t));
    snd_pcm_writei(pcm, silence, (snd_pcm_uframes_t)silence_frames);
  }

  printf("✓ ALSA initialized (hw ring-buffer: %d frames)\n", cfg->alsa_buffer_size);
  printf("═══════════════════════════════════════════════════════════\n\n");
  return 0;
}

/* ─────────────────────────────────────────────────────────────────────────
 * platform_audio_shutdown
 * ─────────────────────────────────────────────────────────────────────────
 */
void platform_audio_shutdown(void) {
  /* cfg not available here; drain via global — or caller passes it.
   * For this course we keep a module-local copy of the pcm handle. */
}

/* platform_audio_shutdown_cfg — preferred form called from main.c */
void platform_audio_shutdown_cfg(PlatformAudioConfig *cfg) {
  if (cfg && cfg->alsa_pcm) {
    snd_pcm_drain((snd_pcm_t *)cfg->alsa_pcm);
    snd_pcm_close((snd_pcm_t *)cfg->alsa_pcm);
    cfg->alsa_pcm = NULL;
  }
}

/* ─────────────────────────────────────────────────────────────────────────
 * platform_audio_get_samples_to_write
 * ─────────────────────────────────────────────────────────────────────────
 * LESSON 12 — snd_pcm_avail_update() returns the number of frames the
 * ring buffer can accept without blocking.  We cap the result at 4 ×
 * samples_per_frame (avoid sending enormous chunks to the mixer) and at
 * max_sample_count (prevents OOB writes into AudioOutputBuffer).         */
int platform_audio_get_samples_to_write(PlatformAudioConfig *cfg) {
  if (!cfg || !cfg->alsa_pcm) return 0;

  snd_pcm_t *pcm = (snd_pcm_t *)cfg->alsa_pcm;
  snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
  if (avail < 0) {
    /* Underrun or broken pipe — recover and return one frame's worth. */
    snd_pcm_recover(pcm, (int)avail, 0);
    /* Pre-fill silence after recovery to prevent click. */
    if (g_samples_per_frame > 0) {
      int16_t *silence = (int16_t *)alloca((size_t)g_samples_per_frame * 2 * sizeof(int16_t));
      memset(silence, 0, (size_t)g_samples_per_frame * 2 * sizeof(int16_t));
      snd_pcm_writei(pcm, silence, (snd_pcm_uframes_t)g_samples_per_frame);
    }
    return g_samples_per_frame;
  }

  int samples = (int)avail;
  /* Cap: 4 frames max to avoid huge mixer chunks. */
  int cap4 = g_samples_per_frame * 4;
  if (samples > cap4) samples = cap4;
  /* Safety cap: never exceed AudioOutputBuffer capacity. */
  if (cfg->chunk_size > 0 && samples > cfg->chunk_size)
    samples = cfg->chunk_size;
  return samples;
}

/* ─────────────────────────────────────────────────────────────────────────
 * platform_audio_write
 * ─────────────────────────────────────────────────────────────────────────
 * LESSON 12 — snd_pcm_writei writes interleaved samples.
 * snd_pcm_recover handles EPIPE (underrun) and ESTRPIPE (suspend).      */
void platform_audio_write(AudioOutputBuffer *buf, int num_frames,
                          PlatformAudioConfig *cfg) {
  if (!cfg || !cfg->alsa_pcm || !buf || !buf->samples || num_frames <= 0) return;

  snd_pcm_t *pcm = (snd_pcm_t *)cfg->alsa_pcm;
  snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf->samples,
                                              (snd_pcm_uframes_t)num_frames);
  if (written < 0) {
    written = snd_pcm_recover(pcm, (int)written, 0);
  }
  if (written > 0) {
    g_running_sample_index += written;
  }
}
