#include "../_common/backend.h"
#include "../../engine.h"

#include "../../_common/base.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "../../game/game-loader.h"
#include "../../game/inputs.h"
#include "../_common/adaptive-fps.h"
#include "../_common/config.h"
#include "../_common/frame-timing.h"
#include "../_common/inputs-recording.h"
#include "./audio.h"
#include "./hooks/inputs/joystick.h"
#include "./hooks/inputs/keyboard.h"
#include "./inputs/mouse.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <linux/joystick.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if DE100_INTERNAL
#include "../_common/frame-stats.h"
#endif

typedef struct {
  Display *display;
  Window window;
  GLXContext gl_context;
  GLuint texture_id;
  int width;
  int height;
} OpenGLState;

typedef struct {
  Display *display;
  Window window;
  int screen;
  Atom wm_delete_window;
  Colormap colormap;
  XVisualInfo *visual;
  GLXContext gl_context;
  GLuint texture_id;
  void *alsa_handle;
  int joystick_fds[4];
  int joystick_count;
  bool window_is_active;
  int last_width;
  int last_height;
} X11PlatformState;

de100_file_scoped_global_var OpenGLState g_gl = {0};
de100_file_scoped_global_var bool g_window_is_active = true;
de100_file_scoped_global_var int g_last_window_width = 0;
de100_file_scoped_global_var int g_last_window_height = 0;

// ═══════════════════════════════════════════════════════════════════════════
// OpenGL Functions
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void opengl_update_projection(int window_width,
                                                          int window_height) {
  // TODO: Make this call configurable
  glViewport(0, 0, window_width, window_height); // Add this!
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, window_width, window_height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

de100_file_scoped_fn inline bool opengl_init(Display *display, Window window,
                                             int width, int height) {
  int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};

  XVisualInfo *visual =
      glXChooseVisual(display, DefaultScreen(display), visual_attribs);
  if (!visual) {
    fprintf(stderr, "❌ No suitable OpenGL visual found\n");
    return false;
  }

  g_gl.gl_context = glXCreateContext(display, visual, NULL, GL_TRUE);
  if (!g_gl.gl_context) {
    fprintf(stderr, "❌ Failed to create OpenGL context\n");
    XFree(visual);
    return false;
  }

  glXMakeCurrent(display, window, g_gl.gl_context);

  g_gl.display = display;
  g_gl.window = window;
  g_gl.width = width;
  g_gl.height = height;

  glGenTextures(1, &g_gl.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  opengl_update_projection(width, height);

  glEnable(GL_TEXTURE_2D);

  printf("✅ OpenGL initialized (version: %s)\n", glGetString(GL_VERSION));
  XFree(visual);
  return true;
}

de100_file_scoped_fn inline void
opengl_display_buffer(GameBackBuffer *backbuffer, int window_width,
                      int window_height) {
  (void)window_width;
  (void)window_height;
  if (!de100_memory_is_valid(backbuffer->memory))
    return;

  // Center the backbuffer in the window
  int offset_x = (window_width - backbuffer->width) / 2;
  int offset_y = (window_height - backbuffer->height) / 2;

  // Or fixed offset like Casey:
  // int offset_x = 10;
  // int offset_y = 10;

  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, backbuffer->width, backbuffer->height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, backbuffer->memory.base);

  // Draw at offset with BACKBUFFER size, not window size
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(offset_x, offset_y);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(offset_x + backbuffer->width, offset_y);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(offset_x + backbuffer->width, offset_y + backbuffer->height);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(offset_x, offset_y + backbuffer->height);
  glEnd();

  glXSwapBuffers(g_gl.display, g_gl.window);
}

