#ifndef DE100_PLATFORMS_RAYLIB_INPUTS_KEYBOARD_H
#define DE100_PLATFORMS_RAYLIB_INPUTS_KEYBOARD_H

#include "../../../../engine.h"
#include "../../../../game/inputs.h"
#include "../../../_common/config.h"

// Updated to take PlatformAudioConfig (mirrors X11 pattern)
void handle_keyboard_inputs(EnginePlatformState *platform_state,
                            EngineGameState *game_state);

#endif // DE100_PLATFORMS_RAYLIB_INPUTS_KEYBOARD_H
