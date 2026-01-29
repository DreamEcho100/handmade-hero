#include "backend.h"

#include "../../_common/base.h"
#include "../../_common/file.h"
#include "../../_common/time.h"
#include "../../game/backbuffer.h"
#include "../../game/base.h"
#include "../../game/config.h"
#include "../../game/game-loader.h"
#include "../../game/input.h"
#include "../../game/memory.h"
#include "../_common/audio.h"
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
#include <fcntl.h>
#include <linux/joystick.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <x86intrin.h> // For __rdtsc() CPU cycle counter

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
file_scoped_global_var int last_window_width = 0;
file_scoped_global_var int last_window_height = 0;

// ═══════════════════════════════════════════════════════════════
// 🎮 ADAPTIVE FPS STATE
// ═══════════════════════════════════════════════════════════════
// Tracks frame performance to automatically adjust target FPS
// Casey's pattern: Don't hardcode FPS, adapt to machine capability!
// ═══════════════════════════════════════════════════════════════
typedef struct {
  // int target_fps;           // Current target (starts at monitor refresh
  // rate)
  // int monitor_hz;           // Max we can go (from XRandR)
  uint32 frames_sampled;    // How many frames we've counted so far
  uint32 frames_missed;     // How many were too slow
  uint32 sample_window;     // Evaluate every N frames (e.g., 300 = 5 seconds)
  real32 miss_threshold;    // If >5% frames miss, reduce FPS
  real32 recover_threshold; // If <1% frames miss, try higher FPS

  // ✅ NEW: Cooldown to prevent ping-ponging
  uint32 frames_since_last_change; // Frames since last FPS change
  uint32 cooldown_frames;          // Don't change FPS for N frames
} AdaptiveFPS;

#if DE100_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 📊 FRAME TIME STATISTICS (Debug Build Only)
// ═══════════════════════════════════════════════════════════════
// Tracks performance stats for the entire session
// Only compiled in debug builds (Casey's pattern)
// ═══════════════════════════════════════════════════════════════
typedef struct {
  uint32 frame_count;       // Total frames rendered
  uint32 missed_frames;     // Frames that took >target time
  real32 min_frame_time_ms; // Fastest frame
  real32 max_frame_time_ms; // Slowest frame
  real32 avg_frame_time_ms; // Running sum (divide by frame_count at end)
} FrameStats;

file_scoped_global_var FrameStats g_frame_stats = {0};
#endif

// ═══════════════════════════════════════════════════════════════
// 🔧 INITIALIZE OPENGL CONTEXT
// ═══════════════════════════════════════════════════════════════
// Sets up OpenGL for rendering our pixel backbuffer
//
// WHY OPENGL?
// - Solves RGBA vs BGRA color format mismatch with Raylib
// - GPU accelerated texture upload (faster than XPutImage)
// - Built-in VSync with glXSwapBuffers
// - Same rendering path as Raylib (both use OpenGL internally)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn bool init_opengl(Display *display, Window window, int width,
                                int height) {
  // Ask X11 for an OpenGL-capable visual (pixel format)
  int visual_attribs[] = {
      GLX_RGBA,           // We want RGBA color mode
      GLX_DEPTH_SIZE, 24, // 24-bit depth buffer (unused for 2D, but required)
      GLX_DOUBLEBUFFER,   // Enable double buffering (VSync)
      None                // Terminator
  };

  XVisualInfo *visual =
      glXChooseVisual(display, DefaultScreen(display), visual_attribs);
  if (!visual) {
    fprintf(stderr, "❌ No suitable OpenGL visual found\n");
    return false;
  }

  // Create OpenGL rendering context
  // NULL = no sharing with other contexts
  // GL_TRUE = direct rendering (faster, GPU direct access)
  g_gl.gl_context = glXCreateContext(display, visual, NULL, GL_TRUE);
  if (!g_gl.gl_context) {
    fprintf(stderr, "❌ Failed to create OpenGL context\n");
    XFree(visual);
    return false;
  }

  // Bind context to our window (like "activating" the context)
  glXMakeCurrent(display, window, g_gl.gl_context);

  // Store state for later use
  g_gl.display = display;
  g_gl.window = window;
  g_gl.width = width;
  g_gl.height = height;

  // Create GPU texture to hold our CPU-rendered pixels
  glGenTextures(1, &g_gl.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);

  // GL_NEAREST = no filtering (sharp pixels, important for pixel art!)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Setup 2D orthographic projection (no perspective)
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1); // (0,0) = top-left, Y-down (like canvas)
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Enable texturing (we'll draw our backbuffer as a textured quad)
  glEnable(GL_TEXTURE_2D);

  printf("✅ OpenGL initialized (version: %s)\n", glGetString(GL_VERSION));
  XFree(visual);
  return true;
}

