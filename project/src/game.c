#include "game.h"
#include "base/base.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>

// ═══════════════════════════════════════════════════════════════
// PLATFORM-SHARED STATE (declared extern in game.h)
// ═══════════════════════════════════════════════════════════════
// GameOffscreenBuffer g_backbuffer = {0};
// GameSoundOutput sound_output = {0};
bool is_game_running = true;

// CONFIGURATION (could be const)
int KEYBOARD_CONTROLLER_INDEX = 0;

file_scoped_global_var inline real32 apply_deadzone(real32 value) {
  if (fabsf(value) < CONTROLLER_DEADZONE) {
    return 0.0f;
  }
  return value;
}

file_scoped_global_var inline bool
controller_has_input(GameControllerInput *controller) {
  return (fabsf(controller->end_x) > CONTROLLER_DEADZONE ||
          fabsf(controller->end_y) > CONTROLLER_DEADZONE ||
          controller->up.ended_down || controller->down.ended_down ||
          controller->left.ended_down || controller->right.ended_down);
}

INIT_BACKBUFFER_STATUS init_backbuffer(GameOffscreenBuffer *buffer, int width,
                                       int height, int bytes_per_pixel,
                                       pixel_composer_fn composer) {
  buffer->memory.base = NULL;
  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = bytes_per_pixel;
  buffer->pitch = buffer->width * buffer->bytes_per_pixel;
  int initial_initial_buffer_size = buffer->pitch * buffer->height;
  PlatformMemoryBlock memory_block =
      platform_allocate_memory(NULL, initial_initial_buffer_size,
                               PLATFORM_MEMORY_READ | PLATFORM_MEMORY_WRITE);
  if (!memory_block.base) {
    fprintf(stderr,
            "platform_allocate_memory failed: could not allocate %d "
            "bytes\n",
            initial_initial_buffer_size);
    return INIT_BACKBUFFER_STATUS_MMAP_FAILED;
  }
  buffer->memory = memory_block;

  // Set pixel composer function for Raylib (R8G8B8A8)
  buffer->compose_pixel = composer;

  return INIT_BACKBUFFER_STATUS_SUCCESS;
}

void render_weird_gradient(GameOffscreenBuffer *buffer, GameState *game_state) {
  uint8_t *row = (uint8_t *)buffer->memory.base;

  // The following is correct for X11
  for (int y = 0; y < buffer->height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < buffer->width; ++x) {

      *pixels++ =
          buffer->compose_pixel(0, // Default red value (for both backends)
                                (y + game_state->gradient_state.offset_y), //
                                (x + game_state->gradient_state.offset_x),
                                255 // Full opacity for Raylib
          );
    }
    row += buffer->pitch;
  }
}

void testPixelAnimation(GameOffscreenBuffer *buffer, GameState *game_state,
                        int pixelColor) {
  // Test pixel animation
  uint32_t *pixels = (uint32_t *)buffer->memory.base;
  int total_pixels = buffer->width * buffer->height;

  int test_offset = game_state->pixel_state.offset_y * buffer->width +
                    game_state->pixel_state.offset_x;

  if (test_offset < total_pixels) {
    pixels[test_offset] = pixelColor;
  }

  if (game_state->pixel_state.offset_x + 1 < buffer->width - 1) {
    game_state->pixel_state.offset_x += 1;
  } else {
    game_state->pixel_state.offset_x = 0;
    if (game_state->pixel_state.offset_y + 75 < buffer->height - 1) {
      game_state->pixel_state.offset_y += 75;
    } else {
      game_state->pixel_state.offset_y = 0;
    }
  }
}

inline void process_game_button_state(bool is_down, GameButtonState *old_state,
                                      GameButtonState *new_state) {
  new_state->ended_down = is_down;
  if (old_state->ended_down != new_state->ended_down) {
    new_state->half_transition_count++;
  }
  // new_state->half_transition_count +=
  //     ((old_state->ended_down != new_state->ended_down && 1) || 0);
}

