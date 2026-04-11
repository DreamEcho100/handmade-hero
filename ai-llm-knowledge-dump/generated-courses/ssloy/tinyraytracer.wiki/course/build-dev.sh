#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# build-dev.sh — TinyRaytracer Course
#
# Usage:
#   ./build-dev.sh [--backend=x11|raylib] [-r|--run] [-d|--debug]
#
# Options:
#   --backend=x11      Build with X11/GLX backend (Linux)
#   --backend=raylib   Build with Raylib backend (default)
#   -r, --run          Run the binary after a successful build
#   -d, --debug        Enable debug flags: -O0 -g -DDEBUG -fsanitize=address,undefined
#
# Output: ./build/game
#
# ─────────────────────────────────────────────────────────────────────────────
set -e
mkdir -p build

BACKEND="raylib"
COORD_MODE="explicit"
RUN_AFTER_BUILD=false
DEBUG=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        --coord-mode=*)
            COORD_MODE="${1#*=}"
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        -d|--debug)
            DEBUG=true
        ;;
        --help|-h)
            echo "Usage: $0 [--backend=x11|raylib] [-r|--run] [-d|--debug]"
            exit 0
        ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
        ;;
    esac
    shift
done

# ── Common source files ────────────────────────────────────────────────────
# Single-header libraries:
#   stb_image.h  → STB_IMAGE_IMPLEMENTATION in utils/stb_image_impl.c (X11 only;
#                  Raylib links its own internal copy)
#   fast_obj.h   → FAST_OBJ_IMPLEMENTATION  in mesh.c
SOURCES="src/utils/draw-shapes.c src/utils/draw-text.c src/utils/backbuffer.c src/game/main.c src/game/intersect.c src/game/lighting.c src/game/raytracer.c src/game/render.c src/game/stereo.c src/game/stereogram.c src/game/voxel.c src/game/envmap.c src/game/mesh.c"

# ── Coord mode flag ────────────────────────────────────────────────────────
case "$COORD_MODE" in
    explicit) COORD_MODE_FLAG="-DRENDER_COORD_MODE=1" ;;
    implicit) COORD_MODE_FLAG="-DRENDER_COORD_MODE=2" ;;
    hybrid)   COORD_MODE_FLAG="-DRENDER_COORD_MODE=3" ;;
    *) echo "Unknown coord-mode: $COORD_MODE"; exit 1 ;;
esac

# ── Compiler flags ─────────────────────────────────────────────────────────
BASE_FLAGS="-Wall -Wextra -DBACKEND=$BACKEND $COORD_MODE_FLAG"

if [[ "$DEBUG" == true ]]; then
    FLAGS="$BASE_FLAGS -O0 -g -DDEBUG -fsanitize=address,undefined"
    echo "🐛 Debug build (ASan + UBSan enabled)"
else
    FLAGS="$BASE_FLAGS -O2"
fi

# ── Backend-specific setup ─────────────────────────────────────────────────
DETECTED_OS=""
case "$(uname -s)" in
    Linux*)               DETECTED_OS="linux" ;;
    Darwin*)              DETECTED_OS="macos" ;;
    MINGW*|MSYS*|CYGWIN*) DETECTED_OS="windows" ;;
    *)                    DETECTED_OS="posix" ;;
esac

case "$BACKEND" in
    x11)
        BACKEND_LIBS="-lm -lpthread -lX11 -lxkbcommon -lGL -lGLX"
        SOURCES="$SOURCES src/utils/stb_image_impl.c src/platforms/x11/base.c src/platforms/x11/main.c"
    ;;
    raylib)
        case "$DETECTED_OS" in
            windows) BACKEND_LIBS="-lm -lraylib -lpthread -ldl" ;;
            macos)   BACKEND_LIBS="-lm -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            *)       BACKEND_LIBS="-lm -lraylib -lpthread -ldl" ;;
        esac
        SOURCES="$SOURCES src/platforms/raylib/main.c"
    ;;
    *)
        echo "Error: Unknown backend '$BACKEND'" >&2
        echo "Available: x11, raylib" >&2
        exit 1
    ;;
esac

BINARY="./build/game"
INCLUDE_FLAGS="-Isrc"

echo "Building TinyRaytracer course (backend=$BACKEND)..."
clang $FLAGS $INCLUDE_FLAGS -o "$BINARY" $SOURCES $BACKEND_LIBS
echo "✓ Build complete: $BINARY"

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running ═══"
    exec "$BINARY"
fi
