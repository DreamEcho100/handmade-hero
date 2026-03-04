/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  main_x11.c  —  X11 / ALSA / OpenGL Platform Backend  (Phase 2)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  WHAT IS A PLATFORM LAYER?
 *  ─────────────────────────
 *  Handmade Hero principle: The platform layer is a thin translator.
 *
 *  Its only jobs are:
 *    1. Map OS events      →  GameInput  (X11 KeyPress/KeyRelease → button)
 *    2. Map OS timer       →  delta_time (gettimeofday → seconds)
 *    3. Map ALSA callbacks →  AudioOutputBuffer (PCM samples out to hardware)
 *    4. Call game functions; the game NEVER calls platform functions back.
 *
 *  This file uses three Linux subsystems:
 *    • X11   — windowing and keyboard input (the Linux display protocol)
 *    • ALSA  — audio output (Advanced Linux Sound Architecture)
 *    • GLX   — OpenGL context on top of an X11 window
 *
 *  JS CONTEXT
 *  ──────────
 *  In a browser you'd use:  window / canvas (display), Web Audio API (audio),
 *  WebGL (GPU rendering).  This file is the C equivalent of those three APIs
 *  wired together.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  POSIX_C_SOURCE 200809L: unlocks POSIX.1-2008 APIs (nanosleep, etc.)
 *  without it, some function signatures aren't visible to the compiler.
 *  JS: like `"use strict"` but for which C standard library functions exist.
 */
#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "platform.h"

/* X11 display, window, and keyboard headers */
#include <X11/X.h>
#include <X11/XKBlib.h>  /* XkbSetDetectableAutoRepeat */
#include <X11/Xlib.h>
#include <X11/keysym.h>  /* XK_Left, XK_a, etc. */

/* OpenGL + GLX (OpenGL on X11) headers */
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>   /* GLX extension function pointers */

/* ALSA (audio) */
#include <alsa/asoundlib.h>

/* Standard C library */
#include <alloca.h>    /* stack allocation — used by snd_pcm_hw_params_alloca */
#include <stdbool.h>   /* bool, true, false */
#include <stdio.h>
#include <stdlib.h>    /* JS: malloc / free */
#include <string.h>    /* JS: memset / memcpy */
#include <sys/time.h>  /* JS: struct timeval | C: wall-clock time via gettimeofday */
#include <time.h>      /* struct timespec, nanosleep */

/*
 * GLX extension function pointer type for VSync.
 *
 * GLX extensions are not guaranteed to exist on every driver; we must
 * query them at runtime with glXGetProcAddressARB.  This typedef declares
 * the exact signature of the function we want to load dynamically.
 *
 * JS: like `const glXSwapIntervalEXT = gl.getExtension('EXT_swap_control').fn`
 */
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ══════════════════════════════════════════════════════════════════════════
 * Platform State
 * ══════════════════════════════════════════════════════════════════════════
 *
 * All X11/ALSA/OpenGL handles live in one struct so they're easy to track.
 *
 * JS: like a module-level singleton `const g_x11 = { display: null, ... }`
 * C:  `static` at file scope = private to this translation unit (.c file).
 *     `= {0}` zero-initialises every field at program start.
 *
 * Pointer types:
 *   Display*      — opaque handle to the X server connection
 *   Window        — an integer ID (X11 uses integers for window handles)
 *   GLXContext    — opaque handle to the OpenGL rendering context
 *   GLuint        — unsigned int alias used by OpenGL for object IDs
 *   snd_pcm_t*    — opaque ALSA PCM device handle
 *   int16_t*      — JS: Int16Array — pointer to heap-allocated PCM samples
 */
typedef struct {
  Display     *display;          /* connection to the X11 server */
  Window       window;           /* our application window ID */
  GLXContext   gl_context;       /* OpenGL rendering context */
  GLuint       texture_id;       /* OpenGL texture object for backbuffer */
  int          screen;           /* X11 screen number (usually 0) */
  int          width;            /* current window width  in pixels */
  int          height;           /* current window height in pixels */
  bool         vsync_enabled;    /* true if we got a working VSync extension */

  /* Audio (ALSA) */
  snd_pcm_t       *pcm_handle;        /* ALSA PCM device handle */
  int16_t         *sample_buffer;     /* CPU-side PCM scratch buffer */
  int              sample_buffer_size; /* capacity in stereo frames */
  int              samples_per_second; /* actual rate reported by ALSA hardware */
  snd_pcm_uframes_t hw_buffer_size;   /* hardware ring buffer size in frames */
} X11State;

/* JS: module-level singletons */
static X11State g_x11           = {0};
static double   g_start_time    = 0.0;   /* set on first get_time() call */
static double   g_last_frame_time = 0.0;
static const double g_target_frame_time = 1.0 / 60.0; /* 16.667 ms */

