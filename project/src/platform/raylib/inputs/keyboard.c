#include "../../../game.h"
#include "../../_common/input.h"
#include "../audio.h"
#include <linux/input-event-codes.h>
#include <raylib.h>
#include <stdio.h>

void handle_keyboard_inputs(GameSoundOutput *sound_output,
                            GameInput *new_game_input) {
  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  if (IsKeyDown(KEY_W)) {
    process_game_button_state(true, &new_controller1->move_up);
  }
  if (IsKeyReleased(KEY_W)) {
    process_game_button_state(false, &new_controller1->move_up);
  }

  if (IsKeyDown(KEY_A)) {
    process_game_button_state(true, &new_controller1->move_left);
  }
  if (IsKeyReleased(KEY_A)) {
    process_game_button_state(false, &new_controller1->move_left);
  }

  if (IsKeyDown(KEY_S)) {
    process_game_button_state(true, &new_controller1->move_down);
  }
  if (IsKeyReleased(KEY_S)) {
    process_game_button_state(false, &new_controller1->move_down);
  }

  if (IsKeyDown(KEY_D)) {
    process_game_button_state(true, &new_controller1->move_right);
  }
  if (IsKeyReleased(KEY_D)) {
    process_game_button_state(false, &new_controller1->move_right);
  }

  if (IsKeyDown(KEY_Q)) {
    process_game_button_state(true, &new_controller1->left_shoulder);
  }
  if (IsKeyReleased(KEY_Q)) {
    process_game_button_state(false, &new_controller1->left_shoulder);
  }

  if (IsKeyDown(KEY_E)) {
    process_game_button_state(true, &new_controller1->right_shoulder);
  }
  if (IsKeyReleased(KEY_E)) {
    process_game_button_state(false, &new_controller1->right_shoulder);
  }

  if (IsKeyDown(KEY_SPACE)) {
    process_game_button_state(true, &new_controller1->start);
  }
  if (IsKeyDown(KEY_ESCAPE)) {
    process_game_button_state(false, &new_controller1->back);
  }

  // Alt+F4 quits (OS-level, not game-level)
  if (IsKeyReleased(KEY_F4) &&
      (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
    printf("ALT+F4 released - exiting\n");
    is_game_running = false;
  }

  if (IsKeyPressed(KEY_F1)) {
    printf("F1 pressed - showing audio debug\n");
    raylib_debug_audio(sound_output);
  }
}
