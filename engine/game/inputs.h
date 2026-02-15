#ifndef DE100_GAME_INPUT_H
#define DE100_GAME_INPUT_H

#include "../_common/memory.h"
#include "./inputs-base.h"

#define BASE_JOYSTICK_DEADZONE 0.4

#define MAX_CONTROLLER_COUNT 5
#define MAX_KEYBOARD_COUNT 1
#define MAX_JOYSTICK_COUNT (MAX_CONTROLLER_COUNT - MAX_KEYBOARD_COUNT)

extern int KEYBOARD_CONTROLLER_INDEX;

#define CONTROLLER_DEADZONE 0

// ═══════════════════════════════════════════════════════════════
// GAME MUST DEFINE THESE BEFORE INCLUDING THIS HEADER:
//   - DE100_GAME_BUTTON_COUNT
//   - DE100_GAME_BUTTON_FIELDS
// ═══════════════════════════════════════════════════════════════

#if !defined(DE100_GAME_BUTTON_COUNT)
#define DE100_GAME_BUTTON_COUNT 0
#endif

#if !defined(DE100_GAME_BUTTON_FIELDS)
#define DE100_GAME_BUTTON_FIELDS GameButtonState ___DUMMY_NAME_TO_AVOID_WARNING;
#endif

typedef struct {
  union {
    GameButtonState buttons[DE100_GAME_BUTTON_COUNT];
    struct {
      DE100_GAME_BUTTON_FIELDS
    };
  };

  f32 stick_avg_x;
  f32 stick_avg_y;
  int controller_index;
  bool32 is_analog;
  bool is_connected;
} GameControllerInput;

typedef struct {
  GameControllerInput controllers[5];

  /*
  ┌─────────────────────────────────────────────────────────────────────────┐
  │                    X11 MOUSE INPUT FLOW                                 │
  ├─────────────────────────────────────────────────────────────────────────┤
  │                                                                         │
  │  X11 Server                                                             │
  │  ┌─────────────────────────────────────────────────────────────────┐   │
  │  │ Mouse Hardware Events                                            │   │
  │  │   ↓                                                              │   │
  │  │ MotionNotify, ButtonPress, ButtonRelease                         │   │
  │  └─────────────────────────────────────────────────────────────────┘   │
  │           │                                                             │
  │           ▼                                                             │
  │  ┌─────────────────────────────────────────────────────────────────┐   │
  │  │ XNextEvent() / XPending()                                        │   │
  │  │ (in x11_process_pending_events)                                  │   │
  │  └─────────────────────────────────────────────────────────────────┘   │
  │           │                                                             │
  │           ▼                                                             │
  │  ┌─────────────────────────────────────────────────────────────────┐   │
  │  │ x11_handle_event()                                               │   │
  │  │   case MotionNotify:  → handle_mouse_motion()                    │   │
  │  │   case ButtonPress:   → handle_mouse_button_press()              │   │
  │  │   case ButtonRelease: → handle_mouse_button_release()            │   │
  │  └─────────────────────────────────────────────────────────────────┘   │
  │           │                                                             │
  │           ▼                                                             │
  │  ┌─────────────────────────────────────────────────────────────────┐   │
  │  │ GameInput                                                        │   │
  │  │   .mouse_x = window-relative X                                   │   │
  │  │   .mouse_y = window-relative Y                                   │   │
  │  │   .mouse_z = scroll wheel delta                                  │   │
  │  │   .mouse_buttons[0..4] = button states                           │   │
  │  └─────────────────────────────────────────────────────────────────┘   │
  │           │                                                             │
  │           ▼                                                             │
  │  ┌─────────────────────────────────────────────────────────────────┐   │
  │  │ game_update_and_render()                                         │   │
  │  │   - Use input->mouse_x, input->mouse_y for cursor               │   │
  │  │   - Check input->mouse_buttons[n].ended_down for clicks         │   │
  │  └─────────────────────────────────────────────────────────────────┘   │
  │                                                                         │
  │  ═══════════════════════════════════════════════════════════════════   │
  │  ALTERNATIVE: Polling (like Casey's GetCursorPos approach)              │
  │  ═══════════════════════════════════════════════════════════════════   │
  │                                                                         │
  │  Each frame, call:                                                      │
  │    poll_mouse_position(display, window, input);                         │
  │                                                                         │
  │  This uses XQueryPointer() which:                                       │
  │    - Gets current mouse position                                        │
  │    - Gets current button state                                          │
  │    - Works even if no events pending                                    │
  │                                                                         │
  │  Trade-off:                                                             │
  │    Events: Lower latency, but might miss if queue overflows             │
  │    Polling: Always current, but extra X11 round-trip                    │
  │                                                                         │
  │  Recommendation: Use BOTH                                               │
  │    - Events for button press/release (accurate transition count)        │
  │    - Polling for position (always current)                              │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

  */
  // Mouse input
  GameButtonState mouse_buttons[5]; // LMB, MMB, RMB, XButton1, XButton2
  int32 mouse_x;
  int32 mouse_y;
  int32 mouse_z; // Mouse wheel (future)
} GameInput;

//

void prepare_input_frame(GameInput *old_input, GameInput *new_input);
void process_game_button_state(bool is_down, GameButtonState *new_state);

#endif
