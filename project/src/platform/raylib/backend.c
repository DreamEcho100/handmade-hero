#define _POSIX_C_SOURCE 199309L

#include "backend.h"
#include "../../base.h"
#include "../../game.h"
#include "audio.h"
#include <assert.h>

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
  int gamepad_id;        // Which Raylib gamepad ID (0-3)
  char device_name[128]; // For debugging
} RaylibJoystickState;

file_scoped_global_var RaylibJoystickState g_joysticks[MAX_JOYSTICK_COUNT] = {
    0};

typedef struct {
  Texture2D texture;
  bool has_texture;
} OffscreenBufferMeta;

file_scoped_global_var OffscreenBufferMeta game_buffer_meta = {0};
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

// ═══════════════════════════════════════════════════════════════
// 🎮 Initialize gamepad (Raylib cross-platform!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn void
raylib_init_gamepad(GameControllerInput *controller_old_input,
                    GameControllerInput *controller_new_input) {

  // Initialize ALL controllers FIRST
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    controller_old_input[i].controller_index = i;
    controller_old_input[i].is_connected = false;

    controller_new_input[i].controller_index = i;
    controller_new_input[i].is_connected = false;

    int g_joysticks_index = i - 1; // Adjust for keyboard at index 0
    if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
      g_joysticks[g_joysticks_index].gamepad_id = -1;
      memset(g_joysticks[g_joysticks_index].device_name, 0,
             sizeof(g_joysticks[g_joysticks_index].device_name));
    }
  }

  // Mark keyboard (slot 0) as connected AFTER the loop
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  printf("Searching for gamepad...\n");

  // Try gamepads 0-3 (Raylib supports up to 4 controllers)
  for (int i = MAX_KEYBOARD_COUNT; i < MAX_JOYSTICK_COUNT; i++) {

    if (IsGamepadAvailable(i - MAX_KEYBOARD_COUNT)) {
      const char *name = GetGamepadName(i);
      printf("✅ Gamepad %d connected: %s\n", i, name);

      int raylib_gamepad_id = i - MAX_KEYBOARD_COUNT;

      // Store controller index AND file descriptor separately
      controller_old_input[i].controller_index = i;
      controller_old_input[i].is_connected = true;
      controller_old_input[i].is_analog = true; // Joysticks are analog

      controller_new_input[i].controller_index = i;
      controller_new_input[i].is_connected = true;
      controller_new_input[i].is_analog = true; // Joysticks are analog

      int g_joysticks_index =
          i - MAX_KEYBOARD_COUNT; // Adjust for keyboard at index 0

      if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
        g_joysticks[g_joysticks_index].gamepad_id = raylib_gamepad_id;
        strncpy(g_joysticks[g_joysticks_index].device_name, name,
                sizeof(g_joysticks[g_joysticks_index].device_name) - 1);
      }
    }
  }
}

