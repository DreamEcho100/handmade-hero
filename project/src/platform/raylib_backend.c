#include "../game/game.h"
#include "platform.h"
#include <raylib.h>
#include <stdlib.h>

int platform_main() {
  int width = 800, height = 600;

  InitWindow(width, height, "Raylib Backend");
  SetTargetFPS(60);

  uint32_t *pixels = malloc(width * height * sizeof(uint32_t));
  Image img = {pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D tex = LoadTextureFromImage(img);

  GameState state = {0};

  while (!WindowShouldClose()) {
    game_update(&state, pixels, width, height);
    UpdateTexture(tex, pixels);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(tex, 0, 0, WHITE);
    EndDrawing();
  }

  return 0;
}
