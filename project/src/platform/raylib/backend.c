#define _POSIX_C_SOURCE 199309L
#include "backend.h"
#include "../base.h"
#include "audio.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

typedef struct {
  void *memory;
  int width;
  int height;
  int bytes_per_pixel;
  Texture2D texture;
  bool has_texture;
} OffscreenBuffer;

typedef struct {
  int offset_x;
  int offset_y;
} GradientState;

typedef struct {
  int offset_x;
  int offset_y;
} PixelState;

// Button states (mirrors Casey's XInput button layout)
typedef struct {
  bool up;
  bool down;
  bool left;
  bool right;

  bool start;
  bool back;
  bool a_button;
  bool b_button;
  bool x_button;
  bool y_button;
  bool left_shoulder;
  bool right_shoulder;

  // Analog sticks (normalized -1.0 to +1.0, Raylib style)
  float left_stick_x;
  float left_stick_y;
  float right_stick_x;
  float right_stick_y;
  float left_trigger;
  float right_trigger;

  GradientState gradient_state;
} GameControls;

typedef struct {
  GameControls controls;
  GradientState gradient;
  PixelState pixel;
  int speed;
  bool is_running;
  int gamepad_id; // Which gamepad to use (0-3)
} GameState;

file_scoped_global_var OffscreenBuffer g_backbuffer;
file_scoped_global_var GameState g_game_state = {0}; // Zero-initialized struct
file_scoped_global_var struct timespec g_frame_start;
file_scoped_global_var struct timespec g_frame_end;

// file_scoped_global_var int g_joystick_fd = -1;

/*
Will be added when needed
typedef struct {
    int width;
    int height;
} X11WindowDimension;

file_scoped_fn X11WindowDimension
get_window_dimension(Display *display, Window window) {
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);
    return (X11WindowDimension){attrs.width, attrs.height};
}
*/

file_scoped_global_var OffscreenBuffer g_backbuffer;

