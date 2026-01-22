#ifndef ENGINE_PLATFORM_RAYLIB_INPUTS_KEYBOARD_H
#define ENGINE_PLATFORM_RAYLIB_INPUTS_KEYBOARD_H

#include "../../../game/input.h"
#include "../../_common/audio.h"

// Updated to take PlatformAudioConfig (mirrors X11 pattern)
void handle_keyboard_inputs(PlatformAudioConfig *audio_config,
                            GameInput *new_game_input);

#endif // ENGINE_PLATFORM_RAYLIB_INPUTS_KEYBOARD_H
