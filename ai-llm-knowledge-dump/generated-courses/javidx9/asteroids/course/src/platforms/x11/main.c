#define _POSIX_C_SOURCE 200809L

/* ═══════════════════════════════════════════════════════════════════════════
 * platforms/x11/main.c — X11/GLX/ALSA Backend for Asteroids
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * This backend uses Xlib for windowing, GLX for OpenGL, and ALSA for audio.
 * It implements the platform/game contract from platform.h.
 *
 * KEY DIFFERENCES FROM RAYLIB BACKEND
 * ─────────────────────────────────────
 * Raylib:  ~120 lines.  Raylib handles all the heavy lifting.
 * X11:     ~350 lines.  Manual Xlib + GLX + ALSA + frame timing.
 * Same game logic in both; radically different implementation complexity.
 *
 * AUDIO MODEL
 * ───────────
 * ALSA uses SND_PCM_FORMAT_FLOAT_LE (float32).  Each game frame:
 *   1. game_audio_update()              — expire finished voices
 *   2. platform_audio_get_samples_to_write() — how much ALSA can accept
 *   3. game_get_audio_samples()         — synthesize PCM
 *   4. platform_audio_write()           — push to ALSA ring buffer
 *
 * FRAME TIMING
 * ────────────
 * Two-phase sleep:
 *   Phase 1: coarse nanosleep(1ms) loop until 3ms before deadline.
 *   Phase 2: spin-wait to exact deadline.
 * When VSync is available (GLX_EXT or GLX_MESA) the software limiter
 * is skipped; glXSwapBuffers blocks at the display refresh rate.
 *
 * SCALE MODE
 * ──────────
 * TAB cycles: FIXED (letterbox) → DYNAMIC_MATCH → DYNAMIC_ASPECT.
 * FIXED:          backbuffer stays 800×600; GPU letterbox scales it.
 * DYNAMIC modes:  backbuffer resizes to window; GPU texture reallocated.
 *
 * DUAL-KEY BUTTONS
 * ────────────────
 * Asteroids maps two physical keys to one logical button:
 *   thrust:       UP  or W
 *   rotate_left:  LEFT or A
 *   rotate_right: RIGHT or D
 *   restart:      ENTER or R
 * A per-button down-count ensures that releasing one key while the other
 * is held does NOT clear the button state.
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
#include "../../game/game.h"
#include "../../platform.h"
#include "../../utils/perf.h"
#include "./audio.h"
#include "./base.h"

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

/* ── Global X11 state ────────────────────────────────────────────────────── */
X11State g_x11 = {0};

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame timing
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Two-phase sleep: coarse nanosleep + spin-wait eliminates scheduler jitter
 * in the critical 3ms window before the frame deadline.
 */

typedef struct {
  struct timespec frame_start;
  struct timespec work_end;
  struct timespec frame_end;
  float work_seconds;
  float total_seconds;
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
  struct timespec frame_end;
  get_timespec(&frame_end);
  g_timing.total_seconds =
      timespec_diff_seconds(&g_timing.frame_start, &frame_end);

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
  printf("\n═══════════════════════════════════════════════════════════\n");
  printf("FRAME TIME STATISTICS\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Total frames:   %u\n",  g_stats.frame_count);
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
 * VSync detection
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
 * Scale mode
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void apply_scale_mode(PlatformGameProps *props, int win_w, int win_h) {
  int new_w, new_h;
  switch (props->scale_mode) {
  case WINDOW_SCALE_MODE_FIXED:
    new_w = GAME_W; new_h = GAME_H; break;
  case WINDOW_SCALE_MODE_DYNAMIC_MATCH:
    new_w = win_w;  new_h = win_h;  break;
  case WINDOW_SCALE_MODE_DYNAMIC_ASPECT:
    platform_compute_aspect_size(win_w, win_h, 4.0f, 3.0f, &new_w, &new_h);
    break;
  default:
    new_w = GAME_W; new_h = GAME_H; break;
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
    fprintf(stderr, "❌ XOpenDisplay failed\n");
    return -1;
  }

  /* Suppress auto-repeat (gives clean press/release pairs). */
  Bool ok;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &ok);

  g_x11.screen   = DefaultScreen(g_x11.display);
  g_x11.window_w = GAME_W;
  g_x11.window_h = GAME_H;

  int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "❌ glXChooseVisual: no matching visual\n");
    return -1;
  }

  Colormap cmap = XCreateColormap(
      g_x11.display, RootWindow(g_x11.display, g_x11.screen),
      visual->visual, AllocNone);

  XSetWindowAttributes wa = {
      .colormap   = cmap,
      .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask
                    | StructureNotifyMask,
  };

  g_x11.window = XCreateWindow(
      g_x11.display, RootWindow(g_x11.display, g_x11.screen),
      100, 100, GAME_W, GAME_H, 0, visual->depth, InputOutput,
      visual->visual, CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  if (!g_x11.gl_context) {
    fprintf(stderr, "❌ glXCreateContext failed\n");
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
  if (g_x11.texture_id)  glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)  XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display) XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input processing
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Asteroids maps two keys to one button (UP/W = thrust, LEFT/A = rotate_left,
 * etc.).  g_*_down counters prevent releasing one key from clearing a button
 * that the other key is still holding.
 */

