#include "inputs.h"

int KEYBOARD_CONTROLLER_INDEX = 0;

inline void process_game_button_state(bool is_down,
                                      GameButtonState *new_state) {
  if (new_state->ended_down != is_down) {
    ++new_state->half_transition_count;
  }
  new_state->ended_down = is_down;
}

// ═══════════════════════════════════════════════════════════
// Clear new inputs buttons to released state
// ═══════════════════════════════════════════════════════════
// Backend keyboard only sends events on press/release.
// If no event, button stays in old state (wrong!).
// So we must explicitly clear to "not pressed".
// ═══════════════════════════════════════════════════════════
void prepare_input_frame(GameInput *old_input, GameInput *new_input) {
  // ═══════════════════════════════════════════════════════════
  // Clear new inputs buttons to released state
  // ═══════════════════════════════════════════════════════════
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *old_ctrl = &old_input->controllers[i];
    GameControllerInput *new_ctrl = &new_input->controllers[i];

    // Copy connection state
    new_ctrl->is_connected = old_ctrl->is_connected;
    new_ctrl->is_analog = old_ctrl->is_analog;
    new_ctrl->controller_index = old_ctrl->controller_index;

    // Preserve analog stick state
    new_ctrl->stick_avg_x = old_ctrl->stick_avg_x;
    new_ctrl->stick_avg_y = old_ctrl->stick_avg_y;

    local_persist_var int new_ctrl_btns_size =
        (int)ArraySize(new_ctrl->buttons);

    // Buttons - preserve state, clear transition count
    for (int btn = 0; btn < new_ctrl_btns_size; btn++) {
      new_ctrl->buttons[btn].ended_down = old_ctrl->buttons[btn].ended_down;
      new_ctrl->buttons[btn].half_transition_count = 0;
    }

    // ─────────────────────────────────────────────────────────────────
    // ADD: Preserve mouse button states
    // ─────────────────────────────────────────────────────────────────
    for (uint32 b = 0; b < ArraySize(new_input->mouse_buttons); b++) {
      new_input->mouse_buttons[b].ended_down =
          old_input->mouse_buttons[b].ended_down;
      new_input->mouse_buttons[b].half_transition_count = 0;
    }

    // Mouse position doesn't need preservation (polled fresh each frame)
    // Mouse wheel should reset to 0 each frame (it's a delta)
    new_input->mouse_z = 0;
  }
}