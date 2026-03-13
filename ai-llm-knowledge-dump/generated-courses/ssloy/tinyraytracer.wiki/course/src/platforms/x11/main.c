#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/main.c — TinyRaytracer Course
 * (Adapted from Platform Foundation Course — audio stripped, raytracer keys)
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "./base.h"
#include "../../platform.h"
#include "../../game/base.h"
#include "../../game/main.h"
#include "../../utils/math.h"

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

static inline void timing_begin(void) {
  get_timespec(&g_timing.frame_start);
}

static inline void timing_mark_work_done(void) {
  get_timespec(&g_timing.work_end);
  g_timing.work_seconds = timespec_diff_seconds(&g_timing.frame_start,
                                                  &g_timing.work_end);
}

static void timing_sleep_until_target(void) {
  float elapsed = g_timing.work_seconds;
  if (elapsed < TARGET_SECONDS) {
    float threshold = TARGET_SECONDS - 0.003f;
    while (elapsed < threshold) {
      struct timespec sleep_ts = { .tv_sec = 0, .tv_nsec = 1000000L };
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
  g_timing.total_seconds = timespec_diff_seconds(&g_timing.frame_start,
                                                   &g_timing.frame_end);
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
  if (g_stats.frame_count == 0) return;
  printf("\nFrame Stats: %u frames, %u missed, %.2f-%.2f ms (avg %.2f)\n",
         g_stats.frame_count, g_stats.missed_frames,
         g_stats.min_frame_ms, g_stats.max_frame_ms,
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
      return;
    }
  }
  if (ext && strstr(ext, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC fn =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (fn) { fn(1); g_x11.vsync_enabled = true; return; }
  }
  g_x11.vsync_enabled = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Window + GL init
 * ═══════════════════════════════════════════════════════════════════════════
 */
static int init_window(void) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) { fprintf(stderr, "XOpenDisplay failed\n"); return -1; }

  Bool ok;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);
  g_x11.screen   = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) { fprintf(stderr, "glXChooseVisual failed\n"); return -1; }

  Colormap cmap = XCreateColormap(g_x11.display,
                                   RootWindow(g_x11.display, g_x11.screen),
                                   visual->visual, AllocNone);
  XSetWindowAttributes wa = {
    .colormap   = cmap,
    .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                  ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                  StructureNotifyMask,
  };

  g_x11.window = XCreateWindow(
      g_x11.display, RootWindow(g_x11.display, g_x11.screen),
      100, 100, GAME_W, GAME_H, 0,
      visual->depth, InputOutput, visual->visual,
      CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);
  if (!g_x11.gl_context) { fprintf(stderr, "glXCreateContext failed\n"); return -1; }

  XStoreName(g_x11.display, g_x11.window, TITLE);
  XMapWindow(g_x11.display, g_x11.window);

  g_x11.wm_delete_window = XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glViewport(0, 0, GAME_W, GAME_H);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glOrtho(0, GAME_W, GAME_H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GAME_W, GAME_H, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  return 0;
}

static void shutdown_window(void) {
  if (g_x11.texture_id) glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)  XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display) XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input — raytracer key bindings
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void handle_key(GameInput *curr, KeySym sym, int is_down) {
  switch (sym) {
  case XK_Escape: case XK_q: case XK_Q:
    UPDATE_BUTTON(&curr->buttons.quit, is_down); break;
  /* Navigation — arrows + WASD */
  case XK_Left:  case XK_a: UPDATE_BUTTON(&curr->buttons.camera_left, is_down); break;
  case XK_Right: case XK_d: UPDATE_BUTTON(&curr->buttons.camera_right, is_down); break;
  case XK_Up:    UPDATE_BUTTON(&curr->buttons.camera_up, is_down); break;
  case XK_Down:  UPDATE_BUTTON(&curr->buttons.camera_down, is_down); break;
  case XK_w: case XK_W: UPDATE_BUTTON(&curr->buttons.camera_forward, is_down); break;
  case XK_s:             UPDATE_BUTTON(&curr->buttons.camera_backward, is_down); break;
  /* Exports (uppercase for S to avoid WASD conflict) */
  case XK_p: case XK_P: UPDATE_BUTTON(&curr->buttons.export_ppm, is_down); break;
  case XK_A:             UPDATE_BUTTON(&curr->buttons.export_anaglyph, is_down); break;
  case XK_S:             UPDATE_BUTTON(&curr->buttons.export_sidebyside, is_down); break;
  case XK_g: UPDATE_BUTTON(&curr->buttons.export_stereogram, is_down); break;
  case XK_G: UPDATE_BUTTON(&curr->buttons.export_stereogram_cross, is_down); break;
  case XK_L: UPDATE_BUTTON(&curr->buttons.export_glsl, is_down); break;
  /* Feature toggles */
  case XK_v: case XK_V: UPDATE_BUTTON(&curr->buttons.toggle_voxels, is_down); break;
  case XK_f: case XK_F: UPDATE_BUTTON(&curr->buttons.toggle_floor, is_down); break;
  case XK_b: case XK_B: UPDATE_BUTTON(&curr->buttons.toggle_boxes, is_down); break;
  case XK_m: case XK_M: UPDATE_BUTTON(&curr->buttons.toggle_meshes, is_down); break;
  case XK_r: case XK_R: UPDATE_BUTTON(&curr->buttons.toggle_reflections, is_down); break;
  case XK_t: case XK_T: UPDATE_BUTTON(&curr->buttons.toggle_refractions, is_down); break;
  case XK_h: case XK_H: UPDATE_BUTTON(&curr->buttons.toggle_shadows, is_down); break;
  case XK_e: case XK_E: UPDATE_BUTTON(&curr->buttons.toggle_envmap, is_down); break;
  case XK_x: case XK_X: UPDATE_BUTTON(&curr->buttons.toggle_aa, is_down); break;
  case XK_c: case XK_C: UPDATE_BUTTON(&curr->buttons.cycle_envmap_mode, is_down); break;
  /* Render scale */
  case XK_Tab: UPDATE_BUTTON(&curr->buttons.scale_cycle, is_down); break;
  }
}

