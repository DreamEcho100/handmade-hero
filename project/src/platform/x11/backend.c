#define _POSIX_C_SOURCE                                                        \
  199309L // Enable POSIX functions like nanosleep, clock_gettime

#include "backend.h"
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
#include <time.h>
#include <unistd.h>
#include <x86intrin.h> // For __rdtsc() CPU cycle counter

#include "../../base.h"
#include "../../game.h"
#include "../_common/backbuffer.h"
#include "../_common/input.h"

#if HANDMADE_INTERNAL
#include "../_common/debug.h"
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

// ═══════════════════════════════════════════════════════════════
// 🎮 ADAPTIVE FPS STATE
// ═══════════════════════════════════════════════════════════════
// Tracks frame performance to automatically adjust target FPS
// Casey's pattern: Don't hardcode FPS, adapt to machine capability!
// ═══════════════════════════════════════════════════════════════
typedef struct {
  int target_fps;           // Current target (starts at monitor refresh rate)
  int monitor_hz;           // Max we can go (from XRandR)
  uint32_t frames_sampled;  // How many frames we've counted so far
  uint32_t frames_missed;   // How many were too slow
  uint32_t sample_window;   // Evaluate every N frames (e.g., 300 = 5 seconds)
  real32 miss_threshold;    // If >5% frames miss, reduce FPS
  real32 recover_threshold; // If <1% frames miss, try higher FPS
} AdaptiveFPS;

