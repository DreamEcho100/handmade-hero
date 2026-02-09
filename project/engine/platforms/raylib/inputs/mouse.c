#include "./mouse.h"
#include "../../../game/inputs.h"

#include <raylib.h>

void raylib_poll_mouse(GameInput *input) {
  // ─────────────────────────────────────────────────────────────────
  // MOUSE POSITION
  // ─────────────────────────────────────────────────────────────────
  Vector2 mouse_pos = GetMousePosition();
  input->mouse_x = (int32)mouse_pos.x;
  input->mouse_y = (int32)mouse_pos.y;

  // Mouse wheel (Raylib returns float, we store as int)
  input->mouse_z = (int32)GetMouseWheelMove();

  // ─────────────────────────────────────────────────────────────────
  // MOUSE BUTTONS
  // ─────────────────────────────────────────────────────────────────
  // Raylib button mapping:
  //   MOUSE_BUTTON_LEFT   = 0
  //   MOUSE_BUTTON_RIGHT  = 1
  //   MOUSE_BUTTON_MIDDLE = 2
  //   MOUSE_BUTTON_SIDE   = 3 (back)
  //   MOUSE_BUTTON_EXTRA  = 4 (forward)
  //
  // Casey's mapping (from handmade.h):
  //   MouseButtons[0] = Left
  //   MouseButtons[1] = Middle
  //   MouseButtons[2] = Right
  //   MouseButtons[3] = XButton1 (back)
  //   MouseButtons[4] = XButton2 (forward)
  // ─────────────────────────────────────────────────────────────────

  // Left button
  process_game_button_state(IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                            &input->mouse_buttons[0]);

  // Middle button (Casey has middle at index 1)
  process_game_button_state(IsMouseButtonDown(MOUSE_BUTTON_MIDDLE),
                            &input->mouse_buttons[1]);

  // Right button (Casey has right at index 2)
  process_game_button_state(IsMouseButtonDown(MOUSE_BUTTON_RIGHT),
                            &input->mouse_buttons[2]);

  // Side button (back/XButton1)
  process_game_button_state(IsMouseButtonDown(MOUSE_BUTTON_SIDE),
                            &input->mouse_buttons[3]);

  // Extra button (forward/XButton2)
  process_game_button_state(IsMouseButtonDown(MOUSE_BUTTON_EXTRA),
                            &input->mouse_buttons[4]);
}
