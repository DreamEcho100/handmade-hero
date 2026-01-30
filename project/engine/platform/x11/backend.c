#include "backend.h"
#include "../../_common/base.h"
#include "../../_common/time.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "../../game/game-loader.h"
#include "../../game/input.h"
#include "../../game/memory.h"
#include "../_common/adaptive-fps.h"
#include "../_common/config.h"
#include "../_common/frame-timing.h"
#include "../_common/startup.h"
#include "audio.h"
#include "inputs/joystick.h"
#include "inputs/keyboard.h"

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
#include <string.h>
#include <unistd.h>

#if DE100_INTERNAL
#include "../_common/frame-stats.h"
#endif

// ═══════════════════════════════════════════════════════════════
// 🎮 OPENGL STATE
// ═══════════════════════════════════════════════════════════════
// Holds OpenGL context and rendering resources
// This lives for the entire program (process lifetime)
// ═══════════════════════════════════════════════════════════════
typedef struct {
  Display *display;      // X11 connection
  Window window;         // X11 window handle
  GLXContext gl_context; // OpenGL rendering context
  GLuint texture_id;     // GPU texture for our pixel backbuffer
  int width;             // Window width
  int height;            // Window height
} OpenGLState;

file_scoped_global_var OpenGLState g_gl = {0};
file_scoped_global_var bool g_window_is_active = true; // Track focus state
file_scoped_global_var int g_last_window_width = 0;
file_scoped_global_var int g_last_window_height = 0;

#if DE100_INTERNAL
file_scoped_global_var FrameStats g_frame_stats = {0};
#endif

// ═══════════════════════════════════════════════════════════════════════════
// OPENGL FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

file_scoped_fn inline bool opengl_init(Display *display, Window window,
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

  glEnable(GL_TEXTURE_2D);

  printf("✅ OpenGL initialized (version: %s)\n", glGetString(GL_VERSION));
  XFree(visual);
  return true;
}

file_scoped_fn inline void opengl_display_buffer(GameBackBuffer *backbuffer) {
  if (!backbuffer->memory.base)
    return;

  glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, backbuffer->width, backbuffer->height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, backbuffer->memory.base);

  glClear(GL_COLOR_BUFFER_BIT);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(0, 0);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(backbuffer->width, 0);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(backbuffer->width, backbuffer->height);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(0, backbuffer->height);
  glEnd();

  glXSwapBuffers(g_gl.display, g_gl.window);
}

#if DE100_SANITIZE_WAVE_1_MEMORY
file_scoped_fn inline void opengl_cleanup(void) {
  if (g_gl.gl_context) {
    glXMakeCurrent(g_gl.display, None, NULL);
    glXDestroyContext(g_gl.display, g_gl.gl_context);
    g_gl.gl_context = NULL;
  }
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// X11 FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

file_scoped_fn inline uint32 x11_get_monitor_refresh_rate(Display *display) {
  XRRScreenConfiguration *screen_config =
      XRRGetScreenInfo(display, RootWindow(display, DefaultScreen(display)));

  if (!screen_config) {
    printf("⚠️  XRandR not available, defaulting to 60Hz\n");
    return FPS_60;
  }

  short refresh_rate = XRRConfigCurrentRate(screen_config);
  XRRFreeScreenConfigInfo(screen_config);

  printf("🖥️  Monitor refresh rate detected: %dHz\n", refresh_rate);
  return refresh_rate;
}

file_scoped_fn inline void
x11_handle_event(GameBackBuffer *backbuffer, Display *display, XEvent *event,
                 GameInput *new_game_input,
                 PlatformAudioConfig *platform_audio_config) {
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

  case Expose: {
    if (event->xexpose.count != 0)
      break;
    printf("Repainting window\n");
    opengl_display_buffer(backbuffer);
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
    handleEventKeyPress(event, new_game_input, platform_audio_config);
    break;
  }

  case KeyRelease: {
    handleEventKeyRelease(event, new_game_input);
    break;
  }

  default:
    break;
  }
}