#if DE100_SANITIZE_WAVE_1_MEMORY
de100_file_scoped_fn inline void opengl_cleanup(void) {
  if (g_gl.gl_context) {
    glXMakeCurrent(g_gl.display, None, NULL);
    glXDestroyContext(g_gl.display, g_gl.gl_context);
    g_gl.gl_context = NULL;
  }
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// X11 Functions
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void x11_handle_event(Display *display,
                                                  XEvent *event,
                                                  EnginePlatformState *platform,
                                                  EngineGameState *game) {
  switch (event->type) {
  case ConfigureNotify: {
    int new_width = event->xconfigure.width;
    int new_height = event->xconfigure.height;

    if (new_width != g_last_window_width ||
        new_height != g_last_window_height) {
      printf("Window resized: %dx%d → %dx%d\n", g_last_window_width,
             g_last_window_height, new_width, new_height);
      g_last_window_width = new_width;
      g_last_window_height = new_height;

      // Update OpenGL projection to match new window size
      opengl_update_projection(new_width, new_height);
    }
    break;
  }

  case ClientMessage: {
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if ((Atom)event->xclient.data.l[0] == wmDelete) {
      printf("Window close requested\n");
      is_game_running = false;
    }
    break;
  }

  // In Expose handler:
  case Expose: {
    if (event->xexpose.count != 0)
      break;
    printf("Repainting window\n");
    opengl_display_buffer(&game->backbuffer, g_last_window_width,
                          g_last_window_height);
    XFlush(display);
    break;
  }

  case FocusIn: {
    printf("Window gained focus\n");
    g_window_is_active = true;
    break;
  }

  case FocusOut: {
    printf("Window lost focus\n");
    g_window_is_active = false;
    break;
  }

  case DestroyNotify: {
    printf("Window destroyed\n");
    is_game_running = false;
    break;
  }

  case KeyPress: {
    handleEventKeyPress(event, game, platform);
    break;
  }

  case KeyRelease: {
    handleEventKeyRelease(event, game, platform);
    break;
  }

  case ButtonPress: {
    handle_mouse_button_press(event, game->inputs);
    break;
  }

  case ButtonRelease: {
    handle_mouse_button_release(event, game->inputs);
    break;
  }

  default:
    break;
  }
}

de100_file_scoped_fn inline void
x11_process_pending_events(Display *display, EnginePlatformState *platform,
                           EngineGameState *game) {
  XEvent event;
  while (XPending(display) > 0) {
    XNextEvent(display, &event);
    x11_handle_event(display, &event, platform, game);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Audio Functions
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void
audio_generate_and_send(EnginePlatformState *platform, EngineGameState *game) {
  u32 samples_to_generate =
      linux_get_samples_to_write(&platform->config.audio, &game->audio);

#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_THREE_SECONDS_CHECK) {
    printf("[AUDIO] samples_to_generate=%d, RSI=%ld\n", samples_to_generate,
           (long)platform->config.audio.running_sample_index);
  }
#endif

  if (samples_to_generate > 0) {
    if (samples_to_generate > platform->config.audio.max_samples_per_call) {
      samples_to_generate = platform->config.audio.max_samples_per_call;
    }

    GameAudioOutputBuffer audio_buffer = {
        .samples_per_second = game->audio.samples_per_second,
        .sample_count = samples_to_generate,
        .samples = (i16 *)game->audio.samples};

    platform->code.get_audio_samples(&game->memory, &audio_buffer);
    linux_send_samples_to_alsa(&platform->config.audio, &audio_buffer);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// X11 Platform Initialization
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline int x11_init(EngineState *engine) {
  g_last_window_width = engine->game.config.window_width;
  g_last_window_height = engine->game.config.window_height;

  // TODO: remove the use of `callloc`
  X11PlatformState *x11 = calloc(1, sizeof(X11PlatformState));
  if (!x11) {
    fprintf(stderr, "❌ Failed to allocate X11 state\n");
    return 1;
  }
  engine->platform.backend = x11;

  x11->display = XOpenDisplay(NULL);
  if (!x11->display) {
    fprintf(stderr, "❌ Cannot connect to X server\n");
    return 1;
  }

  x11->screen = DefaultScreen(x11->display);
  Window root = RootWindow(x11->display, x11->screen);

  int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual =
      glXChooseVisual(x11->display, x11->screen, visual_attribs);
  if (!visual) {
    fprintf(stderr, "❌ No OpenGL visual available\n");
    return 1;
  }

  Colormap colormap =
      XCreateColormap(x11->display, root, visual->visual, AllocNone);

  XSetWindowAttributes attrs = {0};
  attrs.colormap = colormap;
  attrs.event_mask =
      ExposureMask | StructureNotifyMask | KeyPressMask |
      KeyReleaseMask |    // Keyboard events
      ButtonPressMask |   // Mouse button press
      ButtonReleaseMask | // Mouse button release
      // NOTE: PointerMotionMask is NOT needed - we poll with XQueryPointer!
      FocusChangeMask; // Window focus

  x11->window = XCreateWindow(
      x11->display, root, 0, 0, engine->game.config.window_width,
      engine->game.config.window_height, 0, visual->depth, InputOutput,
      visual->visual, CWColormap | CWEventMask, &attrs);

  if (!x11->window) {
    fprintf(stderr, "❌ Failed to create X11 window\n");
    return 1;
  }

  printf("✅ Created window\n");

  Atom wmDelete = XInternAtom(x11->display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(x11->display, x11->window, &wmDelete, 1);
  XStoreName(x11->display, x11->window, engine->game.config.window_title);
  XMapWindow(x11->display, x11->window);

  if (!opengl_init(x11->display, x11->window, engine->game.config.window_width,
                   engine->game.config.window_height)) {
    return 1;
  }

  linux_load_alsa();
  linux_init_audio(&engine->platform.config.audio,
                   engine->game.config.initial_audio_sample_rate,
                   engine->game.config.audio_game_update_hz);

  linux_init_joystick(engine->platform.old_inputs->controllers,
                      engine->game.inputs->controllers);

  printf("✅ X11 platform initialized\n");

  adaptive_fps_init();

#if DE100_INTERNAL
  frame_stats_init();

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🎮 ADAPTIVE FRAME RATE CONTROL\n");
  printf("══════���════════════════════════════════════════════════════\n");
  printf("Initial target:  %dHz (%.2fms/frame)\n",
         engine->game.config.max_allowed_refresh_rate_hz,
         1000.0f / (f32)engine->game.config.max_allowed_refresh_rate_hz);
  printf("═══════════════════════════════════════════════════════════\n\n");
#endif
  return 0;
}

#if DE100_SANITIZE_WAVE_1_MEMORY
de100_file_scoped_fn inline void x11_shutdown(EngineState *engine) {
  X11PlatformState *x11 = x11_get_state(engine);
  if (!x11)
    return;

  linux_close_joysticks();
  linux_unload_alsa(&engine->platform.config.audio);

  if (x11->gl_context) {
    glXMakeCurrent(x11->display, None, NULL);
    glXDestroyContext(x11->display, x11->gl_context);
  }
  if (x11->window) {
    XDestroyWindow(x11->display, x11->window);
  }
  if (x11->display) {
    XCloseDisplay(x11->display);
  }

  free(x11);
  engine->platform.backend = NULL;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Main Platform Entry Point
// ═══════════════════════════════════════════════════════════════════════════

int platform_main(void) {
  EngineState engine = {0};
  engine.platform.code = (GameCode){0};

  if (engine_init(&engine)) {
    return 1;
  }

  if (x11_init(&engine) != 0) {
    engine_shutdown(&engine);
    return 1;
  }

  X11PlatformState *x11 = engine_get_backend(&engine, X11PlatformState);

  engine.platform.code.init(&engine.game.thread_context, &engine.game.memory,
                            engine.game.inputs, &engine.game.backbuffer);

  while (is_game_running) {
#if DE100_INTERNAL
    if (FRAME_LOG_EVERY_TEN_SECONDS_CHECK) {
      printf("[HEALTH CHECK] frame=%u, RSI=%lld, marker_idx=%d\n",
             g_frame_counter,
             (long long)engine.platform.config.audio.running_sample_index,
             g_debug_marker_index);
    }
#endif

    frame_timing_begin();

    handle_game_reload_check(&engine.platform.code, &engine.platform.paths);

    prepare_input_frame(engine.platform.old_inputs, engine.game.inputs);
    x11_poll_mouse(x11->display, x11->window, engine.game.inputs);
    ;
    linux_poll_joystick(engine.game.inputs);

    // Input recording/playback: record after getting real inputs, playback
    // overwrites it
    if (input_recording_is_recording(&engine.platform.memory_state)) {
      input_recording_record_frame(&engine.platform.memory_state,
                                   engine.game.inputs);
    }

    if (input_recording_is_playing(&engine.platform.memory_state)) {
      input_recording_playback_frame(&engine.platform.memory_state,
                                     engine.game.inputs);
    }

    engine.platform.code.update_and_render(
        &engine.game.thread_context, &engine.game.memory, engine.game.inputs,
        &engine.game.backbuffer);

    audio_generate_and_send(&engine.platform, &engine.game);

    x11_process_pending_events(x11->display, &engine.platform, &engine.game);

#if DE100_INTERNAL
    int display_marker_index =
        (g_debug_marker_index - 1 + MAX_DEBUG_AUDIO_MARKERS) %
        MAX_DEBUG_AUDIO_MARKERS;
    linux_debug_sync_display(&engine.game.backbuffer, &engine.game.audio,
                             &engine.platform.config.audio,
                             g_debug_audio_markers, MAX_DEBUG_AUDIO_MARKERS,
                             display_marker_index);
#endif

    opengl_display_buffer(&engine.game.backbuffer, g_last_window_width,
                          g_last_window_height);
    XSync(x11->display, False);

#if DE100_INTERNAL
    linux_debug_capture_flip_state(&engine.platform.config.audio);
#endif

    frame_timing_mark_work_done();
    frame_timing_sleep_until_target(
        engine.game.config.target_seconds_per_frame);
    frame_timing_end();

    f32 frame_time_ms = frame_timing_get_ms();
    f32 target_frame_time_ms =
        engine.game.config.target_seconds_per_frame * 1000.0f;

    if (frame_time_ms > (target_frame_time_ms + 5.0f)) {
      printf("⚠️  MISSED FRAME! %.2fms (target: %.2fms, over by: %.2fms)\n",
             frame_time_ms, target_frame_time_ms,
             frame_time_ms - target_frame_time_ms);
    }

#if DE100_INTERNAL
    frame_stats_record(frame_time_ms,
                       engine.game.config.target_seconds_per_frame);
#endif

    g_frame_counter++;

#if DE100_INTERNAL
    if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
      printf(
          "[X11] %.2fms/f, %.2ff/s, %.2fmc/f (work: %.2fms, sleep: %.2fms)\n",
          frame_time_ms, frame_timing_get_fps(), frame_timing_get_mcpf(),
          g_frame_timing.work_seconds * 1000.0f,
          g_frame_timing.sleep_seconds * 1000.0f);
    }
#endif

    if (engine.game.config.prefer_adaptive_fps) {
      adaptive_fps_update(&engine.game.config, frame_time_ms);
    }

    engine_swap_inputs(&engine);
  }

  printf("[%.3fs] Exiting, freeing memory...\n",
         de100_get_wall_clock() - g_initial_game_time_ms);
#if DE100_SANITIZE_WAVE_1_MEMORY
  x11_shutdown(&engine);
#endif
  engine_shutdown(&engine);

  printf("✅ Cleanup complete\n");
  printf("[%.3fs] Memory freed\n",
         de100_get_wall_clock() - g_initial_game_time_ms);

#if DE100_INTERNAL
  frame_stats_print();
#endif

  printf("Goodbye!\n");
  return 0;
}
