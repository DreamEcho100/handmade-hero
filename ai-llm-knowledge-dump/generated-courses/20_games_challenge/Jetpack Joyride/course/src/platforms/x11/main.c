#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/main.c — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * X11/GLX + ALSA backend. Adapted from Platform Foundation Course.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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
 * Frame timing
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

static inline void timing_begin(void) { get_timespec(&g_timing.frame_start); }

static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &g_timing.work_end);
}

static void timing_sleep_until_target(void) {
  float elapsed = g_timing.work_seconds;

  if (elapsed < TARGET_SECONDS) {
    float threshold = TARGET_SECONDS - 0.003f;

    while (elapsed < threshold) {
      struct timespec sleep_ts = {.tv_sec = 0, .tv_nsec = 1000000L};
      nanosleep(&sleep_ts, NULL);
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }

    while (elapsed < TARGET_SECONDS) {
      struct timespec now;
      get_timespec(&now);
      elapsed = timespec_diff_seconds(&g_timing.frame_start, &now);
    }
  }
}

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
  printf("\nFRAME TIME STATISTICS\n");
  printf("Total frames:   %u\n", g_stats.frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", g_stats.missed_frames,
         (float)g_stats.missed_frames / g_stats.frame_count * 100.0f);
  printf("Min frame time: %.2f ms\n", g_stats.min_frame_ms);
  printf("Max frame time: %.2f ms\n", g_stats.max_frame_ms);
  printf("Avg frame time: %.2f ms\n",
         g_stats.total_frame_ms / (float)g_stats.frame_count);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VSync
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void setup_vsync(void) {
  const char *ext = glXQueryExtensionsString(g_x11.display, g_x11.screen);

  if (ext && strstr(ext, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC fn =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (fn) {
      fn(g_x11.display, g_x11.window, 1);
      g_x11.vsync_enabled = true;
      printf("VSync enabled (GLX_EXT)\n");
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
      printf("VSync enabled (MESA)\n");
      return;
    }
  }

  g_x11.vsync_enabled = false;
  printf("VSync not available -- software frame limiter active\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Scale mode
 * ═══════════════════════════════════════════════════════════════════════════
 */
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
    platform_compute_aspect_size(win_w, win_h, 16.0f, 9.0f, &new_w, &new_h);
    break;
  default:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  }

  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);

    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_w, new_h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Window + GL init
 * ═══════════════════════════════════════════════════════════════════════════
 */
static int init_window(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "XOpenDisplay failed\n");
    return -1;
  }

  Bool ok;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);

  g_x11.screen = DefaultScreen(g_x11.display);
  /* Open at 3x native resolution. */
  g_x11.window_w = GAME_W * 3;
  g_x11.window_h = GAME_H * 3;

  int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "glXChooseVisual: no matching visual\n");
    return -1;
  }

  Colormap cmap =
      XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                      visual->visual, AllocNone);

  XSetWindowAttributes wa = {
      .colormap = cmap,
      .event_mask =
          ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask,
  };

  g_x11.window =
      XCreateWindow(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                    100, 100, (unsigned)g_x11.window_w,
                    (unsigned)g_x11.window_h, 0, visual->depth, InputOutput,
                    visual->visual, CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  if (!g_x11.gl_context) {
    fprintf(stderr, "glXCreateContext failed\n");
    return -1;
  }

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);

  g_x11.wm_delete_window =
      XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
 * Input processing
 * ═══════════════════════════════════════════════════════════════════════════
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
        UPDATE_BUTTON(&curr->buttons.action, 1);
        break;
      case XK_p: case XK_P:
        UPDATE_BUTTON(&curr->buttons.pause, 1);
        break;
      case XK_r: case XK_R:
        UPDATE_BUTTON(&curr->buttons.restart, 1);
        break;
      case XK_Left: case XK_a: case XK_A:
        UPDATE_BUTTON(&curr->buttons.left, 1);
        break;
      case XK_Right: case XK_d: case XK_D:
        UPDATE_BUTTON(&curr->buttons.right, 1);
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
        UPDATE_BUTTON(&curr->buttons.action, 0);
        break;
      case XK_p: case XK_P:
        UPDATE_BUTTON(&curr->buttons.pause, 0);
        break;
      case XK_r: case XK_R:
        UPDATE_BUTTON(&curr->buttons.restart, 0);
        break;
      case XK_Left: case XK_a: case XK_A:
        UPDATE_BUTTON(&curr->buttons.left, 0);
        break;
      case XK_Right: case XK_d: case XK_D:
        UPDATE_BUTTON(&curr->buttons.right, 0);
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
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Backbuffer → screen
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void display_backbuffer(Backbuffer *backbuffer,
                               WindowScaleMode scale_mode) {
  int off_x = 0, off_y = 0;
  int dst_w = backbuffer->width;
  int dst_h = backbuffer->height;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    float scale = 0;
    platform_compute_letterbox(g_x11.window_w, g_x11.window_h,
                               backbuffer->width, backbuffer->height,
                               &scale, &off_x, &off_y);
    dst_w = (int)(backbuffer->width  * scale);
    dst_h = (int)(backbuffer->height * scale);
  } else {
    off_x = (g_x11.window_w - backbuffer->width)  / 2;
    off_y = (g_x11.window_h - backbuffer->height) / 2;
  }

  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                  backbuffer->width, backbuffer->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, backbuffer->pixels);

  float x0 = (float)off_x;
  float y0 = (float)off_y;
  float x1 = (float)(off_x + dst_w);
  float y1 = (float)(off_y + dst_h);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
  glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
  glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio drain
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void process_audio(GameState *game_state, AudioOutputBuffer *audio_buf,
                          PlatformAudioConfig *cfg) {
  if (!cfg->alsa_pcm)
    return;

  int frames = platform_audio_get_samples_to_write(cfg);
  if (frames <= 0)
    return;

  if (frames > audio_buf->max_sample_count)
    frames = audio_buf->max_sample_count;

  game_get_audio_samples(game_state, audio_buf, frames);
  platform_audio_write(audio_buf, frames, cfg);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */
int main(void) {
  if (init_window() != 0)
    return 1;
  setup_vsync();

  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }

  if (platform_audio_init(&props.audio_config) != 0) {
    fprintf(stderr, "Audio init failed -- continuing without audio\n");
  }

  /* ── Game state ─────────────────────────────────────────────────────── */
  GameState game_state = {0};
  game_init(&game_state, &props);

  /* Audio loads on background pthread — game starts immediately */

  /* ── Input double buffer ────────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  int is_running = 1;

#ifdef DEBUG
  printf("\n[DEBUG build -- ASan + UBSan + FrameStats active]\n\n");
#endif

  while (is_running && game_state.is_running) {
    timing_begin();

    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    process_events(curr_input, &is_running, &props);
    if (curr_input->buttons.quit.ended_down)
      break;

    /* Delta time */
    float dt = g_timing.total_seconds > 0.0f
                   ? g_timing.total_seconds
                   : (1.0f / TARGET_FPS);
    if (dt > 0.05f) dt = 0.05f;

    /* Sync camera */
    props.world_config.camera_x = game_state.camera.x;
    props.world_config.camera_y = game_state.camera.y;
    props.world_config.camera_zoom = game_state.camera.zoom;

    game_update(&game_state, curr_input, dt);

    /* Audio */
    process_audio(&game_state, &props.audio_buffer, &props.audio_config);

    /* Render */
    TempMemory frame_scratch = arena_begin_temp(&props.scratch);
    game_render(&game_state, &props.backbuffer, props.world_config,
                &props.scratch);
    arena_end_temp(frame_scratch);
    arena_check(&props.scratch);

    display_backbuffer(&props.backbuffer, props.scale_mode);

    /* Frame timing */
    timing_mark_work_done();
    if (!g_x11.vsync_enabled) {
      timing_sleep_until_target();
    }
    timing_end();

#ifdef BENCH_DURATION_S
    {
      static struct timespec _bench_t0 = {0};
      if (_bench_t0.tv_sec == 0) _bench_t0 = g_timing.frame_start;
      double _bench_elapsed = timespec_to_seconds(&g_timing.frame_start)
                            - timespec_to_seconds(&_bench_t0);
      if (_bench_elapsed >= (double)BENCH_DURATION_S) {
        printf("\n[bench] %.1f seconds elapsed\n", _bench_elapsed);
        perf_print_all();
        is_running = 0;
      }
    }
#endif
  }

  /* ── Cleanup ────────────────────────────────────────────────────────── */
#ifdef DEBUG
  print_frame_stats();
#endif

  game_cleanup(&game_state);
  platform_audio_shutdown_cfg(&props.audio_config);
  platform_game_props_free(&props);
  shutdown_window();
  return 0;
}
