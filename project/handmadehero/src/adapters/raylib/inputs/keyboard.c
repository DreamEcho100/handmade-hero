// IWYU pragma: keep // clangd: unused-include-ignore // NOLINTNEXTLINE(clang-diagnostic-unused-include)
#include "../../../inputs.h" // NOLINT(clang-diagnostic-unused-include)

#include "../../../../../engine/game/base.h"
#include "../../../../../engine/game/inputs.h"
#include "../../../../../engine/platforms/_common/inputs-recording.h"
#include "../../../../../engine/platforms/raylib/audio.h"
#include "../../../../../engine/platforms/raylib/inputs/keyboard.h"
#include <raylib.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🎮 KEYBOARD INPUT HANDLING
// ═══════════════════════════════════════════════════════════════════════════
// Mirrors: X11's handleEventKeyPress/handleEventKeyRelease
// ═══════════════════════════════════════════════════════════════════════════

void handle_keyboard_inputs(EnginePlatformState *platform_state,
                            EngineGameState *game_state) {

  GameControllerInput *keyboard =
      &game_state->inputs->controllers[KEYBOARD_CONTROLLER_INDEX];

  keyboard->is_connected = true;
  keyboard->is_analog = false;

  // ─────────────────────────────────────────────────────────────────────
  // MOVEMENT KEYS (WASD)
  // ─────────────────────────────────────────────────────────────────────

  // W - Move Up
  if (IsKeyPressed(KEY_W)) {
    process_game_button_state(true, &keyboard->move_up);
  }
  if (IsKeyReleased(KEY_W)) {
    process_game_button_state(false, &keyboard->move_up);
  }

  // A - Move Left
  if (IsKeyPressed(KEY_A)) {
    process_game_button_state(true, &keyboard->move_left);
  }
  if (IsKeyReleased(KEY_A)) {
    process_game_button_state(false, &keyboard->move_left);
  }

  // S - Move Down
  if (IsKeyPressed(KEY_S)) {
    process_game_button_state(true, &keyboard->move_down);
  }
  if (IsKeyReleased(KEY_S)) {
    process_game_button_state(false, &keyboard->move_down);
  }

  // D - Move Right
  if (IsKeyPressed(KEY_D)) {
    process_game_button_state(true, &keyboard->move_right);
  }
  if (IsKeyReleased(KEY_D)) {
    process_game_button_state(false, &keyboard->move_right);
  }

  // ─────────────────────────────────────────────────────────────────────
  // SHOULDER BUTTONS (Q/E)
  // ─────────────────────────────────────────────────────────────────────

  if (IsKeyPressed(KEY_Q)) {
    process_game_button_state(true, &keyboard->left_shoulder);
  }
  if (IsKeyReleased(KEY_Q)) {
    process_game_button_state(false, &keyboard->left_shoulder);
  }

  if (IsKeyPressed(KEY_E)) {
    process_game_button_state(true, &keyboard->right_shoulder);
  }
  if (IsKeyReleased(KEY_E)) {
    process_game_button_state(false, &keyboard->right_shoulder);
  }

  // ─────────────────────────────────────────────────────────────────────
  // ACTION BUTTONS (Arrow Keys)
  // ─────────────────────────────────────────────────────────────────────

  if (IsKeyPressed(KEY_UP)) {
    process_game_button_state(true, &keyboard->action_up);
  }
  if (IsKeyReleased(KEY_UP)) {
    process_game_button_state(false, &keyboard->action_up);
  }

  if (IsKeyPressed(KEY_DOWN)) {
    process_game_button_state(true, &keyboard->action_down);
  }
  if (IsKeyReleased(KEY_DOWN)) {
    process_game_button_state(false, &keyboard->action_down);
  }

  if (IsKeyPressed(KEY_LEFT)) {
    process_game_button_state(true, &keyboard->action_left);
  }
  if (IsKeyReleased(KEY_LEFT)) {
    process_game_button_state(false, &keyboard->action_left);
  }

  if (IsKeyPressed(KEY_RIGHT)) {
    process_game_button_state(true, &keyboard->action_right);
  }
  if (IsKeyReleased(KEY_RIGHT)) {
    process_game_button_state(false, &keyboard->action_right);
  }

  // ─────────────────────────────────────────────────────────────────────
  // MENU BUTTONS
  // ─────────────────────────────────────────────────────────────────────

  if (IsKeyPressed(KEY_SPACE)) {
    process_game_button_state(true, &keyboard->start);
  }
  if (IsKeyReleased(KEY_SPACE)) {
    process_game_button_state(false, &keyboard->start);
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    process_game_button_state(true, &keyboard->back);
  }
  if (IsKeyReleased(KEY_ESCAPE)) {
    process_game_button_state(false, &keyboard->back);
  }

  // ─────────────────────────────────────────────────────────────────────
  // SPECIAL KEYS
  // ─────────────────────────────────────────────────────────────────────

  // Alt+F4 - Quit
  if (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_F4)) {
    printf("ALT+F4 pressed - exiting\n");
    is_game_running = false;
  }

  // F1 - Audio debug
  if (IsKeyPressed(KEY_F1)) {

    printf("F1 pressed - showing audio debug\n");
    if (platform_state->config.audio.is_initialized) {
      raylib_debug_audio_latency(&platform_state->config.audio);
    }
  }

  // P - Pause toggle
  if (IsKeyPressed(KEY_P)) {
    g_game_is_paused = !g_game_is_paused;
    printf("🎮 Game %s\n", g_game_is_paused ? "PAUSED" : "RESUMED");
  }

  if (IsKeyPressed(KEY_L)) {
    // Input Recording Toggle (Casey's Day 23)
    printf("🎬 L pressed - Toggling inputs recording/playback\n");
    // Clear inputs when playback should stop, to avoid "stuck keys"
    INPUT_RECORDING_TOGGLE_RESULT_CODE result =
        input_recording_toggle(platform_state->paths.exe_directory.path,
                               &platform_state->memory_state);
    if (result == INPUT_RECORDING_TOGGLE_STOPPED_PLAYBACK) {
      for (uint32 c = 0; c < MAX_CONTROLLER_COUNT; c++) {
        GameControllerInput *ctrl = &game_state->inputs->controllers[c];
        for (uint32 b = 0; b < ArraySize(ctrl->buttons); b++) {
          ctrl->buttons[b].ended_down = false;
          ctrl->buttons[b].half_transition_count = 0;
        }
        ctrl->stick_avg_x = 0.0f;
        ctrl->stick_avg_y = 0.0f;
      }
      for (uint32 c = 0; c < MAX_CONTROLLER_COUNT; c++) {
        GameControllerInput *ctrl = &platform_state->old_inputs->controllers[c];
        for (uint32 b = 0; b < ArraySize(ctrl->buttons); b++) {
          ctrl->buttons[b].ended_down = false;
          ctrl->buttons[b].half_transition_count = 0;
        }
        ctrl->stick_avg_x = 0.0f;
        ctrl->stick_avg_y = 0.0f;
      }
    }
  }
}
