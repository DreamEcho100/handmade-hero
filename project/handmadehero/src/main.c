#include "main.h"
#include "../../engine/_common/base.h"
#include "../../engine/game/backbuffer.h"
#include "../../engine/game/input.h"
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

// de100_file_scoped_global_var inline bool
// controller_has_input(GameControllerInput *controller) {
//   return (fabsf(controller->stick_avg_x) > CONTROLLER_DEADZONE ||
//           fabsf(controller->stick_avg_y) > CONTROLLER_DEADZONE ||
//           controller->move_up.ended_down ||
//           controller->move_down.ended_down ||
//           controller->move_left.ended_down ||
//           controller->move_right.ended_down);
// }

de100_file_scoped_global_var void render_player(GameBackBuffer *backbuffer,
                                                int pos_x, int pos_y) {
  uint8 *end_of_buffer =
      (uint8 *)backbuffer->memory + backbuffer->pitch * backbuffer->height;

  uint32 color = 0xFFFFFFFF; // White
  int top = pos_y;
  int bottom = pos_y + 10;

  for (int X = pos_x; X < pos_x + 10; ++X) {
    uint8 *pixel = ((uint8 *)backbuffer->memory +
                    X * backbuffer->bytes_per_pixel + top * backbuffer->pitch);
    for (int Y = top; Y < bottom; ++Y) {
      // Bounds checking!
      if ((pixel >= (uint8 *)backbuffer->memory) &&
          ((pixel + 4) <= (uint8 *)end_of_buffer)) {
        *(uint32 *)pixel = color;
      }
      pixel += backbuffer->pitch;
    }
  }
}

void render_weird_gradient(GameBackBuffer *buffer,
                           HandMadeHeroGameState *game_state) {
  uint8 *row = (uint8 *)buffer->memory;

  // The following is correct for X11
  for (int y = 0; y < buffer->height; ++y) {
    int offset_x = game_state->gradient_state.offset_x;
    uint32 *pixel = (uint32 *)row;
    int offset_y = game_state->gradient_state.offset_y;

    for (int x = 0; x < buffer->width; ++x) {
      uint8 blue = (uint8)(x + offset_x);
      uint8 green = (uint8)(y + offset_y);

      // RGBA format - works for BOTH OpenGL X11 AND Raylib!
      *pixel++ = (0xFF000000u |  // Alpha = 255
                  (blue << 8) | // Blue
                  (green << 8) | // Green
                  (0));
    }
    row += buffer->pitch;
  }
}

void testPixelAnimation(GameBackBuffer *buffer,
                        HandMadeHeroGameState *game_state) {
  // Test pixel animation
  uint32 *pixels = (uint32 *)buffer->memory;
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
void handle_controls(GameControllerInput *input,
                     HandMadeHeroGameState *game_state) {

  if (input->action_down.ended_down) {
    game_state->player_state.t_jump = 4.0; // Start jump
  }

  // Simple jump arc
  if (game_state->player_state.t_jump > 0) {
    game_state->player_state.y +=
        (int)(5.0f * sinf(0.5f * M_PI * game_state->player_state.t_jump));
  }
  game_state->player_state.t_jump -= 0.033f; // Tick down jump timer

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

    real32 stick_avg_x = apply_deadzone(input->stick_avg_x);
    real32 stick_avg_y = apply_deadzone(input->stick_avg_y);

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
  if (game_state->audio.tone.frequency < 20)
    game_state->audio.tone.frequency = 20;
  if (game_state->audio.tone.frequency > 2000)
    game_state->audio.tone.frequency = 2000;

  // // Update wave_period when tone_hz changes (prevent audio clicking)
  // audio_output->wave_period =
  //     audio_output->samples_per_second / game_state->audio.tone.frequency;
}

GAME_UPDATE_AND_RENDER(game_update_and_render) {

  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage;

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick input (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {

    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;
    DEV_ASSERT((&input->controllers[i].terminator -
                &input->controllers[i].buttons[0]) ==
               (ArraySize(input->controllers[i].buttons)));

    GameControllerInput *c = &input->controllers[i];
    if (c->is_connected) {
      active_controller = c;
      break;
    }
  }

  // Fallback: Always use keyboard (controller[0]) if nothing else
  if (!active_controller) {
    active_controller = &input->controllers[KEYBOARD_CONTROLLER_INDEX];
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
