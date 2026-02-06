#include "main.h"
#include "../../engine/_common/base.h"
#include "../../engine/game/backbuffer.h"
#include "../../engine/game/inputs.h"
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

de100_file_scoped_global_var inline real32 apply_deadzone(real32 value) {
  if (fabsf(value) < CONTROLLER_DEADZONE) {
    return 0.0f;
  }
  return value;
}
/**
 * Wraps a Y coordinate to stay within [0, height).
 *
 * Handles both positive overflow (y >= height) and negative values (y < 0).
 *
 * Examples (assuming height = 720):
 *   wrap(5)    -> 5      (already valid)
 *   wrap(720)  -> 0      (wraps to top)
 *   wrap(725)  -> 5      (wraps around)
 *   wrap(-1)   -> 719    (wraps to bottom)
 *   wrap(-5)   -> 715    (wraps around)
 *
 * Formula breakdown for y = -5, height = 720:
 *   Step 1: y % height        = -5 % 720  = -5   (C allows negative modulo)
 *   Step 2: + height          = -5 + 720  = 715  (shift to positive range)
 *   Step 3: % height          = 715 % 720 = 715  (ensure < height)
 *
 * @param y           The Y coordinate (can be negative or >= height)
 * @param backbuffer  Buffer containing height dimension
 * @return            Wrapped Y in range [0, height)
 */
de100_file_scoped_global_var inline int32
player_wrap_y(int32 y, GameBackBuffer *backbuffer) {
  return ((y % backbuffer->height) + backbuffer->height) % backbuffer->height;
}

de100_file_scoped_global_var inline int32
player_wrap_x(int32 x, GameBackBuffer *backbuffer) {
  return ((x % backbuffer->width) + backbuffer->width) % backbuffer->width;
}

// render_player(GameBackBuffer *backbuffer, int32 *player_x, int32 *player_y)
de100_file_scoped_global_var inline void
render_player(GameBackBuffer *backbuffer, int32 player_x, int32 player_y) {
  // uint8 *end_of_buffer = (uint8 *)(backbuffer->memory.base) +
  //                        (backbuffer->pitch * backbuffer->height);
  uint32 color = 0xFFFFFFFF;

  local_persist_var int32 player_height = 10;
  local_persist_var int32 player_width = 10;
  // Wrap Y position
  int32 wrapped_top = player_wrap_y(player_y, backbuffer);
  int32 wrapped_bottom = player_wrap_y(player_y + player_height, backbuffer);

  for (int32 x = player_x; x < player_x + player_width; ++x) {
    int32 wrapped_x = player_wrap_x(x, backbuffer);

    for (int32 y = wrapped_top; y < wrapped_bottom; ++y) {
      int32 wrapped_y = player_wrap_y(y, backbuffer);
      uint8 *pixel_pos = (
          // Initial pos to start from
          (uint8 *)backbuffer->memory.base
          // offset to the row/x postion
          + wrapped_x * backbuffer->bytes_per_pixel
          // offset to the column/y postion
          + wrapped_y * backbuffer->pitch);

      // NOTE: if we don't cast it to `uint32 *` it will only use the first
      // byte of the color which will lead to incorrect colors!
      *(uint32 *)pixel_pos = color;
    }
  }
}

void render_weird_gradient(GameBackBuffer *backbuffer,
                           HandMadeHeroGameState *game_state) {
  uint8 *row = (uint8 *)backbuffer->memory.base;

  // The following is correct for X11
  for (int y = 0; y < backbuffer->height; ++y) {
    int offset_x = game_state->gradient_state.offset_x;
    uint32 *pixel = (uint32 *)row;
    int offset_y = game_state->gradient_state.offset_y;

    for (int x = 0; x < backbuffer->width; ++x) {
      uint8 blue = (uint8)(x + offset_x);
      uint8 green = (uint8)(y + offset_y);

      // RGBA format - works for BOTH OpenGL X11 AND Raylib!
      *pixel++ = (0xFF000000u |  // Alpha = 255
                  (blue << 16) | // Blue
                  (green << 8) | // Green
                  (0));
    }
    row += backbuffer->pitch;
  }
}

