#ifndef ENGINE_PLATFORM_X11_INPUTS_KEYBOARD_H
#define ENGINE_PLATFORM_X11_INPUTS_KEYBOARD_H

#include "../../../game/input.h"
#include "../../../game/audio.h"
#include "../../_common/audio.h"
#include <X11/Xlib.h>

void handleEventKeyPress(XEvent *event, GameInput *new_game_input,
                           PlatformAudioConfig *platform_audio_config);
void handleEventKeyRelease(XEvent *event, GameInput *new_game_input);

#endif // ENGINE_PLATFORM_X11_INPUTS_KEYBOARD_H