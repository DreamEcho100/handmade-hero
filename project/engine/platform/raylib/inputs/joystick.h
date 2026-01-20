
#ifndef ENGINE_PLATFORM_RAYLIB_INPUTS_JOYSTICK_H
#define ENGINE_PLATFORM_RAYLIB_INPUTS_JOYSTICK_H

#include "../../../game/input.h"

void raylib_init_gamepad(GameControllerInput *controller_old_input, GameControllerInput *controller_new_input);
void raylib_poll_gamepad(GameInput *new_input);
void debug_joystick_state(GameInput *game_input);

#endif // ENGINE_PLATFORM_RAYLIB_INPUTS_JOYSTICK_H