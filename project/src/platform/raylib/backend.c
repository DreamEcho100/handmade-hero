#include "backend.h"
#include "../../base.h"
#include "../../game.h"
#include "../_common/backbuffer.h"
#include "../_common/input.h"
#include "audio.h"
#include "inputs/joystick.h"
#include "inputs/keyboard.h"

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════
// 🎨 RAYLIB-SPECIFIC TYPES
// ═══════════════════════════════════════════════════════════════

typedef struct {
  Texture2D texture;
  bool has_texture;
} OffscreenBufferMeta;

file_scoped_global_var OffscreenBufferMeta g_game_buffer_meta = {0};

// ═══════════════════════════════════════════════════════════════
// 🖼️ BACKBUFFER MANAGEMENT
// ═══════════════════════════════════════════════════════════════

/**
 * RESIZE BACKBUFFER (Day 2-3)
 *
 * Steps:
 * 1. Free old CPU pixel memory
 * 2. Free old GPU texture
 * 3. Allocate new CPU pixel memory
 * 4. Create new Raylib texture (GPU)
 *
 * This is called when:
 * - Window is first created
 * - Window is resized by user
 */
file_scoped_fn void resize_back_buffer(GameOffscreenBuffer *backbuffer,
                                       OffscreenBufferMeta *backbuffer_meta,
                                       int width, int height) {
  printf("Resizing backbuffer → %dx%d\n", width, height);

  // Guard against invalid sizes
  if (width <= 0 || height <= 0) {
    printf("⚠️  Rejected resize: invalid size\n");
    return;
  }

  int old_width = backbuffer->width;
  int old_height = backbuffer->height;

  // Update dimensions FIRST (we'll need them for allocation)
  backbuffer->width = width;
  backbuffer->height = height;
  backbuffer->pitch = backbuffer->width * backbuffer->bytes_per_pixel;

  // ─────────────────────────────────────────────────────────────
  // STEP 1: FREE OLD PIXEL MEMORY (if exists)
  // ─────────────────────────────────────────────────────────────
  if (backbuffer->memory.base && old_width > 0 && old_height > 0) {
    platform_free_memory(&backbuffer->memory);
  }

  // ─────────────────────────────────────────────────────────────
  // STEP 2: FREE OLD TEXTURE (if exists)
  // ─────────────────────────────────────────────────────────────
  if (backbuffer_meta->has_texture) {
    UnloadTexture(backbuffer_meta->texture);
    backbuffer_meta->has_texture = false;
  }

  // ─────────────────────────────────────────────────────────────
  // STEP 3: ALLOCATE NEW BACKBUFFER MEMORY
  // ─────────────────────────────────────────────────────────────
  int buffer_size = width * height * backbuffer->bytes_per_pixel;
  PlatformMemoryBlock backbuffer_memory = platform_allocate_memory(
      NULL, buffer_size, PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);

  if (!backbuffer_memory.base) {
    fprintf(stderr,
            "❌ platform_allocate_memory failed: could not allocate %d bytes\n",
            buffer_size);
    return;
  }

  backbuffer->memory = backbuffer_memory;

  // ─────────────────────────────────────────────────────────────
  // STEP 4: CREATE RAYLIB TEXTURE (GPU)
  // ─────────────────────────────────────────────────────────────
  // Raylib's Image is just a wrapper around our pixel buffer
  Image img = {.data = backbuffer->memory.base,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format =
                   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, // 32-bit RGBA (like X11)
               .mipmaps = 1};

  backbuffer_meta->texture = LoadTextureFromImage(img);
  backbuffer_meta->has_texture = true;

  printf("✅ Raylib texture created successfully\n");
}

// ═══════════════════════════════════════════════════════════════
// 🖥️ DISPLAY UPDATE (BLIT)
// ═══════════════════════════════════════════════════════════════

/**
 * UPDATE WINDOW FROM BACKBUFFER (Day 2-3)
 *
 * Equivalent to:
 * - X11:     XPutImage()
 * - Windows: StretchDIBits()
 * - OpenGL:  glTexImage2D() + DrawQuad
 *
 * This uploads our CPU-rendered pixels to the GPU and draws them.
 */
file_scoped_fn void
update_window_from_backbuffer(GameOffscreenBuffer *backbuffer,
                              OffscreenBufferMeta *backbuffer_meta) {

  if (!backbuffer_meta->has_texture || !backbuffer->memory.base) {
    return;
  }

  // Upload CPU pixels → GPU texture (like glTexImage2D)
  UpdateTexture(backbuffer_meta->texture, backbuffer->memory.base);

  // Draw GPU texture → screen (fullscreen quad)
  DrawTexture(backbuffer_meta->texture, 0, 0, WHITE);
}

