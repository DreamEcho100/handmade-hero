#include "./mouse.h"
#include "../../../game/inputs.h"

#include <X11/Xlib.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
// POLLING-BASED MOUSE INPUT (Casey's Day 25 Approach)
// ═══════════════════════════════════════════════════════════════════════════
//
// WHY POLLING INSTEAD OF EVENTS?
// ───────────────────────────────
// Event-based mouse tracking is inherently laggy because:
//   1. Events are queued and processed after they occur
//   2. X11 may coalesce multiple MotionNotify events into one
//   3. Events arrive in batches, not real-time
//
// Casey's Windows approach (Day 25):
//   POINT MouseP;
//   GetCursorPos(&MouseP);              // Poll current screen position
//   ScreenToClient(Window, &MouseP);    // Convert to window coordinates
//   NewInput->MouseX = MouseP.x;
//   NewInput->MouseY = MouseP.y;
//
// Our X11 equivalent:
//   XQueryPointer() does BOTH in one call - gives us window-relative coords
//   directly, plus button state as a bonus!
//
// ═══════════════════════════════════════════════════════════════════════════

void x11_poll_mouse(Display *display, Window window, GameInput *input) {
  if (!display || !window || !input) {
    return;
  }

  Window root_return, child_return;
  int root_x, root_y;
  int win_x, win_y;
  unsigned int mask_return;

  // ─────────────────────────────────────────────────────────────────────
  // XQueryPointer: The X11 equivalent of GetCursorPos + ScreenToClient
  // ─────────────────────────────────────────────────────────────────────
  // Returns:
  //   - root_x, root_y: Screen coordinates (like GetCursorPos)
  //   - win_x, win_y: Window-relative coordinates (like ScreenToClient)
  //   - mask_return: Button and modifier state (like GetKeyState)
  //
  // This is a SYNCHRONOUS call to the X server - it gets the CURRENT
  // mouse position, not a stale event from the queue.
  // ─────────────────────────────────────────────────────────────────────
  Bool on_same_screen =
      XQueryPointer(display, window,
                    &root_return,  // Root window the pointer is on
                    &child_return, // Child window pointer is in (if any)
                    &root_x,       // Pointer X relative to root (screen coords)
                    &root_y,       // Pointer Y relative to root (screen coords)
                    &win_x, // Pointer X relative to our window ← WHAT WE WANT
                    &win_y, // Pointer Y relative to our window ← WHAT WE WANT
                    &mask_return // Button/modifier mask
      );

  if (!on_same_screen) {
    // Mouse is on a different screen (multi-monitor edge case)
    // Keep previous position, don't update
    return;
  }

  // ─────────────────────────────────────────────────────────────────────
  // POSITION: Direct assignment from current state
  // ─────────────────────────────────────────────────────────────────────
  // This is the KEY fix for mouse lag - we're getting the position
  // RIGHT NOW, not from a queued event that happened milliseconds ago.
  // ─────────────────────────────────────────────────────────────────────
  input->mouse_x = win_x;
  input->mouse_y = win_y;
  // Note: mouse_z (scroll wheel) cannot be polled - it's event-only

  // ─────────────────────────────────────────────────────────────────────
  // BUTTONS: Poll state and track transitions
  // ─────────────────────────────────────────────────────────────────────
  // mask_return contains the current button state as a bitmask:
  //   Button1Mask (1<<8) = Left mouse button
  //   Button2Mask (1<<9) = Middle mouse button
  //   Button3Mask (1<<10) = Right mouse button
  //
  // This matches Casey's Windows approach:
  //   Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0],
  //                               GetKeyState(VK_LBUTTON) & (1 << 15));
  // ─────────────────────────────────────────────────────────────────────

  bool lmb_down = (mask_return & Button1Mask) != 0;
  bool mmb_down = (mask_return & Button2Mask) != 0;
  bool rmb_down = (mask_return & Button3Mask) != 0;

  // process_game_button_state handles transition counting properly
  process_game_button_state(lmb_down, &input->mouse_buttons[0]); // LMB
  process_game_button_state(mmb_down, &input->mouse_buttons[1]); // MMB
  process_game_button_state(rmb_down, &input->mouse_buttons[2]); // RMB

  // XButton1/2 (back/forward mouse buttons) are NOT in the standard mask
  // They must be handled via events (ButtonPress/ButtonRelease)
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENT-BASED HANDLERS
// ═══════════════════════════════════════════════════════════════════════════
// These are ONLY for things that CANNOT be polled:
//   1. Scroll wheel (discrete events, no persistent state)
//   2. XButton1/2 (back/forward, not in standard button mask)
//
// We do NOT handle MotionNotify here - polling is better for position!
// ═══════════════════════════════════════════════════════════════════════════

void handle_mouse_button_press(XEvent *event, GameInput *input) {
  if (!event || !input) {
    return;
  }

  unsigned int button = event->xbutton.button;

  switch (button) {
  // ─────────────────────────────────────────────────────────────────
  // SCROLL WHEEL: Cannot be polled, must use events
  // ─────────────────────────────────────────────────────────────────
  // X11 treats scroll wheel as button presses:
  //   Button4 = Scroll up
  //   Button5 = Scroll down
  // There's no "scroll wheel position" to poll - only discrete events.
  // ─────────────────────────────────────────────────────────────────
  case Button4: // Scroll up
    input->mouse_z += 1;
    break;

  case Button5: // Scroll down
    input->mouse_z -= 1;
    break;

  // ─────────────────────────────────────────────────────────────────
  // EXTRA MOUSE BUTTONS: Not in standard mask, use events
  // ─────────────────────────────────────────────────────────────────
  // Button 8 = XButton1 (Back/Thumb1)
  // Button 9 = XButton2 (Forward/Thumb2)
  // These map to MouseButtons[3] and [4] to match Casey's layout.
  // ─────────────────────────────────────────────────────────────────
  case 8: // XButton1 (Back)
    process_game_button_state(true, &input->mouse_buttons[3]);
    break;

  case 9: // XButton2 (Forward)
    process_game_button_state(true, &input->mouse_buttons[4]);
    break;

  // Button1/2/3 (LMB/MMB/RMB) are handled by polling, ignore here
  default:
    break;
  }
}

void handle_mouse_button_release(XEvent *event, GameInput *input) {
  if (!event || !input) {
    return;
  }

  unsigned int button = event->xbutton.button;

  switch (button) {
  // Scroll wheel "releases" are meaningless, ignore them
  case Button4:
  case Button5:
    break;

  // Extra mouse button releases
  case 8: // XButton1 (Back)
    process_game_button_state(false, &input->mouse_buttons[3]);
    break;

  case 9: // XButton2 (Forward)
    process_game_button_state(false, &input->mouse_buttons[4]);
    break;

  // Button1/2/3 (LMB/MMB/RMB) are handled by polling, ignore here
  default:
    break;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// DEPRECATED: Event-based mouse motion
// ═══════════════════════════════════════════════════════════════════════════
// This function is NO LONGER USED. Keeping it commented for reference.
// Polling via x11_poll_mouse() is the correct approach.
// ═══════════════════════════════════════════════════════════════════════════
/*
void handle_mouse_motion(XEvent *event, GameInput *input) {
    // DON'T USE THIS - it causes lag!
    // Events are queued and may be coalesced by X11.
    // Use x11_poll_mouse() instead for responsive mouse tracking.
    input->mouse_x = event->xmotion.x;
    input->mouse_y = event->xmotion.y;
}
*/