/* ══════════════════════════════════════════════════════════════════════════
 * Audio Initialization  (Casey's Latency Model — ALSA)
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  Casey's Latency Model:
 *  ──────────────────────
 *  The key insight: we know exactly how much audio is queued in the hardware
 *  ring buffer at any moment because ALSA exposes snd_pcm_delay().
 *
 *  Each frame we calculate:
 *    samples_to_write = target_buffered - delay_frames
 *
 *  where:
 *    target_buffered  = latency_samples + safety_samples
 *    delay_frames     = snd_pcm_delay() = frames currently queued in HW
 *
 *  This keeps the hardware buffer topped up to exactly our target latency
 *  without ever over-filling (which would increase latency) or under-filling
 *  (which causes an underrun / audible glitch).
 *
 *  JS analogy: imagine a streaming buffer.  You always want ~100ms of video
 *  pre-downloaded.  Each tick you check how much is buffered, then download
 *  exactly enough to reach 100ms again.
 *
 * ══════════════════════════════════════════════════════════════════════════
 */

int platform_audio_init(PlatformAudioConfig *config) {
  int err; /* ALSA returns negative errno values on error */

  config->samples_per_second = 48000; /* request 48 kHz stereo */

  /* ── Casey's latency calculations ──────────────────────────────────────
   * These mirror the Raylib backend exactly — the model is the same.
   */
  config->samples_per_frame = config->samples_per_second / config->hz;
  config->latency_samples   = config->samples_per_frame * config->frames_of_latency;
  config->safety_samples    = config->samples_per_frame / 3;
  config->running_sample_index = 0;

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🔊 ALSA AUDIO (Casey's Latency Model)\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("  Sample rate:     %d Hz\n", config->samples_per_second);
  printf("  Game update:     %d Hz\n", config->hz);
  printf("  Samples/frame:   %d (%.1f ms)\n", config->samples_per_frame,
         (float)config->samples_per_frame / config->samples_per_second * 1000.0f);
  printf("  Target latency:  %d samples (%.1f ms, %d frames)\n",
         config->latency_samples,
         (float)config->latency_samples / config->samples_per_second * 1000.0f,
         config->frames_of_latency);
  printf("  Safety margin:   %d samples (%.1f ms)\n", config->safety_samples,
         (float)config->safety_samples / config->samples_per_second * 1000.0f);

  /*
   * Open the default PCM device for playback.
   *
   * "default" is an ALSA alias that routes through PulseAudio / PipeWire
   * if present, or directly to hardware otherwise.
   *
   * SND_PCM_STREAM_PLAYBACK = output (as opposed to capture/recording)
   * 0 = blocking mode (function waits until device is ready)
   *
   * JS: like `await navigator.mediaDevices.getUserMedia({audio: true})`
   *     but for output.
   *
   * `&g_x11.pcm_handle` passes a pointer-to-pointer so ALSA can write
   * the handle into our struct.
   * JS: like passing `{ ref: null }` and having the callee set `.ref`.
   */
  err = snd_pcm_open(&g_x11.pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    fprintf(stderr, "ALSA: Cannot open audio device: %s\n", snd_strerror(err));
    config->is_initialized = 0;
    return 1;
  }

  /*
   * Hardware parameters setup
   * ──────────────────────────
   * ALSA requires you to negotiate a set of hardware parameters with the
   * driver before you can send any audio.  Think of it like HTTP content
   * negotiation: you propose values, the driver adjusts to what it supports.
   *
   * snd_pcm_hw_params_alloca — stack-allocates a params struct (like alloca)
   * snd_pcm_hw_params_any   — fills params with all values the HW supports
   */
  snd_pcm_hw_params_t *hw_params;
  snd_pcm_hw_params_alloca(&hw_params);   /* stack alloc — no free needed */
  snd_pcm_hw_params_any(g_x11.pcm_handle, hw_params); /* fill with "any" */

  /* Interleaved access: L R L R L R …  (stereo frames back-to-back) */
  snd_pcm_hw_params_set_access(g_x11.pcm_handle, hw_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);

  /* 16-bit signed little-endian samples (S16_LE = standard on x86) */
  snd_pcm_hw_params_set_format(g_x11.pcm_handle, hw_params,
                               SND_PCM_FORMAT_S16_LE);

  /* Stereo (2 channels) */
  snd_pcm_hw_params_set_channels(g_x11.pcm_handle, hw_params, 2);

  /*
   * Request sample rate.
   *
   * _set_rate_near: asks for the closest rate the hardware supports.
   * It writes the actual rate back into `sample_rate`, so we update
   * config->samples_per_second to match reality.
   */
  unsigned int sample_rate = config->samples_per_second;
  snd_pcm_hw_params_set_rate_near(g_x11.pcm_handle, hw_params, &sample_rate, 0);
  config->samples_per_second = sample_rate; /* store actual negotiated rate */

  /*
   * Hardware ring buffer size:  latency_samples + safety + 2× headroom.
   *
   * The HW ring buffer is the circular buffer inside ALSA's kernel driver.
   * The CPU writes into it; the DMA controller reads from it and feeds the
   * DAC.  Making it large gives us headroom so frame spikes don't cause
   * underruns.
   *
   * _set_buffer_size_near: request nearest size the hardware allows.
   * We store the actual negotiated size in g_x11.hw_buffer_size for use
   * in platform_audio_get_samples_to_write (underrun recovery).
   */
  snd_pcm_uframes_t buffer_frames =
      (config->latency_samples + config->safety_samples) * 2;
  snd_pcm_hw_params_set_buffer_size_near(g_x11.pcm_handle, hw_params,
                                         &buffer_frames);
  g_x11.hw_buffer_size = buffer_frames;

  /*
   * Period size: how many frames ALSA transfers per interrupt.
   * ~1/4 of the buffer = 4 interrupts per buffer fill.
   *
   * Smaller period = more interrupts = lower latency but more CPU overhead.
   * Larger period = fewer interrupts = higher latency but less overhead.
   */
  snd_pcm_uframes_t period_frames = buffer_frames / 4;
  snd_pcm_hw_params_set_period_size_near(g_x11.pcm_handle, hw_params,
                                         &period_frames, 0);

  /* Commit the negotiated parameters to the hardware */
  err = snd_pcm_hw_params(g_x11.pcm_handle, hw_params);
  if (err < 0) {
    fprintf(stderr, "ALSA: Cannot set hardware parameters: %s\n",
            snd_strerror(err));
    snd_pcm_close(g_x11.pcm_handle);
    config->is_initialized = 0;
    return 1;
  }

  /* Query the final negotiated values (may differ from what we requested) */
  snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_frames);
  snd_pcm_hw_params_get_period_size(hw_params, &period_frames, 0);

  config->buffer_size_samples = buffer_frames;
  g_x11.hw_buffer_size = buffer_frames;

  printf("  HW buffer:       %lu frames (%.1f ms)\n", buffer_frames,
         (float)buffer_frames / config->samples_per_second * 1000.0f);
  printf("  HW period:       %lu frames (%.1f ms)\n", period_frames,
         (float)period_frames / config->samples_per_second * 1000.0f);

  /*
   * Allocate CPU-side scratch buffer for generating PCM samples.
   *
   * JS: `new Int16Array(samples_per_frame * 4 * 2)` (* 2 = stereo)
   * C:  malloc returns void*, cast to int16_t*
   *     `int16_t` = signed 16-bit integer (range –32768 … 32767)
   */
  g_x11.sample_buffer_size = config->samples_per_frame * 4;
  g_x11.samples_per_second = config->samples_per_second;
  g_x11.sample_buffer =
      (int16_t *)malloc(g_x11.sample_buffer_size * 2 * sizeof(int16_t));

  if (!g_x11.sample_buffer) {
    fprintf(stderr, "ALSA: Cannot allocate sample buffer\n");
    snd_pcm_close(g_x11.pcm_handle);
    config->is_initialized = 0;
    return 1;
  }

  /*
   * Pre-fill with silence then call snd_pcm_prepare.
   *
   * WHY:  The ALSA driver requires the PCM device to be in the PREPARED
   *       state before the first write.  Writing silence first ensures
   *       the device has data from the start (prevents initial click).
   *       snd_pcm_prepare then arms the hardware for playback.
   *
   * Note: ALSA starts playback automatically on the first snd_pcm_writei
   * call (or after snd_pcm_start) — we don't need an explicit "play" call
   * like Raylib's PlayAudioStream.
   */
  memset(g_x11.sample_buffer, 0, config->latency_samples * 2 * sizeof(int16_t));
  snd_pcm_writei(g_x11.pcm_handle, g_x11.sample_buffer, config->latency_samples);

  snd_pcm_prepare(g_x11.pcm_handle); /* put device into PREPARED state */

  config->is_initialized = 1;
  printf("═══════════════════════════════════════════════════════════\n\n");

  return 0;
}

