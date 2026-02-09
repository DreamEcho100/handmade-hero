#ifndef DE100_PLATFORMS_X11_INPUTS_KEYBOARD_H
#define DE100_PLATFORMS_X11_INPUTS_KEYBOARD_H

#include "../../../../engine.h"

#include <X11/Xlib.h>

void handleEventKeyPress(XEvent *event, EngineGameState *game_state, EnginePlatformState *platform_state);
void handleEventKeyRelease(XEvent *event, EngineGameState *game_state, EnginePlatformState *platform_state);

#endif // DE100_PLATFORMS_X11_INPUTS_KEYBOARD_H