inline file_scoped_fn void handle_keyboard_inputs(GameSoundOutput *sound_output,
                                                  GameInput *old_game_input,
                                                  GameInput *new_game_input) {

  GameControllerInput *old_controller1 =
      &old_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];
  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  int upDown = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
  if (upDown) {
    // W/Up = Move up = positive Y
    new_controller1->end_y = +1.0f;
    new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
    new_controller1->is_analog = false;

    // Also set button state for backward compatibility
    process_game_button_state(true, &old_controller1->up, &new_controller1->up);
  }
  int upReleased = IsKeyReleased(KEY_W) || IsKeyReleased(KEY_UP);
  if (upReleased) {
    new_controller1->end_y = 0.0f;
    new_controller1->min_y = new_controller1->max_y = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &old_controller1->up,
                              &new_controller1->up);
  }

  int left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
  if (left) {
    // A/Left = Move left = negative X
    new_controller1->end_x = -1.0f;
    new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
    new_controller1->is_analog = false;

    process_game_button_state(true, &old_controller1->left,
                              &new_controller1->left);
  }
  int leftReleased = IsKeyReleased(KEY_A) || IsKeyReleased(KEY_LEFT);
  if (leftReleased) {
    new_controller1->end_x = 0.0f;
    new_controller1->min_x = new_controller1->max_x = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &old_controller1->left,
                              &new_controller1->left);
  }

  int down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
  if (down) {
    // S/Down = Move down = negative Y
    new_controller1->end_y = -1.0f;
    new_controller1->min_y = new_controller1->max_y = new_controller1->end_y;
    new_controller1->is_analog = false;

    process_game_button_state(true, &old_controller1->down,
                              &new_controller1->down);
  }
  int downReleased = IsKeyReleased(KEY_S) || IsKeyReleased(KEY_DOWN);
  if (downReleased) {
    new_controller1->end_y = 0.0f;
    new_controller1->min_y = new_controller1->max_y = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &old_controller1->down,
                              &new_controller1->down);
  }

  int right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
  if (right) {
    // D/Right = Move right = positive X
    new_controller1->end_x = +1.0f;
    new_controller1->min_x = new_controller1->max_x = new_controller1->end_x;
    new_controller1->is_analog = false;

    process_game_button_state(true, &old_controller1->right,
                              &new_controller1->right);
  }
  int rightReleased = IsKeyReleased(KEY_D) || IsKeyReleased(KEY_RIGHT);
  if (rightReleased) {
    new_controller1->end_x = 0.0f;
    new_controller1->min_x = new_controller1->max_x = 0.0f;
    new_controller1->is_analog = false;
    process_game_button_state(false, &old_controller1->right,
                              &new_controller1->right);
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    printf("ESCAPE pressed - exiting\n");
    is_game_running = false;
  }

  if (IsKeyPressed(KEY_F1)) {
    printf("F1 pressed - showing audio debug\n");
    raylib_debug_audio(sound_output);
  }
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Poll gamepad state (Raylib's cross-platform API!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn void raylib_poll_gamepad(GameInput *old_input,
                                        GameInput *new_input) {

  for (int controller_index = MAX_KEYBOARD_COUNT;
       controller_index < MAX_CONTROLLER_COUNT; controller_index++) {

    // Get controller state
    GameControllerInput *old_controller =
        &old_input->controllers[controller_index];
    GameControllerInput *new_controller =
        &new_input->controllers[controller_index];

    int g_joysticks_index = controller_index - MAX_KEYBOARD_COUNT;
    if (g_joysticks_index < 0 || g_joysticks_index >= MAX_JOYSTICK_COUNT) {
      // Skip keyboard (index 0)
      continue;
    }
    RaylibJoystickState *joystick_state = &g_joysticks[g_joysticks_index];

    // Check if gamepad is still connected
    if (joystick_state->gamepad_id < 0 ||
        !IsGamepadAvailable(joystick_state->gamepad_id)) {
      continue;
    }

    int gamepad = joystick_state->gamepad_id;

    // ═══════════════════════════════════════════════════════════
    // 🎮 BUTTON EVENTS (Raylib maps to standard layout)
    // ═══════════════════════════════════════════════════════════
    // Raylib uses SDL2 button mapping (works across all controllers!)
    // - PlayStation: X=A, O=B, □=X, △=Y
    // - Xbox: A=A, B=B, X=X, Y=Y
    // - Nintendo: B=A, A=B, Y=X, X=Y

    // // Face buttons
    // game_state->controls.a_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    // game_state->controls.b_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    // game_state->controls.x_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    // game_state->controls.y_button =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP);

    // // Shoulder buttons
    // game_state->controls.left_shoulder =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    // game_state->controls.right_shoulder =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);

    // // Menu buttons
    // game_state->controls.back =
    //     IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT); //
    //     Share/Back
    // game_state->controls.start = IsGamepadButtonDown(
    //     gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT); // Options/Start

    // ═══════════════════════════════════════════════════════════
    // 🎮 D-PAD (works on ALL controllers!)
    // ═══════════════════════════════════════════════════════════
    // Raylib treats D-pad as buttons (digital), not axes (analog).
    // We must convert button states to analog values for game layer.
    // ═══════════════════════════════════════════════════════════

    // ✅ Update start position BEFORE reading buttons
    new_controller->start_x = old_controller->end_x;
    new_controller->start_y = old_controller->end_y;

    // ─────────────────────────────────────────────────────────
    // D-PAD UP/DOWN (Y-axis)
    // ─────────────────────────────────────────────────────────
    bool dpad_up = IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP);
    bool dpad_down =
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);

    if (dpad_up && !dpad_down) {
      // Only UP pressed
      new_controller->end_y = +1.0f;
      process_game_button_state(true, &old_controller->up, &new_controller->up);
      process_game_button_state(false, &old_controller->down,
                                &new_controller->down);

    } else if (dpad_down && !dpad_up) {
      // Only DOWN pressed
      new_controller->end_y = -1.0f;
      process_game_button_state(true, &old_controller->down,
                                &new_controller->down);
      process_game_button_state(false, &old_controller->up,
                                &new_controller->up);

    } else if (dpad_up && dpad_down) {
      // Both pressed (cancel out)
      new_controller->end_y = 0.0f;
      process_game_button_state(true, &old_controller->up, &new_controller->up);
      process_game_button_state(true, &old_controller->down,
                                &new_controller->down);

    } else {
      // Neither pressed
      new_controller->end_y = 0.0f;
      process_game_button_state(false, &old_controller->up,
                                &new_controller->up);
      process_game_button_state(false, &old_controller->down,
                                &new_controller->down);
    }

    // ─────────────────────────────────────────────────────────
    // D-PAD LEFT/RIGHT (X-axis)
    // ─────────────────────────────────────────────────────────
    bool dpad_left =
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bool dpad_right =
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);

    if (dpad_left && !dpad_right) {
      // Only LEFT pressed
      new_controller->end_x = -1.0f;
      process_game_button_state(true, &old_controller->left,
                                &new_controller->left);
      process_game_button_state(false, &old_controller->right,
                                &new_controller->right);

    } else if (dpad_right && !dpad_left) {
      // Only RIGHT pressed
      new_controller->end_x = +1.0f;
      process_game_button_state(true, &old_controller->right,
                                &new_controller->right);
      process_game_button_state(false, &old_controller->left,
                                &new_controller->left);

    } else if (dpad_left && dpad_right) {
      // Both pressed (cancel out)
      new_controller->end_x = 0.0f;
      process_game_button_state(true, &old_controller->left,
                                &new_controller->left);
      process_game_button_state(true, &old_controller->right,
                                &new_controller->right);

    } else {
      // Neither pressed
      new_controller->end_x = 0.0f;
      process_game_button_state(false, &old_controller->left,
                                &new_controller->left);
      process_game_button_state(false, &old_controller->right,
                                &new_controller->right);
    }

    // Update min/max (Day 13 pattern)
    new_controller->min_x = new_controller->max_x = new_controller->end_x;
    new_controller->min_y = new_controller->max_y = new_controller->end_y;

    // ═══════════════════════════════════════════════════════════
    // 🎮 ANALOG STICKS (normalized -1.0 to +1.0)
    // ═══════════════════════════════════════════════════════════
    // Platform layer reports RAW values (no filtering!)
    //
    // Raylib automatically:
    // - Normalizes to -1.0 to +1.0 range
    // - Applies small internal deadzone (~0.05)
    // - Handles platform differences (Windows/Linux/Mac)
    //
    // We just pass the values through!
    // ═══════════════════════════════════════════════════════════

    real32 left_stick_x = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
    real32 left_stick_y = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);

    // Must set is_analog EVERY FRAME because:
    // 1. Raylib always returns a value (even 0.0)
    // 2. prepare_input_frame() preserves old value
    // 3. We need to mark "this frame used joystick, not keyboard"
    new_controller->is_analog = true;

    // Update start position (for movement tracking)
    new_controller->start_x = old_controller->end_x;
    new_controller->start_y = old_controller->end_y;

    // Store RAW values (game layer will apply deadzone!)
    new_controller->end_x = left_stick_x;
    new_controller->end_y = left_stick_y;

    // Day 13: Just set min/max to current value
    // Day 14+: These will track actual min/max during frame
    new_controller->min_x = left_stick_x;
    new_controller->max_x = left_stick_x;
    new_controller->min_y = left_stick_y;
    new_controller->max_y = left_stick_y;

    // game_state->controls.right_stick_x =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X) *
    //               32767.0f);
    // game_state->controls.right_stick_y =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y) *
    //               32767.0f);

    // // Triggers (also normalized 0.0 to +1.0)
    // game_state->controls.left_trigger =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER)
    //     *
    //               32767.0f);
    // game_state->controls.right_trigger =
    //     (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER)
    //     *
    //               32767.0f);

    // Debug output for button presses (only print on state change)
    if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
      printf("Button A pressed\n");
    }
    if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
      printf("Button Start pressed\n");
    }
  }
}

