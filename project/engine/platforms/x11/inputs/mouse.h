#ifndef DE100_PLATFORMS_X11_INPUTS_MOUSE_H
#define DE100_PLATFORMS_X11_INPUTS_MOUSE_H

#include "../../../game/inputs.h"
#include <X11/Xlib.h>

// Process mouse motion events
void handle_mouse_motion(XEvent *event, GameInput *input);

// Process mouse button press
void handle_mouse_button_press(XEvent *event, GameInput *input);

// Process mouse button release
void handle_mouse_button_release(XEvent *event, GameInput *input);

// Poll mouse position (alternative to event-based)
void poll_mouse_position(Display *display, Window window, GameInput *input);

#endif // DE100_PLATFORMS_X11_INPUTS_MOUSE_H