file_scoped_fn inline void
x11_process_pending_events(Display *display, GameBackBuffer *backbuffer,
                           GameInput *new_game_input,
                           PlatformAudioConfig *audio_config) {
  XEvent event;
  while (XPending(display) > 0) {
    XNextEvent(display, &event);
    x11_handle_event(backbuffer, display, &event, new_game_input, audio_config);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO HELPERS
// ═══════════════════════════════════════════════════════════════════════════

file_scoped_fn inline void
audio_generate_and_send(GameCode *game, GameMemory *game_memory,
                        GameAudioOutputBuffer *game_audio_output,
                        PlatformAudioConfig *platform_audio_config,
                        MemoryBlock *audio_samples_block, int32 max_samples) {
  int32 samples_to_generate =
      linux_get_samples_to_write(platform_audio_config, game_audio_output);

#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_THREE_SECONDS_CHECK) {
    printf("[AUDIO] samples_to_generate=%d, RSI=%ld\n", samples_to_generate,
           (long)platform_audio_config->running_sample_index);
  }
#endif

  if (samples_to_generate > 0) {
    if (samples_to_generate > max_samples) {
      samples_to_generate = max_samples;
    }

    GameAudioOutputBuffer audio_buffer = {
        .samples_per_second = game_audio_output->samples_per_second,
        .sample_count = samples_to_generate,
        .samples_block = *audio_samples_block};

    // #if DE100_HOT_RELOAD
    game->get_audio_samples(game_memory, &audio_buffer);
    // #else
    //     game_get_audio_samples(game_memory, &audio_buffer);
    // #endif

    linux_send_samples_to_alsa(platform_audio_config, &audio_buffer);
  }
}

// ═══════════════════════════════════════════════════════════════
// 🚀 MAIN PLATFORM ENTRY POINT
// ═══════════════════════════════════════════════════════════════
int platform_main() {
#if DE100_INTERNAL
  real64 absolute_start = get_wall_clock();
  printf("[ABSOLUTE START] Time since epoch: %.3f\n", absolute_start);
#endif

  LoadGameCodeConfig load_game_code_config = {0};
  GameCode game = {0};
  GameConfig game_config = get_default_game_config();
  PlatformConfig platform_config = {0};
  platform_config.audio = (PlatformAudioConfig){0};
  GameMemory game_memory = {0};
  GameInput game_inputs[2] = {0};
  GameBackBuffer game_buffer = {0};
  GameInput *new_game_input = &game_inputs[0];
  GameInput *old_game_input = &game_inputs[1];
  GameAudioOutputBuffer game_audio_output = {0};

  platform_game_startup(&load_game_code_config, &game, &game_config,
                        &game_memory, new_game_input, old_game_input,
                        &game_buffer, &game_audio_output);

  // #if DE100_HOT_RELOAD
  game.init(&game_memory, new_game_input, &game_buffer);
  // #else
  //   game_init(&game_memory, new_game_input, &game_buffer);
  // #endif

  // ─────────────────────────────────────────────────────────────────────────
  // JOYSTICK
  // ─────────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
  printf("[%.3fs] Initializing joystick...\n",
         get_wall_clock() - g_initial_game_time);
#endif
  linux_init_joystick(old_game_input->controllers, new_game_input->controllers);
#if DE100_INTERNAL
  printf("[%.3fs] Joystick initialized\n",
         get_wall_clock() - g_initial_game_time);
#endif

  // ─────────────────────────────────────────────────────────────────────────
  // AUDIO
  // ─────────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
  printf("[%.3fs] Loading ALSA library...\n",
         get_wall_clock() - g_initial_game_time);
#endif
  linux_load_alsa();
#if DE100_INTERNAL
  printf("[%.3fs] ALSA library loaded\n",
         get_wall_clock() - g_initial_game_time);
#endif

  // ─────────────────────────────────────────────────────────────────────────
  // X11 WINDOW
  // ─────────────────────────────────────────────────────────────────────────

  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "❌ Cannot connect to X server\n");
    return 1;
  }

  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);

  int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual = glXChooseVisual(display, screen, visual_attribs);
  if (!visual) {
    fprintf(stderr, "❌ No OpenGL visual available\n");
    return 1;
  }

  Colormap colormap = XCreateColormap(display, root, visual->visual, AllocNone);

  XSetWindowAttributes attrs = {0};
  attrs.colormap = colormap;
  attrs.event_mask =
      ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask;

  Window window =
      XCreateWindow(display, root, 0, 0, game_config.window_width,
                    game_config.window_height, 0, visual->depth, InputOutput,
                    visual->visual, CWColormap | CWEventMask, &attrs);

  if (!window) {
    fprintf(stderr, "❌ Failed to create X11 window\n");
    return 1;
  }

  printf("✅ Created window\n");

  // ═══════════════════════════════════════════════════════════════
  // 🎯 SETUP ADAPTIVE FPS
  // ═══════════════════════════════════════════════════════════════

  platform_config.monitor_refresh_hz = x11_get_monitor_refresh_rate(display);

  AdaptiveFPS adaptive = {0};
  adaptive_fps_init(&adaptive);

#if DE100_INTERNAL
  frame_stats_init(&g_frame_stats);

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🎮 ADAPTIVE FRAME RATE CONTROL\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Monitor refresh: %dHz\n", platform_config.monitor_refresh_hz);
  printf("Initial target:  %dHz (%.2fms/frame)\n", game_config.refresh_rate_hz,
         game_config.target_seconds_per_frame * 1000.0f);
  printf("═══════════════════════════════════════════════════════════\n\n");
