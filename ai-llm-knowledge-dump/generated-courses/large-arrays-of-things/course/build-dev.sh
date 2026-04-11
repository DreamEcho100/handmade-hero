#!/bin/bash
# =============================================================================
# build-dev.sh — Development Build Script for LOATs Course
# =============================================================================
#
# USAGE
#   ./build-dev.sh                         # Raylib backend (default)
#   ./build-dev.sh --backend=x11           # X11/GLX backend
#   ./build-dev.sh -r                      # Build and run
#   ./build-dev.sh --backend=x11 -r        # X11 + run
#   ./build-dev.sh --test                  # Build and run test harness only
#   ./build-dev.sh --lesson=04             # Build per-lesson standalone binary
#   ./build-dev.sh --lesson=04 -r          # Build + run lesson binary
#
# PREREQUISITES
#   Raylib:  sudo apt install libraylib-dev
#   X11:     sudo apt install libx11-dev libxkbcommon-dev libgl-dev
# =============================================================================

set -e
mkdir -p build

# ══════ Defaults ═══════════════════════════════════════════════════════════

BACKEND="raylib"
RUN_AFTER_BUILD=false
TEST_ONLY=false
LESSON=""

# ══════ Argument Parsing ════════════════════════════════════════════════════

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        --test)
            TEST_ONLY=true
        ;;
        --lesson=*)
            LESSON="${1#*=}"
        ;;
        -h|--help)
            head -20 "$0" | tail -16
            exit 0
        ;;
        *)
            echo "Unknown option: $1"
            exit 1
        ;;
    esac
    shift
done

# ══════ Per-lesson standalone binary ═══════════════════════════════════════

if [[ -n "$LESSON" ]]; then
    LESSON_FILE="src/lessons/lesson_${LESSON}.c"
    if [[ ! -f "$LESSON_FILE" ]]; then
        echo "Error: $LESSON_FILE not found"
        echo "Available lessons:"
        ls src/lessons/lesson_*.c 2>/dev/null | sed 's/.*lesson_/  lesson /' | sed 's/\.c//'
        exit 1
    fi
    echo "Building lesson $LESSON..."
    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG \
        -o "build/lesson_${LESSON}" \
        "$LESSON_FILE"
    echo "Build successful: build/lesson_${LESSON}"
    if $RUN_AFTER_BUILD; then
        "./build/lesson_${LESSON}"
    fi
    exit 0
fi

# ══════ Test harness (no platform dependency) ══════════════════════════════

if $TEST_ONLY; then
    echo "Building test harness..."
    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG \
        -o build/test_harness \
        src/test_harness.c \
        src/things/things.c
    echo "Build successful: build/test_harness"
    if $RUN_AFTER_BUILD; then
        ./build/test_harness
    fi
    exit 0
fi

# ══════ Game demo sources (shared across backends) ═════════════════════════

GAME_SOURCES="src/things/things.c src/game/game.c"

BASE_FLAGS="-Wall -Wextra -Werror -std=c11 -O0 -g -DDEBUG"

# ══════ Backend selection ══════════════════════════════════════════════════

case "$BACKEND" in
    raylib)
        echo "Building LOATs demo (Raylib backend)..."
        MAIN_SOURCE="src/platforms/raylib/main.c"
        BACKEND_LIBS="-lraylib -lm -lpthread -ldl"
        # Raylib on some systems needs GL
        if pkg-config --exists raylib 2>/dev/null; then
            BACKEND_LIBS="$(pkg-config --libs raylib) -lm"
        fi
    ;;
    x11)
        echo "Building LOATs demo (X11/GLX backend)..."
        MAIN_SOURCE="src/platforms/x11/main.c"
        BACKEND_LIBS="-lX11 -lGL -lm -lpthread"
    ;;
    *)
        echo "Unknown backend: $BACKEND (use x11 or raylib)"
        exit 1
    ;;
esac

# ══════ Compile ═══════════════════════════════════════════════════════════

OUTPUT="build/loats_demo_${BACKEND}"

gcc $BASE_FLAGS \
    -I src \
    -o "$OUTPUT" \
    $MAIN_SOURCE \
    $GAME_SOURCES \
    $BACKEND_LIBS

echo "Build successful: $OUTPUT"

if $RUN_AFTER_BUILD; then
    "./$OUTPUT"
fi
