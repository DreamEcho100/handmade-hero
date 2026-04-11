#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include "../../game/demo.h"
#include "../../platform.h"
#include "../../utils/backbuffer.h"
#include "./base.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h> /* XSizeHints — introduced L02 */
#include <X11/keysym.h>
#include <stdio.h>
#include <time.h>

#define DEBUG 1

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

/* Begin frame: capture frame_start. */
static inline void timing_begin(void) { get_timespec(&g_timing.frame_start); }

/* Mark work done: capture work_end, compute work_seconds. */
static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &g_timing.work_end);
}

/* Sleep until target using coarse sleep + spin-wait. */
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

/* End frame: capture frame_end, compute total + sleep seconds. */
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
  printf("\n");
  printf("FRAME TIME STATISTICS\n");
  printf("Total frames:   %u\n", g_stats.frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", g_stats.missed_frames,
         (float)g_stats.missed_frames / g_stats.frame_count * 100.0f);
  printf("Min frame time: %.2f ms\n", g_stats.min_frame_ms);
  printf("Max frame time: %.2f ms\n", g_stats.max_frame_ms);
  printf("Avg frame time: %.2f ms\n",
         g_stats.total_frame_ms / (float)g_stats.frame_count);
}
#endif

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
  printf("VSync not available — software frame limiter active\n");
}

static void display_backbuffer(Backbuffer *backbuffer,
                               WindowScaleMode scale_mode) {
  float scale = 1.0f;
  int off_x = 0, off_y = 0;
  int dst_w = backbuffer->width;
  int dst_h = backbuffer->height;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    /* GPU letterbox scaling */
    platform_compute_letterbox(g_x11.window_w, g_x11.window_h,
                               backbuffer->width, backbuffer->height, &scale,
                               &off_x, &off_y);
    dst_w = (int)(backbuffer->width * scale);
    dst_h = (int)(backbuffer->height * scale);
  } else {
    /* Dynamic modes: center backbuffer in window (1:1 scale) */
    off_x = (g_x11.window_w - backbuffer->width) / 2;
    off_y = (g_x11.window_h - backbuffer->height) / 2;
  }

  /* Set viewport and projection to current window size
   * so the full-screen quad maps to real pixels correctly. */
  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // glClear(GL_COLOR_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);

  /* Upload CPU pixels to the already-allocated GPU texture.
   * glTexSubImage2D updates in-place (no re-alloc).
   * GL_RGBA + GL_UNSIGNED_BYTE: reads our [R,G,B,A] bytes correctly. */
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, backbuffer->width, backbuffer->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, backbuffer->pixels);

  float x0 = (float)off_x;
  float y0 = (float)off_y;
  float x1 = (float)(off_x + dst_w);
  float y1 = (float)(off_y + dst_h);

  /* Draw a full-screen textured quad.
   * L08 replaces this with a letterboxed quad. */
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(x0, y0);
  glTexCoord2f(1, 0);
  glVertex2f(x1, y0);
  glTexCoord2f(1, 1);
  glVertex2f(x1, y1);
  glTexCoord2f(0, 1);
  glVertex2f(x0, y1);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* NEW: Add helper to apply scale mode on resize */
static void apply_scale_mode(PlatformGameProps *props, int win_w, int win_h) {
  int new_w, new_h;

  switch (props->scale_mode) {
  case WINDOW_SCALE_MODE_FIXED:
    /* Reset to fixed size */
    new_w = GAME_W;
    new_h = GAME_H;
    break;

  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    /* Match window exactly */
    new_w = win_w;
    new_h = win_h;
    break;

  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    /* Preserve 4:3 aspect ratio */
    platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
    break;

  default:
    new_w = GAME_W;
    new_h = GAME_H;
    break;
  }

  /* Resize backbuffer if needed */
  if (new_w != props->backbuffer.width || new_h != props->backbuffer.height) {
    backbuffer_resize(&props->backbuffer, new_w, new_h);

    /* Reallocate GPU texture */
    glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_w, new_h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
  }
}

