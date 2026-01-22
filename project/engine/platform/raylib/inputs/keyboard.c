#include "keyboard.h"

#include "../../../game/base.h"
#include "../../../game/input.h"
#include "../audio.h"
#include <raylib.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// 🎮 KEYBOARD INPUT HANDLING
// ═══════════════════════════════════════════════════════════════════════════
// Mirrors: X11's handleEventKeyPress/handleEventKeyRelease
// ═══════════════════════════════════════════════════════════════════════════

void handle_keyboard_inputs(PlatformAudioConfig *audio_config,
                            GameInput *new_game_input) {

  GameControllerInput *keyboard =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

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
    if (audio_config && audio_config->is_initialized) {
      raylib_debug_audio_latency(audio_config);
    }
  }

  // P - Pause toggle
  if (IsKeyPressed(KEY_P)) {
    g_game_is_paused = !g_game_is_paused;
    printf("🎮 Game %s\n", g_game_is_paused ? "PAUSED" : "RESUMED");
  }
}