// ═══════════════════════════════════════════════════════════════
// 🖼️ UPDATE WINDOW (OpenGL Renderer)
// ═══════════════════════════════════════════════════════════════
// Replaces XPutImage - uploads pixels to GPU and displays them
//
// PROCESS:
// 1. Upload CPU pixels to GPU texture (glTexImage2D)
// 2. Draw fullscreen quad with that texture
// 3. Swap front/back buffers (this is where VSync happens!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn void update_window_opengl(GameBackBuffer *backbuffer) {
  if (!backbuffer->memory.base)
    return;

  // Upload our CPU-rendered pixels to GPU texture
  glBindTexture(GL_TEXTURE_2D, g_gl.texture_id);
  glTexImage2D(GL_TEXTURE_2D,
               0,       // Mipmap level (0 = base image)
               GL_RGBA, // Internal GPU format
               backbuffer->width, backbuffer->height,
               0,       // Border (must be 0)
               GL_RGBA, // Format of our CPU data (RGBA! Same as Raylib!)
               GL_UNSIGNED_BYTE,       // Data type (8-bit per channel)
               backbuffer->memory.base // Pointer to our pixel data
  );

  // Clear screen to black
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw fullscreen quad with our texture
  // This is like a <canvas> element showing an <img>
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(0, 0); // Top-left
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(backbuffer->width, 0); // Top-right
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(backbuffer->width, backbuffer->height); // Bottom-right
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(0, backbuffer->height); // Bottom-left
  glEnd();

  // Swap buffers (VSync happens here!)
  // Front buffer = what user sees
  // Back buffer = what we just drew
  // This swaps them (and waits for monitor refresh if VSync enabled)
  glXSwapBuffers(g_gl.display, g_gl.window);
}

