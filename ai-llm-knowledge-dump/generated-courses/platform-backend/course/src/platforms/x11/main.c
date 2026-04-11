#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/main.c — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 02 — XOpenDisplay, XCreateWindow, GLXCreateContext, event loop.
 *             XSizeHints to set a fixed initial window size (temporary!).
 *
 * LESSON 03 — glTexSubImage2D with GL_RGBA, glOrtho, full-screen quad.
 *             First pixel pushed to screen.
 *
 * LESSON 07 — XkbKeycodeToKeysym, UPDATE_BUTTON, XPending event drain.
 *             Input fed into GameInput double buffer.
 *
 * LESSON 08 — Letterbox math: scale factor + x/y offset so the canvas
 *             fills the window without distortion on resize.
 *             XSizeHints size-lock removed; ConfigureNotify handled.
 *             snprintf + draw_text for FPS counter.
 *             DE100 frame timing: coarse nanosleep + spin-wait.
 *             FrameStats under #ifdef DEBUG (printed on shutdown).
 *             VSync detection: GLX_EXT → GLX_MESA → software fallback.
 *
 * LESSON 09 — WindowScaleMode: apply_scale_mode() resizes backbuffer on
 *             ConfigureNotify; display_backbuffer() adapts to each mode.
 *             TAB key cycles FIXED → DYNAMIC_MATCH → DYNAMIC_ASPECT.
 *             GPU texture reallocated via glTexImage2D on resize.
 *
 * LESSON 12 — Full ALSA init via platform_audio_init (snd_pcm_hw_params).
 *
 * LESSON 15 — game/demo.c + game/audio_demo.c removed; game/main.c added.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h> /* XSizeHints — introduced L02, removed L08 */
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../game/base.h"
#include "../../game/main.h"
#include "../../platform.h"
#include "../../utils/perf.h"
#include "./audio.h"
#include "./base.h"

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame timing (DE100 pattern — LESSON 08)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * FrameTiming holds three timespec checkpoints per frame:
 *   frame_start — captured at top of loop
 *   work_end    — captured after game logic + render
 *   frame_end   — captured after sleep
 *
 * Two-phase sleep:
 *   Phase 1: coarse nanosleep(1ms) loop until 3ms before deadline.
 *   Phase 2: spin-wait to exact deadline (consumes one CPU core briefly
 *            but eliminates scheduler jitter in the critical window).
 *
 * FrameStats (DEBUG only): min/max/avg frame time, missed frames.
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  struct timespec frame_start;
  struct timespec work_end;
  struct timespec frame_end;
  float work_seconds;
  float total_seconds;
  float sleep_seconds;
} FrameTiming;

#ifdef DEBUG
typedef struct {
  unsigned int frame_count;
  unsigned int missed_frames;
  float min_frame_ms;
  float max_frame_ms;
  float total_frame_ms;
} FrameStats;
static FrameStats g_stats = {0};
#endif

static FrameTiming g_timing = {0};
static const float TARGET_SECONDS = 1.0f / TARGET_FPS;

static inline double timespec_to_seconds(const struct timespec *ts) {
  return (double)ts->tv_sec + (double)ts->tv_nsec * 1e-9;
}

static inline float timespec_diff_seconds(const struct timespec *a,
                                          const struct timespec *b) {
  return (float)(timespec_to_seconds(b) - timespec_to_seconds(a));
}

static inline void get_timespec(struct timespec *ts) {
  clock_gettime(CLOCK_MONOTONIC, ts);
}

/* LESSON 08 — Begin frame: capture frame_start. */
static inline void timing_begin(void) { get_timespec(&g_timing.frame_start); }

/* LESSON 08 — Mark work done: capture work_end, compute work_seconds. */
static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &g_timing.work_end);
}

/* LESSON 08 — Sleep until target using coarse sleep + spin-wait. */
static void timing_sleep_until_target(void) {
  float elapsed = g_timing.work_seconds;

  if (elapsed < TARGET_SECONDS) {
    float threshold = TARGET_SECONDS - 0.003f; /* 3ms spin budget */

    /* Phase 1: coarse 1ms nanosleep */
    while (elapsed < threshold) {
      struct timespec sleep_ts = {.tv_sec = 0, .tv_nsec = 1000000L};
      nanosleep(&sleep_ts, NULL);
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }

    /* Phase 2: spin-wait to exact target */
    while (elapsed < TARGET_SECONDS) {
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
  }
}

/* LESSON 08 — End frame: capture frame_end, compute total + sleep seconds. */
static inline void timing_end(void) {
  get_timespec(&g_timing.frame_end);
  g_timing.total_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &g_timing.frame_end);
  g_timing.sleep_seconds = g_timing.total_seconds - g_timing.work_seconds;

#ifdef DEBUG
  float ms = g_timing.total_seconds * 1000.0f;
  g_stats.frame_count++;
  if (g_stats.frame_count == 1 || ms < g_stats.min_frame_ms)
    g_stats.min_frame_ms = ms;
  if (ms > g_stats.max_frame_ms)
    g_stats.max_frame_ms = ms;
  g_stats.total_frame_ms += ms;
  if (g_timing.total_seconds > TARGET_SECONDS + 0.002f)
    g_stats.missed_frames++;
#endif
}

