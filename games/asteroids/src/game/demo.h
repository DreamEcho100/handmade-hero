#ifndef GAME_DEMO_H
#define GAME_DEMO_H

#include "../utils/backbuffer.h"
#include "../utils/base.h"
#include "base.h"

void demo_render(Backbuffer *backbuffer, GameInput *input,
                 GameWorldConfig world_config, WindowScaleMode scale_mode,
                 int fps);

#endif // GAME_DEMO_H