#!/bin/bash
# LESSON 01 -- Build script. Only supports --backend=raylib at this stage.
# L02 adds the x11 backend case.
# SOURCES grows as new .c files are introduced in later lessons.
set -e
mkdir -p build

BACKEND="raylib"
RUN_AFTER_BUILD=false
DEBUG=true

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*) BACKEND="${1#*=}" ;;
        -r|--run)    RUN_AFTER_BUILD=true ;;
        *) echo "Unknown: $1" >&2; exit 1 ;;
    esac
    shift
done

# Shared source files (both backends compile these).
# Empty for now -- add each .c file as you create it in later lessons:
#   L03: add src/game/demo.c
#   L04: add src/utils/draw-shapes.c
#   L06: add src/utils/draw-text.c
SOURCES="src/game/demo.c src/utils/draw-shapes.c src/utils/draw-text.c src/utils/backbuffer.c"

# Always build in debug mode during development.
# -O0:      no optimization (accurate debugger line numbers)
# -g:       include debug symbols
# -DDEBUG:  define the DEBUG macro for debug-only code paths
# -fsanitize=address,undefined: catch memory bugs and UB at runtime
FLAGS="-Wall -Wextra -O0 -g -DDEBUG -fsanitize=address,undefined -DBACKEND=$BACKEND"

case "$BACKEND" in
    raylib)
        BACKEND_LIBS="-lm -lraylib -lpthread -ldl"
        SOURCES="$SOURCES src/platforms/raylib/main.c"
    ;;
    x11)
        BACKEND_LIBS="-lm -lX11 -lxkbcommon -lGL -lGLX"
        SOURCES="$SOURCES src/platforms/x11/base.c src/platforms/x11/main.c"
    ;;
    *)
        echo "Unknown backend '$BACKEND'. Supported: raylib, x11" >&2
        exit 1
    ;;
esac

clang $FLAGS -Isrc -o ./build/game $SOURCES $BACKEND_LIBS
echo "Build complete: ./build/game (backend=$BACKEND)"

if [[ "$RUN_AFTER_BUILD" == true ]]; then exec ./build/game; fi