// ═══════════════════════════════════════════════════════════════
// 🎮 Initialize gamepad (Raylib cross-platform!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn bool raylib_init_gamepad(GameState *game_state) {
  printf("Searching for gamepad...\n");

  // Try gamepads 0-3 (Raylib supports up to 4 controllers)
  for (int i = 0; i < 4; i++) {
    if (IsGamepadAvailable(i)) {
      const char *name = GetGamepadName(i);
      printf("✅ Gamepad %d connected: %s\n", i, name);

      // Use the first available gamepad
      game_state->gamepad_id = i;
      return true;
    }
  }

  printf("❌ No gamepad found\n");
  printf("  - Is controller plugged in or paired via Bluetooth?\n");
  printf("  - Raylib supports Xbox, PlayStation, Nintendo controllers\n");
  printf("  - Game will run with keyboard input only\n");

  game_state->gamepad_id = -1; // No gamepad
  return false;
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Poll gamepad state (Raylib's cross-platform API!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn void raylib_poll_gamepad(GameState *game_state) {
  // Check if gamepad is still connected
  if (game_state->gamepad_id < 0 ||
      !IsGamepadAvailable(game_state->gamepad_id)) {
    return;
  }

  int gamepad = game_state->gamepad_id;

  // ═══════════════════════════════════════════════════════════
  // 🎮 BUTTON EVENTS (Raylib maps to standard layout)
  // ═══════════════════════════════════════════════════════════
  // Raylib uses SDL2 button mapping (works across all controllers!)
  // - PlayStation: X=A, O=B, □=X, △=Y
  // - Xbox: A=A, B=B, X=X, Y=Y
  // - Nintendo: B=A, A=B, Y=X, X=Y

  // Face buttons
  game_state->controls.a_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
  game_state->controls.b_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
  game_state->controls.x_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
  game_state->controls.y_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP);

  // Shoulder buttons
  game_state->controls.left_shoulder =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
  game_state->controls.right_shoulder =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);

  // Menu buttons
  game_state->controls.back =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT); // Share/Back
  game_state->controls.start = IsGamepadButtonDown(
      gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT); // Options/Start

  // D-pad (works on ALL controllers!)
  game_state->controls.up =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP);
  game_state->controls.down =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
  game_state->controls.left =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
  game_state->controls.right =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);

  // ═══════════════════════════════════════════════════════════
  // 🎮 ANALOG STICKS (normalized -1.0 to +1.0)
  // ═══════════════════════════════════════════════════════════
  // Raylib automatically handles deadzones and normalization!

  game_state->controls.left_stick_x =
      GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
  game_state->controls.left_stick_y =
      GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);
  game_state->controls.right_stick_x =
      GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X);
  game_state->controls.right_stick_y =
      GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y);

  // Triggers (also normalized 0.0 to +1.0)
  game_state->controls.left_trigger =
      GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER);
  game_state->controls.right_trigger =
      GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER);

  // Debug output for button presses (only print on state change)
  if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
    printf("Button A pressed\n");
  }
  if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
    printf("Button Start pressed\n");
  }
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Handle controls (update game state based on input)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn void handle_controls(GameState *game_state) {

  // D-pad / Keyboard controls
  if (game_state->controls.up) {
    game_state->gradient.offset_y += game_state->speed;
  }
  if (game_state->controls.left) {
    game_state->gradient.offset_x += game_state->speed;
  }
  if (game_state->controls.down) {
    game_state->gradient.offset_y -= game_state->speed;
  }
  if (game_state->controls.right) {
    game_state->gradient.offset_x -= game_state->speed;
  }

  // ═══════════════════════════════════════════════════════════
  // 🎮 Analog stick controls (Casey's Day 6 pattern!)
  // ═══════════════════════════════════════════════════════════
  // Raylib gives us -1.0 to +1.0, Casey expects -32767 to +32767
  // So we convert: float * 32767 = int16 range
  // Then apply Casey's >> 12 bit shift

  if (game_state->gamepad_id >= 0) {
    // Convert Raylib's normalized floats to Casey's int16 range
    int16_t stick_x = (int16_t)(game_state->controls.left_stick_x * 32767.0f);
    int16_t stick_y = (int16_t)(game_state->controls.left_stick_y * 32767.0f);

    // Apply Casey's >> 12 math (divide by 4096)
    game_state->gradient.offset_x -= stick_x >> 12;
    game_state->gradient.offset_y -= stick_y >> 12;

    // ═══════════════════════════════════════════════════════════
    // Day 8: Use analog stick for frequency control
    // ═══════════════════════════════════════════════════════════
    // Matches your X11 backend exactly!
    if (stick_y >> 12 != 0) {
      handle_update_tone_frequency(stick_y >> 12);
    }
    if (stick_x >> 12 != 0) {
      handle_update_tone_frequency(stick_x >> 12);
    }

    // Start button resets
    if (game_state->controls.start) {
      game_state->gradient.offset_x = 0;
      game_state->gradient.offset_y = 0;
      printf("START pressed - reset offsets\n");
    }
  }
}

