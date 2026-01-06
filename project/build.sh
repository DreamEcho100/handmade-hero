#!/bin/bash
set -e
mkdir -p build

BACKEND=$1
if [ -z "$BACKEND" ]; then
    BACKEND="x11"
fi

echo "Building with backend: $BACKEND"

# ═══════════════════════════════════════════════════════════════
# Debug build flags (NO OPTIMIZATION during development!)
# ═══════════════════════════════════════════════════════════════
FLAGS="-Isrc -Wall -Wextra -std=c11 -g -O0 -lm"
FLAGS="$FLAGS -DHANDMADE_INTERNAL=1"

SRC="src/main.c src/base/memory.c src/base/debug-file-io.c src/game.c"

if [ "$BACKEND" = "x11" ]; then
    FLAGS="$FLAGS -DUSE_X11 -DARGB -lX11 -Wno-unused-parameter"
    SRC="$SRC src/platform/x11/backend.c src/platform/x11/audio.c"
elif [ "$BACKEND" = "raylib" ]; then
    FLAGS="$FLAGS -DUSE_RAYLIB -DABGR -lraylib -lpthread"
    SRC="$SRC src/platform/raylib/backend.c src/platform/raylib/audio.c"
else
    echo "Unknown backend: $BACKEND"
    exit 1
fi

clang $SRC -o build/game $FLAGS