// Handle game controls
void handle_controls(GameControllerInput *input, GameSoundOutput *sound_output,
                     GameState *game_state) {
  if (input->is_analog) {
    // ═════════════════════════════════════════════════════════
    // ANALOG MOVEMENT (Joystick)
    // ═════════════════════════════════════════════════════════
    // Casey's Day 13 formula:
    //   BlueOffset += (int)(4.0f * Input0->EndX);
    //   ToneHz = 256 + (int)(128.0f * Input0->EndY);
    //
    // end_x/end_y are NORMALIZED (-1.0 to +1.0)!
    // ═════════════════════════════════════════════════════════

    real32 x = apply_deadzone(input->end_x);
    real32 y = apply_deadzone(input->end_y);

    // Horizontal stick controls blue offset
    game_state->gradient_state.offset_x -= (int)(4.0f * x);
    game_state->gradient_state.offset_y -= (int)(4.0f * y);

    // Vertical stick controls tone frequency
    // NOTE: `tone_hz` is moved to the game layer, but initilized by the
    // platform layer
    sound_output->tone_hz = 256 + (int)(128.0f * y);

  } else {
    // ═════════════════════════════════════════════════════════
    // DIGITAL MOVEMENT (Keyboard only)
    // ═════════════════════════════════════════════════════════
    // Option A: Use button states (discrete, snappy)
    if (input->up.ended_down) {
      game_state->gradient_state.offset_y += game_state->speed;
    }
    if (input->down.ended_down) {
      game_state->gradient_state.offset_y -= game_state->speed;
    }
    if (input->left.ended_down) {
      game_state->gradient_state.offset_x += game_state->speed;
    }
    if (input->right.ended_down) {
      game_state->gradient_state.offset_x -= game_state->speed;
    }

    // OR Option B: Use analog values (same formula as joystick)
    // game_state->gradient_state.offset_x += (int)(4.0f * controller->end_x);
    // game_state->gradient_state.offset_y += (int)(4.0f * controller->end_y);

    // Pick ONE, not both!

    // game_state->gradient_state.offset_x += (int)(4.0f * controller->end_x);
    // game_state->gradient_state.offset_y += (int)(4.0f * controller->end_y);
    // sound_output->tone_hz = 256 + (int)(128.0f * controller->end_y);
  }

  // Clamp tone
  if (sound_output->tone_hz < 20)
    sound_output->tone_hz = 20;
  if (sound_output->tone_hz > 2000)
    sound_output->tone_hz = 2000;

  // Update wave_period when tone_hz changes (prevent audio clicking)
  sound_output->wave_period =
      sound_output->samples_per_second / sound_output->tone_hz;
}

void game_update_and_render(GameMemory *memory, GameInput *input,
                            GameOffscreenBuffer *buffer,
                            GameSoundOutput *sound_buffer) {
  GameState *game_state = (GameState *)memory->permanent_storage.base;

  if (!memory->is_initialized) { // false (skip!)
                                 // Never runs again!
                                 // First-time initialization
    game_state->gradient_state = (GradientState){0};
    game_state->pixel_state = (PixelState){0};
    game_state->speed = 5;
    memory->is_initialized = true;
    return;
  }

  static int frame = 0;
  frame++;

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick input (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    GameControllerInput *c = &input->controllers[i];
    if (c->is_connected) {
      // Use helper function instead of hardcoded 0.15!
      if (controller_has_input(c)) {
        active_controller = c;
        break;
      }
    }
  }

  // Priority 2: Check keyboard if no joystick input
  if (!active_controller) {
    GameControllerInput *keyboard =
        &input->controllers[KEYBOARD_CONTROLLER_INDEX];
    bool has_input = (keyboard->up.ended_down || keyboard->down.ended_down ||
                      keyboard->left.ended_down || keyboard->right.ended_down);
    if (has_input) {
      active_controller = keyboard;
    }
  }

  // Fallback: Always use keyboard (controller[0]) if nothing else
  if (!active_controller) {
    active_controller = &input->controllers[KEYBOARD_CONTROLLER_INDEX];
  }

  if (frame % 60 == 0) {
    printf("Frame %d: active_controller=%p\n", frame,
           (void *)active_controller);
    if (active_controller) {
      printf("  is_analog=%d end_x=%.2f end_y=%.2f\n",
             active_controller->is_analog, active_controller->end_x,
             active_controller->end_y);
      printf("  up=%d down=%d left=%d right=%d\n",
             active_controller->up.ended_down,
             active_controller->down.ended_down,
             active_controller->left.ended_down,
             active_controller->right.ended_down);
      printf("Frame %d: is_analog=%d end_x=%.2f end_y=%.2f "
             "up=%d down=%d left=%d right=%d\n",
             frame, active_controller->is_analog, active_controller->end_x,
             active_controller->end_y, active_controller->up.ended_down,
             active_controller->down.ended_down,
             active_controller->left.ended_down,
             active_controller->right.ended_down);
    }
  }

  handle_controls(active_controller, sound_buffer, game_state);

  render_weird_gradient(buffer, game_state);

  int testPixelAnimationColor = buffer->compose_pixel(255, 0, 0, 255);
  testPixelAnimation(buffer, game_state, testPixelAnimationColor);
}

// #ifdef PLATFORM_X11
// // On X11, define colors in BGRA directly
// #define GAME_RED 0x0000FFFF
// #else
// // On Raylib/other, use RGBA
// #define GAME_RED 0xFF0000FF
// #endif