#ifdef DEBUG
static void print_frame_stats(void) {
  if (g_stats.frame_count == 0)
    return;
  printf("\n═══════════════════════════════════════════════════════════\n");
  printf("FRAME TIME STATISTICS\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Total frames:   %u\n", g_stats.frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", g_stats.missed_frames,
         (float)g_stats.missed_frames / g_stats.frame_count * 100.0f);
  printf("Min frame time: %.2f ms\n", g_stats.min_frame_ms);
  printf("Max frame time: %.2f ms\n", g_stats.max_frame_ms);
  printf("Avg frame time: %.2f ms\n",
         g_stats.total_frame_ms / (float)g_stats.frame_count);
  printf("═══════════════════════════════════════════════════════════\n");
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VSync detection (LESSON 08)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Try GLX_EXT_swap_control first (NVidia/AMD), then GLX_MESA_swap_control
 * (Mesa/open-source), then fall back to software frame limiter.            */
static void setup_vsync(void) {
  const char *ext = glXQueryExtensionsString(g_x11.display, g_x11.screen);

  if (ext && strstr(ext, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC fn =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (fn) {
      fn(g_x11.display, g_x11.window, 1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_EXT)\n");
      return;
    }
  }

  if (ext && strstr(ext, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC fn =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (fn) {
      fn(1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (MESA)\n");
      return;
    }
  }

  g_x11.vsync_enabled = false;
  printf("⚠ VSync not available — software frame limiter active\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scale mode (LESSON 09)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * apply_scale_mode — called from ConfigureNotify (window resized) and when
 * the user presses TAB to cycle modes.
 *
 * In FIXED mode: backbuffer stays 800×600; GPU letterbox handles display.
 * In DYNAMIC modes: backbuffer resized to match new window dimensions.
 *
 * GPU texture reallocate: glTexImage2D with NULL data reserves new storage
 * without uploading pixels.  Subsequent frames use glTexSubImage2D in-place.
 * This avoids re-allocating GPU memory every upload frame.                 */
static void apply_scale_mode(PlatformGameProps *props, int win_w, int win_h) {
  int new_w, new_h;

  switch (props->scale_mode) {
  case WINDOW_SCALE_MODE_FIXED:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    new_w = win_w;
    new_h = win_h;
    break;
  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
    break;
  default:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  }

  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);

    /* Reallocate GPU texture storage at new dimensions. */
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_w, new_h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Window + GL init (LESSON 02)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static int init_window(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "❌ XOpenDisplay failed\n");
    return -1;
  }

  /* Suppress auto-repeat key events (gives clean press/release pairs).
   * Without this, holding a key sends fake Release+Press pairs ~30Hz,
   * making half_transition_count fire every cycle. See L02/L07.          */
  {
    Bool ok;
    XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);
    if (!ok)
      fprintf(stderr, "⚠️  XkbSetDetectableAutoRepeat failed\n");
  }

  g_x11.screen = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "❌ glXChooseVisual: no matching visual\n");
    return -1;
  }

  Colormap cmap =
      XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                      visual->visual, AllocNone);

  XSetWindowAttributes wa = {
      .colormap = cmap,
      .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                    StructureNotifyMask | FocusChangeMask,
  };

  /* LESSON 02 — XCreateWindow: position (100,100), initial size GAME_W×GAME_H.
   */
  g_x11.window =
      XCreateWindow(g_x11.display, RootWindow(g_x11.display, g_x11.screen), 100,
                    100, GAME_W, GAME_H, 0, visual->depth, InputOutput,
                    visual->visual, CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  if (!g_x11.gl_context) {
    fprintf(stderr, "❌ glXCreateContext failed\n");
    return -1;
  }

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);
  g_x11.window_focused = true;

  /* WM_DELETE_WINDOW atom — lets us detect the close button. */
  g_x11.wm_delete_window =
      XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  /* LESSON 02 — XSizeHints: temporarily fix window to GAME_W × GAME_H so
   * early lessons (03–07) don't need letterbox math yet.
   * LESSON 08 — This block is REMOVED when we add real letterbox scaling. */
  /* [L02 temporary — removed in L08]
  XSizeHints *hints = XAllocSizeHints();
  hints->flags = PMinSize | PMaxSize;
  hints->min_width  = hints->max_width  = GAME_W;
  hints->min_height = hints->max_height = GAME_H;
  XSetWMNormalHints(g_x11.display, g_x11.window, hints);
  XFree(hints);
  */

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  /* LESSON 03 — Allocate the texture once at init (NULL pixels).
   * Subsequent frames use glTexSubImage2D to update pixels in-place.
   * LESSON 09 — apply_scale_mode() calls glTexImage2D again when resizing.  */
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GAME_W, GAME_H, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);

  return 0;
}