void testPixelAnimation(GameBackBuffer *backbuffer,
                        HandMadeHeroGameState *game_state) {
  // Test pixel animation
  uint32 *pixels = (uint32 *)backbuffer->memory.base;
  int total_pixels = backbuffer->width * backbuffer->height;

  int test_offset = game_state->pixel_state.offset_y * backbuffer->width +
                    game_state->pixel_state.offset_x;

  if (test_offset < total_pixels) {
    pixels[test_offset] = 0xFF0000FF;
  }

  if (game_state->pixel_state.offset_x + 1 < backbuffer->width - 1) {
    game_state->pixel_state.offset_x += 1;
  } else {
    game_state->pixel_state.offset_x = 0;
    if (game_state->pixel_state.offset_y + 75 < backbuffer->height - 1) {
      game_state->pixel_state.offset_y += 75;
    } else {
      game_state->pixel_state.offset_y = 0;
    }
  }
}

// Handle game controls
void handle_controls(GameControllerInput *inputs,
                     HandMadeHeroGameState *game_state) {
  if (inputs->action_down.ended_down && game_state->player_state.t_jump <= 0) {
    game_state->player_state.t_jump = 4.0; // Start jump
  }

  // Simple jump arc
  int jump_offset = 0;
  if (game_state->player_state.t_jump > 0) {
    jump_offset =
        (int)(5.0f * sinf(0.5f * M_PI * game_state->player_state.t_jump));
    game_state->player_state.y += jump_offset;
  }
  game_state->player_state.t_jump -= 0.033f; // Tick down jump timer

  if (inputs->is_analog) {
    // ═════════════════════════════════════════════════════════
    // ANALOG MOVEMENT (Joystick)
    // ═════════════════════════════════════════════════════════
    // Casey's Day 13 formula:
    //   BlueOffset += (int)(4.0f * Input0->EndX);
    //   ToneHz = 256 + (int)(128.0f * Input0->EndY);
    //
    // stick_avg_x/stick_avg_y are NORMALIZED (-1.0 to +1.0)!
    // ═════════════════════════════════════════════════════════

    real32 stick_avg_x = apply_deadzone(inputs->stick_avg_x);
    real32 stick_avg_y = apply_deadzone(inputs->stick_avg_y);

    // Horizontal stick controls blue offset
    game_state->gradient_state.offset_x -= (int)(4.0f * stick_avg_x);
    game_state->gradient_state.offset_y -= (int)(4.0f * stick_avg_y);

    // Vertical stick controls tone frequency
    // NOTE: `tone_hz` is moved to the game layer, but initilized by the
    // platform layer
    game_state->audio.tone.frequency = 256 + (int)(128.0f * stick_avg_y);

    // In GameUpdateAndRender:
    game_state->player_state.x += (int)(4.0f * stick_avg_x * game_state->speed);
    game_state->player_state.y += (int)(4.0f * stick_avg_y * game_state->speed);

  } else {
    if (inputs->move_up.ended_down) {
      game_state->gradient_state.offset_y += game_state->speed;
      game_state->player_state.y -= game_state->speed;
    }
    if (inputs->move_down.ended_down) {
      game_state->gradient_state.offset_y -= game_state->speed;
      game_state->player_state.y += game_state->speed;
    }
    if (inputs->move_left.ended_down) {
      game_state->gradient_state.offset_x += game_state->speed;
      game_state->player_state.x -= game_state->speed;
    }
    if (inputs->move_right.ended_down) {
      game_state->gradient_state.offset_x -= game_state->speed;
      game_state->player_state.x += game_state->speed;
    }
  }

  // Clamp tone
  if (game_state->audio.tone.frequency < 20)
    game_state->audio.tone.frequency = 20;
  if (game_state->audio.tone.frequency > 2000)
    game_state->audio.tone.frequency = 2000;

  // // Update wave_period when tone_hz changes (prevent audio clicking)
  // audio_output->wave_period =
  //     audio_output->samples_per_second / game_state->audio.tone.frequency;
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {
  (void)thread_context;

  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage;

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick inputs (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {

    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    GameControllerInput *c = &inputs->controllers[i];
    if (c->is_connected) {
      active_controller = c;
      break;
    }
  }

  // Fallback: Always use keyboard (controller[0]) if nothing else
  if (!active_controller) {
    active_controller = &inputs->controllers[KEYBOARD_CONTROLLER_INDEX];
  }

#if DE100_INTERNAL
  // Show stats every 60 frames
  if (FRAME_LOG_EVERY_FIVE_SECONDS_CHECK) {
    printf("Debug Counter %d: active_controller=%p\n", g_frame_counter,
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
             g_frame_counter, active_controller->is_analog,
             active_controller->stick_avg_x, active_controller->stick_avg_y,
             active_controller->move_up.ended_down,
             active_controller->move_down.ended_down,
             active_controller->move_left.ended_down,
             active_controller->move_right.ended_down);
    }
  }
#endif

  handle_controls(active_controller, game_state);

  render_weird_gradient(buffer, game_state);
  render_player(buffer, game_state->player_state.x, game_state->player_state.y);
  // Render indicators for pressed mouse buttons
  for (int button_index = 0; button_index < 5; ++button_index) {
    if (inputs->mouse_buttons[button_index].ended_down) {
      render_player(buffer,
                    // (10 + 20 * button_index), 0
                    inputs->mouse_x, inputs->mouse_y);
    }
  }

  testPixelAnimation(buffer, game_state);
}

// ═══════════════════════════════════════════════════════════════════════════
// GAME GET SOUND SAMPLES
// ═══════════════════════════════════════════════════════════════════════════
// This is the ONLY place audio samples are generated!
// Platform tells us how many samples to generate, we fill the buffer.
//
// KEY REQUIREMENTS:
// 1. Phase must be CONTINUOUS across calls (stored in game_state)
// 2. Volume changes should be gradual (future: implement ramping)
// 3. Frequency changes should be gradual (future: implement smoothing)
// ═══════════════════════════════════════════════════════════════════════════

GAME_GET_AUDIO_SAMPLES(game_get_audio_samples) {
  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage;

  if (!memory->is_initialized) {
    int16 *sample_out = (int16 *)audio_buffer->samples;
    for (int i = 0; i < audio_buffer->sample_count; ++i) {
      *sample_out++ = 0;
      *sample_out++ = 0;
    }
    return;
  }

  // ═══════════════════════════════════════════════════════════════
  // DEBUG: Log audio state periodically
  // ═══════════════════════════════════════════════════════════════
#if DE100_INTERNAL
  if (FRAME_LOG_EVERY_THREE_SECONDS_CHECK) { // Every ~3 seconds at 30Hz
    SoundSource *tone = &game_state->audio.tone;
    printf("[AUDIO DEBUG] is_playing=%d, freq=%.1f, vol=%.2f, phase=%.2f, "
           "samples=%d\n",
           tone->is_playing, tone->frequency, tone->volume, tone->phase,
           audio_buffer->sample_count);
  }
#endif

  // Get audio state
  SoundSource *tone = &game_state->audio.tone;
  real32 master_volume = game_state->audio.master_volume;

  // Safety check: if not initialized, output silence
  if (!tone->is_playing || tone->frequency <= 0.0f) {
    int16 *sample_out = (int16 *)audio_buffer->samples;
    for (int sample_index = 0; sample_index < audio_buffer->sample_count;
         ++sample_index) {
      *sample_out++ = 0; // Left
      *sample_out++ = 0; // Right
    }
    return;
  }

  // Calculate wave period
  real32 wave_period =
      (real32)audio_buffer->samples_per_second / tone->frequency;

  // Clamp volume
  if (master_volume < 0.0f)
    master_volume = 0.0f;
  if (master_volume > 1.0f)
    master_volume = 1.0f;
  if (tone->volume < 0.0f)
    tone->volume = 0.0f;
  if (tone->volume > 1.0f)
    tone->volume = 1.0f;

  // Volume in 16-bit range
  int16 sample_volume = (int16)(tone->volume * master_volume * 16000.0f);

  // Calculate pan volumes
  real32 left_volume =
      (tone->pan_position <= 0.0f) ? 1.0f : (1.0f - tone->pan_position);
  real32 right_volume =
      (tone->pan_position >= 0.0f) ? 1.0f : (1.0f + tone->pan_position);

  int16 *sample_out = (int16 *)audio_buffer->samples;

  if (FRAME_LOG_EVERY_ONE_SECONDS_CHECK) {
    printf("[AUDIO GEN] sample_count=%d, volume=%.1f, freq=%.1f, phase=%.3f\n",
           audio_buffer->sample_count, game_state->audio.tone.volume,
           game_state->audio.tone.frequency, game_state->audio.tone.phase);
  }

  for (int sample_index = 0; sample_index < audio_buffer->sample_count;
       ++sample_index) {
    real32 sine_value = sinf(tone->phase);
    int16 sample_value = (int16)(sine_value * sample_volume);

    // Apply panning
    int16 left_sample = (int16)(sample_value * left_volume);
    int16 right_sample = (int16)(sample_value * right_volume);

    *sample_out++ = left_sample;
    *sample_out++ = right_sample;

    // Advance phase
    tone->phase += 2.0f * M_PI / wave_period;

    // Wrap phase to prevent floating point precision issues
    if (tone->phase > 2.0f * M_PI) {
      tone->phase -= 2.0f * M_PI;
    }
  }
}
