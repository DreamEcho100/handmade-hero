#!/bin/bash

set -e

mkdir -p build

BACKEND=$1

if [ -z "$BACKEND" ]; then
    BACKEND="sdl"
fi

echo "Building with backend: $BACKEND"

FLAGS="-Isrc -Wall -Wextra -std=c11"

SRC="src/main.c src/game/game.c"

if [ "$BACKEND" = "x11" ]; then
    FLAGS="$FLAGS -DUSE_X11 -lX11"
    SRC="$SRC src/platform/x11_backend.c"
# elif [ "$BACKEND" = "sdl" ]; then
#    FLAGS="$FLAGS -DUSE_SDL `sdl2-config --cflags --libs`"
#    SRC="$SRC src/platform/sdl_backend.c"
elif [ "$BACKEND" = "raylib" ]; then
    FLAGS="$FLAGS -DUSE_RAYLIB -lraylib -lm -ldl -lpthread"
    SRC="$SRC src/platform/raylib_backend.c"
else
    echo "Unknown backend: $BACKEND"
    exit 1
fi

clang $SRC -o build/game $FLAGS