static void shutdown_window(void) {
  if (g_x11.texture_id)
    glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)
    XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display)
    XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input processing (LESSON 07, LESSON 09)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * LESSON 07 — XPending drain loop processes ALL pending events each frame.
 * LESSON 09 — ConfigureNotify triggers apply_scale_mode() on resize.
 *             XK_Tab mapped to cycle_scale_mode button.
 */

static void process_events(GameInput *curr, int *is_running,
                           PlatformGameProps *props) {
  while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);

    switch (ev.type) {
    case KeyPress: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {
      case XK_Escape:
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
        break;
      case XK_space:
        UPDATE_BUTTON(&curr->buttons.play_tone, 1);
        break;
      /* LESSON 09 — TAB cycles scale modes */
      case XK_Tab:
        UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 1);
        break;
      /* LESSON 17 — Camera control keys */
      case XK_Up:
        UPDATE_BUTTON(&curr->buttons.cam_up, 1);
        break;
      case XK_Down:
        UPDATE_BUTTON(&curr->buttons.cam_down, 1);
        break;
      case XK_Left:
        UPDATE_BUTTON(&curr->buttons.cam_left, 1);
        break;
      case XK_Right:
        UPDATE_BUTTON(&curr->buttons.cam_right, 1);
        break;
      case XK_e:
      case XK_E:
        UPDATE_BUTTON(&curr->buttons.zoom_in, 1);
        break;
      case XK_q:
      case XK_Q:
        UPDATE_BUTTON(&curr->buttons.zoom_out, 1);
        break;
      case XK_c:
      case XK_C:
        UPDATE_BUTTON(&curr->buttons.cam_reset, 1);
        break;
      case XK_bracketleft:
        UPDATE_BUTTON(&curr->buttons.prev_scene, 1);
        break;
      case XK_bracketright:
        UPDATE_BUTTON(&curr->buttons.next_scene, 1);
        break;
      case XK_r:
      case XK_R:
        UPDATE_BUTTON(&curr->buttons.reload_scene, 1);
        break;
      case XK_F1:
        UPDATE_BUTTON(&curr->buttons.toggle_debug_hud, 1);
        break;
      case XK_F2:
        UPDATE_BUTTON(&curr->buttons.toggle_runtime_override, 1);
        break;
      case XK_F3:
        UPDATE_BUTTON(&curr->buttons.clear_runtime_override, 1);
        break;
      }
      break;
    }
    case KeyRelease: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {
      case XK_Escape:
        UPDATE_BUTTON(&curr->buttons.quit, 0);
        break;
      case XK_space:
        UPDATE_BUTTON(&curr->buttons.play_tone, 0);
        break;
      case XK_Tab:
        UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 0);
        break;
      /* LESSON 17 — Camera control keys */
      case XK_Up:
        UPDATE_BUTTON(&curr->buttons.cam_up, 0);
        break;
      case XK_Down:
        UPDATE_BUTTON(&curr->buttons.cam_down, 0);
        break;
      case XK_Left:
        UPDATE_BUTTON(&curr->buttons.cam_left, 0);
        break;
      case XK_Right:
        UPDATE_BUTTON(&curr->buttons.cam_right, 0);
        break;
      case XK_e:
      case XK_E:
        UPDATE_BUTTON(&curr->buttons.zoom_in, 0);
        break;
      case XK_q:
      case XK_Q:
        UPDATE_BUTTON(&curr->buttons.zoom_out, 0);
        break;
      case XK_c:
      case XK_C:
        UPDATE_BUTTON(&curr->buttons.cam_reset, 0);
        break;
      case XK_bracketleft:
        UPDATE_BUTTON(&curr->buttons.prev_scene, 0);
        break;
      case XK_bracketright:
        UPDATE_BUTTON(&curr->buttons.next_scene, 0);
        break;
      case XK_r:
      case XK_R:
        UPDATE_BUTTON(&curr->buttons.reload_scene, 0);
        break;
      case XK_F1:
        UPDATE_BUTTON(&curr->buttons.toggle_debug_hud, 0);
        break;
      case XK_F2:
        UPDATE_BUTTON(&curr->buttons.toggle_runtime_override, 0);
        break;
      case XK_F3:
        UPDATE_BUTTON(&curr->buttons.clear_runtime_override, 0);
        break;
      }
      break;
    }
    case ClientMessage:
      if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window) {
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
      }
      break;

    /* LESSON 08/09 — ConfigureNotify: window was resized.
     * L08: update window_w/h so letterbox math recalculates.
     * L09: also call apply_scale_mode() to resize the backbuffer.          */
    case ConfigureNotify: {
      int new_w = ev.xconfigure.width;
      int new_h = ev.xconfigure.height;
      if (new_w != g_x11.window_w || new_h != g_x11.window_h) {
        g_x11.window_w = new_w;
        g_x11.window_h = new_h;
        apply_scale_mode(props, g_x11.window_w, g_x11.window_h);
      }
      break;
    }
    case FocusIn:
      if (!g_x11.window_focused) {
        g_x11.window_focused = true;
        platform_audio_resume_cfg(&props->audio_config);
        printf("[focus] X11 gained focus — audio resumed\n");
      }
      break;
    case FocusOut:
      if (g_x11.window_focused) {
        g_x11.window_focused = false;
        platform_audio_pause_cfg(&props->audio_config);
        printf("[focus] X11 lost focus — audio paused\n");
      }
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Backbuffer → screen (LESSON 03, LESSON 08, LESSON 09)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * FIXED mode:   letterbox the backbuffer into the window (black bars).
 * DYNAMIC modes: backbuffer already sized to fit; center at 1:1 pixel scale.
 *
 * PIXEL FORMAT — GL_RGBA:
 * Our GAME_RGB(r,g,b) stores pixels in memory as [R][G][B][A] bytes.
 * GL_RGBA tells the driver: "read bytes as [Red][Green][Blue][Alpha]"
 * — matches our layout exactly.  GL_BGRA would swap R↔B and is WRONG.     */