static float g_prev_mouse_x = 0.0f;
static float g_prev_mouse_y = 0.0f;

static void process_events(GameInput *curr, int *is_running) {
  /* Reset mouse delta per frame (accumulated from events below). */
  curr->mouse.dx     = 0.0f;
  curr->mouse.dy     = 0.0f;
  curr->mouse.scroll = 0.0f;

  while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);
    switch (ev.type) {
    case KeyPress: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      handle_key(curr, sym, 1);
      if (sym == XK_Escape || sym == XK_q || sym == XK_Q)
        *is_running = 0;
      break;
    }
    case KeyRelease: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      handle_key(curr, sym, 0);
      break;
    }
    /* ── Mouse buttons ────────────────────────────────────────────────── */
    case ButtonPress:
      if (ev.xbutton.button == 1) curr->mouse.left_down   = 1;
      if (ev.xbutton.button == 2) curr->mouse.middle_down = 1;
      if (ev.xbutton.button == 3) curr->mouse.right_down  = 1;
      if (ev.xbutton.button == 4) curr->mouse.scroll += 1.0f; /* wheel up */
      if (ev.xbutton.button == 5) curr->mouse.scroll -= 1.0f; /* wheel down */
      break;
    case ButtonRelease:
      if (ev.xbutton.button == 1) curr->mouse.left_down   = 0;
      if (ev.xbutton.button == 2) curr->mouse.middle_down = 0;
      if (ev.xbutton.button == 3) curr->mouse.right_down  = 0;
      break;
    /* ── Mouse motion ─────────────────────────────────────────────────── */
    case MotionNotify:
      curr->mouse.x  = (float)ev.xmotion.x;
      curr->mouse.y  = (float)ev.xmotion.y;
      curr->mouse.dx = curr->mouse.x - g_prev_mouse_x;
      curr->mouse.dy = curr->mouse.y - g_prev_mouse_y;
      g_prev_mouse_x = curr->mouse.x;
      g_prev_mouse_y = curr->mouse.y;
      break;
    case ClientMessage:
      if ((Atom)ev.xclient.data.l[0] == g_x11.wm_delete_window) {
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
      }
      break;
    case ConfigureNotify:
      g_x11.window_w = ev.xconfigure.width;
      g_x11.window_h = ev.xconfigure.height;
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display backbuffer
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void compute_letterbox(int win_w, int win_h, int canvas_w, int canvas_h,
                              float *scale, int *off_x, int *off_y) {
  float sx = (float)win_w / (float)canvas_w;
  float sy = (float)win_h / (float)canvas_h;
  *scale = (sx < sy) ? sx : sy;
  *off_x = (int)((win_w - canvas_w * *scale) * 0.5f);
  *off_y = (int)((win_h - canvas_h * *scale) * 0.5f);
}

static void display_backbuffer(Backbuffer *bb) {
  float scale; int off_x, off_y;
  compute_letterbox(g_x11.window_w, g_x11.window_h,
                    bb->width, bb->height, &scale, &off_x, &off_y);
  int dst_w = (int)(bb->width  * scale);
  int dst_h = (int)(bb->height * scale);

  glViewport(0, 0, g_x11.window_w, g_x11.window_h);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glOrtho(0, g_x11.window_w, g_x11.window_h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bb->width, bb->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

  float x0 = (float)off_x, y0 = (float)off_y;
  float x1 = (float)(off_x + dst_w), y1 = (float)(off_y + dst_h);

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */
int main(void) {
  if (init_window() != 0) return 1;
  setup_vsync();

  /* ── Backbuffer ──────────────────────────────────────────────────────── */
  Backbuffer bb = {
    .width = GAME_W, .height = GAME_H,
    .bytes_per_pixel = 4, .pitch = GAME_W * 4,
  };
  bb.pixels = (uint32_t *)malloc((size_t)(GAME_W * GAME_H) * 4);
  if (!bb.pixels) { fprintf(stderr, "Out of memory\n"); return 1; }
  memset(bb.pixels, 0, (size_t)(GAME_W * GAME_H) * 4);

  /* ── Game state ──────────────────────────────────────────────────────── */
  RaytracerState state;
  game_init(&state);

  /* ── Input double buffer ─────────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  /* ── Main loop ───────────────────────────────────────────────────────── */
  int is_running = 1;
  while (is_running) {
    timing_begin();
    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);
    process_events(curr_input, &is_running);
    if (curr_input->buttons.quit.ended_down) break;

    float dt = g_timing.total_seconds > 0.0f
               ? g_timing.total_seconds
               : (1.0f / TARGET_FPS);

    game_update(&state, curr_input, dt);
    game_render(&state, &bb);
    display_backbuffer(&bb);

    timing_mark_work_done();
    if (!g_x11.vsync_enabled) timing_sleep_until_target();
    timing_end();
  }

  /* ── Cleanup ─────────────────────────────────────────────────────────── */
#ifdef DEBUG
  print_frame_stats();
#endif
  free(bb.pixels);
  shutdown_window();
  return 0;
}