// ═══════════════════════════════════════════════════════════════
// 🚀 MAIN PLATFORM ENTRY POINT
// ═══════════════════════════════════════════════════════════════

int platform_main() {
  real64 t_start = GetTime(); // ✅ Cross-platform (works on all OSes)
  printf("[%.3fs] Starting platform_main\n", GetTime() - t_start);
  fflush(stdout);

  // ═══════════════════════════════════════════════════════════
  // 💾 STEP 1: ALLOCATE GAME MEMORY (Day 25)
  // ═══════════════════════════════════════════════════════════
  //
  // Casey's "RESERVE vs COMMIT" pattern:
  // - Debug:   Reserve 2TB address space (virtual, no RAM used)
  // - Release: Let OS choose address (NULL)
  // ═══════════════════════════════════════════════════════════

#if HANDMADE_INTERNAL
  void *base_address =
      (void *)TERABYTES(2); // Debug: Fixed address for reproducibility
#else
  void *base_address = NULL; // Release: OS chooses
#endif

  uint64_t permanent_storage_size = MEGABYTES(64);
  uint64_t transient_storage_size = GIGABYTES(1);

  // Allocate permanent storage (survives across levels)
  PlatformMemoryBlock permanent_storage = platform_allocate_memory(
      base_address, permanent_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!permanent_storage.base) {
    fprintf(stderr, "❌ ERROR: Could not allocate permanent storage\n");
    return 1;
  }

  // Allocate transient storage (cleared between levels)
  void *transient_base =
      (uint8_t *)permanent_storage.base + permanent_storage.size;
  PlatformMemoryBlock transient_storage = platform_allocate_memory(
      transient_base, transient_storage_size,
      PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE | PLATFORM_MEMORY_ZEROED);

  if (!transient_storage.base) {
    fprintf(stderr, "❌ ERROR: Could not allocate transient storage\n");
    platform_free_memory(&permanent_storage);
    return 1;
  }

  // Setup game memory structure
  GameMemory game_memory = {.permanent_storage = permanent_storage,
                            .transient_storage = transient_storage,
                            .permanent_storage_size = permanent_storage.size,
                            .transient_storage_size = transient_storage.size};

  printf("✅ Game memory allocated:\n");
  printf("   Permanent: %lu MB at %p\n",
         game_memory.permanent_storage.size / (1024 * 1024),
         game_memory.permanent_storage.base);
  printf("   Transient: %lu GB at %p\n",
         game_memory.transient_storage.size / (1024 * 1024 * 1024),
         game_memory.transient_storage.base);

  // ═══════════════════════════════════════════════════════════
  // 🎮 STEP 2: INITIALIZE INPUT BUFFERS (Day 14)
  // ═══════════════════════════════════════════════════════════

  static GameInput game_inputs[2] = {0}; // Static: survives across frames
  GameInput *new_game_input = &game_inputs[0];
  GameInput *old_game_input = &game_inputs[1];

  GameSoundOutput game_sound_output = {0};
  GameOffscreenBuffer game_buffer = {0};

  // ═══════════════════════════════════════════════════════════
  // 🪟 STEP 3: CREATE WINDOW (Day 2)
  // ═══════════════════════════════════════════════════════════

  InitWindow(1250, 720, "Handmade Hero");
  printf("✅ Window created and shown\n");

  // Enable window resizing (disabled by default in Raylib)
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  printf("✅ Window is now resizable\n");

  // Disable ESC auto-close (let game handle it)
  SetExitKey(KEY_NULL);

  // Set target FPS (Raylib handles frame limiting automatically)
  SetTargetFPS(60);
  printf("✅ VSync enabled (60 FPS target)\n");

  // ═══════════════════════════════════════════════════════════
  // 🎮 STEP 4: INITIALIZE GAMEPAD (Day 14-16)
  // ═══════════════════════════════════════════════════════════

  raylib_init_gamepad(old_game_input->controllers, new_game_input->controllers);

  // ═══════════════════════════════════════════════════════════
  // 🔊 STEP 5: INITIALIZE AUDIO (Day 7-9)
  // ═══════════════════════════════════════════════════════════

  raylib_init_audio(&game_sound_output);

  // ═══════════════════════════════════════════════════════════
  // 🖼️ STEP 6: INITIALIZE BACKBUFFER (Day 2-3)
  // ═══════════════════════════════════════════════════════════

  int init_backbuffer_status = init_backbuffer(&game_buffer, 1280, 720, 4);
  if (init_backbuffer_status != 0) {
    fprintf(stderr, "❌ Failed to initialize backbuffer\n");
    return init_backbuffer_status;
  }

  resize_back_buffer(&game_buffer, &g_game_buffer_meta, game_buffer.width,
                     game_buffer.height);

  printf("✅ Entering main loop...\n");

  // ═══════════════════════════════════════════════════════════
  // 🔁 STEP 7: MAIN GAME LOOP (Day 1-∞)
  // ═══════════════════════════════════════════════════════════

  while (!WindowShouldClose()) {

    // ─────────────────────────────────────────────────────────
    // 🐛 DEBUG: Print controller states (every 300 frames)
    // ─────────────────────────────────────────────────────────
#if HANDMADE_INTERNAL
    static int debug_counter = 0;
    if (++debug_counter % 300 == 5) {
      debug_joystick_state(old_game_input);
    }
#endif

    // ─────────────────────────────────────────────────────────
    // ⌨️ INPUT: Prepare input frame (clear transitions)
    // ─────────────────────────────────────────────────────────
    prepare_input_frame(old_game_input, new_game_input);

    // ─────────────────────────────────────────────────────────
    // 🖥️ HANDLE WINDOW RESIZE (Day 2)
    // ─────────────────────────────────────────────────────────
    if (IsWindowResized()) {
      printf("🖥️  Window resized to: %dx%d\n", GetScreenWidth(),
             GetScreenHeight());
      resize_back_buffer(&game_buffer, &g_game_buffer_meta, GetScreenWidth(),
                         GetScreenHeight());
    }

    // ─────────────────────────────────────────────────────────
    // ⌨️ INPUT: Keyboard (Day 6)
    // ─────────────────────────────────────────────────────────
    handle_keyboard_inputs(&game_sound_output, new_game_input);

    // ─────────────────────────────────────────────────────────
    // 🎮 INPUT: Gamepad (Day 14-16)
    // ─────────────────────────────────────────────────────────
    raylib_poll_gamepad(new_game_input);

    // ─────────────────────────────────────────────────────────
    // 🎨 RENDER & UPDATE (Day 3)
    // ─────────────────────────────────────────────────────────
    if (game_buffer.memory.base) {
      // Casey's pattern: Render THEN update (for next frame)
      BeginDrawing();
      ClearBackground(BLACK);
      update_window_from_backbuffer(&game_buffer, &g_game_buffer_meta);
      EndDrawing();

      // Update game state for NEXT frame
      game_update_and_render(&game_memory, new_game_input, &game_buffer,
                             &game_sound_output);
    }

    // ─────────────────────────────────────────────────────────
    // 🔄 SWAP INPUT BUFFERS (Day 14)
    // ─────────────────────────────────────────────────────────
    GameInput *temp_game_input = new_game_input;
    new_game_input = old_game_input;
    old_game_input = temp_game_input;

    // ─────────────────────────────────────────────────────────
    // 📊 DEBUG: Print FPS (every 60 frames)
    // ─────────────────────────────────────────────────────────
#if HANDMADE_INTERNAL
    static int frame_counter = 0;
    if (++frame_counter >= 60) {
      printf("[Raylib] %.2fms/f, %.0ff/s\n",
             GetFrameTime() * 1000.0f, // ✅ Raylib's cross-platform timer
             (float)GetFPS());         // ✅ Raylib's FPS counter
      frame_counter = 0;
    }
#endif
  }

  // ═══════════════════════════════════════════════════════════
  // 🧹 STEP 8: CLEANUP (Day 25)
  // ═══════════════════════════════════════════════════════════
  //
  // Casey's "Resource Lifetimes in Waves" philosophy:
  // - Wave 1 (Process lifetime): OS cleans up automatically
  // - Wave 2 (State lifetime):   We clean up manually
  //
  // By default, we rely on OS cleanup (Wave 1).
  // Enable HANDMADE_SANITIZE_WAVE_1_MEMORY to manually free everything.
  // ═══════════════════════════════════════════════════════════

#if HANDMADE_SANITIZE_WAVE_1_MEMORY
  printf("[%.3fs] Exiting, freeing memory...\n", GetTime() - t_start);

  // Free Raylib texture (GPU)
  if (g_game_buffer_meta.has_texture) {
    UnloadTexture(g_game_buffer_meta.texture);
    g_game_buffer_meta.has_texture = false;
  }

  // Free backbuffer memory (CPU)
  if (game_buffer.memory.base) {
    printf("Freeing backbuffer memory...\n");
    platform_free_memory(&game_buffer.memory);
    game_buffer.memory.base = NULL;
  }

  // Free game memory
  printf("Freeing game transient memory...\n");
  platform_free_memory(&transient_storage);

  printf("Freeing game permanent memory...\n");
  platform_free_memory(&permanent_storage);

  // Unload audio
  raylib_shutdown_audio(&game_sound_output);

  // Close window and unload Raylib resources
  CloseWindow();

  printf("[%.3fs] ✅ Memory freed\n", GetTime() - t_start);
#endif

  printf("Goodbye!\n");
  return 0;
}