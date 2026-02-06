
#ifndef DE100_PLATFORMS_RAYLIB_INPUTS_JOYSTICK_H
#define DE100_PLATFORMS_RAYLIB_INPUTS_JOYSTICK_H

#include "../../../game/inputs.h"

void raylib_game_initpad(GameControllerInput *controller_old_input, GameControllerInput *controller_new_input);
void raylib_poll_gamepad(GameInput *new_input);
void debug_joystick_state(GameInput *game_input);

#endif // DE100_PLATFORMS_RAYLIB_INPUTS_JOYSTICK_H