#if HANDMADE_INTERNAL
// ═══════════════════════════════════════════════════════════════
// 📊 FRAME TIME STATISTICS (Debug Build Only)
// ═══════════════════════════════════════════════════════════════
// Tracks performance stats for the entire session
// Only compiled in debug builds (Casey's pattern)
// ═══════════════════════════════════════════════════════════════
typedef struct {
  uint32_t frame_count;     // Total frames rendered
  uint32_t missed_frames;   // Frames that took >target time
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
file_scoped_fn void update_window_opengl(GameOffscreenBuffer *backbuffer) {
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
int get_monitor_refresh_rate(Display *display) {
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
// ⏱️ GET CURRENT TIME (Helper Function)
// ═══════════════════════════════════════════════════════════════
// Returns current time in seconds (with nanosecond precision)
// Used for frame timing and profiling
// ═══════════════════════════════════════════════════════════════
static inline real64 get_wall_clock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts); // MONOTONIC = never goes backwards!
  return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// ═══════════════════════════════════════════════════════════════
// 🎮 HANDLE WINDOW EVENTS
// ═══════════════════════════════════════════════════════════════
// Processes X11 window events (resize, close, keyboard, etc.)
// Like addEventListener() in JavaScript
// ═══════════════════════════════════════════════════════════════
inline file_scoped_fn void handle_event(GameOffscreenBuffer *backbuffer,
                                        Display *display, XEvent *event,
                                        GameSoundOutput *sound_output,
                                        GameInput *new_game_input) {
  switch (event->type) {

  case ConfigureNotify: {
    // Window was resized
    int new_width = event->xconfigure.width;
    int new_height = event->xconfigure.height;
    printf("Window resized to: %dx%d\n", new_width, new_height);
    // NOTE: We use fixed-size backbuffer (1280x720), so we ignore resize
    // Casey does this in early Handmade Hero episodes
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
    handleEventKeyPress(event, new_game_input, sound_output);
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
  real64 t_start = get_wall_clock();
  printf("[%.3fs] Starting platform_main\n", get_wall_clock() - t_start);

  // ═══════════════════════════════════════════════════════════════
  // 💾 ALLOCATE GAME MEMORY (Casey's Day 4 pattern)
  // ═══════════════════════════════════════════════════════════════
  // Two memory pools:
  // 1. Permanent (64MB) - game state, assets, never freed
  // 2. Transient (4GB) - per-frame scratch memory, level data
  // ═══════════════════════════════════════════════════════════════

#if HANDMADE_INTERNAL
  // Debug: Reserve address space at 2TB mark (makes debugging easier)
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL; // Release: Let OS choose address
#endif

  uint64_t permanent_storage_size = MEGABYTES(64);
  uint64_t transient_storage_size = GIGABYTES(4);

  printf("[%.3fs] Allocating permanent storage (%lu MB)...\n",
         get_wall_clock() - t_start, permanent_storage_size / (1024 * 1024));

  PlatformMemoryBlock permanent_storage =
      platform_allocate_memory(base_address, permanent_storage_size,
                               PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

  if (!permanent_storage.base) {
    fprintf(stderr, "❌ Could not allocate permanent storage\n");
    return 1;
  }

  printf("[%.3fs] Allocating transient storage (%lu GB)...\n",
         get_wall_clock() - t_start,
         transient_storage_size / (1024 * 1024 * 1024));

  // Place transient storage right after permanent (contiguous memory)
  void *transient_base =
      (uint8_t *)permanent_storage.base + permanent_storage.size;

  PlatformMemoryBlock transient_storage =
      platform_allocate_memory(transient_base, transient_storage_size,
                               PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

  if (!transient_storage.base) {
    fprintf(stderr, "❌ Could not allocate transient storage\n");
    platform_free_memory(&permanent_storage);
    return 1;
  }

  // Package memory blocks for game code
  GameMemory game_memory = {0};
  game_memory.permanent_storage = permanent_storage;
  game_memory.transient_storage = transient_storage;
  game_memory.permanent_storage_size = permanent_storage.size;
  game_memory.transient_storage_size = transient_storage.size;

  printf("✅ Game memory allocated:\n");
  printf("   Permanent: %lu MB at %p\n",
         game_memory.permanent_storage.size / (1024 * 1024),
         game_memory.permanent_storage.base);
  printf("   Transient: %lu GB at %p\n",
         game_memory.transient_storage.size / (1024 * 1024 * 1024),
         game_memory.transient_storage.base);

  // ═══════════════════════════════════════════════════════════════
  // 🎮 INITIALIZE INPUT (Casey's Day 6 pattern)
  // ═══════════════════════════════════════════════════════════════
  // Double-buffered input: new_input (current) and old_input (previous)
  // We swap pointers each frame to preserve button press/release state
  // ═══════════════════════════════════════════════════════════════

  static GameInput game_inputs[2] = {0};
  GameInput *new_game_input = &game_inputs[0];
  GameInput *old_game_input = &game_inputs[1];

  GameSoundOutput game_sound_output = {0};
  GameOffscreenBuffer game_buffer = {0};

  // ═══════════════════════════════════════════════════════════════
  // 🎮 INITIALIZE JOYSTICK (Before main loop!)
  // ═══════════════════════════════════════════════════════════════
  printf("[%.3fs] Initializing joystick...\n", get_wall_clock() - t_start);
  linux_init_joystick(old_game_input->controllers, new_game_input->controllers);
  printf("[%.3fs] Joystick initialized\n", get_wall_clock() - t_start);

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

  int width = 1280, height = 720;

  Window window =
      XCreateWindow(display, root, 0, 0, width, height,
                    0,             // Border width
                    visual->depth, // Color depth
                    InputOutput,   // Window class
                    visual->visual, CWColormap | CWEventMask, &attrs);

  printf("Created window\n");

  // ═══════════════════════════════════════════════════════════════
  // 🎯 SETUP ADAPTIVE FPS
  // ═══════════════════════════════════════════════════════════════

  int monitor_refresh_hz = get_monitor_refresh_rate(display);

  AdaptiveFPS adaptive = {
      .target_fps = monitor_refresh_hz,
      .monitor_hz = monitor_refresh_hz,
      .frames_sampled = 0,
      .frames_missed = 0,
      .sample_window = 600,       // Evaluate every 10 seconds (at 60fps)
      .miss_threshold = 0.03f,    // If >5% frames miss → reduce FPS
      .recover_threshold = 0.005f // If <0.5% frames miss → try higher FPS
  };

  real32 target_seconds_per_frame = 1.0f / (real32)adaptive.target_fps;
#if HANDMADE_INTERNAL
  g_frame_log_counter = 0;
  g_debug_fps = adaptive.target_fps;
#endif

  printf("═══════════════════════════════════════════════════════════\n");
  printf("🎮 ADAPTIVE FRAME RATE CONTROL\n");
  printf("═══════════════════════════════════════════════════════════\n");
  printf("Monitor refresh: %dHz\n", monitor_refresh_hz);
  printf("Initial target:  %dHz (%.2fms/frame)\n", adaptive.target_fps,
         target_seconds_per_frame * 1000.0f);
  printf("Sample window:   %u frames\n", adaptive.sample_window);
  printf("Miss threshold:  %.1f%%\n", adaptive.miss_threshold * 100.0f);
  printf("═══════════════════════════════════════════════════════════\n\n");

  // ═══════════════════════════════════════════════════════════════
  // 🔊 INITIALIZE AUDIO (Casey's Day 7 pattern)
  // ═══════════════════════════════════════════════════════════════
  printf("[%.3fs] Loading ALSA library...\n", get_wall_clock() - t_start);
  linux_load_alsa(); // Dynamically load libasound.so
  printf("[%.3fs] ALSA library loaded\n", get_wall_clock() - t_start);

  int samples_per_second = 48000;
  int bytes_per_sample = sizeof(int16_t) * 2; // 16-bit stereo
  // Casey's pattern: Run game at half monitor refresh rate
  // (60Hz monitor → 30 FPS game, 120Hz → 60 FPS game)
  game_sound_output.game_update_hz = adaptive.target_fps;
  int secondary_buffer_size = samples_per_second * bytes_per_sample;
  if (!linux_init_sound(&game_sound_output, samples_per_second,
                        secondary_buffer_size,
                        game_sound_output.game_update_hz)) {
    fprintf(stderr, "[PLATFORM] Failed to initialize audio\n");
    // Continue without audio (graceful degradation)
  } else {
    printf("[PLATFORM] ✅ Audio initialized successfully\n");
  }

  // Enable window close button
  Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wmDelete, 1);

  XStoreName(display, window, "Handmade Hero (OpenGL)");
  XMapWindow(display, window);

  // Initialize OpenGL
  if (!init_opengl(display, window, width, height)) {
    return 1;
  }

#if HANDMADE_INTERNAL
  // Draw test line at x=100
  for (int y = 0; y < game_buffer.height; y++) {
    uint32_t *pixel =
        (uint32_t *)game_buffer.memory.base + (y * game_buffer.width + 100);
    *pixel = 0x00FF00FF; // Bright green
  }
  printf("[DEBUG] Drew test line at x=100\n");
#endif

  // ═══════════════════════════════════════════════════════════════
  // 🖼️ CREATE BACKBUFFER (Our CPU rendering target)
  // ═══════════════════════════════════════════════════════════════

  init_backbuffer(&game_buffer, width, height, 4); // 4 bytes per pixel (RGBA)

  int buffer_size = width * height * 4;
  game_buffer.memory = platform_allocate_memory(
      NULL, buffer_size, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);
  game_buffer.width = width;
  game_buffer.height = height;
  game_buffer.pitch = width * 4;

  printf("Entering main loop...\n");

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

  while (is_game_running) {
#if HANDMADE_INTERNAL
    if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
      // DEBUG: Track RSI changes in main loop
      static int64_t loop_last_rsi = 0;
      static int loop_count = 0;
      loop_count++;

      if (loop_count <= 10 ||
          game_sound_output.running_sample_index != loop_last_rsi) {
        // Only print first 10 frames for debugging
        if (loop_count <= 10) {
          printf(
              "[LOOP #%d] RSI=%ld (changed by %ld)\n", loop_count,
              (long)game_sound_output.running_sample_index,
              (long)(game_sound_output.running_sample_index - loop_last_rsi));
        }
        loop_last_rsi = game_sound_output.running_sample_index;
      }
    }
#endif

    struct timespec frame_start_time;
    clock_gettime(CLOCK_MONOTONIC, &frame_start_time);
    uint64_t frame_start_cycles = __rdtsc(); // CPU cycles (for profiling)

    // ─────────────────────────────────────────────────────────────
    // PREPARE INPUT
    // ─────────────────────────────────────────────────────────────
    prepare_input_frame(old_game_input, new_game_input);

    // ─────────────────────────────────────────────────────────────
    // PROCESS X11 EVENTS
    // ─────────────────────────────────────────────────────────────
    XEvent event;
    while (XPending(display) > 0) {
      XNextEvent(display, &event);
      handle_event(&game_buffer, display, &event, &game_sound_output,
                   new_game_input);
    }

    // ─────────────────────────────────────────────────────────────
    // POLL JOYSTICK
    // ─────────────────────────────────────────────────────────────
    linux_poll_joystick(new_game_input);

    // ─────────────────────────────────────────────────────────────
    // UPDATE GAME + RENDER (Skip if window inactive)
    // ─────────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────
    // UPDATE GAME + RENDER
    // ─────────────────────────────────────────────────────────────
    if (game_buffer.memory.base && g_window_is_active) {

      // Step 1: Update game logic and render graphics
      game_update_and_render(&game_memory, new_game_input, &game_buffer,
                             &game_sound_output);

      // Step 2: Fill audio buffer (captures Output* debug state)
      game_sound_output.game_update_hz = adaptive.target_fps;
      linux_fill_sound_buffer(&game_sound_output);

#if HANDMADE_INTERNAL
      // Step 3: Draw debug visualization BEFORE flip
      // We draw the PREVIOUS frame's marker (which has complete data)
      // The current frame's marker only has Output* filled, not Flip* yet
      int draw_index = (g_debug_marker_index + MAX_DEBUG_AUDIO_MARKERS - 1) %
                       MAX_DEBUG_AUDIO_MARKERS;

      // Only draw if we have valid data
      if (g_debug_audio_markers[draw_index].flip_play_cursor >= 0) {
        linux_debug_sync_display(&game_buffer, &game_sound_output,
                                 g_debug_audio_markers, MAX_DEBUG_AUDIO_MARKERS,
                                 draw_index);
      }
#endif

      // Step 4: Display frame (FLIP!)
      update_window_opengl(&game_buffer);

#if HANDMADE_INTERNAL
      // Step 5: Capture flip state (fills Flip* fields, advances marker)
      // This MUST happen AFTER glXSwapBuffers!
      linux_debug_capture_flip_state(&game_sound_output);
#endif

      // Step 6: Wait for GPU to finish
      XSync(display, False);

    } else if (!g_window_is_active) {
      // Window in background: sleep to save CPU
      struct timespec sleep_spec = {0, 16000000}; // 16ms
      nanosleep(&sleep_spec, NULL);
    }

    // ─────────────────────────────────────────────────────────────
    // MEASURE WORK TIME
    // ─────────────────────────────────────────────────────────────
    struct timespec work_end_time;
    clock_gettime(CLOCK_MONOTONIC, &work_end_time);
    real32 work_seconds =
        (work_end_time.tv_sec - frame_start_time.tv_sec) +
        (work_end_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;

    // ─────────────────────────────────────────────────────────────
    // ADAPTIVE SLEEP (Casey's Day 18 pattern)
    // ─────────────────────────────────────────────────────────────
    // Sleep for remaining frame time to hit target FPS
    // Two-phase sleep:
    // 1. Sleep in 1ms chunks (leave 3ms margin for OS jitter)
    // 2. Spin-wait for final microseconds (tight timing)
    // ─────────────────────────────────────────────────────────────

    real32 seconds_elapsed = work_seconds;

    if (seconds_elapsed < target_seconds_per_frame) {
      real32 test_seconds =
          target_seconds_per_frame - 0.003f; // Leave 3ms margin

      // Phase 1: Coarse sleep
      while (seconds_elapsed < test_seconds) {
        struct timespec sleep_spec = {0, 1000000}; // 1ms
        nanosleep(&sleep_spec, NULL);

        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        seconds_elapsed =
            (current_time.tv_sec - frame_start_time.tv_sec) +
            (current_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;
      }

      // Phase 2: Spin-wait for precise timing
      while (seconds_elapsed < target_seconds_per_frame) {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        seconds_elapsed =
            (current_time.tv_sec - frame_start_time.tv_sec) +
            (current_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;
      }
    }

    // // ─────────────────────────────────────────────────────────────
    // // FILL AUDIO BUFFER (After sleep!)
    // // ─────────────────────────────────────────────────────────────
    // game_sound_output.game_update_hz = adaptive.target_fps;
    // linux_fill_sound_buffer(&game_sound_output);

    // ─────────────────────────────────────────────────────────────
    // MEASURE TOTAL FRAME TIME
    // ─────────────────────────────────────────────────────────────
    struct timespec frame_final_time;
    clock_gettime(CLOCK_MONOTONIC, &frame_final_time);
    uint64_t frame_final_cycles = __rdtsc();

    real32 total_frame_time =
        (frame_final_time.tv_sec - frame_start_time.tv_sec) +
        (frame_final_time.tv_nsec - frame_start_time.tv_nsec) / 1000000000.0f;

    real32 frame_time_ms = total_frame_time * 1000.0f;
    real32 target_frame_time_ms = target_seconds_per_frame * 1000.0f;
    real32 sleep_time = total_frame_time - work_seconds;
    real32 fps = 1.0f / total_frame_time;
    real32 mcpf = (frame_final_cycles - frame_start_cycles) / 1000000.0f;

    // ─────────────────────────────────────────────────────────────
    // REPORT MISSED FRAMES (Only serious misses >5ms)
    // ─────────────────────────────────────────────────────────────
    if (frame_time_ms > (target_frame_time_ms + 5.0f) && g_window_is_active) {
      printf("⚠️  MISSED FRAME! %.2fms (target: %.2fms, over by: %.2fms)\n",
             frame_time_ms, target_frame_time_ms,
             frame_time_ms - target_frame_time_ms);
    }

#if HANDMADE_INTERNAL
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

    if (total_frame_time > (target_seconds_per_frame + 0.002f)) {
      g_frame_stats.missed_frames++;
    }
#endif

    // ─────────────────────────────────────────────────────────────
    // ADAPTIVE FPS EVALUATION
    // ─────────────────────────────────────────────────────────────
    // Only runs every 300 frames (~5 seconds)
    // Adjusts target FPS based on performance
    // ─────────────────────────────────────────────────────────────

    adaptive.frames_sampled++;
#if HANDMADE_INTERNAL
    g_frame_log_counter++;
#endif

    if (frame_time_ms > (target_frame_time_ms + 2.0f) && g_window_is_active) {
      adaptive.frames_missed++;
    }

    // Time to evaluate? (Every 300 frames = 5 seconds at 60fps)
    if (adaptive.frames_sampled >= adaptive.sample_window) {
#if HANDMADE_INTERNAL
      if (FRAME_LOG_EVERY_ONE_SECONDS_CHECK ||
          (g_frame_stats.frame_count % adaptive.sample_window == 0)) {
        printf("[AUDIO] SndPcmPrepare called at frame %d\n",
               g_frame_stats.frame_count);
        printf("[ADAPTIVE] Evaluating at frame %d, RSI=%ld\n",
               g_frame_stats.frame_count,
               (long)game_sound_output.running_sample_index);
      }
#endif
      real32 miss_rate =
          (real32)adaptive.frames_missed / (real32)adaptive.frames_sampled;

      // Too many missed frames? Reduce target FPS
      if (miss_rate > adaptive.miss_threshold) {
        int old_target = adaptive.target_fps;

        if (adaptive.target_fps == adaptive.monitor_hz) {
          switch (adaptive.monitor_hz) {
          case FPS_120:
            adaptive.target_fps = FPS_90;
            break;
          case FPS_90:
            adaptive.target_fps = FPS_60;
            break;
          case FPS_60:
            adaptive.target_fps = FPS_45;
            break;
          case FPS_45:
            adaptive.target_fps = FPS_30;
            break;
          default:
            adaptive.target_fps = FPS_30;
            break;
          }
        } else {
          switch (adaptive.target_fps) {
          case FPS_120:
            adaptive.target_fps = FPS_90;
            break;
          case FPS_90:
            adaptive.target_fps = FPS_60;
            break;
          case FPS_60:
            adaptive.target_fps = FPS_45;
            break;
          case FPS_45:
            adaptive.target_fps = FPS_30;
            break;
          default:
            adaptive.target_fps = FPS_30;
            break;
          }
        }

        if (adaptive.target_fps <= 0) {
          fprintf(stderr, "❌ BUG: adaptive.target_fps=%d after reduction!\n",
                  adaptive.target_fps);
          adaptive.target_fps = FPS_30; // Emergency fallback
        }

        if (adaptive.target_fps != old_target) {
          target_seconds_per_frame = 1.0f / (real32)adaptive.target_fps;
          game_sound_output.game_update_hz = adaptive.target_fps;

#if HANDMADE_INTERNAL
          g_debug_fps = adaptive.target_fps;
#endif

          printf("\n⚠️  ADAPTIVE FPS: REDUCING TARGET %d → %d FPS\n", old_target,
                 adaptive.target_fps);
        }
      }
      // Performance recovered? Try higher FPS
      else if (miss_rate < adaptive.recover_threshold &&
               adaptive.target_fps < adaptive.monitor_hz) {
        int old_target = adaptive.target_fps;

        switch (adaptive.target_fps) {
        case FPS_30:
          adaptive.target_fps = FPS_45;
          break;
        case FPS_45:
          adaptive.target_fps = FPS_60;
          break;
        case FPS_60:
          adaptive.target_fps = FPS_90;
          break;
        case FPS_90:
          adaptive.target_fps = FPS_120;
          break;
        default:
          adaptive.target_fps = adaptive.monitor_hz;
          break;
        }

        if (adaptive.target_fps != old_target) {
          target_seconds_per_frame = 1.0f / (real32)adaptive.target_fps;
          game_sound_output.game_update_hz = adaptive.target_fps;
#if HANDMADE_INTERNAL
          g_debug_fps = adaptive.target_fps;
#endif
          printf("\n✅ ADAPTIVE FPS: INCREASING TARGET %d → %d FPS (miss "
                 "rate: "
                 "%.1f%%)\n\n",
                 old_target, adaptive.target_fps, miss_rate * 100.0f);
        }
      }

      // Reset sample window
      adaptive.frames_sampled = 0;
      adaptive.frames_missed = 0;
    }

#if HANDMADE_INTERNAL
    // Print stats every 5 seconds (300 frames at 60fps)
    if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
      printf("[X11] %.2fms/f, %.2ff/s, %.2fmc/f (work: %.2fms, sleep: "
             "%.2fms)\n",
             total_frame_time * 1000.0f, fps, mcpf, work_seconds * 1000.0f,
             sleep_time * 1000.0f);

      real32 current_miss_rate =
          adaptive.frames_sampled > 0
              ? (real32)adaptive.frames_missed / (real32)adaptive.frames_sampled
              : 0.0f;

      printf("[Adaptive] Target: %d FPS | Sampled: %u/%u | Misses: %u "
             "(%.1f%%) "
             "| Next eval in: %u frames\n",
             adaptive.target_fps, adaptive.frames_sampled,
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

  // ═══════════════════════════════════════════════════════════════
  // 🧹 CLEANUP (Casey's "Wave Lifetime" pattern)
  // ═══════════════════════════════════════════════════════════════
  // By default, we DON'T manually clean up process-lifetime resources
  // The OS does it faster when the process exits (<1ms vs 17ms)
  //
  // Only clean up if HANDMADE_SANITIZE_WAVE_1_MEMORY is enabled
  // (useful for memory leak detection tools like Valgrind)
  // ═══════════════════════════════════════════════════════════════

#if HANDMADE_SANITIZE_WAVE_1_MEMORY
  printf("[%.3fs] Exiting, freeing memory...\n", get_wall_clock() - t_start);

  linux_close_joysticks();
  linux_unload_alsa(&game_sound_output);

  if (game_buffer.memory.base) {
    platform_free_memory(&game_buffer.memory);
  }

  platform_free_memory(&transient_storage);
  platform_free_memory(&permanent_storage);

  glXMakeCurrent(display, None, NULL);
  glXDestroyContext(display, g_gl.gl_context);
  XFreeColormap(display, colormap);
  XDestroyWindow(display, window);
  XCloseDisplay(display);

  printf("[%.3fs] Memory freed\n", get_wall_clock() - t_start);
#endif

#if HANDMADE_INTERNAL
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