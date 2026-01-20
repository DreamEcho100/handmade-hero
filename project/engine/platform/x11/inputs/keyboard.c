#include "../../../game/base.h"
#include "../../../game/input.h"
#include "../audio.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>

void handleEventKeyPress(XEvent *event, GameInput *new_game_input,
                         GameSoundOutput *sound_output) {
  KeySym key = XLookupKeysym(&event->xkey, 0);
  // printf("pressed\n");

  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

  switch (key) {
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

  case (XK_space): {
    process_game_button_state(true, &new_controller1->start);
    break;
  }
  case (XK_Escape): {
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
    linux_debug_audio_latency(sound_output);
    break;
  }
  // ← ADD THIS: Pause key (P key)
  case (XK_p):
  case (XK_P): {
    g_game_is_paused = !g_game_is_paused;
    printf("🎮 Game %s\n", g_game_is_paused ? "PAUSED" : "RESUMED");
  }
  }
}

void handleEventKeyRelease(XEvent *event, GameInput *new_game_input) {
  KeySym key = XLookupKeysym(&event->xkey, 0);

  GameControllerInput *new_controller1 =
      &new_game_input->controllers[KEYBOARD_CONTROLLER_INDEX];

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