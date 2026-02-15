#ifndef DE100_X11_MOUSE_H
#define DE100_X11_MOUSE_H

#include "../../../game/inputs.h"
#include <X11/Xlib.h>

// ═══════════════════════════════════════════════════════════════════════════
// X11 MOUSE INPUT API
// ═══════════════════════════════════════════════════════════════════════════
//
// USAGE:
//   1. Call x11_poll_mouse() ONCE per frame in your main loop
//   2. Handle ButtonPress/ButtonRelease events for scroll wheel only
//   3. Do NOT use MotionNotify events - polling is better!
//
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Poll current mouse position and button state.
 *
 * Call this ONCE per frame, BEFORE processing X11 events.
 * This gives you the CURRENT mouse position, not stale event data.
 *
 * Equivalent to Casey's Windows approach:
 *   GetCursorPos(&MouseP);
 *   ScreenToClient(Window, &MouseP);
 *   NewInput->MouseX = MouseP.x;
 *   NewInput->MouseY = MouseP.y;
 *
 * @param display  X11 display connection
 * @param window   Target window for coordinate conversion
 * @param input    GameInput to update (mouse_x, mouse_y, mouse_buttons[0-2])
 */
void x11_poll_mouse(Display *display, Window window, GameInput *input);

/**
 * Handle mouse button press events.
 *
 * Only needed for:
 *   - Scroll wheel (Button4/5) - cannot be polled
 *   - XButton1/2 (Button8/9) - not in standard mask
 *
 * LMB/MMB/RMB are polled, so ButtonPress for those is ignored.
 */
void handle_mouse_button_press(XEvent *event, GameInput *input);

/**
 * Handle mouse button release events.
 *
 * Only needed for XButton1/2 releases.
 * Scroll wheel and LMB/MMB/RMB releases are ignored.
 */
void handle_mouse_button_release(XEvent *event, GameInput *input);

#endif // DE100_X11_MOUSE_H