/* Per-button key-down counts for dual-key bindings. */
static int g_thrust_down       = 0;
static int g_rotate_left_down  = 0;
static int g_rotate_right_down = 0;
static int g_restart_down      = 0;

static void process_events(GameInput *curr, int *is_running,
                           PlatformGameProps *props) {
  while (XPending(g_x11.display) > 0) {
    XEvent ev;
    XNextEvent(g_x11.display, &ev);

    switch (ev.type) {

    /* ── KeyPress ──────────────────────────────────────────────────────── */
    case KeyPress: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {

      case XK_Escape:
        UPDATE_BUTTON(&curr->buttons.quit, 1);
        *is_running = 0;
        break;

      case XK_Tab:
        UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 1);
        break;

      case XK_Up:
      case XK_w: case XK_W:
        if (!g_thrust_down) UPDATE_BUTTON(&curr->buttons.thrust, 1);
        g_thrust_down++;
        break;

      case XK_Left:
      case XK_a: case XK_A:
        if (!g_rotate_left_down) UPDATE_BUTTON(&curr->buttons.rotate_left, 1);
        g_rotate_left_down++;
        break;

      case XK_Right:
      case XK_d: case XK_D:
        if (!g_rotate_right_down) UPDATE_BUTTON(&curr->buttons.rotate_right, 1);
        g_rotate_right_down++;
        break;

      case XK_space:
        UPDATE_BUTTON(&curr->buttons.fire, 1);
        break;

      case XK_Return:
      case XK_r: case XK_R:
        if (!g_restart_down) UPDATE_BUTTON(&curr->buttons.restart, 1);
        g_restart_down++;
        break;
      }
      break;
    }

    /* ── KeyRelease ────────────────────────────────────────────────────── */
    case KeyRelease: {
      KeySym sym = XkbKeycodeToKeysym(g_x11.display, ev.xkey.keycode, 0, 0);
      switch (sym) {

      case XK_Escape:
        UPDATE_BUTTON(&curr->buttons.quit, 0);
        break;

      case XK_Tab:
        UPDATE_BUTTON(&curr->buttons.cycle_scale_mode, 0);
        break;

      case XK_Up:
      case XK_w: case XK_W:
        g_thrust_down--;
        if (g_thrust_down <= 0) {
          g_thrust_down = 0;
          UPDATE_BUTTON(&curr->buttons.thrust, 0);
        }
        break;

      case XK_Left:
      case XK_a: case XK_A:
        g_rotate_left_down--;
        if (g_rotate_left_down <= 0) {
          g_rotate_left_down = 0;
          UPDATE_BUTTON(&curr->buttons.rotate_left, 0);
        }
        break;

      case XK_Right:
      case XK_d: case XK_D:
        g_rotate_right_down--;
        if (g_rotate_right_down <= 0) {
          g_rotate_right_down = 0;
          UPDATE_BUTTON(&curr->buttons.rotate_right, 0);
        }
        break;

      case XK_space:
        UPDATE_BUTTON(&curr->buttons.fire, 0);
        break;

      case XK_Return:
      case XK_r: case XK_R:
        g_restart_down--;
        if (g_restart_down <= 0) {
          g_restart_down = 0;
          UPDATE_BUTTON(&curr->buttons.restart, 0);
        }
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

    default:
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Backbuffer → screen
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * PIXEL FORMAT — GL_RGBA:
 * Our pixels are stored as [R][G][B][A] bytes in memory.
 * GL_RGBA tells the driver to read bytes as [Red][Green][Blue][Alpha].
 * GL_BGRA would swap R↔B and produce wrong colours.
 */
static void display_backbuffer(Backbuffer *bb, WindowScaleMode scale_mode) {
  int off_x = 0, off_y = 0;
  int dst_w = bb->width;
  int dst_h = bb->height;

  if (scale_mode == WINDOW_SCALE_MODE_FIXED) {
    float scale = 0;
    platform_compute_letterbox(g_x11.window_w, g_x11.window_h,
                               bb->width, bb->height,
                               &scale, &off_x, &off_y);
    dst_w = (int)(bb->width  * scale);
    dst_h = (int)(bb->height * scale);
  } else {
    off_x = (g_x11.window_w - bb->width)  / 2;
    off_y = (g_x11.window_h - bb->height) / 2;
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
                  bb->width, bb->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, bb->pixels);

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
 * Audio drain loop
 * ═══════════════════════════════════════════════════════════════════════════
 */
static void process_audio(GameAudioState *audio, AudioOutputBuffer *audio_buf,
                          PlatformAudioConfig *cfg) {
  if (!cfg->alsa_pcm) return;

  int frames = platform_audio_get_samples_to_write(cfg);
  if (frames <= 0) return;
  if (frames > audio_buf->max_sample_count)
    frames = audio_buf->max_sample_count;

  game_get_audio_samples(audio, audio_buf, frames);
  platform_audio_write(audio_buf, frames, cfg);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */
int main(void) {
  /* ── Window ─────────────────────────────────────────────────────────── */
  if (init_window() != 0)
    return 1;
  setup_vsync();

  /* ── Shared resources ────────────────────────────────────────────────── */
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0) {
    fprintf(stderr, "❌ Out of memory\n");
    shutdown_window();
    return 1;
  }

  if (platform_audio_init(&props.audio_config) != 0)
    fprintf(stderr, "⚠ Audio init failed — continuing without audio\n");

  /* ── Game state ──────────────────────────────────────────────────────── */
  GameState state;
  memset(&state, 0, sizeof(state));
  state.audio.samples_per_second = AUDIO_SAMPLE_RATE;
  game_audio_init(&state.audio);
  asteroids_init(&state);

  /* ── Input double buffer ────────────────────────────────────────────── */
  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameInput *curr_input = &inputs[0];
  GameInput *prev_input = &inputs[1];

  /* ── Main loop ───────────────────────────────────────────────────────── */
  int is_running = 1;

#ifdef DEBUG
  printf("\n[DEBUG build — ASan + UBSan + FrameStats active]\n\n");
#endif

  while (is_running) {
    timing_begin();

    platform_swap_input_buffers(&curr_input, &prev_input);
    prepare_input_frame(curr_input, prev_input);

    process_events(curr_input, &is_running, &props);
    if (curr_input->buttons.quit.ended_down) break;

    /* TAB: cycle scale mode */
    if (button_just_pressed(curr_input->buttons.cycle_scale_mode)) {
      props.scale_mode = (WindowScaleMode)(
          (props.scale_mode + 1) % WINDOW_SCALE_MODE_COUNT);
      apply_scale_mode(&props, g_x11.window_w, g_x11.window_h);
    }

    /* ── dt ─────────────────────────────────────────────────────────────
     * First frame g_timing.total_seconds = 0; use 1/TARGET_FPS fallback.
     * Clamp above to 66ms (avoid spiral-of-death on slow frames).       */
    float dt = g_timing.total_seconds > 0.0f
                   ? g_timing.total_seconds
                   : (1.0f / TARGET_FPS);
    if (dt > 0.066f) dt = 0.066f;

    /* ── Game update ─────────────────────────────────────────────────── */
    PERF_BEGIN_NAMED(x11_update, "x11/asteroids_update");
    asteroids_update(&state, curr_input, dt);
    PERF_END(x11_update);

    /* ── Audio ───────────────────────────────────────────────────────── */
    float dt_ms = dt * 1000.0f;
    PERF_BEGIN_NAMED(x11_audio_update, "x11/game_audio_update");
    game_audio_update(&state.audio, dt_ms);
    PERF_END(x11_audio_update);

    PERF_BEGIN_NAMED(x11_process_audio, "x11/process_audio");
    process_audio(&state.audio, &props.audio_buffer, &props.audio_config);
    PERF_END(x11_process_audio);

    /* ── Render ──────────────────────────────────────────────────────── */
    PERF_BEGIN_NAMED(x11_render, "x11/asteroids_render+display");
    asteroids_render(&state, &props.backbuffer, props.world_config);
    display_backbuffer(&props.backbuffer, props.scale_mode);
    PERF_END(x11_render);

    /* ── Frame timing ────────────────────────────────────────────────── */
    timing_mark_work_done();
    if (!g_x11.vsync_enabled)
      timing_sleep_until_target();
    timing_end();

    /* ── Bench auto-exit ─────────────────────────────────────────────── */
#ifdef BENCH_DURATION_S
    {
      static struct timespec _bench_t0 = {0};
      if (_bench_t0.tv_sec == 0) _bench_t0 = g_timing.frame_start;
      double _bench_elapsed = timespec_to_seconds(&g_timing.frame_start)
                            - timespec_to_seconds(&_bench_t0);
      if (_bench_elapsed >= (double)BENCH_DURATION_S) {
        printf("\n[bench] %.1f seconds elapsed — profiler summary:\n",
               _bench_elapsed);
        perf_print_all();
        is_running = 0;
      }
    }
#endif
  }

  /* ── Cleanup ─────────────────────────────────────────────────────────── */
#ifdef DEBUG
  print_frame_stats();
#endif

  platform_audio_shutdown_cfg(&props.audio_config);
  platform_game_props_free(&props);
  shutdown_window();
  return 0;
}
