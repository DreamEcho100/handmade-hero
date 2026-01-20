
#ifndef ENGINE_PLATFORM_X11_INPUTS_JOYSTICK_H
#define ENGINE_PLATFORM_X11_INPUTS_JOYSTICK_H

#include "../../../game/input.h"

void linux_close_joysticks(void);
void linux_init_joystick(GameControllerInput *controller_old_input, GameControllerInput *controller_new_input);
void debug_joystick_state(GameInput *game_input);
void linux_poll_joystick(GameInput *new_input);

#endif // ENGINE_PLATFORM_X11_INPUTS_JOYSTICK_H