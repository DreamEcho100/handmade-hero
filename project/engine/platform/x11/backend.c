#include "backend.h"
#include "../../engine.h"

#include "../../_common/base.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "../../game/game-loader.h"
#include "../../game/input.h"
#include "../../game/memory.h"
#include "../_common/adaptive-fps.h"
#include "../_common/config.h"
#include "../_common/frame-timing.h"
#include "../_common/input-recording.h"
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

de100_file_scoped_global_var OpenGLState g_gl = {0};
de100_file_scoped_global_var bool g_window_is_active =
    true; // Track focus state
de100_file_scoped_global_var int g_last_window_width = 0;
de100_file_scoped_global_var int g_last_window_height = 0;

#if DE100_INTERNAL
de100_file_scoped_global_var FrameStats g_frame_stats = {0};
#endif

// ═══════════════════════════════════════════════════════════════════════════
// OPENGL FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

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

  glEnable(GL_TEXTURE_2D);

  printf("✅ OpenGL initialized (version: %s)\n", glGetString(GL_VERSION));
  XFree(visual);
  return true;
}

de100_file_scoped_fn inline void
opengl_display_buffer(GameBackBuffer *backbuffer) {
  if (!backbuffer->memory)
    return;

  glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, backbuffer->width, backbuffer->height,
               0, GL_RGBA, GL_UNSIGNED_BYTE, backbuffer->memory);

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
de100_file_scoped_fn inline void opengl_cleanup(void) {
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

de100_file_scoped_fn inline uint32
x11_get_monitor_refresh_rate(Display *display) {
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

de100_file_scoped_fn inline void
x11_handle_event(GameBackBuffer *backbuffer, Display *display, XEvent *event,
                 GameInput *new_game_input,
                 PlatformAudioConfig *platform_audio_config,
                 GameMemoryState *game_memory_state) {
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
    handleEventKeyPress(event, new_game_input, platform_audio_config,
                        game_memory_state);
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

de100_file_scoped_fn inline void x11_process_pending_events(
    Display *display, GameBackBuffer *backbuffer, GameInput *new_game_input,
    PlatformAudioConfig *audio_config, GameMemoryState *game_memory_state) {
  XEvent event;
  while (XPending(display) > 0) {
    XNextEvent(display, &event);
    x11_handle_event(backbuffer, display, &event, new_game_input, audio_config,
                     game_memory_state);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO HELPERS
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline void
audio_generate_and_send(GameCode *game, GameMemory *game_memory,
                        GameAudioOutputBuffer *game_audio_output,
                        PlatformAudioConfig *platform_audio_config,
                        void *audio_samples, int32 max_samples) {
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
        .samples = (int16 *)audio_samples};

    // #if DE100_HOT_RELOAD
    game->get_audio_samples(game_memory, &audio_buffer);
    // #else
    //     game_get_audio_samples(game_memory, &audio_buffer);
    // #endif

    linux_send_samples_to_alsa(platform_audio_config, &audio_buffer);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// X11 PLATFORM-SPECIFIC STATE
// ═══════════════════════════════════════════════════════════════════════════
//
// Cast from engine->platform.backend
// Only X11 backend includes this header.
//
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
  // X11
  Display *display;
  Window window;
  int screen;
  Atom wm_delete_window;
  Colormap colormap;
  XVisualInfo *visual;

  // OpenGL
  GLXContext gl_context;
  GLuint texture_id;

  // ALSA (or could be separate)
  void *alsa_handle; // snd_pcm_t* (avoid including alsa headers here)

  // Joysticks
  int joystick_fds[4];
  int joystick_count;

  // Window state
  bool window_is_active;
  int last_width;
  int last_height;

} X11PlatformState;

// ═══════════════════════════════════════════════════════════════════════════
// X11 PLATFORM INIT
// ═══════════════════════════════════════════════════════════════════════════

de100_file_scoped_fn inline int x11_init(EngineState *engine) {
  // Allocate platform-specific state
  X11PlatformState *x11 = calloc(1, sizeof(X11PlatformState));
  if (!x11) {
    fprintf(stderr, "❌ Failed to allocate X11 state\n");
    return 1;
  }
  engine->platform.backend = x11;

  // Connect to X server
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
      ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask;

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

  // Init OpenGL
  if (!opengl_init(x11->display, x11->window, engine->game.config.window_width,
                   engine->game.config.window_height)) {
    return 1;
  }

  linux_load_alsa();
  linux_init_audio(&engine->game.audio, &engine->platform.config.audio,
                   engine->game.config.initial_audio_sample_rate,
                   engine->platform.config.audio.buffer_size_bytes,
                   engine->game.config.audio_game_update_hz);

  linux_init_joystick(engine->platform.old_input->controllers,
                      engine->game.input->controllers);

  printf("✅ X11 platform initialized\n");

  // 🎯 SETUP ADAPTIVE FPS
  engine->platform.config.monitor_refresh_hz =
      x11_get_monitor_refresh_rate(x11->display);

  adaptive_fps_init(&engine->platform.adaptive_fps);

#if DE100_INTERNAL
  frame_stats_init(&engine->platform.frame_stats);

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🎮 ADAPTIVE FRAME RATE CONTROL\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Monitor refresh: %dHz\n", engine->platform.config.monitor_refresh_hz);
  printf("Initial target:  %dHz (%.2fms/frame)\n",
         engine->game.config.refresh_rate_hz,
         engine->game.config.target_seconds_per_frame * 1000.0f);
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

// ═══════════════════════════════════════════════════════════════
// 🚀 MAIN PLATFORM ENTRY POINT
// ═══════════════════════════════════════════════════════════════
int platform_main() {
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

  engine.platform.code.init(&engine.game.memory, engine.game.input,
                            &engine.game.backbuffer);

  while (is_game_running) {
#if DE100_INTERNAL
    if (FRAME_LOG_EVERY_TEN_SECONDS_CHECK) {
      printf("[HEALTH CHECK] frame=%u, RSI=%lld, marker_idx=%d\n",
             g_frame_counter,
             (long long)engine.platform.config.audio.running_sample_index,
             g_debug_marker_index);
    }
#endif

    // ───────────────────────────────────────────────────────────────────────
    // FRAME START
    // ───────────────────────────────────────────────────────────────────────

    frame_timing_begin(&engine.platform.frame_timing);

    // ───────────────────────────────────────────────────────────────────────
    // HOT RELOAD CHECK
    // ───────────────────────────────────────────────────────────────────────
    // #if DE100_HOT_RELOAD
    handle_game_reload_check(&engine.platform.code,
                             &engine.platform.code_paths);
    // #endif

    // ───────────────────────────────────────────────────────────────────────
    // FRAME START
    // ───────────────────────────────────────────────────────────────────────

    frame_timing_begin(&engine.platform.frame_timing);

    // ───────────────────────────────────────────────────────────────────────
    // INPUT
    // ───────────────────────────────────────────────────────────────────────

    prepare_input_frame(engine.platform.old_input, engine.game.input);
    linux_poll_joystick(engine.game.input);

    // ═══════════════════════════════════════════════════════════════════
    // INPUT RECORDING/PLAYBACK (NEW! - Casey's Day 23)
    // ═══════════════════════════════════════════════════════════════════
    // This is where the magic happens:
    //
    // 1. If RECORDING: Save this frame's input to the file
    //    - The game uses the REAL input from keyboard/joystick
    //    - We just log it for later playback
    //
    // 2. If PLAYING BACK: REPLACE input with recorded input
    //    - The game thinks it's getting real input
    //    - But we're feeding it the recorded input instead
    //    - This is why playback_frame takes a pointer - it OVERWRITES
    //
    // ORDER MATTERS:
    //   - Record AFTER getting real input (so we record what we got)
    //   - Playback AFTER getting real input (so we overwrite it)
    // ═══════════════════════════════════════════════════════════════════

    if (input_recording_is_recording(&engine.platform.memory_state)) {
      // We're recording: save this frame's input
      input_recording_record_frame(&engine.platform.memory_state,
                                   engine.game.input);
    }

    if (input_recording_is_playing(&engine.platform.memory_state)) {
      // We're playing back: REPLACE input with recorded input
      // This overwrites whatever we got from keyboard/joystick
      input_recording_playback_frame(&engine.platform.memory_state,
                                     engine.game.input);
    }

    //
    // ───────────────────────────────────────────────────────────────────────
    // GAME UPDATE
    //
    // ───────────────────────────────────────────────────────────────────────

    // Update engine.platform.code logic and render graphics
    // #if DE100_HOT_RELOAD
    engine.platform.code.update_and_render(
        &engine.game.memory, engine.game.input, &engine.game.backbuffer);
    // #else
    //     game_update_and_render(&engine.game.memory, engine.game.input,
    //     &engine.game.backbuffer);
    // #endif

    // ───────────────────────────────────────────────────────────────────────
    // AUDIO
    // ───────────────────────────────────────────────────────────────────────

    audio_generate_and_send(&engine.platform.code, &engine.game.memory,
                            &engine.game.audio, &engine.platform.config.audio,
                            engine.game.audio.samples,
                            engine.platform.config.audio.max_samples_per_call);

    // ───────────────────────────────────────────────────────────────────────
    // X11 EVENTS
    // ───────────────────────────────────────────────────────────────────────

    x11_process_pending_events(x11->display, &engine.game.backbuffer,
                               engine.game.input, &engine.platform.config.audio,
                               &engine.platform.memory_state);

    // ───────────────────────────────────────────────────────────────────────
    // DEBUG VISUALIZATION
    // ───────────────────────────────────────────────────────────────────────

#if DE100_INTERNAL
    int display_marker_index =
        (g_debug_marker_index - 1 + MAX_DEBUG_AUDIO_MARKERS) %
        MAX_DEBUG_AUDIO_MARKERS;
    linux_debug_sync_display(&engine.game.backbuffer, &engine.game.audio,
                             &engine.platform.config.audio,
                             g_debug_audio_markers, MAX_DEBUG_AUDIO_MARKERS,
                             display_marker_index);
#endif

    // ───────────────────────────────────────────────────────────────────────
    // DISPLAY
    // ───────────────────────────────────────────────────────────────────────

    opengl_display_buffer(&engine.game.backbuffer);
    XSync(x11->display, False);

#if DE100_INTERNAL
    linux_debug_capture_flip_state(&engine.platform.config.audio);
#endif

    // ───────────────────────────────────────────────────────────────────────
    // FRAME TIMING
    // ───────────────────────────────────────────────────────────────────────

    frame_timing_mark_work_done(&engine.platform.frame_timing);
    frame_timing_sleep_until_target(
        &engine.platform.frame_timing,
        engine.game.config.target_seconds_per_frame);
    frame_timing_end(&engine.platform.frame_timing);

    real32 frame_time_ms = frame_timing_get_ms(&engine.platform.frame_timing);
    real32 target_frame_time_ms =
        engine.game.config.target_seconds_per_frame * 1000.0f;

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
                       engine.game.config.target_seconds_per_frame);
#endif

    g_frame_counter++;

#if DE100_INTERNAL
    if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
      printf(
          "[X11] %.2fms/f, %.2ff/s, %.2fmc/f (work: %.2fms, sleep: %.2fms)\n",
          frame_time_ms, frame_timing_get_fps(&engine.platform.frame_timing),
          frame_timing_get_mcpf(&engine.platform.frame_timing),
          engine.platform.frame_timing.work_seconds * 1000.0f,
          engine.platform.frame_timing.sleep_seconds * 1000.0f);
    }
#endif

    // ───────────────────────────────────────────────────────────────────────
    // ADAPTIVE FPS
    // ───────────────────────────────────────────────────────────────────────

    adaptive_fps_update(&engine.platform.adaptive_fps, &engine.game.config,
                        &engine.platform.config, frame_time_ms);

    // ─────────────────────────────────────────────────────────────
    // SWAP INPUT BUFFERS
    // ─────────────────────────────────────────────────────────────
    // Swap pointers (old becomes new, new becomes old)
    // This preserves button press/release state across frames
    // ─────────────────────────────────────────────────────────────
    GameInput *temp = engine.game.input;
    engine.game.input = engine.platform.old_input;
    engine.platform.old_input = temp;
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

  // Clean up recording/playback if still active
  input_recording_end(&engine.platform.memory_state);
  input_recording_playback_end(&engine.platform.memory_state);

  engine_shutdown(&engine);
  x11_shutdown(&engine);

  printf("[%.3fs] Memory freed\n", get_wall_clock() - g_initial_game_time);
#endif

#if DE100_INTERNAL
  frame_stats_print(&g_frame_stats);
#endif

  printf("Goodbye!\n");
  return 0;
}