#endif

  // ─────────────────────────────────────────────────────────────────────────
  // WINDOW SETUP
  // ─────────────────────────────────────────────────────────────────────────

  Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDelete, 1);
  XStoreName(display, window, game_config.window_title);
  XMapWindow(display, window);

  if (!opengl_init(display, window, game_config.window_width,
                   game_config.window_height)) {
    return 1;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // BACKBUFFER
  // ─────────────────────────────────────────────────────────────────────────

  init_backbuffer(&game_buffer, game_config.window_width,
                  game_config.window_height, 4);

  int buffer_size = game_config.window_width * game_config.window_height * 4;
  game_buffer.memory =
      memory_alloc(NULL, buffer_size,
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(game_buffer.memory)) {
    fprintf(stderr, "❌ Failed to allocate backbuffer memory\n");
    return 1;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // AUDIO SETUP
  // ─────────────────────────────────────────────────────────────────────────
  int bytes_per_sample = sizeof(int16) * 2;
  int secondary_buffer_size =
      game_config.initial_audio_sample_rate * bytes_per_sample;

  game_audio_output.samples_per_second = game_config.initial_audio_sample_rate;
  platform_config.audio.bytes_per_sample = bytes_per_sample;
  platform_config.audio.secondary_buffer_size = secondary_buffer_size;

  linux_init_audio(&game_audio_output, &platform_config.audio,
                   game_config.initial_audio_sample_rate, secondary_buffer_size,
                   game_config.audio_game_update_hz);

  // ─────────────────────────────────────────────────────────────────────────
  // AUDIO SAMPLE BUFFER
  // ─────────────────────────────────────────────────────────────────────────

  int32 samples_per_frame =
      game_audio_output.samples_per_second / game_config.audio_game_update_hz;
  int32 max_samples_per_call = samples_per_frame * 3;
  int sample_buffer_size =
      max_samples_per_call * platform_config.audio.bytes_per_sample;

  MemoryBlock audio_samples_block =
      memory_alloc(NULL, sample_buffer_size,
                   MEMORY_FLAG_READ | MEMORY_FLAG_WRITE | MEMORY_FLAG_ZEROED);

  if (!memory_is_valid(audio_samples_block)) {
    fprintf(stderr, "❌ Failed to allocate sound sample buffer\n");
    return 1;
  }

  printf("✅ Sound sample buffer allocated: %d bytes\n", sample_buffer_size);

  printf("Entering main loop...\n");

  FrameTiming frame_timing = {0};

  printf("Entering main loop...\n");

  // ─────────────────────────────────────────────────────────────────────────
  // MAIN GAME LOOP
  // ─────────────────────────────────────────────────────────────────────────
  if (game_buffer.memory.base) {
    while (is_game_running) {
#if DE100_INTERNAL
      if (FRAME_LOG_EVERY_TEN_SECONDS_CHECK) {
        printf("[HEALTH CHECK] frame=%u, RSI=%lld, marker_idx=%d\n",
               g_frame_counter,
               (long long)platform_config.audio.running_sample_index,
               g_debug_marker_index);
      }
#endif

      // ───────────────────────────────────────────────────────────────────────
      // FRAME START
      // ───────────────────────────────────────────────────────────────────────

      frame_timing_begin(&frame_timing);

      // ───────────────────────────────────────────────────────────────────────
      // HOT RELOAD CHECK
      // ───────────────────────────────────────────────────────────────────────
      // #if DE100_HOT_RELOAD
      handle_game_reload_check(&game, &load_game_code_config);
      // #endif

      // ───────────────────────────────────────────────────────────────────────
      // FRAME START
      // ───────────────────────────────────────────────────────────────────────

      frame_timing_begin(&frame_timing);

      // ───────────────────────────────────────────────────────────────────────
      // INPUT
      // ───────────────────────────────────────────────────────────────────────

      prepare_input_frame(old_game_input, new_game_input);
      linux_poll_joystick(new_game_input);

      //     //
      //     ───────────────────────────────────────────────────────────────────────
      //     // GAME UPDATE
      //     //
      //     ───────────────────────────────────────────────────────────────────────

      // Update game logic and render graphics
      // #if DE100_HOT_RELOAD
      game.update_and_render(&game_memory, new_game_input, &game_buffer);
      // #else
      //     game_update_and_render(&game_memory, new_game_input, &game_buffer);
      // #endif

      // ───────────────────────────────────────────────────────────────────────
      // AUDIO
      // ───────────────────────────────────────────────────────────────────────

      audio_generate_and_send(&game, &game_memory, &game_audio_output,
                              &platform_config.audio, &audio_samples_block,
                              max_samples_per_call);

      // ───────────────────────────────────────────────────────────────────────
      // X11 EVENTS
      // ───────────────────────────────────────────────────────────────────────

      x11_process_pending_events(display, &game_buffer, new_game_input,
                                 &platform_config.audio);

      // ───────────────────────────────────────────────────────────────────────
      // DEBUG VISUALIZATION
      // ───────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
      int display_marker_index =
          (g_debug_marker_index - 1 + MAX_DEBUG_AUDIO_MARKERS) %
          MAX_DEBUG_AUDIO_MARKERS;
      linux_debug_sync_display(&game_buffer, &game_audio_output,
                               &platform_config.audio, g_debug_audio_markers,
                               MAX_DEBUG_AUDIO_MARKERS, display_marker_index);
#endif

      // ───────────────────────────────────────────────────────────────────────
      // DISPLAY
      // ───────────────────────────────────────────────────────────────────────

      opengl_display_buffer(&game_buffer);
      XSync(display, False);

#if DE100_INTERNAL
      linux_debug_capture_flip_state(&platform_config.audio);
#endif

      // ───────────────────────────────────────────────────────────────────────
      // FRAME TIMING
      // ───────────────────────────────────────────────────────────────────────

      frame_timing_mark_work_done(&frame_timing);
      frame_timing_sleep_until_target(&frame_timing,
                                      game_config.target_seconds_per_frame);
      frame_timing_end(&frame_timing);

      real32 frame_time_ms = frame_timing_get_ms(&frame_timing);
      real32 target_frame_time_ms =
          game_config.target_seconds_per_frame * 1000.0f;

      // ───────────────────────────────────────────────────────────────────────
      // MISSED FRAME REPORTING
      // ───────────────────────────────────────────────────────────────────────

      if (frame_time_ms > (target_frame_time_ms + 5.0f)) {
        printf("⚠️  MISSED FRAME! %.2fms (target: %.2fms, over by: %.2fms)\n",
               frame_time_ms, target_frame_time_ms,
               frame_time_ms - target_frame_time_ms);
      }

      // ───────────────────────────────────────────────────────────────────────
      // FRAME STATS
      // ───────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
      frame_stats_record(&g_frame_stats, frame_time_ms,
                         game_config.target_seconds_per_frame);
#endif

      g_frame_counter++;

#if DE100_INTERNAL
      if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
        printf(
            "[X11] %.2fms/f, %.2ff/s, %.2fmc/f (work: %.2fms, sleep: %.2fms)\n",
            frame_time_ms, frame_timing_get_fps(&frame_timing),
            frame_timing_get_mcpf(&frame_timing),
            frame_timing.work_seconds * 1000.0f,
            frame_timing.sleep_seconds * 1000.0f);
      }
#endif

      // ───────────────────────────────────────────────────────────────────────
      // ADAPTIVE FPS
      // ───────────────────────────────────────────────────────────────────────

      adaptive_fps_update(&adaptive, &game_config, &platform_config,
                          frame_time_ms);

      // ─────────────────────────────────────────────────────────────
      // SWAP INPUT BUFFERS
      // ─────────────────────────────────────────────────────────────
      // Swap pointers (old becomes new, new becomes old)
      // This preserves button press/release state across frames
      // ─────────────────────────────────────────────────────────────
      GameInput *temp = new_game_input;
      new_game_input = old_game_input;
      old_game_input = temp;
    }
  }

  // ═══════════════════════════════════════════════════════════════
  // 🧹 CLEANUP (Casey's "Wave Lifetime" pattern)
  // ═══════════════════════════════════════════════════════════════
  // By default, we DON'T manually clean up process-lifetime resources
  // The OS does it faster when the process exits (<1ms vs 17ms)
  //
  // Only clean up if DE100_SANITIZE_WAVE_1_MEMORY is enabled
  // (useful for memory leak detection tools like Valgrind)
  // ═══════════════════════════════════════════════════════════════

#if DE100_SANITIZE_WAVE_1_MEMORY
  printf("[%.3fs] Exiting, freeing memory...\n",
         get_wall_clock() - g_initial_game_time);

  linux_close_joysticks();
  linux_unload_alsa(&game_audio_output, &platform_config.audio);

  free_platform_game_startup(&load_game_code_config, &game, &game_config,
                             &game_memory, new_game_input, old_game_input,
                             &game_buffer, &game_audio_output);

  opengl_cleanup();
  XFreeColormap(display, colormap);
  XFree(visual);
  XDestroyWindow(display, window);
  XCloseDisplay(display);

  printf("[%.3fs] Memory freed\n", get_wall_clock() - g_initial_game_time);
#endif

#if DE100_INTERNAL
  frame_stats_print(&g_frame_stats);
#endif

  printf("Goodbye!\n");
  return 0;
}