inline file_scoped_fn void
render_weird_gradient(OffscreenBuffer *buffer, GradientState *gradient_state) {
  int pitch = buffer->width * buffer->bytes_per_pixel;
  uint8_t *row = (uint8_t *)buffer->memory;

  for (int y = 0; y < buffer->height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < buffer->width; ++x) {
      uint8_t blue = (x + gradient_state->offset_x);
      uint8_t green = (y + gradient_state->offset_y);

      // *pixels++ = (0xFF000000 | (green << 8) | (blue << 16));
      // For RGBA format:
      uint8_t red = 0;
      uint8_t alpha = 255;
      *pixels++ = (alpha << 24) | (blue << 16) | (green << 8) | red;
    }
    row += pitch;
  }
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void resize_back_buffer(OffscreenBuffer *buffer, int width,
                                       int height) {
  printf("Resizing back buffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = buffer->width;
  int old_height = buffer->height;

  // Update first!
  buffer->width = width;
  buffer->height = height;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (buffer->memory && old_width > 0 && old_height > 0) {
    munmap(buffer->memory, old_width * old_height * buffer->bytes_per_pixel);
  }
  buffer->memory = NULL;

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (buffer->has_texture) {
    UnloadTexture(buffer->texture);
    buffer->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * buffer->bytes_per_pixel;
  buffer->memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (buffer->memory == MAP_FAILED) {
    buffer->memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // // memset(buffer->memory, 0, buffer_size); // Raylib does not auto-clear
  // like
  // // mmap
  // memset(buffer->memory, 0, buffer_size);

  buffer->width = width;
  buffer->height = height;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = buffer->memory,
               .width = buffer->width,
               .height = buffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};
  buffer->texture = LoadTextureFromImage(img);
  buffer->has_texture = true;
  printf("Raylib texture created successfully\n");
}

/********************************************************************
 UPDATE WINDOW (BLIT)
 Equivalent to XPutImage or StretchDIBits
*********************************************************************/
file_scoped_fn void update_window_from_backbuffer(OffscreenBuffer *buffer) {
  if (!buffer->has_texture || !buffer->memory)
    return;

  // Upload CPU → GPU
  UpdateTexture(buffer->texture, buffer->memory);

  // Draw GPU texture → screen
  DrawTexture(buffer->texture, 0, 0, WHITE);
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void ResizeBackBuffer(OffscreenBuffer *buffer, int width,
                                     int height) {
  printf("Resizing back buffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = buffer->width;
  int old_height = buffer->height;

  // Update first!
  buffer->width = width;
  buffer->height = height;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (buffer->memory && old_width > 0 && old_height > 0) {
    munmap(buffer->memory, old_width * old_height * buffer->bytes_per_pixel);
  }
  buffer->memory = NULL;

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (buffer->has_texture) {
    UnloadTexture(buffer->texture);
    buffer->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * buffer->bytes_per_pixel;
  buffer->memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (buffer->memory == MAP_FAILED) {
    buffer->memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // memset(buffer->memory, 0, buffer_size); // Raylib does not auto-clear like
  // mmap
  memset(buffer->memory, 0, buffer_size);

  buffer->width = width;
  buffer->height = height;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = buffer->memory,
               .width = buffer->width,
               .height = buffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};
  buffer->texture = LoadTextureFromImage(img);
  buffer->has_texture = true;
  printf("Raylib texture created successfully\n");
}

/**
 * MAIN FUNCTION
 *
 * Same purpose as X11 version, but MUCH simpler!
 */
int platform_main() {
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

  // Initialize game state
  g_game_state.controls = (GameControls){0};
  g_game_state.gradient = (GradientState){0};
  g_game_state.pixel = (PixelState){0};
  g_game_state.speed = 5;
  g_game_state.is_running = true;
  g_game_state.gamepad_id = -1; // No gamepad yet

  // ═══════════════════════════════════════════════════════════
  // 🎮 Initialize gamepad (cross-platform!)
  // ═══════════════════════════════════════════════════════════
  raylib_init_gamepad(&g_game_state);

  // ═══════════════════════════════════════════════════════════
  // 🔊 Initialize audio (Day 7-9)
  // ═══════════════════════════════════════════════════════════
  raylib_init_audio();

  // Initialize backbuffer
  g_backbuffer.memory = NULL;
  g_backbuffer.width = 1280;
  g_backbuffer.height = 720;
  g_backbuffer.bytes_per_pixel = 4;
  memset(&g_backbuffer.texture, 0, sizeof(g_backbuffer.texture));
  g_backbuffer.has_texture = false;

  resize_back_buffer(&g_backbuffer, g_backbuffer.width, g_backbuffer.height);

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
      printf("Window resized to: %dx%d\n", GetScreenWidth(), GetScreenHeight());
      ResizeBackBuffer(&g_backbuffer, GetScreenWidth(), GetScreenHeight());
    }

    // ═══════════════════════════════════════════════════════
    // ⌨️ KEYBOARD INPUT (cross-platform!)
    // ═══════════════════════════════════════════════════════

    g_game_state.controls.up = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    g_game_state.controls.left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    g_game_state.controls.down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    g_game_state.controls.right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);

    if (g_game_state.controls.up) {
      g_game_state.gradient.offset_y += g_game_state.speed;
    }
    if (g_game_state.controls.left) {
      g_game_state.gradient.offset_x += g_game_state.speed;
    }
    if (g_game_state.controls.down) {
      g_game_state.gradient.offset_y -= g_game_state.speed;
    }
    if (g_game_state.controls.right) {
      g_game_state.gradient.offset_x -= g_game_state.speed;
    }

    // Musical keyboard (Z-X-C-V-B-N-M-Comma)
    if (IsKeyPressed(KEY_Z))
      handle_musical_keypress(KEY_Z);
    if (IsKeyPressed(KEY_X))
      handle_musical_keypress(KEY_X);
    if (IsKeyPressed(KEY_C))
      handle_musical_keypress(KEY_C);
    if (IsKeyPressed(KEY_V))
      handle_musical_keypress(KEY_V);
    if (IsKeyPressed(KEY_B))
      handle_musical_keypress(KEY_B);
    if (IsKeyPressed(KEY_N))
      handle_musical_keypress(KEY_N);
    if (IsKeyPressed(KEY_M))
      handle_musical_keypress(KEY_M);
    if (IsKeyPressed(KEY_COMMA))
      handle_musical_keypress(KEY_COMMA);

    // Volume control ([ and ])
    if (IsKeyPressed(KEY_LEFT_BRACKET)) {
      if (IsKeyPressed(KEY_LEFT_SHIFT)) {
        handle_increase_volume(-500); // Shift+[ = volume down
      } else {
        handle_increase_pan(-10); // [ = pan left
      }
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
      if (IsKeyPressed(KEY_LEFT_SHIFT)) {
        handle_increase_volume(500); // Shift+] = volume up
      } else {
        handle_increase_pan(10); // ] = pan right
      }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      printf("ESCAPE pressed - exiting\n");
      g_game_state.is_running = false;
    }

    if (IsKeyPressed(KEY_F1)) {
      printf("F1 pressed - showing audio debug\n");
      raylib_debug_audio();
    }

    // ═══════════════════════════════════════════════════════
    // 🎮 GAMEPAD INPUT (cross-platform!)
    // ═══════════════════════════════════════════════════════
    raylib_poll_gamepad(&g_game_state);

    // ═══════════════════════════════════════════════════════
    // 🎮 Process all input
    // ═══════════════════════════════════════════════════════
    handle_controls(&g_game_state);

    // ═══════════════════════════════════════════════════════
    // 🎨 RENDER
    // ═══════════════════════════════════════════════════════
    if (g_backbuffer.memory) {
      // Render gradient
      render_weird_gradient(&g_backbuffer, &g_game_state.gradient);

      // Test pixel animation
      uint32_t *pixels = (uint32_t *)g_backbuffer.memory;
      int total_pixels = g_backbuffer.width * g_backbuffer.height;

      int test_offset = g_game_state.pixel.offset_y * g_backbuffer.width +
                        g_game_state.pixel.offset_x;

      if (test_offset < total_pixels) {
        pixels[test_offset] = 0xFF0000FF; // Red pixel
      }

      if (g_game_state.pixel.offset_x + 1 < g_backbuffer.width - 1) {
        g_game_state.pixel.offset_x += 1;
      } else {
        g_game_state.pixel.offset_x = 0;
        if (g_game_state.pixel.offset_y + 75 < g_backbuffer.height - 1) {
          g_game_state.pixel.offset_y += 75;
        } else {
          g_game_state.pixel.offset_y = 0;
        }
      }

      BeginDrawing();
      ClearBackground(BLACK);
      update_window_from_backbuffer(&g_backbuffer);
      EndDrawing();
    }

    clock_gettime(CLOCK_MONOTONIC, &g_frame_end);

    real64 ms_per_frame =
        (g_frame_end.tv_sec - g_frame_start.tv_sec) * 1000.0 +
        (g_frame_end.tv_nsec - g_frame_start.tv_nsec) * 1000000.0;
    real64 fps = 1000.0 / ms_per_frame;

    printf("%.2fms/f, %.2ff/s\n", ms_per_frame, fps);

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
  printf("Goodbye!\n");

  return 0;
}
