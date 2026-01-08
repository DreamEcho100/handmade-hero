#define _POSIX_C_SOURCE 199309L

#include "backend.h"
#include "../../base.h"
#include "../../game.h"
#include "../_common/backbuffer.h"
#include "../_common/input.h"
#include "audio.h"
#include "inputs/joystick.h"
#include "inputs/keyboard.h"
#include <assert.h>

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
  Texture2D texture;
  bool has_texture;
} OffscreenBufferMeta;

file_scoped_global_var OffscreenBufferMeta game_buffer_meta = {0};
file_scoped_global_var struct timespec g_frame_start;
file_scoped_global_var struct timespec g_frame_end;

// Raylib pixel composer (R8G8B8A8 format)
file_scoped_fn uint32_t compose_pixel_rgba(uint8_t r, uint8_t g, uint8_t b,
                                           uint8_t a) {
  return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) |
         (uint32_t)r;
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void resize_back_buffer(GameOffscreenBuffer *backbuffer,
                                       OffscreenBufferMeta *backbuffer_meta,
                                       int width, int height) {
  printf("Resizing back backbuffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = backbuffer->width;
  int old_height = backbuffer->height;

  // Update first!
  backbuffer->width = width;
  backbuffer->height = height;
  backbuffer->pitch = backbuffer->width * backbuffer->bytes_per_pixel;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (backbuffer->memory.base && old_width > 0 && old_height > 0) {
    platform_free_memory(&backbuffer->memory);
  }

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (backbuffer_meta->has_texture) {
    UnloadTexture(backbuffer_meta->texture);
    backbuffer_meta->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * backbuffer->bytes_per_pixel;
  PlatformMemoryBlock backbuffer_memory = platform_allocate_memory(
      NULL, buffer_size, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

  if (!backbuffer_memory.base) {
    fprintf(stderr,
            "platform_allocate_memory failed: could not allocate %d bytes\n",
            buffer_size);
    return;
  }

  backbuffer->memory = backbuffer_memory;

  backbuffer->width = width;
  backbuffer->height = height;
  backbuffer->pitch = backbuffer->width * backbuffer->bytes_per_pixel;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = backbuffer->memory.base,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               // format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
               .mipmaps = 1};
  backbuffer_meta->texture = LoadTextureFromImage(img);
  backbuffer_meta->has_texture = true;
  printf("Raylib texture created successfully\n");
}

/********************************************************************
 UPDATE WINDOW (BLIT)
 Equivalent to XPutImage or StretchDIBits
*********************************************************************/
file_scoped_fn void
update_window_from_backbuffer(GameOffscreenBuffer *backbuffer,
                              OffscreenBufferMeta *backbuffer_meta) {
  if (!backbuffer_meta->has_texture || !backbuffer->memory.base)
    return;

  // Upload CPU → GPU
  UpdateTexture(backbuffer_meta->texture, backbuffer->memory.base);
  // Draw GPU texture → screen
  DrawTexture(backbuffer_meta->texture, 0, 0, WHITE);
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void ResizeBackBuffer(GameOffscreenBuffer *backbuffer,
                                     OffscreenBufferMeta *backbuffer_meta,
                                     int width, int height) {
  printf("Resizing back backbuffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = backbuffer->width;
  int old_height = backbuffer->height;

  // Update first!
  backbuffer->width = width;
  backbuffer->height = height;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (backbuffer->memory.base && old_width > 0 && old_height > 0) {
    platform_free_memory(&backbuffer->memory);
  }

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (backbuffer_meta->has_texture) {
    UnloadTexture(backbuffer_meta->texture);
    backbuffer_meta->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * backbuffer->bytes_per_pixel;
  PlatformMemoryBlock backbuffer_memory = platform_allocate_memory(
      NULL, buffer_size, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);
  if (!backbuffer_memory.base) {
    fprintf(stderr,
            "platform_allocate_memory failed: could not allocate %d bytes\n",
            buffer_size);
    return;
  }
  backbuffer->memory = backbuffer_memory;

  backbuffer->width = width;
  backbuffer->height = height;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = backbuffer->memory.base,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};
  backbuffer_meta->texture = LoadTextureFromImage(img);
  backbuffer_meta->has_texture = true;
  printf("Raylib texture created successfully\n");
}

// Helper to get current time in seconds
static inline double get_wall_clock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int platform_main() {
  double t_start = get_wall_clock();
  printf("[%.3fs] Starting platform_main\n", get_wall_clock() - t_start);
  fflush(stdout);

#if HANDMADE_INTERNAL
  // Debug/Development mode: Reserve 2TB of address space for debugging
  // What if your RAM is less than 2TB? No problem, we're just reserving
  // address space, not committing physical memory yet.
  // Which means no actual RAM is used until we commit it (with mmap or
  // similar).
  //
  void *base_address = (void *)TERABYTES(2);
#else
  void *base_address = NULL;
#endif

  uint64_t permanent_storage_size = MEGABYTES(64);
  uint64_t transient_storage_size = GIGABYTES(1);
  PlatformMemoryBlock permanent_storage = platform_allocate_memory(
      base_address, permanent_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!permanent_storage.base) {
    fprintf(stderr, "ERROR: Could not allocate permanent storage\n");
    return 1;
  }

  // Calculate next address
  void *transient_base =
      (uint8_t *)permanent_storage.base + permanent_storage.size;

  PlatformMemoryBlock transient_storage = platform_allocate_memory(
      transient_base, transient_storage_size, // ← Actually allocate it!
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!transient_storage.base) {
    fprintf(stderr, "ERROR: Could not allocate transient storage\n");
    platform_free_memory(&permanent_storage);
    return 1;
  }

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

  if (game_memory.permanent_storage.base &&
      game_memory.transient_storage.base) {
    static GameInput game_inputs[2] = {0}; // Static - survives across frames!
    GameInput *new_game_input = &game_inputs[0];
    GameInput *old_game_input = &game_inputs[1];

    GameSoundOutput game_sound_output = {0};
    GameOffscreenBuffer game_buffer = {0};

    /**
     * InitWindow() does ALL of this:
     * - XOpenDisplay() - Connect to display server
     * - XCreateSimpleWindow() - Create the window
     * - XStoreName() - Set window title
     * - XSetWMProtocols() - Set up close handler
     * - XSelectInput() - Register for events
     * - XMapWindow() - Show the window
     *
     * Raylib handles all platform differences internally.
     * Works on Windows, Linux, macOS, web, mobile!
     */
    InitWindow(1250, 720, "Handmade Hero");
    printf("Window created and shown\n");

    /**
     * ENABLE WINDOW RESIZING
     *
     * By default, Raylib creates a FIXED-SIZE window.
     * This is different from X11/Windows which allow resizing by default.
     *
     * SetWindowState(FLAG_WINDOW_RESIZABLE) tells Raylib:
     * "Allow the user to resize this window"
     *
     * This is like setting these properties in X11:
     * - Setting size hints with XSetWMNormalHints()
     * - Allowing min/max size changes
     *
     * WHY FIXED BY DEFAULT?
     * Games often need a specific resolution and don't want the window resized.
     * But for Casey's lesson, we want to demonstrate resize events!
     *
     * WEB ANALOGY:
     * It's like the difference between:
     * - <div style="resize: none">  (default Raylib)
     * - <div style="resize: both">  (after SetWindowState)
     */
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    printf("Window is now resizable\n");

    /**
     * OPTIONAL: Set target FPS
     *
     * Raylib has built-in frame rate limiting.
     * We don't need this for Day 002, but it's useful later.
     */
    SetTargetFPS(60);

    // ═══════════════════════════════════════════════════════════
    // 🎮 Initialize gamepad (cross-platform!)
    // ═══════════════════════════════════════════════════════════
    raylib_init_gamepad(old_game_input->controllers,
                        new_game_input->controllers);

    // ═══════════════════════════════════════════════════════════
    // 🔊 Initialize audio (Day 7-9)
    // ═══════════════════════════════════════════════════════════
    raylib_init_audio(&game_sound_output);

    int init_backbuffer_status =
        init_backbuffer(&game_buffer, 1280, 720, 4, compose_pixel_rgba);
    if (init_backbuffer_status != 0) {
      fprintf(stderr, "Failed to initialize backbuffer\n");
      return init_backbuffer_status;
    }

    resize_back_buffer(&game_buffer, &game_buffer_meta, game_buffer.width,
                       game_buffer.height);

    printf("Entering main loop...\n");

    clock_gettime(CLOCK_MONOTONIC, &g_frame_start);

    /**
     * EVENT LOOP
     *
     * WindowShouldClose() checks if:
     * - User clicked X button (WM_CLOSE / ClientMessage)
     * - User pressed ESC key
     *
     * It's like: while (!g_Running) in X11
     *
     * Raylib handles the event queue internally - we don't see XNextEvent()
     */
    while (!WindowShouldClose()) {
      // ═══════════════════════════════════════════════════════════
      // 🐛 DEBUG: Print controller states (TEMPORARY!)
      // ═══════════════════════════════════════════════════════════
      static int frame_count = 0;
      if (frame_count++ % 60 == 0) { // Print once per second (60 FPS)
        debug_joystick_state(old_game_input);
      }

      prepare_input_frame(old_game_input, new_game_input);

      /**
       * CHECK FOR WINDOW RESIZE EVENT
       *
       * Casey's WM_SIZE equivalent.
       * In X11, this is ConfigureNotify event.
       *
       * IsWindowResized() returns true when window size changes.
       * This is like checking event.type == ConfigureNotify in X11.
       *
       * When window is resized, we:
       * 1. Toggle the color (like Casey does)
       * 2. Log the new size (like printf in X11 version)
       *
       * WEB ANALOGY:
       * window.addEventListener('resize', () => {
       *   console.log('Window resized!');
       *   toggleColor();
       * });
       */
      if (IsWindowResized()) {
        printf("Window resized to: %dx%d\n", GetScreenWidth(),
               GetScreenHeight());
        ResizeBackBuffer(&game_buffer, &game_buffer_meta, GetScreenWidth(),
                         GetScreenHeight());
      }

      // ═══════════════════════════════════════════════════════
      // ⌨️ KEYBOARD INPUT (cross-platform!)
      // ═══════════════════════════════════════════════════════
      handle_keyboard_inputs(&game_sound_output, new_game_input);

      // ═══════════════════════════════════════════════════════
      // 🎮 GAMEPAD INPUT (cross-platform!)
      // ═══════════════════════════════════════════════════════
      raylib_poll_gamepad(new_game_input);

      // ═══════════════════════════════════════════════════════
      // 🎨 RENDER & UPDATE (Match X11 order)
      // ═══════════════════════════════════════════════════════
      if (game_buffer.memory.base) {
        // Step 1: Display current backbuffer (like X11's update_window)
        BeginDrawing();
        ClearBackground(BLACK);
        update_window_from_backbuffer(&game_buffer, &game_buffer_meta);
        EndDrawing();

        // Step 2: Update game state for NEXT frame (like X11)
        game_update_and_render(&game_memory, new_game_input, &game_buffer,
                               &game_sound_output);
      }

      GameInput *temp_game_input = new_game_input;
      new_game_input = old_game_input;
      old_game_input = temp_game_input;

      clock_gettime(CLOCK_MONOTONIC, &g_frame_end);

      real64 ms_per_frame =
          (g_frame_end.tv_sec - g_frame_start.tv_sec) * 1000.0 +
          (g_frame_end.tv_nsec - g_frame_start.tv_nsec) / 1000000.0;
      real64 fps = 1000.0 / ms_per_frame;

      // Show FPS every 60 frames to verify performance
      static int frame_counter = 0;
      if (++frame_counter >= 60) {
        printf("[Raylib] %.2fms/f, %.2ff/s\n", ms_per_frame, fps);
        frame_counter = 0;
      }

      g_frame_start = g_frame_end;
    }

    /**
     * STEP 9: CLEANUP - CASEY'S "RESOURCE LIFETIMES IN WAVES" PHILOSOPHY
     *
     * Raylib handles most resources as process-lifetime (Wave 1).
     * The OS will bulk-clean everything instantly when the process exits.
     * We only need to manually clean up state-lifetime resources (Wave 2),
     * like our backbuffer, if we were replacing them during runtime.
     *
     * So, following Casey's philosophy, we just exit cleanly!
     */

#if HANDMADE_SANITIZE_WAVE_1_MEMORY
    printf("[%.3fs] Exiting, freeing memory...\n", get_wall_clock() - t_start);

    // Free Raylib texture
    if (game_buffer_meta.has_texture) {
      UnloadTexture(game_buffer_meta.texture);
      game_buffer_meta.has_texture = false;
    }

    // Free backbuffer memory
    if (game_buffer.memory.base) {
      printf("Freeing backbuffer memory...\n");
      platform_free_memory(&game_buffer.memory);
      game_buffer.memory.base = NULL;
    }

    // Free Transient and Permanent storage
    printf("Freeing game transient memory...\n");
    platform_free_memory(&transient_storage);

    printf("Freeing game permanent memory...\n");
    platform_free_memory(&permanent_storage);

    // Unload audio
    raylib_shutdown_audio(&game_sound_output);

    // Close window and unload Raylib resources
    CloseWindow();

    printf("[%.3fs] Memory freed\n", get_wall_clock() - t_start);
#endif

    printf("Goodbye!\n");
  }

  return 0;
}