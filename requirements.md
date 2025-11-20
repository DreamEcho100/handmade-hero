#

## 🏗️ 1. Install dependencies

```bash
sudo apt update
sudo apt install \
build-essential clang \
  libx11-dev libxext-dev libxrandr-dev libxi-dev libxfixes-dev libxshmfence-dev \
  build-essential cmake git libglfw3-dev libgl1-mesa-dev libxcursor-dev libpulse-dev \
  libasound2-dev \
  mesa-common-dev libgl1-mesa-dev \
  libsdl2-dev \
  gdb valgrind;
```

## 🏃 2. Download Raylib source

```bash
cd ~
git clone https://github.com/raysan5/raylib.git
cd raylib
```

## 🏗️ 3. Build Raylib

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

cmake .. → generates the Makefiles
make → builds the library
sudo make install → installs headers + library in /usr/local/lib and /usr/local/include

```c
#include <raylib.h>
/*
 * clang test.c -o test -lraylib -lm -lpthread -ldl -lrt -lX11 -lGL && ./test
 * && rm test
 */

int main() {
  InitWindow(800, 600, "Raylib Test");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Hello Raylib!", 200, 200, 20, LIGHTGRAY);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
```