void platform_audio_shutdown(void) {
  if (g_x11.sample_buffer) {
    free(g_x11.sample_buffer);     /* JS: like `buffer = null` */
    g_x11.sample_buffer = NULL;
  }
  if (g_x11.pcm_handle) {
    snd_pcm_drain(g_x11.pcm_handle); /* flush remaining samples to speaker */
    snd_pcm_close(g_x11.pcm_handle);
    g_x11.pcm_handle = NULL;
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Audio Update  (Casey's Latency Model — ALSA)
 * ══════════════════════════════════════════════════════════════════════════
 */

/*
 * platform_audio_get_samples_to_write
 * ─────────────────────────────────────
 * Casey's model: query the exact play position, calculate how many samples
 * we need to write to keep the buffer at target_buffered depth.
 *
 * snd_pcm_delay(handle, &delay_frames):
 *   Returns how many frames are currently queued in the hardware buffer
 *   (i.e., how many frames separate the write pointer from the play head).
 *
 * JS analogy: if you're streaming video and you want 100ms in the buffer,
 *   delay_frames  = "how much is currently buffered"
 *   samples_to_write = target - currently_buffered
 *
 * Underrun recovery (-EPIPE):
 *   EPIPE = "broken pipe" — the play head caught up to the write pointer
 *   and the hardware ran out of data (audible glitch / silence).
 *   We recover by calling snd_pcm_prepare to reset the device, then
 *   continue writing.  The game keeps running — one glitch, not a crash.
 */
static int platform_audio_get_samples_to_write(PlatformAudioConfig *config) {
  if (!config->is_initialized || !g_x11.pcm_handle)
    return 0;

  /*
   * snd_pcm_delay: ALSA's answer to "how many frames are ahead of the
   * play cursor right now?"
   *
   * This is the key ALSA advantage over Raylib: we get an exact number,
   * not just a boolean.  It lets us do precise latency management.
   */
  snd_pcm_sframes_t delay_frames = 0;
  int err = snd_pcm_delay(g_x11.pcm_handle, &delay_frames);

  if (err < 0) {
    if (err == -EPIPE) {
      /* Underrun: hardware ran out of data — reset and continue */
      fprintf(stderr, "ALSA: Underrun! Recovering...\n");
      snd_pcm_prepare(g_x11.pcm_handle);
      delay_frames = 0; /* pretend buffer is empty; we'll refill it */
    } else {
      return 0; /* unknown error — skip this frame */
    }
  }

  /*
   * snd_pcm_avail_update: how many frames of free space exist in the
   * hardware ring buffer right now?  We clamp our write to this to avoid
   * blocking (or an error from trying to write more than there's room for).
   */
  snd_pcm_sframes_t avail_frames = snd_pcm_avail_update(g_x11.pcm_handle);
  if (avail_frames < 0) {
    if (avail_frames == -EPIPE) {
      /* Another underrun path — avail_update can also return -EPIPE */
      snd_pcm_prepare(g_x11.pcm_handle);
      avail_frames = g_x11.hw_buffer_size; /* assume all space is free */
      delay_frames = 0;
    } else {
      return 0;
    }
  }

  /*
   * Casey's core calculation:
   *
   *   target_buffered  = latency_samples + safety_samples
   *   samples_to_write = target_buffered - delay_frames
   *
   * If delay_frames is already at target, samples_to_write ≤ 0 → write nothing.
   * If delay_frames is below target, we write just enough to top it back up.
   *
   * This is analogous to TCP's flow control: we maintain a window of exactly
   * the right size rather than flooding or starving the receiver.
   */
  int target_buffered  = config->latency_samples + config->safety_samples;
  int samples_to_write = target_buffered - (int)delay_frames;

  /* Never try to write more than ALSA has room for (would block or error) */
  if (samples_to_write > (int)avail_frames)
    samples_to_write = (int)avail_frames;

  /* Ignore tiny writes — they're not worth the overhead */
  if (samples_to_write < config->samples_per_frame / 4)
    samples_to_write = 0;

  /* Never exceed our scratch buffer capacity */
  if (samples_to_write > g_x11.sample_buffer_size)
    samples_to_write = g_x11.sample_buffer_size;

  return samples_to_write;
}

static void platform_audio_update(GameState *game_state,
                                  PlatformAudioConfig *config) {
  if (!config->is_initialized || !g_x11.pcm_handle)
    return;

  int samples_to_write = platform_audio_get_samples_to_write(config);
  if (samples_to_write <= 0)
    return;

  /*
   * Build the buffer descriptor and ask the game to fill it.
   *
   * The game writes PCM frames into g_x11.sample_buffer.
   * It doesn't know or care that we're using ALSA.
   */
  AudioOutputBuffer buffer = {
      .samples            = g_x11.sample_buffer,
      .samples_per_second = g_x11.samples_per_second,
      .sample_count       = samples_to_write,
  };

  game_get_audio_samples(game_state, &buffer);

  /*
   * snd_pcm_writei: write interleaved frames to the ALSA ring buffer.
   *
   * Returns the number of frames actually written, or a negative error code.
   *
   * snd_pcm_recover: handles common errors (underrun, suspend) automatically.
   * It's the recommended error handler for snd_pcm_writei failures.
   */
  snd_pcm_sframes_t frames_written =
      snd_pcm_writei(g_x11.pcm_handle, g_x11.sample_buffer, samples_to_write);

  if (frames_written < 0) {
    frames_written = snd_pcm_recover(g_x11.pcm_handle, frames_written, 0);
    if (frames_written < 0) {
      fprintf(stderr, "ALSA: Write error: %s\n", snd_strerror(frames_written));
      return;
    }
  }

  config->running_sample_index += frames_written; /* monotonic total */
}

/* ══════════════════════════════════════════════════════════════════════════
 * Timing
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Unlike Raylib (which provides GetFrameTime()), X11 has no built-in timer.
 * We implement our own using POSIX gettimeofday().
 *
 * struct timeval  { time_t tv_sec; suseconds_t tv_usec; }
 * JS: like `{ seconds: Math.floor(Date.now()/1000), microseconds: Date.now()%1000*1000 }`
 *
 * We store g_start_time on the first call so that our timestamps are relative
 * to program start (easier to reason about in debug output than Unix epoch).
 */

static double get_time(void) {
  struct timeval tv;  /* JS: struct timeval | C: wall-clock seconds + microseconds */
  gettimeofday(&tv, NULL); /* NULL = don't need timezone info */

  double now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
  if (g_start_time == 0.0)
    g_start_time = now; /* initialise on first call */

  return now - g_start_time; /* seconds since program start */
}

/*
 * sleep_ms: sleep for a given number of milliseconds.
 *
 * Used as a VSync fallback when GLX_EXT_swap_control is not available.
 * nanosleep is the POSIX high-resolution sleep function.
 *
 * JS: like `await new Promise(r => setTimeout(r, ms))`
 *     but blocking (no event loop here).
 */
static void sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec  = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L; /* remaining ms → nanoseconds */
  nanosleep(&ts, NULL); /* NULL = don't need remaining time on interruption */
}

/* ══════════════════════════════════════════════════════════════════════════
 * VSync Setup
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  VSync (vertical synchronisation) locks buffer swaps to the monitor's
 *  refresh rate so we never present a half-drawn frame (tearing).
 *
 *  OpenGL on X11 does NOT have a standard VSync API.  It's provided via
 *  vendor extensions that may or may not be present on a given driver:
 *
 *    GLX_EXT_swap_control   — standard extension (most Mesa/NVIDIA drivers)
 *    GLX_MESA_swap_control  — Mesa-specific fallback
 *
 *  Extension query pattern:
 *  ─────────────────────────
 *  1. glXQueryExtensionsString() — get the list of supported extensions as
 *     a whitespace-separated C string.
 *  2. strstr() — search for the extension name in that string.
 *     JS: like `extensions.includes('GLX_EXT_swap_control')`
 *  3. glXGetProcAddressARB() — dynamically load the function pointer.
 *     JS: like `const fn = module.getExportedFunction('glXSwapIntervalEXT')`
 *  4. Call the function with interval=1 to lock to monitor refresh.
 *
 *  Fallback:
 *  ─────────
 *  If neither extension is available, g_x11.vsync_enabled stays false.
 *  The main loop then manually sleeps (sleep_ms) to target 60 fps.
 *  This is less accurate but portable.
 */

static void setup_vsync(void) {
  /* Get list of supported GLX extensions for our screen */
  const char *extensions =
      glXQueryExtensionsString(g_x11.display, g_x11.screen);

  /* Try the standard extension first */
  if (extensions && strstr(extensions, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT) {
      glXSwapIntervalEXT(g_x11.display, g_x11.window, 1); /* 1 = sync every frame */
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_EXT_swap_control)\n");
      return;
    }
  }

  /* Fall back to the Mesa-specific extension */
  if (extensions && strstr(extensions, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (glXSwapIntervalMESA) {
      glXSwapIntervalMESA(1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_MESA_swap_control)\n");
      return;
    }
  }

  /* Neither extension available — main loop will manual-sleep */
  g_x11.vsync_enabled = false;
  printf("⚠ VSync not available\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * Platform Init
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Brings up the full X11 + GLX + OpenGL stack.
 *
 * Pipeline overview:
 *   XOpenDisplay   → connect to X server
 *   XkbSetDetectableAutoRepeat → fix key-repeat events
 *   glXChooseVisual → negotiate a GL-capable framebuffer format
 *   XCreateWindow  → create the OS window
 *   glXCreateContext → create the OpenGL rendering context
 *   glXMakeCurrent → bind the GL context to the window
 *   glOrtho        → set up 2-D orthographic projection
 *   glGenTextures  → allocate a GPU texture for our backbuffer
 *   setup_vsync    → try to enable VSync
 *   platform_audio_init → initialise ALSA
 */

int platform_init(PlatformGameProps *platform_game_props) {
  /*
   * Connect to the X display server.
   *
   * NULL = use the DISPLAY environment variable (e.g., ":0" or ":1").
   * JS: like `const display = new XServer(process.env.DISPLAY)`
   */
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  /*
   * XkbSetDetectableAutoRepeat
   * ───────────────────────────
   * Problem:  When you hold a key, the OS fires repeated
   *           KeyRelease + KeyPress events at ~30 Hz by default.
   *           To OUR input system these look like rapid taps, not a hold.
   *           UPDATE_BUTTON would count dozens of transitions per second,
   *           and move_left.half_transition_count would explode.
   *
   * Fix:  XkbSetDetectableAutoRepeat tells the X server to suppress the
   *       fake KeyRelease during auto-repeat.  We only get:
   *         KeyPress  (initial)
   *         KeyPress  KeyPress  …  (repeats, no interleaved release)
   *         KeyRelease (actual release)
   *
   *       Our UPDATE_BUTTON macro detects "no change → no increment", so
   *       held keys correctly show half_transition_count = 1 (pressed once).
   *
   * JS: there's no equivalent because browsers already de-duplicate this.
   *     In raw X11 you have to ask explicitly.
   */
  Bool supported;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
  printf("✓ Auto-repeat detection: %s\n",
         supported ? "enabled" : "not supported");

  g_x11.screen = DefaultScreen(g_x11.display); /* usually screen 0 */
  g_x11.width  = platform_game_props->width;
  g_x11.height = platform_game_props->height;

  /*
   * GLX Visual / Framebuffer negotiation
   * ──────────────────────────────────────
   * A "Visual" in X11 describes a framebuffer format (color depth, alpha,
   * depth buffer, etc.).  For OpenGL we need a specific one.
   *
   * glXChooseVisual scans available visuals and returns the best match:
   *   GLX_RGBA       — true-colour (not palettised)
   *   GLX_DEPTH_SIZE 24 — 24-bit depth buffer (for 3D; we don't use it
   *                       but the driver may require it)
   *   GLX_DOUBLEBUFFER — two framebuffers: one displaying, one being drawn
   *
   * JS: roughly `gl = canvas.getContext('webgl', { depth: true, antialias: false })`
   */
  int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual  = glXChooseVisual(g_x11.display, g_x11.screen, visual_attribs);
  if (!visual) {
    fprintf(stderr, "Failed to choose GLX visual\n");
    return 1;
  }

  /*
   * Colormap: required by X11 for windows that use a custom Visual.
   * We allocate none ourselves (AllocNone) — the system manages colors.
   */
  Colormap colormap =
      XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                      visual->visual, AllocNone);

  /*
   * XSetWindowAttributes: configure what events we want and which colormap.
   *
   * event_mask selects which X events are delivered to our window:
   *   ExposureMask       — window needs redraw (e.g., uncovered by another window)
   *   KeyPressMask       — key was pressed
   *   KeyReleaseMask     — key was released
   *   StructureNotifyMask — resize, move, destroy notifications
   *
   * JS: like `window.addEventListener('keydown', ...)` for each event type.
   */
  XSetWindowAttributes attrs;
  attrs.colormap   = colormap;
  attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     StructureNotifyMask;

  g_x11.window = XCreateWindow(
      g_x11.display,
      RootWindow(g_x11.display, g_x11.screen), /* parent = root window */
      100, 100,                                /* initial x, y position */
      platform_game_props->width,
      platform_game_props->height,
      0,              /* border width */
      visual->depth,  /* color depth from chosen visual */
      InputOutput,    /* window type (not input-only) */
      visual->visual,
      CWColormap | CWEventMask, /* which attrs fields are valid */
      &attrs);

  /*
   * GLX rendering context
   * ──────────────────────
   * An OpenGL context holds all GL state (textures, shaders, buffers).
   * GL_TRUE = "direct rendering" — bypass the X server for drawing,
   * talk directly to the GPU driver.  Faster than indirect rendering.
   *
   * JS: this is like the WebGL context object you get from getContext('webgl').
   */
  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  if (!g_x11.gl_context) {
    fprintf(stderr, "Failed to create GLX context\n");
    XFree(visual); /* always free visual info when done with it */
    return 1;
  }

  XFree(visual); /* done with visual struct — free it */

  XStoreName(g_x11.display, g_x11.window, platform_game_props->title);
  XMapWindow(g_x11.display, g_x11.window);  /* make it visible */
  XFlush(g_x11.display);                    /* flush pending X commands */

  /* Bind the GL context to the window — all GL calls now draw here */
  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  /*
   * OpenGL 2-D setup
   * ─────────────────
   *
   * glOrtho sets up an orthographic (no perspective) projection matrix
   * that maps pixel coordinates to clip space.  Arguments:
   *   left=0, right=width, bottom=height, top=0, near=-1, far=1
   *
   * bottom=height, top=0 means Y=0 is the TOP of the screen (matching our
   * backbuffer where pixel[0] is the top-left corner).
   *
   * JS: like `ctx.setTransform(1,0,0,1,0,0)` setting up a simple 2D canvas
   *     coordinate system.
   */
  glViewport(0, 0, g_x11.width, g_x11.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, platform_game_props->width, platform_game_props->height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);  /* we're 2-D, no depth needed */
  glEnable(GL_TEXTURE_2D);   /* enable 2-D texture mapping */

  /*
   * Create a GPU texture for our software backbuffer.
   *
   * GL_NEAREST = no smoothing — pixel-art games look better crisp.
   * We update this texture every frame with the CPU-rendered pixels.
   */
  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  setup_vsync(); /* try to enable VSync via GLX extension */

  printf("✓ OpenGL initialized: %s\n", glGetString(GL_VERSION));

  if (platform_audio_init(&platform_game_props->audio) != 0) {
    fprintf(stderr,
            "Warning: Audio initialization failed, continuing without audio\n");
    /* non-fatal: game continues without sound */
  }
  return 0;
}

void platform_shutdown(void) {
  platform_audio_shutdown();
  if (g_x11.texture_id)
    glDeleteTextures(1, &g_x11.texture_id);  /* free GPU texture */
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL); /* unbind before destroying */
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)
    XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display)
    XCloseDisplay(g_x11.display);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Input Handling
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  X11 event model vs Raylib polling model:
 *  ─────────────────────────────────────────
 *  Raylib:  IsKeyDown() returns the current state right now (polling).
 *  X11:     The X server queues events.  We drain the queue with XPending +
 *           XNextEvent.  Between events the input state doesn't change.
 *
 *  XPending(display) — returns the number of events waiting in the queue.
 *  XNextEvent(display, &event) — pops the next event from the queue.
 *
 *  JS: like processing `window.eventQueue` in a while loop each frame,
 *      where the OS pushes events into the queue between frames.
 *
 *  Key symbol lookup:
 *  XLookupKeysym(&event.xkey, 0) — converts the raw hardware keycode to a
 *  portable KeySym constant like XK_Left, XK_a.
 *  JS: like `e.key` on a KeyboardEvent — OS-independent key name.
 */

static void platform_get_input(GameState *game_state, GameInput *input,
                               PlatformGameProps *platform_game_props) {
  XEvent event;

  /* Drain the entire event queue this frame (may be 0 or many events) */
  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &event);

    switch (event.type) {

    case KeyPress: {
      /* JS: like the `keydown` event */
      KeySym key = XLookupKeysym(&event.xkey, 0);
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape:
        platform_game_props->is_running = false;
        game_state->should_quit = 1;
        break;
      case XK_r:
      case XK_R:
        game_state->should_restart = 1;
        break;
      case XK_x:
      case XK_X:
        UPDATE_BUTTON(input->rotate_x, 1);
        game_state->current_piece.next_rotate_x_value =
            TETROMINO_ROTATE_X_GO_RIGHT;
        break;
      case XK_z:
      case XK_Z:
        UPDATE_BUTTON(input->rotate_x, 1);
        game_state->current_piece.next_rotate_x_value =
            TETROMINO_ROTATE_X_GO_LEFT;
        break;
      case XK_Left:
      case XK_A:
      case XK_a:
        UPDATE_BUTTON(input->move_left, 1);
        break;
      case XK_Right:
      case XK_D:
      case XK_d:
        UPDATE_BUTTON(input->move_right, 1);
        break;
      case XK_Down:
      case XK_S:
      case XK_s:
        UPDATE_BUTTON(input->move_down, 1);
        break;
      }
      break;
    }

    case KeyRelease: {
      /* JS: like the `keyup` event */
      KeySym key = XLookupKeysym(&event.xkey, 0);
      switch (key) {
      case XK_r:
      case XK_R:
        game_state->should_restart = 0;
        break;
      case XK_x:
      case XK_X:
      case XK_z:
      case XK_Z:
        UPDATE_BUTTON(input->rotate_x, 0);
        game_state->current_piece.next_rotate_x_value = TETROMINO_ROTATE_X_NONE;
        break;
      case XK_Left:
      case XK_A:
      case XK_a:
        UPDATE_BUTTON(input->move_left, 0);
        break;
      case XK_Right:
      case XK_D:
      case XK_d:
        UPDATE_BUTTON(input->move_right, 0);
        break;
      case XK_Down:
      case XK_S:
      case XK_s:
        UPDATE_BUTTON(input->move_down, 0);
        break;
      }
      break;
    }

    case ClientMessage:
      /* Window manager sent WM_DELETE_WINDOW (user clicked the close button) */
      platform_game_props->is_running = false;
      game_state->should_quit = 1;
      break;

    case ConfigureNotify:
      /*
       * Window was resized or moved.  We only care if the size changed.
       * Update our viewport so rendering fills the new window dimensions.
       * glOrtho re-establishes the 2-D projection for the new size.
       */
      if (event.xconfigure.width  != g_x11.width ||
          event.xconfigure.height != g_x11.height) {
        g_x11.width  = event.xconfigure.width;
        g_x11.height = event.xconfigure.height;
        glViewport(0, 0, g_x11.width, g_x11.height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, g_x11.width, g_x11.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
      }
      break;
    }
  }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Display Backbuffer
 * ══════════════════════════════════════════════════════════════════════════
 *
 *  Software renderer → GPU → screen pipeline (X11 / OpenGL):
 *  ──────────────────────────────────────────────────────────
 *
 *  1. game_render() filled bb->pixels (CPU array, RGBA32)
 *  2. glTexImage2D() uploads those pixels to a GPU texture
 *  3. glBegin(GL_QUADS) draws a textured quad that fills the window
 *  4. glXSwapBuffers() presents the back buffer (double-buffering)
 *
 *  The quad is positioned so the backbuffer is centred inside the window
 *  using the same offset calculation as the Raylib backend.
 *
 *  glTexImage2D vs Raylib's UpdateTexture:
 *  ─────────────────────────────────────────
 *  Raylib: UpdateTexture — uploads to an EXISTING texture (fast path)
 *  OpenGL: glTexImage2D  — (re)allocates AND uploads the texture.
 *          This is slightly less optimal (the driver may allocate new GPU
 *          memory each call) but simpler and correct for our use case.
 *
 * COURSE NOTE: The commented-out old implementation block that appeared in
 * the reference file has been removed.  That block was an earlier version
 * without centring support; the code below is the final, correct version.
 */

