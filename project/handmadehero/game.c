#include "game.h"
#include "../engine/_common/base.h"
#include "../engine/game/backbuffer.h"
#include "../engine/game/input.h"
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if HANDMADE_INTERNAL
#include "../engine/_common/debug-file-io.h"
#endif

file_scoped_global_var inline real32 apply_deadzone(real32 value) {
  if (fabsf(value) < CONTROLLER_DEADZONE) {
    return 0.0f;
  }
  return value;
}

file_scoped_global_var inline bool
controller_has_input(GameControllerInput *controller) {
  return (fabsf(controller->stick_avg_x) > CONTROLLER_DEADZONE ||
          fabsf(controller->stick_avg_y) > CONTROLLER_DEADZONE ||
          controller->move_up.ended_down || controller->move_down.ended_down ||
          controller->move_left.ended_down ||
          controller->move_right.ended_down);
}

void render_weird_gradient(GameOffscreenBuffer *buffer, GameState *game_state) {
  uint8_t *row = (uint8_t *)buffer->memory.base;

  // The following is correct for X11
  for (int y = 0; y < buffer->height; ++y) {
    uint32_t *pixel = (uint32_t *)row;
    int offset_x = game_state->gradient_state.offset_x;
    int offset_y = game_state->gradient_state.offset_y;

    for (int x = 0; x < buffer->width; ++x) {
      uint8_t blue = (uint8_t)(x + offset_x);
      uint8_t green = (uint8_t)(y + offset_y);

      // RGBA format - works for BOTH OpenGL X11 AND Raylib!
      *pixel++ = (0xFF000000u |  // Alpha = 255
                  (blue << 16) | // Blue
                  (green << 8) | // Green
                  0);
    }
    row += buffer->pitch;
  }
}

void testPixelAnimation(GameOffscreenBuffer *buffer, GameState *game_state) {
  // Test pixel animation
  uint32_t *pixels = (uint32_t *)buffer->memory.base;
  int total_pixels = buffer->width * buffer->height;

  int test_offset = game_state->pixel_state.offset_y * buffer->width +
                    game_state->pixel_state.offset_x;

  if (test_offset < total_pixels) {
    pixels[test_offset] = 0xFF0000FF;
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
    // stick_avg_x/stick_avg_y are NORMALIZED (-1.0 to +1.0)!
    // ═════════════════════════════════════════════════════════

    real32 x = apply_deadzone(input->stick_avg_x);
    real32 y = apply_deadzone(input->stick_avg_y);

    // Horizontal stick controls blue offset
    game_state->gradient_state.offset_x -= (int)(4.0f * x);
    game_state->gradient_state.offset_y -= (int)(4.0f * y);

    // Vertical stick controls tone frequency
    // NOTE: `tone_hz` is moved to the game layer, but initilized by the
    // platform layer
    sound_output->tone_hz = 256 + (int)(128.0f * y);
  } else {
    if (input->move_up.ended_down) {
      game_state->gradient_state.offset_y += game_state->speed;
    }
    if (input->move_down.ended_down) {
      game_state->gradient_state.offset_y -= game_state->speed;
    }
    if (input->move_left.ended_down) {
      game_state->gradient_state.offset_x += game_state->speed;
    }
    if (input->move_right.ended_down) {
      game_state->gradient_state.offset_x -= game_state->speed;
    }
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

#if HANDMADE_INTERNAL
    char *Filename = __FILE__;

    DebugReadFileResult file = debug_platform_read_entire_file(Filename);
    if (file.contents.base) {
      debug_platform_write_entire_file("out/test.out", file.size,
                                       file.contents.base);
      debug_platform_free_file_memory(&file.contents);
      printf("Wrote test.out\n");
    }
#endif

    game_state->gradient_state = (GradientState){0};
    game_state->pixel_state = (PixelState){0};
    game_state->speed = 5;
    memory->is_initialized = true;
    return;
  }

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick input (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {

    ASSERT((&input->controllers[i].terminator -
            &input->controllers[i].buttons[i]) ==
           (ArraySize(input->controllers[i].buttons)));

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

  // Fallback: Always use keyboard (controller[0]) if nothing else
  if (!active_controller) {
    active_controller = &input->controllers[KEYBOARD_CONTROLLER_INDEX];
  }

#if HANDMADE_INTERNAL
  // Show stats every 60 frames
  static int debug_counter = 0;
  if (++debug_counter % 300 == 5) {
    printf("Debug Counter %d: active_controller=%p\n", debug_counter,
           (void *)active_controller);
    if (active_controller) {
      printf("  is_analog=%d stick_avg_x=%.2f stick_avg_y=%.2f\n",
             active_controller->is_analog, active_controller->stick_avg_x,
             active_controller->stick_avg_y);
      printf("  up=%d down=%d left=%d right=%d\n",
             active_controller->move_up.ended_down,
             active_controller->move_down.ended_down,
             active_controller->move_left.ended_down,
             active_controller->move_right.ended_down);
      printf("Debug Counter %d: is_analog=%d stick_avg_x=%.2f stick_avg_y=%.2f "
             "up=%d down=%d left=%d right=%d\n",
             debug_counter, active_controller->is_analog,
             active_controller->stick_avg_x, active_controller->stick_avg_y,
             active_controller->move_up.ended_down,
             active_controller->move_down.ended_down,
             active_controller->move_left.ended_down,
             active_controller->move_right.ended_down);
    }
  }
#endif

  handle_controls(active_controller, sound_buffer, game_state);

  render_weird_gradient(buffer, game_state);

  testPixelAnimation(buffer, game_state);
}