static void process_events(GameInput *curr, int *is_running,
                           PlatformGameProps *props) {
  /* XPending drain loop */
  while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);

    switch (ev.type) {
    case KeyPress: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {
      case XK_Escape:
      case XK_q:
      case XK_Q: {
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
        break;
      }
      case XK_space: {
        UPDATE_BUTTON(&curr->buttons.play_tone, 1);
        break;
      }
      case XK_Tab: {
        UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 1);
        break;
      }
      }
      break;
    }
    case KeyRelease: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {
      case XK_Escape:
      case XK_q:
      case XK_Q: {
        UPDATE_BUTTON(&curr->buttons.quit, 0);
        break;
      }
      case XK_space: {
        UPDATE_BUTTON(&curr->buttons.play_tone, 0);
        break;
      }
      case XK_Tab: {
        UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 0);
        break;
      }
      }
      break;
    }
    case ClientMessage: {
      if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window) {
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
      }
      break;
    }
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

static int init_window(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "XOpenDisplay failed\n");
    return 1;
  }

  Bool is_ok = false;
  XkbSetDetectableAutoRepeat(g_x11.display, true, &is_ok);
  if (!is_ok) {
    fprintf(stderr, "XkbSetDetectableAutoRepeat failed\n");
    return 1;
  }

  g_x11.screen = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "glXChooseVisual failed\n");
    return 1;
  }

  Window root_window = RootWindow(g_x11.display, g_x11.screen);

  XSetWindowAttributes wa = {
      .colormap = XCreateColormap(g_x11.display, root_window, visual->visual,
                                  AllocNone),
      .event_mask =
          ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask,
  };

  g_x11.window = XCreateWindow(       //
      g_x11.display,                  // connection
      root_window,                    // parent window
      0, 0,                           // x, y
      g_x11.window_w, g_x11.window_h, // width, height
      0,                              // border width
      visual->depth,                  // depth
      InputOutput,                    // what kind of window
      visual->visual,                 // visual
      CWColormap | CWEventMask,       // mask
      &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);

  g_x11.wm_delete_window =
      XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  /* Allocate texture storage (was missing — glTexSubImage2D needs this!) */
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

int main(void) {
  /* ── Window ────────────────────────────────────────────────────────── */
  if (init_window() != 0) {
    return 1;
  }
  setup_vsync();

  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "Failed to allocate game props\n");
    return 1;
  }
  Backbuffer *backbuffer = &props.backbuffer;

  /* ── Input double buffer ───────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  int is_running = 1;

  while (is_running) {
    /* Begin frame timing */
    timing_begin();

    /* Swap + prepare input buffers */
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    /* Drain X11 event queue */
    process_events(curr_input, &is_running, &props);
    if (curr_input->buttons.quit.ended_down) {
      break;
    }

    /* Handle scale mode cycling (on key press, not hold) */
    if (curr_input->buttons.cycle_scale_mode.ended_down &&
        curr_input->buttons.cycle_scale_mode.half_transitions > 0) {
      props.scale_mode = (props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT;
      apply_scale_mode(&props, g_x11.window_w, g_x11.window_h);
      printf("Scale mode: %d\n", props.scale_mode);
    }

    // TODO: Handle window resizing + scale mode in a more robust way.
    /*
    if ((props.backbuffer.width != g_x11.window_w) ||
        (props.backbuffer.height != g_x11.window_h)) {
      // backbuffer_resize(&props.backbuffer, g_x11.window_w, g_x11.window_h);
      apply_scale_mode(&props, g_x11.window_w, g_x11.window_h);
    }
    */

    /* Rolling 60-frame FPS average.
     * A raw (1/total_seconds) truncation oscillates +/-2 fps every frame. */
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

    demo_render(&props.backbuffer, curr_input, props.world_config,
                props.scale_mode, fps_display); /* fps=0 until L08 */
    display_backbuffer(backbuffer, props.scale_mode);

    /* Frame timing: mark work done, sleep, end */
    timing_mark_work_done();
    if (!g_x11.vsync_enabled) {
      timing_sleep_until_target();
    }
    timing_end();
  }

#ifdef DEBUG
  print_frame_stats();
#endif

  platform_game_props_free(&props);
  shutdown_window();

  return 0;
}
