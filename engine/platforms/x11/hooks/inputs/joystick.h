
#ifndef DE100_PLATFORMS_X11_INPUTS_JOYSTICK_H
#define DE100_PLATFORMS_X11_INPUTS_JOYSTICK_H

#include "../../../../game/inputs.h"

void linux_close_joysticks(void);
void linux_init_joystick(GameControllerInput *controller_old_input,
                         GameControllerInput *controller_new_input);
void debug_joystick_state(GameInput *game_input);
void linux_poll_joystick(GameInput *new_input);

#endif // DE100_PLATFORMS_X11_INPUTS_JOYSTICK_H
