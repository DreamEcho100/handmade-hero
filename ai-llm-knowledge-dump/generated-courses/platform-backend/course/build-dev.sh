#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# build-dev.sh — Platform Foundation Course
#
# Usage:
#   ./build-dev.sh [--backend=x11|raylib] [--bench=N] [--stress=N] [-r|--run]
#
# Options:
#   --backend=x11             Build with X11/GLX + ALSA backend (Linux)
#   --backend=raylib          Build with Raylib backend (default)
#   --bench=N                 Bench mode: -O2, no ASan, ENABLE_PERF, auto-exit
#                             after N seconds and print profiler summary.
#                             Example: --bench=25
#   --stress=N                Add N extra stress-test rects to the demo.
#                             Enables culling benchmark: pan camera to push rects
#                             off-screen and observe frame time stay flat.
#                             Example: --stress=343
#   -r, --run                 Run the binary after a successful build
#
# Always uses explicit coord mode: -DRENDER_COORD_MODE=1 (render_explicit.h).
#
# Output: ./build/game
#
# ─────────────────────────────────────────────────────────────────────────────
# LESSON 01 — SOURCES variable, --backend flag, -r flag introduced here.
# LESSON 09 — --bench flag, ENABLE_PERF, BENCH_DURATION_S.
# LESSON 17 — --stress=N flag, STRESS_RECTS, culling benchmark.
# ─────────────────────────────────────────────────────────────────────────────
set -e
mkdir -p build

BACKEND="raylib"
RUN_AFTER_BUILD=false
BENCH_DURATION_S=""
STRESS_RECTS=""
FORCE_SCENE_INDEX=""
LOCK_SCENE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        --bench=*)
            BENCH_DURATION_S="${1#*=}"
        ;;
        --stress=*)
            STRESS_RECTS="${1#*=}"
        ;;
        --force-scene=*)
            FORCE_SCENE_INDEX="${1#*=}"
        ;;
        --lock-scene)
            LOCK_SCENE=1
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        --help|-h)
            echo "Usage: $0 [--backend=x11|raylib] [--bench=N] [--stress=N] [-r|--run]"
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
DEMO_SRC="src/game/demo_explicit.c"

# ── Common source files ────────────────────────────────────────────────────
# SOURCES lists every file shared by both backends.
# Platform-specific files are appended below in the backend case.
#
# NOTE: demo_explicit.c and audio_demo.c remain in the course as teaching
# material, but the live runtime now routes through game/main.c.
SOURCES="src/utils/backbuffer.c src/utils/draw-shapes.c src/utils/draw-text.c $DEMO_SRC src/game/audio_demo.c src/game/main.c"

# ── Compiler flags ─────────────────────────────────────────────────────────
# LESSON 09 — Bench mode: --bench=N switches to -O2 without ASan and enables
# the in-game profiler (ENABLE_PERF) + auto-exit (BENCH_DURATION_S=N).
# Standard dev mode: -O0 + ASan/UBSan for fast error detection.
if [[ -n "$BENCH_DURATION_S" ]]; then
    # Bench mode: realistic timings — no sanitizers, optimised build.
    BASE_FLAGS="-O2 -DNDEBUG -Wall -Wextra -DBACKEND=$BACKEND $RENDER_COORD_FLAG \
    -DENABLE_PERF -DBENCH_DURATION_S=${BENCH_DURATION_S}"
    SOURCES="$SOURCES src/utils/perf.c"
    echo "Bench mode: O2, no ASan, ENABLE_PERF, BENCH_DURATION_S=${BENCH_DURATION_S}s"
else
    BASE_FLAGS="-O0 -g -DDEBUG -fsanitize=address,undefined -Wall -Wextra -DBACKEND=$BACKEND $RENDER_COORD_FLAG"
fi

# LESSON 17 — Stress test: add N extra rects to the demo.
if [[ -n "$STRESS_RECTS" ]]; then
    BASE_FLAGS="$BASE_FLAGS -DSTRESS_RECTS=${STRESS_RECTS}"
    echo "Stress mode: STRESS_RECTS=${STRESS_RECTS}"
fi

if [[ -n "$FORCE_SCENE_INDEX" ]]; then
    BASE_FLAGS="$BASE_FLAGS -DCOURSE_FORCE_SCENE_INDEX=${FORCE_SCENE_INDEX}"
    echo "Scene override: COURSE_FORCE_SCENE_INDEX=${FORCE_SCENE_INDEX}"
fi

if [[ "$LOCK_SCENE" == 1 ]]; then
    BASE_FLAGS="$BASE_FLAGS -DCOURSE_LOCK_SCENE=1"
    echo "Scene override lock: COURSE_LOCK_SCENE=1"
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
        BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX"
        SOURCES="$SOURCES src/platforms/x11/base.c src/platforms/x11/audio.c src/platforms/x11/main.c"
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

echo "Building platform-backend course (backend=$BACKEND)..."
clang $BASE_FLAGS $INCLUDE_FLAGS -o "$BINARY" $SOURCES $BACKEND_LIBS
echo "✓ Build complete: $BINARY"

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running ═══"
    exec "$BINARY"
fi
