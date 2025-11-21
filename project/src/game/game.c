#include "game.h"

void game_update(GameState *state, uint32_t *pixels, int width, int height) {
  state->t += 0.01f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int i = y * width + x;

      float v = (float)x / width + state->t;
      uint8_t r = (uint8_t)(255 * v);
      uint8_t g = (uint8_t)((255 * y) / height);
      uint8_t b = 128;

      pixels[i] = (r << 16) | (g << 8) | b;
    }
  }
}