// ═══════════════════════════════════════════════════════════════
// 🖥️ GET MONITOR REFRESH RATE
// ═══════════════════════════════════════════════════════════════
// Uses XRandR extension to query monitor's native refresh rate
// This becomes our initial FPS target (adaptive FPS may lower it)
// ═══════════════════════════════════════════════════════════════
uint32 get_monitor_refresh_rate(Display *display) {
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

// ═══════════════════════════════════════════════════════════════
// 🎮 HANDLE WINDOW EVENTS
// ═══════════════════════════════════════════════════════════════
// Processes X11 window events (resize, close, keyboard, etc.)
// Like addEventListener() in JavaScript
// ═══════════════════════════════════════════════════════════════
inline file_scoped_fn void
handle_event(GameBackBuffer *backbuffer, Display *display, XEvent *event,
             GameInput *new_game_input,
             PlatformAudioConfig *platform_audio_config) {
  switch (event->type) {

  // Then in the event handler:
  case ConfigureNotify: {
    int new_width = event->xconfigure.width;
    int new_height = event->xconfigure.height;

    // Only log if size actually changed
    if (new_width != last_window_width || new_height != last_window_height) {
      printf("Window resized: %dx%d → %dx%d\n", last_window_width,
             last_window_height, new_width, new_height);
      last_window_width = new_width;
      last_window_height = new_height;
    }
    break;
  }

  case ClientMessage: {
    // Window close button clicked
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if ((Atom)event->xclient.data.l[0] == wmDelete) {
      printf("Window close requested\n");
      is_game_running = false;
    }
    break;
  }

  case Expose: {
    // Window needs repainting (uncovered, moved, etc.)
    if (event->xexpose.count != 0)
      break; // Wait for last expose event
    printf("Repainting window\n");
    update_window_opengl(backbuffer);
    XFlush(display); // Send commands to X server
    break;
  }

  case FocusIn: {
    printf("Window gained focus\n");
    g_window_is_active = true; // Window is active again
    break;
  }

  case FocusOut: { // ADD THIS CASE
    printf("Window lost focus\n");
    g_window_is_active = false; // Window in background
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

// ═══════════════════════════════════════════════════════════════
// 🚀 MAIN PLATFORM ENTRY POINT
// ═══════════════════════════════════════════════════════════════
int platform_main() {
  g_initial_game_time = get_wall_clock();
  printf("[%.3fs] Starting platform_main\n",
         get_wall_clock() - g_initial_game_time);

  // ─────────────────────────────────────────────────────────────────────
  // LOAD GAME CODE
  // ─────────────────────────────────────────────────────────────────────

  LoadGameCodeConfig load_game_code_config = {0};
  load_game_code_config.game_main_lib_path = DE100_GAME_MAIN_SHARED_LIB_PATH;
  load_game_code_config.game_main_lib_tmp_path =
      DE100_GAME_MAIN_TMP_SHARED_LIB_PATH;
  load_game_code_config.game_startup_lib_path =
      DE100_GAME_STARTUP_SHARED_LIB_PATH;
  load_game_code_config.game_startup_lib_tmp_path =
      DE100_GAME_STARTUP_TMP_SHARED_LIB_PATH;
  load_game_code_config.game_init_lib_path = DE100_GAME_INIT_SHARED_LIB_PATH;
  load_game_code_config.game_init_lib_tmp_path =
      DE100_GAME_INIT_TMP_SHARED_LIB_PATH;

  GameCode game = {0};
  load_game_code(&game, &load_game_code_config, GAME_CODE_CATEGORY_ANY);
  // exit(1);
  if (game.is_valid) {
    printf("✅ Game code loaded successfully\n");
    // NOTE: do on a separate thread
    de100_file_delete(
        load_game_code_config.game_main_lib_tmp_path); // Clean up temp file
    de100_file_delete(
        load_game_code_config.game_startup_lib_tmp_path); // Clean up temp file
    de100_file_delete(
        load_game_code_config.game_init_lib_tmp_path); // Clean up temp file
  } else {
    printf("❌ Failed to load game code, using stubs\n");
  }

  GameConfig game_config = get_default_game_config();
  PlatformConfig platform_config = {0};
  GameMemory game_memory = {0};
  GameInput game_inputs[2] = {0};
  GameBackBuffer game_buffer = {0};
  GameInput *new_game_input = &game_inputs[0];
  GameInput *old_game_input = &game_inputs[1];
  GameAudioOutputBuffer game_audio_output = {0};

  game.startup(&game_config);
  platform_game_startup(&game_config, &game_memory, new_game_input,
                        old_game_input, &game_buffer, &game_audio_output);
  game.init(&game_memory, new_game_input, &game_buffer);

  // ═══════════════════════════════════════════════════════════════
  // 🎮 INITIALIZE JOYSTICK (Before main loop!)
  // ═══════════════════════════════════════════════════════════════
  printf("[%.3fs] Initializing joystick...\n",
         get_wall_clock() - g_initial_game_time);
  linux_init_joystick(old_game_input->controllers, new_game_input->controllers);
  printf("[%.3fs] Joystick initialized\n",
         get_wall_clock() - g_initial_game_time);

  // ═══════════════════════════════════════════════════════════════
  // 🔊 INITIALIZE AUDIO (Casey's Day 7 pattern)
  // ═══════════════════════════════════════════════════════════════
  printf("[%.3fs] Loading ALSA library...\n",
         get_wall_clock() - g_initial_game_time);
  linux_load_alsa(); // Dynamically load libasound.so
  printf("[%.3fs] ALSA library loaded\n",
         get_wall_clock() - g_initial_game_time);

  // ═══════════════════════════════════════════════════════════════
  // 🖥️ CREATE X11 WINDOW + OPENGL CONTEXT
  // ═══════════════════════════════════════════════════════════════

  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "❌ Cannot connect to X server\n");
    return 1;
  }

  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);

  // Ask for OpenGL-capable visual
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

  Window window = XCreateWindow(
      display, root, 0, 0, game_config.window_width, game_config.window_height,
      0,             // Border width
      visual->depth, // Color depth
      InputOutput,   // Window class
      visual->visual, CWColormap | CWEventMask, &attrs);

  printf("Created window\n");

  // ═══════════════════════════════════════════════════════════════
  // 🎯 SETUP ADAPTIVE FPS
  // ═══════════════════════════════════════════════════════════════

  platform_config.monitor_refresh_hz = get_monitor_refresh_rate(display);

  AdaptiveFPS adaptive = {
      .frames_sampled = 0,
      .frames_missed = 0,
      .sample_window = 90,     // ✅ FASTER: 1.5 seconds at 60fps
      .miss_threshold = 0.10f, // ✅ 10% to drop (power save causes many misses)
      .recover_threshold = 0.02f, // ✅ 2% to recover
      .frames_since_last_change = 0,
      .cooldown_frames = 180 // ✅ 3 seconds cooldown
  };

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🎮 ADAPTIVE FRAME RATE CONTROL\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Monitor refresh: %dHz\n", platform_config.monitor_refresh_hz);
  printf("Initial target:  %dHz (%.2fms/frame)\n", game_config.refresh_rate_hz,
         game_config.target_seconds_per_frame * 1000.0f);
  printf("Sample window:   %u frames\n", adaptive.sample_window);
  printf("Miss threshold:  %.1f%%\n", adaptive.miss_threshold * 100.0f);
  printf("═══════════════════════════════════════════════════════════\n\n");

  PlatformAudioConfig platform_audio_config = {0};

  // printf("game_config.initial_audio_sample_rate: %d\n",
  //        game_config.initial_audio_sample_rate);
  // printf("game_config.audio_game_update_hz: %d\n",
  //        game_config.audio_game_update_hz);
  printf("game_config.permanent_storage_size: %lu\n",
         game_config.permanent_storage_size);
  printf("game_config.transient_storage_size: %lu\n",
         game_config.transient_storage_size);
  printf("game_config.game_flags: %d\n", game_config.game_flags);
  printf("game_config.window_width: %d\n", game_config.window_width);
  printf("game_config.window_height: %d\n", game_config.window_height);
  printf("game_config.refresh_rate_hz: %d\n", game_config.refresh_rate_hz);
  printf("game_config.prefer_vsync: %d\n", game_config.prefer_vsync);
  printf("game_config.prefer_borderless: %d\n", game_config.prefer_borderless);
  printf("game_config.prefer_resizable: %d\n", game_config.prefer_resizable);
  printf("game_config.prefer_adaptive_fps: %d\n",
         game_config.prefer_adaptive_fps);
  printf("game_config.max_controllers: %d\n", game_config.max_controllers);
  printf("game_config.initial_audio_sample_rate: %d\n",
         game_config.initial_audio_sample_rate);
  printf("game_config.audio_buffer_size_frames: %d\n",
         game_config.audio_buffer_size_frames);
  printf("game_config.audio_game_update_hz: %d\n",
         game_config.audio_game_update_hz);
  printf("game_config.refresh_rate_hz: %d\n", game_config.refresh_rate_hz);
  printf("game_config.target_seconds_per_frame: %f\n",
         game_config.target_seconds_per_frame);
  printf("game_config.refresh_rate_hz: %d\n", game_config.refresh_rate_hz);

  int bytes_per_sample = sizeof(int16) * 2; // 16-bit stereo
  int secondary_buffer_size =
      game_config.initial_audio_sample_rate * bytes_per_sample;
  game_audio_output.samples_per_second = game_config.initial_audio_sample_rate;
  platform_audio_config.bytes_per_sample = bytes_per_sample;
  platform_audio_config.secondary_buffer_size = secondary_buffer_size;
  linux_init_audio(&game_audio_output, &platform_audio_config,
                   game_config.initial_audio_sample_rate, secondary_buffer_size,
                   game_config.audio_game_update_hz);

  // Enable window close button
  Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDelete, 1);

  XStoreName(display, window, "Handmade Hero (OpenGL)");
  XMapWindow(display, window);

  // Initialize OpenGL
  if (!init_opengl(display, window, game_config.window_width,
                   game_config.window_height)) {
    return 1;
  }

  // ═══════════════════════════════════════════════════════════════
  // 🖼️ CREATE BACKBUFFER (Our CPU rendering target)
  // ═══════════════════════════════════════════════════════════════

  init_backbuffer(&game_buffer, game_config.window_width,
                  game_config.window_height, 4); // 4 bytes per pixel (RGBA)

  int buffer_size = game_config.window_width * game_config.window_height * 4;
  game_buffer.memory = platform_allocate_memory(
      NULL, buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(game_buffer.memory)) {
    fprintf(stderr, "❌ Failed to allocate backbuffer memory\n");
    fprintf(stderr, "   Error: %s\n", game_buffer.memory.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(game_buffer.memory.error_code));
    return 1;
  }

  printf("Entering main loop...\n");

  // ─────────────────────────────────────────────────────────────────────
  // ALLOCATE SOUND SAMPLE BUFFER
  // ─────────────────────────────────────────────────────────────────────
  // This buffer must be large enough to hold the maximum samples we might
  // request in a single frame. We target 100ms of audio buffered ahead,
  // so we need at least that much space.
  //
  // At 48kHz: 100ms = 4800 samples = 19200 bytes (stereo 16-bit)
  // ─────────────────────────────────────────────────────────────────────

  int32 samples_per_frame =
      game_audio_output.samples_per_second / game_config.audio_game_update_hz;
  int32 max_samples_per_call = samples_per_frame * 3;
  int sample_buffer_size =
      max_samples_per_call * platform_audio_config.bytes_per_sample;

  PlatformMemoryBlock audio_samples_block = platform_allocate_memory(
      NULL, sample_buffer_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!platform_memory_is_valid(audio_samples_block)) {
    fprintf(stderr, "❌ Failed to allocate sound sample buffer\n");
    fprintf(stderr, "   Error: %s\n", audio_samples_block.error_message);
    fprintf(stderr, "   Code: %s\n",
            platform_memory_strerror(audio_samples_block.error_code));
    return 1;
  }

  printf("✅ Sound sample buffer allocated: %d bytes\n", sample_buffer_size);

  // ═══════════════════════════════════════════════════════════════
  // 🔄 MAIN GAME LOOP (Casey's Day 8+ pattern)
  // ═══════════════════════════════════════════════════════════════
  // Each iteration:
  // 1. Process input (keyboard, mouse, joystick)
  // 2. Update game logic
  // 3. Render frame
  // 4. Sleep to maintain target FPS
  // 5. Fill audio buffer
  // ═══════════════════════════════════════════════════════════════
  if (game_buffer.memory.base) {
    while (is_game_running) {
#if DE100_INTERNAL
      if (FRAME_LOG_EVERY_TEN_SECONDS_CHECK) { // Every 30 seconds at 60fps
        printf("[HEALTH CHECK] frame=%u, RSI=%lld, marker_idx=%d\n",
               g_frame_counter,
               (long long)platform_audio_config.running_sample_index,
               g_debug_marker_index);
      }
#endif
#if DE100_INTERNAL
      if (g_frame_counter <= 10 || FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
        // DEBUG: Track RSI changes in main loop
        local_persist_var int64 loop_last_rsi = 0;

        if (platform_audio_config.running_sample_index != loop_last_rsi) {
          // Only print first 10 frames for debugging
          if (g_frame_counter <= 10) {
            printf("[LOOP #%d] RSI=%ld (changed by %ld)\n", g_frame_counter,
                   (long)platform_audio_config.running_sample_index,
                   (long)(platform_audio_config.running_sample_index -
                          loop_last_rsi));
          }
          loop_last_rsi = platform_audio_config.running_sample_index;
        }
      }
#endif

      // ═══════════════════════════════════════════════════════════
      // 🔄 HOT RELOAD CHECK (Casey's Day 21/22 pattern)
      // ═══════════════════════════════════════════════════════════
      // Check periodically if game code has been recompiled
      // This allows changing game logic without restarting!
      // ═══════════════════════════════════════════════════════════
      if (g_reload_requested ||
          game_main_code_needs_reload(
              &game, load_game_code_config.game_main_lib_path)) {
        if (g_reload_requested) {
          g_reload_requested = false;
          printf("🔄 Hot reload requested by user!\n");
        }

        printf("🔄 Hot reload triggered! at g_frame_counter: %d\n",
               g_frame_counter);
        printf("[HOT RELOAD] Before: update_and_render=%p "
               "get_audio_samples=%p\n",
               (void *)game.update_and_render, (void *)game.get_audio_samples);

        unload_game_code(&game);

        printf("[HOT RELOAD] After:  update_and_render=%p "
               "get_audio_samples=%p\n",
               (void *)game.update_and_render, (void *)game.get_audio_samples);

        load_game_code(&game, &load_game_code_config, GAME_CODE_CATEGORY_MAIN);

        if (game.is_valid) {
          printf("✅ Hot reload successful!\n");

          // NOTE: do on a separate thread
          de100_file_delete(load_game_code_config
                                .game_main_lib_tmp_path); // Clean up temp file
        } else {
          printf("⚠️  Hot reload failed, using stubs\n");
        }
      }

      PlatformTimeSpec frame_start_time;
      platform_get_timespec(&frame_start_time);
#if DE100_INTERNAL
      uint64 frame_start_cycles = __rdtsc(); // CPU cycles (for profiling)
#endif

      // ─────────────────────────────────────────────────────────────
      // PREPARE INPUT
      // ─────────────────────────────────────────────────────────────
      prepare_input_frame(old_game_input, new_game_input);

      // ─────────────────────────────────────────────────────────────
      // POLL JOYSTICK
      // ─────────────────────────────────────────────────────────────
      linux_poll_joystick(new_game_input);

      // Step 1: Update game logic and render graphics
      game.update_and_render(&game_memory, new_game_input, &game_buffer);

      // Step 2: Generate audio based on ALSA's needs
      int32 samples_to_generate = linux_get_samples_to_write(
          &platform_audio_config, &game_audio_output);

#if DE100_INTERNAL
      if (FRAME_LOG_EVERY_THREE_SECONDS_CHECK) {
        printf("[MAIN] samples_to_generate=%d, RSI=%ld\n", samples_to_generate,
               (long)platform_audio_config.running_sample_index);
      }
#endif

      if (samples_to_generate > 0) {
        int32 max_samples =
            sample_buffer_size / platform_audio_config.bytes_per_sample;
        if (samples_to_generate > max_samples) {
          // printf("⚠️  Clamping: %d → %d\n", samples_to_generate,
          // max_samples);
          samples_to_generate = max_samples;
        }

        GameAudioOutputBuffer audio_buffer = {
            .samples_per_second = game_audio_output.samples_per_second,
            .sample_count = samples_to_generate,
            .samples_block = audio_samples_block};

        game.get_audio_samples(&game_memory, &audio_buffer);
        linux_send_samples_to_alsa(&platform_audio_config, &audio_buffer);
      }

      // ─────────────────────────────────────────────────────────────
      // PROCESS X11 EVENTS
      // ─────────────────────────────────────────────────────────────
      XEvent event;
      while (XPending(display) > 0) {
        XNextEvent(display, &event);
        // GameAudioState game_audio_state =
        //     (GameState)(game_memory.permanent_storage.base);
        handle_event(&game_buffer, display, &event, new_game_input,
                     &platform_audio_config
                     // Where to store and get `game_audio_state`?
                     // I mean it should be initilized inside the game render
                     // first time but how to access it?
        );
      }

#if DE100_INTERNAL
      // ═══════════════════════════════════════════════════════════════
      // DRAW DEBUG VISUALIZATION (ALWAYS, even when paused!)
      // ═══════════════════════════════════════════════════════════════
      // Pass the PREVIOUS marker index (the one we just filled)
      // g_debug_marker_index points to the NEXT slot to fill
      int display_marker_index =
          (g_debug_marker_index - 1 + MAX_DEBUG_AUDIO_MARKERS) %
          MAX_DEBUG_AUDIO_MARKERS;
      linux_debug_sync_display(&game_buffer, &game_audio_output,
                               &platform_audio_config, g_debug_audio_markers,
                               MAX_DEBUG_AUDIO_MARKERS, display_marker_index);
#endif

      // ─────────────────────────────────────────────────────────────
      // DISPLAY FRAME (ALWAYS, even when paused!)
      // ─────────────────────────────────────────────────────────────
      update_window_opengl(&game_buffer);
      XSync(display, False);

#if DE100_INTERNAL
      // ═══════════════════════════════════════════════════════════════
      // CAPTURE FLIP STATE (after display)
      // ═══════════════════════════════════════════════════════════════
      linux_debug_capture_flip_state(&platform_audio_config);
#endif

      // ─────────────────────────────────────────────────────────────
      // MEASURE WORK TIME
      // ─────────────────────────────────────────────────────────────
      PlatformTimeSpec work_end_time;
      platform_get_timespec(&work_end_time);
      real32 work_seconds = (real32)platform_timespec_diff_seconds(
          &frame_start_time, &work_end_time);

      // ─────────────────────────────────────────────────────────────
      // ADAPTIVE SLEEP (Casey's Day 18 pattern)
      // ─────────────────────────────────────────────────────────────
      // Sleep for remaining frame time to hit target FPS
      // Two-phase sleep:
      // 1. Sleep in 1ms chunks (leave 3ms margin for OS jitter)
      // 2. Spin-wait for final microseconds (tight timing)
      // ─────────────────────────────────────────────────────────────

      real32 seconds_elapsed = work_seconds;

      if (seconds_elapsed < game_config.target_seconds_per_frame) {
        real32 test_seconds =
            game_config.target_seconds_per_frame - 0.003f; // Leave 3ms margin

        // Phase 1: Coarse sleep
        while (seconds_elapsed < test_seconds) {
          platform_sleep_ms(1);

          PlatformTimeSpec current_time;
          platform_get_timespec(&current_time);
          seconds_elapsed = (real32)platform_timespec_diff_seconds(
              &frame_start_time, &current_time);
        }

        // Phase 2: Spin-wait for precise timing
        while (seconds_elapsed < game_config.target_seconds_per_frame) {
          PlatformTimeSpec current_time;
          platform_get_timespec(&current_time);
          seconds_elapsed = (real32)platform_timespec_diff_seconds(
              &frame_start_time, &current_time);
        }
      }

      // ─────────────────────────────────────────────────────────────
      // MEASURE TOTAL FRAME TIME
      // ─────────────────────────────────────────────────────────────
      PlatformTimeSpec frame_final_time;
      platform_get_timespec(&frame_final_time);
#if DE100_INTERNAL
      uint64 frame_final_cycles = __rdtsc();
#endif

      real32 total_frame_time = (real32)platform_timespec_diff_seconds(
          &frame_start_time, &frame_final_time);

      real32 frame_time_ms = total_frame_time * 1000.0f;
      real32 target_frame_time_ms =
          game_config.target_seconds_per_frame * 1000.0f;
#if DE100_INTERNAL
      real32 sleep_time = total_frame_time - work_seconds;
      real32 fps = 1.0f / total_frame_time;
      real32 mcpf = (frame_final_cycles - frame_start_cycles) / 1000000.0f;
#endif

      // ─────────────────────────────────────────────────────────────
      // REPORT MISSED FRAMES (Only serious misses >5ms)
      // ─────────────────────────────────────────────────────────────
      if (frame_time_ms > (target_frame_time_ms + 5.0f)) {
        printf("⚠️  MISSED FRAME! %.2fms (target: %.2fms, over by: %.2fms)\n",
               frame_time_ms, target_frame_time_ms,
               frame_time_ms - target_frame_time_ms);
      }

#if DE100_INTERNAL
      // Update debug statistics
      g_frame_stats.frame_count++;
      if (frame_time_ms < g_frame_stats.min_frame_time_ms ||
          g_frame_stats.frame_count == 1) {
        g_frame_stats.min_frame_time_ms = frame_time_ms;
      }
      if (frame_time_ms > g_frame_stats.max_frame_time_ms) {
        g_frame_stats.max_frame_time_ms = frame_time_ms;
      }
      g_frame_stats.avg_frame_time_ms += frame_time_ms;

      if (total_frame_time > (game_config.target_seconds_per_frame + 0.002f)) {
        g_frame_stats.missed_frames++;
      }
#endif

      // ─────────────────────────────────────────────────────────────
      // ADAPTIVE FPS EVALUATION
      // ─────────────────────────────────────────────────────────────
      // Only runs every 300 frames (~5 seconds)
      // Adjusts target FPS based on performance
      // ─────────────────────────────────────────────────────────────

      // ─────────────────────────────────────────────────────────────
      // ADAPTIVE FPS EVALUATION - WITH POWER MODE AWARENESS
      // ─────────────────────────────────────────────────────────────

      adaptive.frames_sampled++;
      adaptive.frames_since_last_change++;
#if DE100_INTERNAL
      g_frame_counter++;
#endif

      // Track if this frame missed (with larger tolerance for variance)
      bool frame_missed = (frame_time_ms > (target_frame_time_ms + 3.0f));
      if (frame_missed) {
        adaptive.frames_missed++;
      }

      // ═══════════════════════════════════════════════════════════
      // QUICK RECOVERY CHECK: Did we just have 10 good frames in a row?
      // ═══════════════════════════════════════════════════════════
      // This allows fast recovery when switching from power save to performance

      local_persist_var uint32 consecutive_good_frames = 0;
      local_persist_var real32 recent_frame_times[10] = {0};
      local_persist_var int recent_frame_index = 0;

      recent_frame_times[recent_frame_index] = frame_time_ms;
      recent_frame_index = (recent_frame_index + 1) % 10;

      if (!frame_missed) {
        consecutive_good_frames++;
      } else {
        consecutive_good_frames = 0;
      }

      // Quick recovery: 30 consecutive good frames AND we're below monitor rate
      // AND cooldown has passed
      if (consecutive_good_frames >= 30 &&
          game_config.refresh_rate_hz < platform_config.monitor_refresh_hz &&
          adaptive.frames_since_last_change >= 90) { // 1.5 second cooldown

        // Check if recent frames have headroom (averaging under 80% of target)
        real32 avg_recent = 0;
        for (int i = 0; i < 10; i++) {
          avg_recent += recent_frame_times[i];
        }
        avg_recent /= 10.0f;

        real32 headroom_threshold = target_frame_time_ms * 0.80f;

        if (avg_recent < headroom_threshold) {
          uint32 old_target = game_config.refresh_rate_hz;

          // Jump up one tier
          switch (game_config.refresh_rate_hz) {
          case FPS_30:
            game_config.refresh_rate_hz = FPS_45;
            break;
          case FPS_45:
            game_config.refresh_rate_hz = FPS_60;
            break;
          case FPS_60:
            game_config.refresh_rate_hz = FPS_90;
            break;
          case FPS_90:
            game_config.refresh_rate_hz = FPS_120;
            break;
          default:
            game_config.refresh_rate_hz = platform_config.monitor_refresh_hz;
            break;
          }

          if (game_config.refresh_rate_hz >
              platform_config.monitor_refresh_hz) {
            game_config.refresh_rate_hz = platform_config.monitor_refresh_hz;
          }

          if (game_config.refresh_rate_hz != old_target) {
            game_config.target_seconds_per_frame =
                1.0f / (real32)game_config.refresh_rate_hz;
            adaptive.frames_since_last_change = 0;
            adaptive.frames_sampled = 0;
            adaptive.frames_missed = 0;
            consecutive_good_frames = 0;

            g_fps = game_config.refresh_rate_hz;
#if DE100_INTERNAL
            printf("🚀 QUICK RECOVERY: %d → %d Hz (avg: %.1fms, headroom: "
                   "%.1fms)\n",
                   old_target, game_config.refresh_rate_hz, avg_recent,
                   headroom_threshold);
#endif
          }
        }
      }

      // ═══════════════════════════════════════════════════════════
      // REGULAR EVALUATION: Full sample window complete
      // ═══════════════════════════════════════════════════════════

      if (adaptive.frames_sampled >= adaptive.sample_window &&
          adaptive.frames_since_last_change >= adaptive.cooldown_frames) {

        real32 miss_rate =
            (real32)adaptive.frames_missed / (real32)adaptive.frames_sampled;

#if DE100_INTERNAL
        printf("\n[ADAPTIVE] Sample: %u frames, %u missed (%.1f%%), target: %d "
               "FPS\n",
               adaptive.frames_sampled, adaptive.frames_missed,
               miss_rate * 100.0f, game_config.refresh_rate_hz);
#endif

        // ═══════════════════════════════════════════════════════════
        // REDUCE FPS: Too many missed frames
        // ═══════════════════════════════════════════════════════════

        if (miss_rate > adaptive.miss_threshold) {
          uint32 old_target = game_config.refresh_rate_hz;

          switch (game_config.refresh_rate_hz) {
          case FPS_120:
            game_config.refresh_rate_hz = FPS_90;
            break;
          case FPS_90:
            game_config.refresh_rate_hz = FPS_60;
            break;
          case FPS_60:
            game_config.refresh_rate_hz = FPS_45;
            break;
          case FPS_45:
            game_config.refresh_rate_hz = FPS_30;
            break;
          default:
            game_config.refresh_rate_hz = FPS_30;
            break;
          }

          if (game_config.refresh_rate_hz != old_target) {
            game_config.target_seconds_per_frame =
                1.0f / (real32)game_config.refresh_rate_hz;
            adaptive.frames_since_last_change = 0;

            g_fps = game_config.refresh_rate_hz;
            printf("⚠️  ADAPTIVE: %d → %d Hz (miss rate: %.1f%%)\n", old_target,
                   game_config.refresh_rate_hz, miss_rate * 100.0f);
          }
        }

        // ═══════════════════════════════════════════════════════════
        // INCREASE FPS: Performance recovered
        // ═══════════════════════════════════════════════════════════

        else if (miss_rate < adaptive.recover_threshold &&
                 game_config.refresh_rate_hz <
                     platform_config.monitor_refresh_hz) {

          uint32 old_target = game_config.refresh_rate_hz;

          switch (game_config.refresh_rate_hz) {
          case FPS_30:
            game_config.refresh_rate_hz = FPS_45;
            break;
          case FPS_45:
            game_config.refresh_rate_hz = FPS_60;
            break;
          case FPS_60:
            game_config.refresh_rate_hz = FPS_90;
            break;
          case FPS_90:
            game_config.refresh_rate_hz = FPS_120;
            break;
          default:
            game_config.refresh_rate_hz = platform_config.monitor_refresh_hz;
            break;
          }

          if (game_config.refresh_rate_hz >
              platform_config.monitor_refresh_hz) {
            game_config.refresh_rate_hz = platform_config.monitor_refresh_hz;
          }

          if (game_config.refresh_rate_hz != old_target) {
            game_config.target_seconds_per_frame =
                1.0f / (real32)game_config.refresh_rate_hz;
            adaptive.frames_since_last_change = 0;

            g_fps = game_config.refresh_rate_hz;
#if DE100_INTERNAL
            printf("✅ ADAPTIVE: %d → %d Hz (miss rate: %.1f%%)\n", old_target,
                   game_config.refresh_rate_hz, miss_rate * 100.0f);
#endif
          }
        }

        // Reset counters
        adaptive.frames_sampled = 0;
        adaptive.frames_missed = 0;
      }

      // Reset sampled frames if we haven't changed but window is full
      // (prevents accumulating stale data)
      else if (adaptive.frames_sampled >= adaptive.sample_window) {
        adaptive.frames_sampled = 0;
        adaptive.frames_missed = 0;
      }

#if DE100_INTERNAL
      // Print stats every 5 seconds (300 frames at 60fps)
      if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
        printf("[X11] %.2fms/f, %.2ff/s, %.2fmc/f (work: %.2fms, sleep: "
               "%.2fms)\n",
               total_frame_time * 1000.0f, fps, mcpf, work_seconds * 1000.0f,
               sleep_time * 1000.0f);

        real32 current_miss_rate = adaptive.frames_sampled > 0
                                       ? (real32)adaptive.frames_missed /
                                             (real32)adaptive.frames_sampled
                                       : 0.0f;

        printf("[Adaptive] Target: %d FPS | Sampled: %u/%u | Misses: %u "
               "(%.1f%%) "
               "| Next eval in: %u frames\n",
               game_config.refresh_rate_hz, adaptive.frames_sampled,
               adaptive.sample_window, adaptive.frames_missed,
               current_miss_rate * 100.0f,
               adaptive.sample_window - adaptive.frames_sampled);
      }
#endif

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
  linux_unload_alsa(&game_audio_output, &platform_audio_config);

  if (game_buffer.memory.base) {
    platform_free_memory(&game_buffer.memory);
  }

  platform_free_memory(&transient_storage);
  platform_free_memory(&permanent_storage);

  glXMakeCurrent(display, None, NULL);
  glXDestroyContext(display, g_gl.gl_context);
  XFreeColormap(display, colormap);
  XFree(visual);
  XDestroyWindow(display, window);
  XCloseDisplay(display);

  printf("[%.3fs] Memory freed\n", get_wall_clock() - g_initial_game_time);
#endif

#if DE100_INTERNAL
  // Print final statistics
  printf("\n═══════════════════════════════════════════════════════════\n");
  printf("📊 FRAME TIME STATISTICS\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Total frames:   %u\n", g_frame_stats.frame_count);
  printf("Missed frames:  %u (%.2f%%)\n", g_frame_stats.missed_frames,
         (real32)g_frame_stats.missed_frames / g_frame_stats.frame_count *
             100.0f);
  printf("Min frame time: %.2fms\n", g_frame_stats.min_frame_time_ms);
  printf("Max frame time: %.2fms\n", g_frame_stats.max_frame_time_ms);
  printf("Avg frame time: %.2fms\n",
         g_frame_stats.avg_frame_time_ms / g_frame_stats.frame_count);
  printf("═══════════════════════════════════════════════════════════\n");
#endif

  printf("Goodbye!\n");
  return 0;
}