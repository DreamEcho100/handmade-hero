#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# build-dev.sh — Jetpack Joyride (SlimeEscape)
#
# Usage:
#   ./build-dev.sh [--backend=x11|raylib] [--bench=N] [-r|--run]
#
# Options:
#   --backend=x11             Build with X11/GLX + ALSA backend (Linux)
#   --backend=raylib          Build with Raylib backend (default)
#   --bench=N                 Bench mode: -O2, no ASan, ENABLE_PERF, auto-exit
#                             after N seconds and print profiler summary.
#   -r, --run                 Run the binary after a successful build
#
# Always uses explicit coord mode: -DRENDER_COORD_MODE=1 (render_explicit.h).
#
# Output: ./build/game
#
# ─────────────────────────────────────────────────────────────────────────────
set -e
mkdir -p build

BACKEND="raylib"
RUN_AFTER_BUILD=false
BENCH_DURATION_S=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        --bench=*)
            BENCH_DURATION_S="${1#*=}"
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        --help|-h)
            echo "Usage: $0 [--backend=x11|raylib] [--bench=N] [-r|--run]"
            exit 0
        ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
        ;;
    esac
    shift
done

RENDER_COORD_FLAG="-DRENDER_COORD_MODE=1"

# ── Common source files ────────────────────────────────────────────────────
SOURCES="src/utils/backbuffer.c src/utils/draw-shapes.c src/utils/draw-text.c src/utils/sprite.c src/utils/loaded-audio.c src/utils/particles.c src/game/player.c src/game/obstacles.c src/game/audio.c src/game/high_scores.c src/game/main.c"

# ── Compiler flags ─────────────────────────────────────────────────────────
if [[ -n "$BENCH_DURATION_S" ]]; then
    BASE_FLAGS="-O2 -DNDEBUG -Wall -Wextra -DBACKEND=$BACKEND $RENDER_COORD_FLAG \
-DENABLE_PERF -DBENCH_DURATION_S=${BENCH_DURATION_S}"
    SOURCES="$SOURCES src/utils/perf.c"
    echo "Bench mode: O2, no ASan, ENABLE_PERF, BENCH_DURATION_S=${BENCH_DURATION_S}s"
else
    BASE_FLAGS="-O0 -g -DDEBUG -fsanitize=address,undefined -Wall -Wextra -DBACKEND=$BACKEND $RENDER_COORD_FLAG"
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
        BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX -lpthread"
        SOURCES="$SOURCES src/platforms/x11/base.c src/platforms/x11/audio.c src/platforms/x11/main.c"
    ;;
    raylib)
        # -Wl,-z,muldefs: allow multiple definitions of stb_vorbis symbols
        # (our copy and Raylib's bundled copy are identical).
        case "$DETECTED_OS" in
            windows) BACKEND_LIBS="-lm -lraylib -lpthread -ldl -Wl,-z,muldefs" ;;
            macos)   BACKEND_LIBS="-lm -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            *)       BACKEND_LIBS="-lm -lraylib -lpthread -ldl -Wl,-z,muldefs" ;;
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

echo "Building Jetpack Joyride (backend=$BACKEND)..."
clang $BASE_FLAGS $INCLUDE_FLAGS -o "$BINARY" $SOURCES $BACKEND_LIBS
echo "Build complete: $BINARY"

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "Running..."
    exec "$BINARY"
fi
