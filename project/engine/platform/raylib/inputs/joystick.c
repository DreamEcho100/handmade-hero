#include "../../../game/input.h"
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int gamepad_id;        // Which Raylib gamepad ID (0-3)
  char device_name[128]; // For debugging
} RaylibJoystickState;

file_scoped_global_var RaylibJoystickState g_joysticks[MAX_JOYSTICK_COUNT] = {
    0};

void raylib_game_initpad(GameControllerInput *controller_old_input,
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

void raylib_poll_gamepad(GameInput *new_input) {
  for (int controller_index = MAX_KEYBOARD_COUNT;
       controller_index < MAX_CONTROLLER_COUNT; controller_index++) {
    GameControllerInput *new_controller =
        &new_input->controllers[controller_index];

    int g_joysticks_index = controller_index - MAX_KEYBOARD_COUNT;
    if (g_joysticks_index < 0 || g_joysticks_index >= MAX_JOYSTICK_COUNT) {
      continue;
    }
    RaylibJoystickState *joystick_state = &g_joysticks[g_joysticks_index];

    if (joystick_state->gamepad_id < 0 ||
        !IsGamepadAvailable(joystick_state->gamepad_id)) {
      continue;
    }

    int gamepad = joystick_state->gamepad_id;

    // ═══════════════════════════════════════════════════════════
    // 🎮 FACE BUTTONS (action_* buttons)
    // ═══════════════════════════════════════════════════════════

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP),
        &new_controller->action_up);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN),
        &new_controller->action_down);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT),
        &new_controller->action_left);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT),
        &new_controller->action_right);

    // ═══════════════════════════════════════════════════════════
    // 🎮 SHOULDER BUTTONS
    // ═══════════════════════════════════════════════════════════

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1),
        &new_controller->left_shoulder);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1),
        &new_controller->right_shoulder);

    // ═══════════════════════════════════════════════════════════
    // 🎮 MENU BUTTONS
    // ═══════════════════════════════════════════════════════════

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT),
        &new_controller->back);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT),
        &new_controller->start);

    // ═══════════════════════════════════════════════════════════
    // 🎮 ANALOG STICKS (ALWAYS store values!)
    // ═══════════════════════════════════════════════════════════

    real32 left_stick_x = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
    real32 left_stick_y = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);

    // ✅ ALWAYS update analog values (even if 0.0!)
    new_controller->stick_avg_x = left_stick_x;
    new_controller->stick_avg_y = left_stick_y;

    // Determine if controller is using analog input
    if (fabsf(left_stick_x) > BASE_JOYSTICK_DEADZONE ||
        fabsf(left_stick_y) > BASE_JOYSTICK_DEADZONE) {
      new_controller->is_analog = true;
    } else {
      new_controller->is_analog = false;
    }

    // ═══════════════════════════════════════════════════════════
    // 🎮 D-PAD BUTTONS (ALWAYS processed, not guarded!)
    // ═══════════════════════════════════════════════════════════
    // NOTE: These buttons are SEPARATE from stick values!
    // User can press D-pad even while stick is deflected.
    // ═══════════════════════════════════════════════════════════

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP),
        &new_controller->move_up);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN),
        &new_controller->move_down);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT),
        &new_controller->move_left);

    process_game_button_state(
        IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT),
        &new_controller->move_right);
  }

  // ═══════════════════════════════════════════════════════════
  // 🆕 DAY 17: ANALOG → DIGITAL CONVERSION (AFTER ALL CONTROLLERS!)
  // ═══════════════════════════════════════════════════════════
  // This runs ONCE per controller, converting analog to buttons.
  // Uses a SEPARATE threshold (0.5) vs deadzone (0.05)!
  // ═══════════════════════════════════════════════════════════

  for (int controller_index = MAX_KEYBOARD_COUNT;
       controller_index < MAX_CONTROLLER_COUNT; controller_index++) {
    GameControllerInput *new_controller =
        &new_input->controllers[controller_index];

    if (!new_controller->is_connected || !new_controller->is_analog) {
      continue;
    }

    // ✅ IMPORTANT: These process_game_button_state() calls will
    //    OVERRIDE D-pad button states if stick is deflected!
    //    This is INTENTIONAL - stick takes priority over D-pad.
    //    (User can still use D-pad when stick is centered.)

    process_game_button_state(
        (new_controller->stick_avg_x < -BASE_JOYSTICK_DEADZONE),
        &new_controller->move_left);

    process_game_button_state(
        (new_controller->stick_avg_x > BASE_JOYSTICK_DEADZONE),
        &new_controller->move_right);

    process_game_button_state(
        (new_controller->stick_avg_y < -BASE_JOYSTICK_DEADZONE),
        &new_controller->move_down);

    process_game_button_state(
        (new_controller->stick_avg_y > BASE_JOYSTICK_DEADZONE),
        &new_controller->move_up);
  }
}

void debug_joystick_state(GameInput *game_input) {
  printf("\n🎮 Controller States:\n");
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *c = &game_input->controllers[i];
    // LinuxJoystickState *joystick_state = NULL;
    // if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
    //   joystick_state = &g_joysticks[i - 1];
    // }
    RaylibJoystickState *joystick_state = NULL;
    if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
      joystick_state = &g_joysticks[i - 1];
    }
    printf("  [%d] connected=%d analog=%d gamepad_id=%d stick_avg_x=%.2f "
           "stick_avg_y=%.2f\n",
           i, c->is_connected, c->is_analog,
           joystick_state ? joystick_state->gamepad_id : -1, c->stick_avg_x,
           c->stick_avg_y);
  }
}