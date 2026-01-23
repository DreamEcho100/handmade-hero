#!/bin/bash

set -e
mkdir -p build out

BACKEND="x11"
HANDMADE_SANITIZE_WAVE_1_MEMORY="0"
SHOULD_BUILD_GAME=false
SHOULD_BUILD_PLATFORM=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
            ;;
        --HANDMADE_SANITIZE_WAVE_1_MEMORY=*)
            HANDMADE_SANITIZE_WAVE_1_MEMORY="${1#*=}"
            ;;
        --build-game)
            SHOULD_BUILD_GAME=true
            ;;
        --build-platform)
            SHOULD_BUILD_PLATFORM=true
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
    shift
done

# Determine build mode
BUILD_MODE="all"
if [ "$SHOULD_BUILD_GAME" = true ] || [ "$SHOULD_BUILD_PLATFORM" = true ]; then
    if [ "$SHOULD_BUILD_GAME" = true ] && [ "$SHOULD_BUILD_PLATFORM" = true ]; then
        BUILD_MODE="all"
    elif [ "$SHOULD_BUILD_GAME" = true ]; then
        BUILD_MODE="game"
    else
        BUILD_MODE="platform"
    fi
fi

echo "Building with backend: $BACKEND, mode: $BUILD_MODE, sanitize: $HANDMADE_SANITIZE_WAVE_1_MEMORY"

# Common flags
COMMON_FLAGS="-Isrc -std=c11 -g -O0 -Werror -Wall -Wextra -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1"
COMMON_FLAGS="$COMMON_FLAGS -ffunction-sections -fdata-sections -Wl,--gc-sections"

# Sanitize flags
SANITIZE_FLAGS=""
if [ "$HANDMADE_SANITIZE_WAVE_1_MEMORY" = "1" ]; then
    SANITIZE_FLAGS="-fsanitize=address,leak,undefined -fno-omit-frame-pointer -DHANDMADE_SANITIZE_WAVE_1_MEMORY"
    echo "Enabled sanitizers"
fi

# Game build
if [ "$BUILD_MODE" = "all" ] || [ "$BUILD_MODE" = "game" ]; then
    echo "Building game library..."
    GAME_FLAGS="$COMMON_FLAGS $SANITIZE_FLAGS -shared -fPIC -fvisibility=default -lm"

     # Delete old file first
    rm -f build/libgame.so

    clang game.c -o build/libgame.so $GAME_FLAGS
    # Force update modification time
    touch build/libgame.so

    if [ -f build/libgame.so ]; then
        echo "Game library built: build/libgame.so"
        echo "Exported symbols (first 10):"
        nm -D build/libgame.so | grep " T " | head -10
    else
        echo "Error: Game library not built"
        exit 1
    fi
fi

# Platform build
if [ "$BUILD_MODE" = "all" ] || [ "$BUILD_MODE" = "platform" ]; then
    echo "Building platform executable..."
    PLATFORM_FLAGS="$COMMON_FLAGS $SANITIZE_FLAGS -rdynamic -Wl,-Map=build/game.map -Lbuild -Wl,-rpath=build/ -lm"
    
    # Common platform sources
    PLATFORM_SRC="$PLATFORM_SRC ../engine/main.c"

    PLATFORM_SRC="$PLATFORM_SRC ../engine/_common/debug-file-io.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/_common/dll.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/_common/file.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/_common/memory.c"

    PLATFORM_SRC="$PLATFORM_SRC ../engine/game/audio.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/game/backbuffer.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/game/base.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/game/game-loader.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/game/input.c"
    PLATFORM_SRC="$PLATFORM_SRC ../engine/game/memory.c"

    # Backend-specific sources and libs
    if [ "$BACKEND" = "x11" ]; then
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/x11/audio.c"
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/x11/backend.c"
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/x11/inputs/joystick.c"
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/x11/inputs/keyboard.c"
        PLATFORM_LIBS="-lX11 -lXrandr -lGL -lGLX -lasound -ldl"
    elif [ "$BACKEND" = "raylib" ]; then
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/raylib/audio.c"
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/raylib/backend.c"
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/raylib/inputs/joystick.c"
        PLATFORM_SRC="$PLATFORM_SRC ../engine/platform/raylib/inputs/keyboard.c"
        PLATFORM_LIBS="-lraylib -lpthread -ldl"
    else
        echo "Unknown backend: $BACKEND"
        exit 1
    fi
    
    clang $PLATFORM_SRC -o build/game $PLATFORM_FLAGS $PLATFORM_LIBS
    if [ -f build/game ]; then
        echo "Platform executable built: build/game"
        if [ -f build/game.map ]; then
            echo "Map file: build/game.map"
        fi
    else
        echo "Error: Platform executable not built"
        exit 1
    fi
fi

echo "Build complete!"
