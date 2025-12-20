#!/bin/bash

set -e

mkdir -p build

BACKEND=$1

if [ -z "$BACKEND" ]; then
    BACKEND="x11"
fi

echo "Building with backend: $BACKEND"

FLAGS="-Isrc -Wall -Wextra -std=c11"

SRC="src/main.c src/base.c src/game.c"

if [ "$BACKEND" = "x11" ]; then
     # -ldl = Link against libdl (for dlopen, dlsym, dlclose)
    # -lm  = Link against math library
    # -O2  = Optimization level 2
    FLAGS="$FLAGS -DUSE_X11 -DARGB -lX11 -ldl -lm -O2 -Wno-unused-parameter"
    SRC="$SRC src/platform/x11/backend.c src/platform/x11/audio.c"
elif [ "$BACKEND" = "raylib" ]; then
    FLAGS="$FLAGS -DUSE_RAYLIB -DABGR -lraylib -lm -ldl -lpthread"
    SRC="$SRC src/platform/raylib/backend.c src/platform/raylib/audio.c"
else
    echo "Unknown backend: $BACKEND"
    echo "Available backends: x11, raylib"
    exit 1
fi

clang $SRC -o build/game $FLAGS