static void display_backbuffer(Backbuffer *backbuffer,
                               WindowScaleMode scale_mode) {
  int off_x = 0, off_y = 0;
  int dst_w = backbuffer->width;
  int dst_h = backbuffer->height;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    /* LESSON 08 — GPU letterbox scaling */
    float scale = 0;
    platform_compute_letterbox(g_x11.window_w, g_x11.window_h,
                               backbuffer->width, backbuffer->height, &scale,
                               &off_x, &off_y);
    dst_w = (int)(backbuffer->width * scale);
    dst_h = (int)(backbuffer->height * scale);
  } else {
    /* LESSON 09 — Dynamic modes: center backbuffer in window at 1:1 scale. */
    off_x = (g_x11.window_w - backbuffer->width) / 2;
    off_y = (g_x11.window_h - backbuffer->height) / 2;
  }

  /* LESSON 08 — Reset viewport + projection to current window size each frame
   * so the quad maps to real pixels correctly.                              */
  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  /* LESSON 03 — glTexSubImage2D: update pixels in-place (no GPU re-alloc). */
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, backbuffer->width, backbuffer->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, backbuffer->pixels);

  float x0 = (float)off_x;
  float y0 = (float)off_y;
  float x1 = (float)(off_x + dst_w);
  float y1 = (float)(off_y + dst_h);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(x0, y0);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(x1, y0);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(x1, y1);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(x0, y1);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio drain loop (LESSON 12)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void process_audio(GameAppState *game, AudioOutputBuffer *audio_buf,
                          PlatformAudioConfig *cfg) {
  if (!cfg->alsa_pcm || !g_x11.window_focused)
    return;

  int frames = platform_audio_get_samples_to_write(cfg);
  if (frames <= 0)
    return;

  if (frames > audio_buf->max_sample_count)
    frames = audio_buf->max_sample_count;

  game_app_get_audio_samples(game, audio_buf, frames);
  platform_audio_write(audio_buf, frames, cfg);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  /* ── Window ────────────────────────────────────────────────────────── */
  if (init_window() != 0)
    return 1;
  setup_vsync();

  /* ── Shared resource allocation (backbuffer + audio) ─────────────── */
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "❌ Out of memory\n");
    return 1;
  }

  if (platform_audio_init(&props.audio_config) != 0) {
    fprintf(stderr, "⚠ Audio init failed — continuing without audio\n");
  }

  /* ── Game state ────────────────────────────────────────────────────── */
  GameAppState *game = NULL;
  if (game_app_init(&game, &props.perm) != 0) {
    fprintf(stderr, "❌ Failed to initialize game facade\n");
    platform_audio_shutdown_cfg(&props.audio_config);
    shutdown_window();
    platform_game_props_free(&props);
    return 1;
  }

  /* ── Input double buffer ───────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  /* ── Main loop ─────────────────────────────────────────────────────── */
  int is_running = 1;

