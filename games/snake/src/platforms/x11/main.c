#define _POSIX_C_SOURCE 200809L

#include "audio.h"
#include "base.h"

#include "../../game/main.h"
#include "../../platform.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);

static double g_last_frame_time = 0.0;
static const double g_target_frame_time = 1.0 / 60.0;

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing
 * ═══════════════════════════════════════════════════════════════════════════
 */

static inline double platform_get_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void sleep_ms(int ms) {
  struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
  nanosleep(&ts, NULL);
}

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
      printf("✓ VSync enabled\n");
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
  printf("⚠ VSync not available\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Init
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *props) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "❌ Failed to open display\n");
    return 1;
  }

  Bool supported;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);

  g_x11.screen = DefaultScreen(g_x11.display);
  g_x11.width = props->width;
  g_x11.height = props->height;

  int attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(g_x11.display, g_x11.screen, attribs);
  if (!visual) {
    fprintf(stderr, "❌ No GLX visual\n");
    return 1;
  }

  Colormap cmap =
      XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                      visual->visual, AllocNone);

  XSetWindowAttributes wa = {.colormap = cmap,
                             .event_mask = ExposureMask | KeyPressMask |
                                           KeyReleaseMask |
                                           StructureNotifyMask};

  g_x11.window =
      XCreateWindow(g_x11.display, RootWindow(g_x11.display, g_x11.screen), 100,
                    100, props->width, props->height, 0, visual->depth,
                    InputOutput, visual->visual, CWColormap | CWEventMask, &wa);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  XFree(visual);

  if (!g_x11.gl_context) {
    fprintf(stderr, "❌ Failed to create GL context\n");
    return 1;
  }

  XStoreName(g_x11.display, g_x11.window, props->title);
  XMapWindow(g_x11.display, g_x11.window);

  g_x11.wm_delete_window =
      XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(g_x11.display, g_x11.window, &g_x11.wm_delete_window, 1);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  glViewport(0, 0, g_x11.width, g_x11.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, props->width, props->height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  setup_vsync();

  /* X11AudioConfig lives in g_x11.audio.config — set defaults before init */
  g_x11.audio.config.hz = 60;
  g_x11.audio.config.frames_of_latency = 2;
  if (platform_audio_init(&g_x11.audio.config, &props->game.audio) != 0) {
    fprintf(stderr, "⚠ Audio init failed, continuing without audio\n");
  }

  printf("✓ Platform initialized\n");
  return 0;
}

void platform_shutdown(void) {
  platform_audio_shutdown();
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
 * Input
 * ═══════════════════════════════════════════════════════════════════════════
 */

void platform_get_input(GameInput *input, PlatformGameProps *props) {
  XEvent event;

  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &event);

    switch (event.type) {
    case KeyPress: {
      KeySym key = XkbKeycodeToKeysym(g_x11.display, event.xkey.keycode, 0, 0);
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape:
        props->is_running = false;
        input->quit = 1;
        break;
      case XK_r:
      case XK_R:
      case XK_space:
        input->restart = 1;
        break;
      case XK_Left:
      case XK_a:
      case XK_A:
        UPDATE_BUTTON(input->turn_left, 1);
        break;
      case XK_Right:
      case XK_d:
      case XK_D:
        UPDATE_BUTTON(input->turn_right, 1);
        break;
      }
      break;
    }
    case KeyRelease: {
      KeySym key = XkbKeycodeToKeysym(g_x11.display, event.xkey.keycode, 0, 0);
      switch (key) {
      case XK_Left:
      case XK_a:
      case XK_A:
        UPDATE_BUTTON(input->turn_left, 0);
        break;
      case XK_Right:
      case XK_d:
      case XK_D:
        UPDATE_BUTTON(input->turn_right, 0);
        break;
      }
      break;
    }
    case ClientMessage:
      if ((Atom)event.xclient.data.l[0] == g_x11.wm_delete_window) {
        props->is_running = false;
        input->quit = 1;
      }
      break;
    case ConfigureNotify:
      if (event.xconfigure.width != g_x11.width ||
          event.xconfigure.height != g_x11.height) {
        g_x11.width = event.xconfigure.width;
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Display
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void display_backbuffer(Backbuffer *bb) {
  glClear(GL_COLOR_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bb->width, bb->height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, bb->pixels);

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(0, 0);
  glTexCoord2f(1, 0);
  glVertex2f(bb->width, 0);
  glTexCoord2f(1, 1);
  glVertex2f(bb->width, bb->height);
  glTexCoord2f(0, 1);
  glVertex2f(0, bb->height);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio Processing (abstracted)
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void process_audio(PlatformGameProps *props,
                          GameAudioState *game_audio) {
  if (!props->game.audio.is_initialized)
    return;

  int samples_to_write = platform_audio_get_samples_to_write(
      &g_x11.audio.config, &props->game.audio);

  if (samples_to_write > 0) {
    /* Clamp to allocated buffer capacity — platform_audio_get_samples_to_write
     * already does this, but guard here too as a defence-in-depth measure. */
    if (props->game.audio.max_sample_count > 0 &&
        samples_to_write > props->game.audio.max_sample_count) {
      samples_to_write = props->game.audio.max_sample_count;
    }
    props->game.audio.sample_count = samples_to_write;
    game_get_audio_samples(game_audio, &props->game.audio);
    platform_audio_write(&g_x11.audio.config, &props->game.audio,
                         samples_to_write);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Loop
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  PlatformGameProps props = {0};
  if (platform_game_props_init(&props) != 0)
    return 1;
  if (platform_init(&props) != 0)
    return 1;

  GameInput inputs[2];
  memset(inputs, 0, sizeof(inputs));
  GameState state = {0};
  state.audio.samples_per_second = props.game.audio.samples_per_second;
  game_init(&state, &props.game.audio);

  g_last_frame_time = platform_get_time();

  while (props.is_running) {
    double now = platform_get_time();
    float dt = (float)(now - g_last_frame_time);
    if (dt > 0.05f)
      dt = 0.05f;
    g_last_frame_time = now;

    /* Double-buffered input: swap, prepare, then fill */
    platform_swap_input_buffers(&inputs[0], &inputs[1]);
    prepare_input_frame(&inputs[0], &inputs[1]);
    platform_get_input(&inputs[1], &props);
    if (inputs[1].quit)
      break;

    game_update(&state, &inputs[1], &props.game.audio, dt);
    game_audio_update(&state.audio, dt);
    process_audio(&props, &state.audio);

    game_render(&state, &props.game.backbuffer);
    display_backbuffer(&props.game.backbuffer);

    if (!g_x11.vsync_enabled) {
      double elapsed = platform_get_time() - now;
      if (elapsed < g_target_frame_time) {
        sleep_ms((int)((g_target_frame_time - elapsed) * 1000.0));
      }
    }
  }

  platform_game_props_free(&props);
  platform_shutdown();
  return 0;
}