static void platform_display_backbuffer(Backbuffer *bb) {
  glClear(GL_COLOR_BUFFER_BIT); /* clear to black (letterbox areas) */

  /* Centre the backbuffer in the window (letterbox when window is larger) */
  int offset_x = (g_x11.width  - bb->width)  / 2;
  int offset_y = (g_x11.height - bb->height) / 2;
  if (offset_x < 0) offset_x = 0; /* clamp: don't draw outside window */
  if (offset_y < 0) offset_y = 0;

  /*
   * Upload our CPU pixels to the GPU texture.
   *
   * Arguments: target, mip level, internal format, w, h, border,
   *            pixel format, pixel type, data pointer
   *
   * GL_RGBA / GL_UNSIGNED_BYTE = 4 bytes per pixel: R G B A (matches
   * our backbuffer format defined in platform.h).
   */
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bb->width, bb->height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

  /*
   * Draw a textured quad that maps our texture onto the window.
   *
   * glTexCoord2f(u, v) sets the texture coordinate for the NEXT vertex.
   * glVertex2f(x, y) emits a vertex in our orthographic screen space.
   *
   *  (0,0) ──── (1,0)      texture space
   *    |            |
   *  (0,1) ──── (1,1)
   *
   * Vertices go counter-clockwise when viewed from the front.
   */
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(offset_x,            offset_y);
  glTexCoord2f(1.0f, 0.0f); glVertex2f(offset_x + bb->width, offset_y);
  glTexCoord2f(1.0f, 1.0f); glVertex2f(offset_x + bb->width, offset_y + bb->height);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(offset_x,            offset_y + bb->height);
  glEnd();

  /* Present: swap back buffer ↔ front buffer (GPU flips, no tearing) */
  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  /* Handmade Hero principle: The platform layer is a thin translator.
   *
   * Every line in main() is platform glue.  The game functions (game_init,
   * game_update, game_render) are identical to the Raylib build.
   * Only the platform calls around them differ.
   */

  /*
   * Initialise platform props (window size, backbuffer allocation, audio cfg).
   *
   * JS: `const props = platformGamePropsInit()` — a factory that also
   *     heap-allocates the pixel buffer for us.
   */
  PlatformGameProps platform_game_props = {0};
  if (platform_game_props_init(&platform_game_props) != 0) {
    return 1;
  }

  /* Open X11 display, create window, init OpenGL and ALSA */
  if (platform_init(&platform_game_props) != 0) {
    return 1;
  }

  /*
   * Double-buffer input (same pattern as Raylib backend).
   *
   * current_input  — being filled this frame
   * old_input      — what the game had last frame (for transition detection)
   *
   * JS: like keeping `prevInput = { ...input }` before polling each frame.
   */
  GameInput inputs[2]      = {0};
  GameInput *current_input = &inputs[0];
  GameInput *old_input     = &inputs[1];

  GameState game_state = {0};
  game_init(&game_state);

  /* Only initialise game audio if the platform audio layer succeeded */
  if (platform_game_props.audio.is_initialized) {
    game_audio_init(&game_state.audio,
                    platform_game_props.audio.samples_per_second);
    game_music_play(&game_state.audio);
  }

  g_last_frame_time = get_time(); /* record the time before the first frame */

  /* ─── Main Loop ─────────────────────────────────────────────────────── */
  while (platform_game_props.is_running) {
    /*
     * Compute delta_time manually (X11 has no GetFrameTime()).
     *
     * JS: `const dt = (performance.now() - lastTime) / 1000`
     */
    double current_time = get_time();
    float delta_time    = (float)(current_time - g_last_frame_time);
    g_last_frame_time   = current_time;

    /*
     * prepare_input_frame:  save current → old, reset transition counts.
     * Must be called BEFORE XPending/XNextEvent so we don't overwrite
     * last frame's data before it's been saved.
     */
    prepare_input_frame(old_input, current_input);

    /* Drain X11 event queue — fills current_input with this frame's events */
    platform_get_input(&game_state, current_input, &platform_game_props);

    /*
     * COURSE NOTE: Removed debug printf from reference - not needed in final code
     * (was: printf("Move Left: %d\n", current_input->move_left.half_transition_count))
     */

    if (game_state.should_quit) {
      break;
    }

    game_update(&game_state, current_input, delta_time);
    platform_audio_update(&game_state, &platform_game_props.audio);
    game_render(&platform_game_props.backbuffer, &game_state);
    platform_display_backbuffer(&platform_game_props.backbuffer);

    /*
     * Manual frame-rate cap (VSync fallback).
     *
     * If VSync is enabled, glXSwapBuffers already blocked until the monitor
     * refresh — we don't need to sleep.
     *
     * If VSync is NOT available, we calculate how long the frame took and
     * sleep for the remainder of 1/60 s.  This isn't perfectly accurate
     * (sleep may overshoot by a few ms) but it prevents the game from
     * running at hundreds of fps and burning the CPU.
     */
    if (!g_x11.vsync_enabled) {
      double frame_time = get_time() - current_time;
      if (frame_time < g_target_frame_time) {
        sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
      }
    }

    platform_swap_input_buffers(old_input, current_input);
  }

  platform_game_props_free(&platform_game_props); /* free backbuffer */
  platform_shutdown();                            /* close ALSA, GL, X11 */
  return 0;
}