#ifdef DEBUG
  printf("\n[DEBUG build — ASan + UBSan + FrameStats active]\n\n");
#endif

  while (is_running) {
    /* LESSON 08 — Begin frame timing */
    timing_begin();

    /* LESSON 07 — Swap + prepare input buffers */
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* LESSON 07/09 — Drain X11 event queue (handles resize + TAB) */
    process_events(curr_input, &is_running, &props);
    if (curr_input->buttons.quit.ended_down)
      break;

    /* LESSON 09 — Cycle scale mode on TAB press */
    if (button_just_pressed(curr_input->buttons.cycle_scale_mode)) {
      props.scale_mode =
          (WindowScaleMode)((props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT);
      apply_scale_mode(&props, g_x11.window_w, g_x11.window_h);
    }

    float dt = g_timing.total_seconds > 0.0f ? g_timing.total_seconds
                                             : (1.0f / TARGET_FPS);
    PERF_BEGIN_NAMED(x11_game_update, "x11/game_app_update");
    game_app_update(game, curr_input, dt, &props.level);
    PERF_END(x11_game_update);

    /* LESSON 12 — Write audio samples to ALSA */
    PERF_BEGIN_NAMED(x11_process_audio, "x11/process_audio");
    process_audio(game, &props.audio_buffer, &props.audio_config);
    PERF_END(x11_process_audio);

    /* LESSON 08 — Rolling 60-frame FPS average */
    static float frame_time_accum = 0.0f;
    static int frame_time_count = 0;
    static int fps_display = TARGET_FPS;
    frame_time_accum += g_timing.total_seconds;
    frame_time_count++;
    if (frame_time_count >= 60) {
      fps_display = (frame_time_accum > 0.0f)
                        ? (int)(60.0f / frame_time_accum + 0.5f)
                        : TARGET_FPS;
      frame_time_accum = 0.0f;
      frame_time_count = 0;
    }

    /* LESSON 03/08/09 — Render demo frame + push to screen
     *
     * LESSON 16 — TempMemory frame pattern: wrap each frame in a checkpoint.
     * All scratch allocations inside demo_render are freed by arena_end_temp.
     * arena_check asserts no orphaned arena_begin_temp calls remain.        */
    PERF_BEGIN_NAMED(x11_demo_render, "x11/demo_render+display");
    TempMemory frame_scratch = arena_begin_temp(&props.scratch);
    game_app_render(game, &props.backbuffer, fps_display, props.scale_mode,
                    props.world_config, &props.perm, &props.level,
                    &props.scratch);
    arena_end_temp(frame_scratch);
    arena_check(&props.scratch);

    display_backbuffer(&props.backbuffer, props.scale_mode);
    PERF_END(x11_demo_render);

    /* LESSON 08 — Frame timing: mark work done, sleep, end */
    timing_mark_work_done();
    if (!g_x11.vsync_enabled) {
      timing_sleep_until_target();
    }
    timing_end();

    /* LESSON 09 — BENCH_DURATION_S: auto-exit after N seconds.
     * Uses the accumulated frame_start timestamps for wall-clock time.     */
#ifdef BENCH_DURATION_S
    {
      static struct timespec _bench_t0 = {0};
      if (_bench_t0.tv_sec == 0)
        _bench_t0 = g_timing.frame_start;
      double _bench_elapsed = timespec_to_seconds(&g_timing.frame_start) -
                              timespec_to_seconds(&_bench_t0);
      if (_bench_elapsed >= (double)BENCH_DURATION_S) {
        printf("\n[bench] %.1f seconds elapsed — printing profiler summary.\n",
               _bench_elapsed);
        perf_print_all();
        is_running = 0;
      }
    }
#endif
  }

  /* ── Cleanup ───────────────────────────────────────────────────────── */
#ifdef DEBUG
  print_frame_stats();
#endif

  platform_audio_shutdown_cfg(&props.audio_config);
  platform_game_props_free(&props);
  shutdown_window();
  return 0;
}
