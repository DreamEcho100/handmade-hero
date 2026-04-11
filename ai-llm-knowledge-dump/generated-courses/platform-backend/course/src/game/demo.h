#ifndef GAME_DEMO_H
#define GAME_DEMO_H

/* game/demo.h — Platform Foundation Course
 * Declares demo_render(); included by both backends.
 * LESSON 15 — remove when creating a game course.
 *
 * LESSON 09 — added WindowScaleMode + world_config parameters.
 * LESSON 16 — added Arena *scratch parameter for per-frame allocations.
 * LESSON 17 — added float dt (delta time in seconds) for camera movement. */

#include "../game/base.h"
#include "../utils/arena.h"
#include "../utils/backbuffer.h"
#include "../utils/base.h"

void demo_render(Backbuffer *backbuffer, GameInput *input, int fps,
                 WindowScaleMode scale_mode, GameWorldConfig world_config,
                 Arena *perm, Arena *level, Arena *scratch, float dt);

#endif /* GAME_DEMO_H */
