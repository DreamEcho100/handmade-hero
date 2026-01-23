#include "game.h"
// #include "../engine/_common/base.h"
#include "../engine/game/backbuffer.h"
#include "../engine/game/game-loader.h"
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

void render_weird_gradient(GameOffscreenBuffer *buffer,
                           HandMadeHeroGameState *game_state) {
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
                  (0));
    }
    row += buffer->pitch;
  }
}

void testPixelAnimation(GameOffscreenBuffer *buffer,
                        HandMadeHeroGameState *game_state) {
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
void handle_controls(GameControllerInput *input,
                     HandMadeHeroGameState *game_state) {
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
    game_state->audio.tone.frequency = 256 + (int)(128.0f * y);
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
      (HandMadeHeroGameState *)memory->permanent_storage.base;

  if (!memory->is_initialized) { // false (skip!)
                                 // Never runs again!
                                 // First-time initialization
#if HANDMADE_INTERNAL
    printf("[GAME] First-time init\n");
#endif

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
    printf("[GAME INIT] game_update_and_render from build id %d\n",
           1); // change this number each rebuild

    // Initialize audio state
    game_state->audio.tone.frequency = 256;
    game_state->audio.tone.phase = 0.0f;
    game_state->audio.tone.volume = 1.0f;
    game_state->audio.tone.pan_position = 0.0f;
    game_state->audio.tone.is_playing = true;
    game_state->audio.master_volume = 1.0f;

    game_state->gradient_state = (GradientState){0};
    game_state->pixel_state = (PixelState){0};
    game_state->speed = 5;

    memory->is_initialized = true;
#if HANDMADE_INTERNAL
    printf("[GAME] Init complete\n");
#endif
    return;
  }

  // For reload debugging:
  static int version = 1; // CHANGE THIS between compiles
  if ((g_frame_counter % 120) == 0) {
    printf("[GAME] update_and_render version=%d\n", version);
  }

  // Find active controller
  GameControllerInput *active_controller = NULL;

  // Priority 1: Check for active joystick input (controllers 1-4)
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {

    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    ASSERT((&input->controllers[i].terminator -
            &input->controllers[i].buttons[0]) ==
           (ArraySize(input->controllers[i].buttons)));

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

  handle_controls(active_controller, game_state);

  render_weird_gradient(buffer, game_state);

  testPixelAnimation(buffer, game_state);
}

// ═══════════════════════════════════════════════════════════════════════════
// GAME GET SOUND SAMPLES - Click-Free Version
// ═══════════════════════════════════════════════════════════════════════════
//
// KEY INSIGHT: The phase accumulator must be CONTINUOUS across all calls.
// Each call picks up exactly where the last one left off.
//
// Click causes:
// 1. Phase discontinuity (jumping to wrong point in wave)
// 2. Frequency changes mid-buffer (wave period changes)
// 3. Volume changes mid-buffer (amplitude jumps)
//
// This implementation ensures smooth, continuous audio.
// ═══════════════════════════════════════════════════════════════════════════

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

GAME_GET_SOUND_SAMPLES(game_get_audio_samples) {
  HandMadeHeroGameState *game_state =
      (HandMadeHeroGameState *)memory->permanent_storage.base;

  if (!memory->is_initialized) {
    int16_t *sample_out = (int16_t *)sound_buffer->samples_block.base;
    for (int i = 0; i < sound_buffer->sample_count; ++i) {
      *sample_out++ = 0;
      *sample_out++ = 0;
    }
    return;
  }

  // ═══════════════════════════════════════════════════════════════
  // DEBUG: Log audio state periodically
  // ═══════════════════════════════════════════════════════════════
#if HANDMADE_INTERNAL
  static int audio_debug_counter = 0;
  if (++audio_debug_counter % 100 == 1) { // Every ~3 seconds at 30Hz
    SoundSource *tone = &game_state->audio.tone;
    printf("[AUDIO DEBUG] is_playing=%d, freq=%.1f, vol=%.2f, phase=%.2f, "
           "samples=%d\n",
           tone->is_playing, tone->frequency, tone->volume, tone->phase,
           sound_buffer->sample_count);
  }
#endif

  // Get audio state
  SoundSource *tone = &game_state->audio.tone;
  real32 master_volume = game_state->audio.master_volume;

  // Safety check: if not initialized, output silence
  if (!tone->is_playing || tone->frequency <= 0.0f) {
    int16_t *sample_out = (int16_t *)sound_buffer->samples_block.base;
    for (int sample_index = 0; sample_index < sound_buffer->sample_count;
         ++sample_index) {
      *sample_out++ = 0; // Left
      *sample_out++ = 0; // Right
    }
    return;
  }

  // Calculate wave period
  real32 wave_period =
      (real32)sound_buffer->samples_per_second / tone->frequency;

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
  int16_t sample_volume = (int16_t)(tone->volume * master_volume * 16000.0f);

  // Calculate pan volumes
  real32 left_volume =
      (tone->pan_position <= 0.0f) ? 1.0f : (1.0f - tone->pan_position);
  real32 right_volume =
      (tone->pan_position >= 0.0f) ? 1.0f : (1.0f + tone->pan_position);

  int16_t *sample_out = (int16_t *)sound_buffer->samples_block.base;

  for (int sample_index = 0; sample_index < sound_buffer->sample_count;
       ++sample_index) {
    real32 sine_value = sinf(tone->phase);
    int16_t sample_value = (int16_t)(sine_value * sample_volume);

    // Apply panning
    int16_t left_sample = (int16_t)(sample_value * left_volume);
    int16_t right_sample = (int16_t)(sample_value * right_volume);

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
