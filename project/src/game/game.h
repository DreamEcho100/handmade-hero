#ifndef GAME_H
#define GAME_H

#include <stdint.h>

typedef struct {
  float t;
} GameState;

void game_update(GameState *state, uint32_t *pixels, int width, int height);

#endif