// Raylib pixel composer (R8G8B8A8 format)
file_scoped_fn uint32_t compose_pixel_rgba(uint8_t r, uint8_t g, uint8_t b,
                                           uint8_t a) {
  return (a << 24) | (b << 16) | (g << 8) | r;
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

void prepare_input_frame(GameInput *old_input, GameInput *new_input) {
  // ═══════════════════════════════════════════════════════════
  // Clear new input buttons to released state
  // ═══════════════════════════════════════════════════════════
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *old_ctrl = &old_input->controllers[i];
    GameControllerInput *new_ctrl = &new_input->controllers[i];

    // Preserve connection state
    new_ctrl->is_connected = old_ctrl->is_connected;
    new_ctrl->is_analog = old_ctrl->is_analog;

    // Set start position = last frame's end position
    new_ctrl->start_x = old_ctrl->end_x;
    new_ctrl->start_y = old_ctrl->end_y;

    // Preserve analog values (for joystick held positions)
    new_ctrl->end_x = old_ctrl->end_x;
    new_ctrl->end_y = old_ctrl->end_y;
    new_ctrl->min_x = new_ctrl->max_x = new_ctrl->end_x;
    new_ctrl->min_y = new_ctrl->max_y = new_ctrl->end_y;

    // Buttons - preserve state, clear transition count
    for (int btn = 0; btn < MAX_CONTROLLER_COUNT; btn++) {
      new_ctrl->buttons[btn].ended_down = old_ctrl->buttons[btn].ended_down;
      new_ctrl->buttons[btn].half_transition_count = 0;
    }
  }
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
  uint64_t transient_storage_size = GIGABYTES(4);
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
        printf("\n🎮 Controller States:\n");
        for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
          GameControllerInput *c = &old_game_input->controllers[i];
          // LinuxJoystickState *joystick_state = NULL;
          // if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
          //   joystick_state = &g_joysticks[i - 1];
          // }
          RaylibJoystickState *joystick_state = NULL;
          if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
            joystick_state = &g_joysticks[i - 1];
          }
          printf("  [%d] connected=%d analog=%d gamepad_id=%d end_x=%.2f "
                 "end_y=%.2f\n",
                 i, c->is_connected, c->is_analog,
                 joystick_state ? joystick_state->gamepad_id : -1, c->end_x,
                 c->end_y);
        }
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
      handle_keyboard_inputs(&game_sound_output, old_game_input,
                             new_game_input);

      // ═══════════════════════════════════════════════════════
      // 🎮 GAMEPAD INPUT (cross-platform!)
      // ═══════════════════════════════════════════════════════
      raylib_poll_gamepad(old_game_input, new_game_input);

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
  RED;

  return 0;
}