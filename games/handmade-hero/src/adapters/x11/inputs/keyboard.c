// IWYU pragma: keep // clangd: unused-include-ignore // NOLINTNEXTLINE(clang-diagnostic-unused-include)
#include "../../../inputs.h"

#include "../../../../../../engine/game/base.h"
#include "../../../../../../engine/game/inputs.h"
#include "../../../../../../engine/platforms/_common/inputs-recording.h"
#include "../../../../../../engine/platforms/x11/audio.h"
#include "../../../../../../engine/platforms/x11/hooks/inputs/keyboard.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>

void handleEventKeyPress(XEvent *event, EngineGameState *game_state,
                         EnginePlatformState *platform_state) {
  KeySym key = XLookupKeysym(&event->xkey, 0);
  // printf("pressed\n");

  GameControllerInput *new_controller1 =
      &game_state->inputs->controllers[KEYBOARD_CONTROLLER_INDEX];

  switch (key) {
  case (XK_F5): {
    printf("🔄 F5 pressed - Manual reload requested\n");
    // Set a flag that the main loop will check
    extern bool g_reload_requested;
    g_reload_requested = true;
    break;
  }

  case (XK_w):
  case (XK_W): {
    process_game_button_state(true, &new_controller1->move_up);
    break;
  }
  case (XK_a):
  case (XK_A): {
    process_game_button_state(true, &new_controller1->move_left);
    break;
  }
  case (XK_s):
  case (XK_S): {
    process_game_button_state(true, &new_controller1->move_down);
    break;
  }
  case (XK_d):
  case (XK_D): {
    process_game_button_state(true, &new_controller1->move_right);
    break;
  }

  case (XK_q):
  case (XK_Q): {
    process_game_button_state(true, &new_controller1->left_shoulder);
    break;
  }
  case (XK_e):
  case (XK_E): {
    process_game_button_state(true, &new_controller1->right_shoulder);
    break;
  }

  case (XK_Up): {
    process_game_button_state(true, &new_controller1->action_up);
    break;
  }
  case (XK_Left): {
    process_game_button_state(true, &new_controller1->action_left);
    break;
  }
  case (XK_Down): {
    process_game_button_state(true, &new_controller1->action_down);
    break;
  }
  case (XK_Right): {
    process_game_button_state(true, &new_controller1->action_right);
    break;
  }

  case (XK_braceright): {
    process_game_button_state(true, &new_controller1->start);
    break;
  }
  case (XK_braceleft): {
    process_game_button_state(true, &new_controller1->back);
    break;
  }

  // Alt+F4 quits (OS-level, not game-level)
  case (XK_F4): {
    if (event->xkey.state & Mod1Mask) {
      printf("ALT+F4 released - exiting\n");
      is_game_running = false;
    }
    break;
  }

  case XK_F1: {
    printf("F1 pressed - showing audio debug\n");
    linux_debug_audio_latency(&platform_state->config.audio);
    break;
  }
  // ← ADD THIS: Pause key (P key)
  case (XK_p):
  case (XK_P): {
    g_game_is_paused = !g_game_is_paused;
    printf("🎮 Game %s\n", g_game_is_paused ? "PAUSED" : "RESUMED");
    break;
  }

  // Input Recording Toggle (Casey's Day 23)
  case (XK_l):
  case (XK_L): {
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

    break;
  }
  }
}

void handleEventKeyRelease(XEvent *event, EngineGameState *game_state,
                           EnginePlatformState *platform_state) {
  (void)platform_state;
  KeySym key = XLookupKeysym(&event->xkey, 0);

  GameControllerInput *new_controller1 =
      &game_state->inputs->controllers[KEYBOARD_CONTROLLER_INDEX];

  switch (key) {
  case (XK_w):
  case (XK_W): {
    process_game_button_state(false, &new_controller1->move_up);
    break;
  }
  case (XK_a):
  case (XK_A): {
    process_game_button_state(false, &new_controller1->move_left);
    break;
  }
  case (XK_s):
  case (XK_S): {
    process_game_button_state(false, &new_controller1->move_down);
    break;
  }
  case (XK_d):
  case (XK_D): {
    process_game_button_state(false, &new_controller1->move_right);
    break;
  }

  case (XK_q):
  case (XK_Q): {
    process_game_button_state(false, &new_controller1->left_shoulder);
    break;
  }
  case (XK_e):
  case (XK_E): {
    process_game_button_state(false, &new_controller1->right_shoulder);
    break;
  }

  case (XK_Up): {
    process_game_button_state(false, &new_controller1->action_up);
    break;
  }
  case (XK_Left): {
    process_game_button_state(false, &new_controller1->action_left);
    break;
  }
  case (XK_Down): {
    process_game_button_state(false, &new_controller1->action_down);
    break;
  }
  case (XK_Right): {
    process_game_button_state(false, &new_controller1->action_right);
    break;
  }

  case (XK_braceright): {
    process_game_button_state(false, &new_controller1->start);
    break;
  }
  case (XK_braceleft): {
    process_game_button_state(false, &new_controller1->back);
    break;
  }
  }
}