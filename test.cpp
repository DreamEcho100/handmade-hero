#include <raylib.h>
/*
 * clang test.cpp -o test -lraylib -lm -lpthread -ldl -lrt -lX11 -lGL && ./test
 * && rm test
 */

int main() {
  InitWindow(800, 600, "Raylib Test");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Hello Raylib!", 200, 200, 20, BLACK);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
