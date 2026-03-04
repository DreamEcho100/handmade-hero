#!/usr/bin/env bash
# build-dev.sh  —  Sugar, Sugar | Developer Build Script
#
# This is a DEV build script — it ALWAYS builds with debug flags.
# Output is ALWAYS: ./build/game
# There is no release mode here; use a separate build-release.sh for that.
#
# Usage:
#   ./build-dev.sh [--backend=x11|raylib] [-r]
#
#   --backend=raylib  Build the Raylib backend (default)
#   --backend=x11     Build the X11 backend
#   -r / --run        Run the game after a successful build
#
# Examples:
#   ./build-dev.sh                       # Raylib, debug, no run
#   ./build-dev.sh -r                    # Raylib, debug, then run
#   ./build-dev.sh --backend=x11 -r      # X11,    debug, then run

set -e  # exit immediately on any error

# --------------------------------------------------------------------------
# Defaults
# --------------------------------------------------------------------------
BACKEND="raylib"
RUN_AFTER_BUILD=0

# --------------------------------------------------------------------------
# Argument parsing
# --------------------------------------------------------------------------
for arg in "$@"; do
    case "$arg" in
        --backend=x11)    BACKEND="x11"    ;;
        --backend=raylib) BACKEND="raylib" ;;
        -r|--run)         RUN_AFTER_BUILD=1 ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [--backend=x11|raylib] [-r]"
            exit 1
        ;;
    esac
done

# --------------------------------------------------------------------------
# Compiler: prefer clang, fall back to gcc
# --------------------------------------------------------------------------
if command -v clang > /dev/null 2>&1; then
    CC="clang"
elif command -v gcc > /dev/null 2>&1; then
    CC="gcc"
else
    echo "Error: neither clang nor gcc found in PATH."
    exit 1
fi

# --------------------------------------------------------------------------
# Debug flags (always — this is a dev build script)
# --------------------------------------------------------------------------
# -O0 -g         : no optimisation, full debug symbols
# -DDEBUG        : enable ASSERT() / DEBUG_TRAP() macros in game.h
# -fsanitize=..  : AddressSanitizer + UndefinedBehaviorSanitizer
#                  ASan:  buffer overflows, use-after-free, double-free
#                  UBSan: signed overflow, null deref, misaligned access
# -Wall -Wextra  : catch common mistakes (uninitialized vars, wrong types)
# -std=c99       : C99 standard (designated initializers, // comments, etc.)
DEBUG_FLAGS="-O0 -g -DDEBUG -fsanitize=address,undefined"
COMMON_FLAGS="-Wall -Wextra -std=c99 $DEBUG_FLAGS"

# --------------------------------------------------------------------------
# Output directory — always build/game
# --------------------------------------------------------------------------
mkdir -p build
OUT="build/game"

# --------------------------------------------------------------------------
# Source files (shared for both backends)
# --------------------------------------------------------------------------
SHARED_SRCS="src/game.c src/levels.c src/audio.c"

# --------------------------------------------------------------------------
# Backend-specific settings
# --------------------------------------------------------------------------
if [ "$BACKEND" = "x11" ]; then
    SRCS="src/main_x11.c $SHARED_SRCS"
    # -lX11: Xlib window management and event handling
    # -lm:   math library (sinf, fminf, etc.)
    # -lasound: ALSA audio (procedural synthesis pushed to hardware each frame)
    LIBS="-lX11 -lm -lasound"
    COMMON_FLAGS="$COMMON_FLAGS -DALSA_AVAILABLE"
else
    SRCS="src/main_raylib.c $SHARED_SRCS"
    # -lraylib:  Raylib window, input, GPU texture, and audio
    # -lm:       math library
    # -lpthread: POSIX threads (Raylib uses them internally)
    # -ldl:      dynamic linking (dlopen/dlclose used by Raylib on Linux)
    LIBS="-lraylib -lm -lpthread -ldl"
fi

# --------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------
CMD="$CC $COMMON_FLAGS -o $OUT $SRCS $LIBS"

echo "Backend : $BACKEND"
echo "Mode    : debug (always for build-dev.sh)"
echo "Compiler: $CC"
echo "Output  : ./$OUT"
echo ""
echo "Running: $CMD"
echo ""

$CMD

chmod +x "$OUT"

echo ""
echo "Build successful → ./$OUT"

# --------------------------------------------------------------------------
# Run
# --------------------------------------------------------------------------
if [ "$RUN_AFTER_BUILD" = "1" ]; then
    echo "Running ./$OUT ..."
    echo ""
    ./"$OUT"
fi
