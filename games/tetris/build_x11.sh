#!/bin/bash
set -e
mkdir -p build
clang src/main_x11.c src/tetris.c -o build/tetris_x11 -lX11 -lxkbcommon
echo "Build OK -> ./build/tetris_x11"

# ./build_x11.sh && ./build/tetris_x11;
