// IWYU pragma: keep // clangd: unused-include-ignore // NOLINTNEXTLINE(clang-diagnostic-unused-include)
#include "../../../inputs.h"

#include "../../../../../engine/game/inputs.h"
#include "../../../../../engine/platforms/x11/inputs/mouse.h"
// #include "../../../../../engine/game/base.h"
#include <X11/Xlib.h>
// #include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// MOUSE BUTTON MAPPING
// ═══════════════════════════════════════════════════════════════════════════
//
// X11 Button Numbers:
//   Button1 = Left Mouse Button   (LMB)
//   Button2 = Middle Mouse Button (MMB)
//   Button3 = Right Mouse Button  (RMB)
//   Button4 = Scroll Up
//   Button5 = Scroll Down
//   Button6+ = Extra buttons (XButton1, XButton2, etc.)
//
// Casey's MouseButtons array:
//   [0] = LMB (VK_LBUTTON)
//   [1] = MMB (VK_MBUTTON)
//   [2] = RMB (VK_RBUTTON)
//   [3] = XButton1 (VK_XBUTTON1)
//   [4] = XButton2 (VK_XBUTTON2)
//
// ═══════════════════════════════════════════════════════════════════════════

// Convert X11 button number to our array index
// Returns -1 if not a button we track (e.g., scroll wheel)
de100_file_scoped_fn inline int x11_button_to_index(unsigned int button) {
  switch (button) {
  case Button1:
    return 0; // LMB
  case Button2:
    return 1; // MMB
  case Button3:
    return 2; // RMB
  // Button4 and Button5 are scroll wheel - handle separately
  case 8:
    return 3; // XButton1 (back)
  case 9:
    return 4; // XButton2 (forward)
  default:
    return -1; // Unknown/scroll
  }
}

void handle_mouse_motion(XEvent *event, GameInput *input) {
  // MotionNotify gives us window-relative coordinates directly
  // No need for ScreenToClient like Windows!
  input->mouse_x = event->xmotion.x;
  input->mouse_y = event->xmotion.y;

#if DE100_INTERNAL && 0 // Verbose logging, disabled by default
  printf("[MOUSE] Motion: x=%d, y=%d\n", input->mouse_x, input->mouse_y);
#endif
}

void handle_mouse_button_press(XEvent *event, GameInput *input) {
  unsigned int button = event->xbutton.button;

  // Handle scroll wheel separately
  if (button == Button4) {
    // Scroll up
    input->mouse_z += 1; // Or some scroll delta
    return;
  }
  if (button == Button5) {
    // Scroll down
    input->mouse_z -= 1;
    return;
  }

  int index = x11_button_to_index(button);
  if (index >= 0 && index < 5) {
    // Use the same pattern as keyboard
    process_game_button_state(true, &input->mouse_buttons[index]);

#if DE100_INTERNAL
    const char *button_names[] = {"LMB", "MMB", "RMB", "XB1", "XB2"};
    printf("[MOUSE] %s pressed at (%d, %d)\n", button_names[index],
           event->xbutton.x, event->xbutton.y);
#endif
  }
}

void handle_mouse_button_release(XEvent *event, GameInput *input) {
  unsigned int button = event->xbutton.button;

  // Ignore scroll wheel releases
  if (button == Button4 || button == Button5) {
    return;
  }

  int index = x11_button_to_index(button);
  if (index >= 0 && index < 5) {
    process_game_button_state(false, &input->mouse_buttons[index]);

#if DE100_INTERNAL
    const char *button_names[] = {"LMB", "MMB", "RMB", "XB1", "XB2"};
    printf("[MOUSE] %s released\n", button_names[index]);
#endif
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// POLLING-BASED MOUSE POSITION
// ═══════════════════════════════════════════════════════════════════════════
// Alternative to event-based: query mouse position each frame.
// This is what Casey does on Windows with GetCursorPos + ScreenToClient.
//
// Pros: Always have current position, even if no motion events
// Cons: Extra X11 round-trip per frame
// ═══════════════════════════════════════════════════════════════════════════

void poll_mouse_position(Display *display, Window window, GameInput *input) {
  Window root_return, child_return;
  int root_x, root_y;
  int win_x, win_y;
  unsigned int mask_return;

  // XQueryPointer gives us both screen coords and window-relative coords
  Bool result = XQueryPointer(
      display, window, &root_return, &child_return, &root_x,
      &root_y,        // Screen coordinates (like GetCursorPos)
      &win_x, &win_y, // Window coordinates (like ScreenToClient result)
      &mask_return    // Button/modifier state
  );

  if (result) {
    input->mouse_x = win_x;
    input->mouse_y = win_y;

    // We can also get button state from mask_return!
    // This is like GetKeyState(VK_LBUTTON) on Windows

    // Button1Mask = LMB, Button2Mask = MMB, Button3Mask = RMB
    bool lmb_down = (mask_return & Button1Mask) != 0;
    bool mmb_down = (mask_return & Button2Mask) != 0;
    bool rmb_down = (mask_return & Button3Mask) != 0;

    // Update button states (same pattern as Casey's GetKeyState approach)
    process_game_button_state(lmb_down, &input->mouse_buttons[0]);
    process_game_button_state(mmb_down, &input->mouse_buttons[1]);
    process_game_button_state(rmb_down, &input->mouse_buttons[2]);

    // Note: XButton1/2 masks vary by system, may need to handle differently
  }
}
