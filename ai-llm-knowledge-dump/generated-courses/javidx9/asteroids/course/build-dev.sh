#!/bin/bash
# =============================================================================
# build-dev.sh — Development Build Script for Asteroids
# =============================================================================
#
# WHAT IS THIS FILE?
# ──────────────────
# Compiles the Asteroids game for development.  Selects the rendering/windowing
# backend, gathers source files, and calls the C compiler with developer flags.
#
# HOW TO USE
# ──────────
#   ./build-dev.sh                         # dev build, Raylib backend
#   ./build-dev.sh --backend=x11           # dev build, X11/OpenGL/ALSA
#   ./build-dev.sh -r                      # build AND immediately run
#   ./build-dev.sh -d                      # AddressSanitizer + UBSan
#   ./build-dev.sh --bench=5               # bench build: profile for 5 s
#   ./build-dev.sh --stress=200            # add 200 extra draw-rect calls
#   ./build-dev.sh --backend=x11 -r        # X11, run after build
#   ./build-dev.sh --bench=5 --backend=x11 # X11 bench build
#
# FLAGS SUMMARY
# ─────────────
#   --backend=NAME   x11 or raylib (default: raylib)
#   -r, --run        Run immediately after a successful build
#   -d, --debug      AddressSanitizer + UBSan + -DDEBUG  (dev only)
#   --bench=N        -O2 -DNDEBUG -DENABLE_PERF -DBENCH_DURATION_S=N
#                    Auto-exits after N seconds and prints profiler table.
#                    Mutually exclusive with --debug.
#   --stress=N       -DSTRESS_RECTS=N  (add N extra rect draw calls/frame)
#   -h, --help       Show this message
#
# PREREQUISITES
# ─────────────
#   Raylib backend:  sudo apt install libraylib-dev
#   X11 backend:     sudo apt install libx11-dev libxkbcommon-dev libasound2-dev
#   (OpenGL/GLX is provided by your GPU driver or mesa)
# =============================================================================

set -e

mkdir -p build

# ══════ Defaults ═══════════════════════════════════════════════════════════

BACKEND="raylib"
RUN_AFTER_BUILD=false
DEBUG_BUILD=false
BENCH_DURATION=""
STRESS_RECTS=""

# ══════ Argument Parsing ════════════════════════════════════════════════════

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        -d|--debug)
            DEBUG_BUILD=true
        ;;
        --bench=*)
            BENCH_DURATION="${1#*=}"
        ;;
        --stress=*)
            STRESS_RECTS="${1#*=}"
        ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --backend=NAME   Select backend: x11, raylib (default: raylib)"
            echo "  -r, --run        Run the game after a successful build"
            echo "  -d, --debug      AddressSanitizer + UBSan + -DDEBUG"
            echo "  --bench=N        Bench build: -O2 -DENABLE_PERF, auto-exit after N seconds"
            echo "  --stress=N       Add N extra draw_rect calls per frame (-DSTRESS_RECTS=N)"
            echo "  --help, -h       Show this help message"
            exit 0
        ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Use --help for usage information" >&2
            exit 1
        ;;
    esac
    shift
done

# ══════ Validation ══════════════════════════════════════════════════════════

if [[ "$DEBUG_BUILD" == true && -n "$BENCH_DURATION" ]]; then
    echo "Error: --debug and --bench are mutually exclusive." >&2
    exit 1
fi

# ══════ Common Sources ══════════════════════════════════════════════════════
#
# Source files shared by ALL backends.  None of these files include X11
# or Raylib headers.
#
# RENDER_COORD_MODE=1 enables explicit world→pixel coordinate helpers in
# render_explicit.h.  This flag is ALWAYS active — explicit mode is the
# only mode the Asteroids course uses.

SOURCES="src/game/game.c src/game/audio.c src/utils/backbuffer.c src/utils/draw-shapes.c src/utils/draw-text.c"
LIBS="-lm"

# ══════ Backend Selection ═══════════════════════════════════════════════════

case "$BACKEND" in
    x11)
        LIBS="$LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound"
        SOURCES="$SOURCES src/platforms/x11/audio.c src/platforms/x11/main.c"
    ;;
    raylib)
        case "$(uname -s)" in
            Darwin) LIBS="$LIBS -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            *)      LIBS="$LIBS -lraylib -lpthread -ldl" ;;
        esac
        SOURCES="$SOURCES src/platforms/raylib/main.c"
    ;;
    *)
        echo "Error: Unknown backend '$BACKEND'" >&2
        echo "Available backends: x11, raylib" >&2
        exit 1
    ;;
esac

# ══════ Compiler Flags ══════════════════════════════════════════════════════

# -DRENDER_COORD_MODE=1: always active — Asteroids uses explicit world→pixel
#   coordinate mode throughout (render_explicit.h helpers).
FLAGS="-Wall -Wextra -DRENDER_COORD_MODE=1"

if [[ -n "$BENCH_DURATION" ]]; then
    # Bench build: optimised, no ASan, profiler active, auto-exit.
    FLAGS="$FLAGS -O2 -DNDEBUG -DENABLE_PERF -DBENCH_DURATION_S=${BENCH_DURATION}"
    SOURCES="$SOURCES src/utils/perf.c"
    echo "⏱  Bench build: -O2 -DENABLE_PERF -DBENCH_DURATION_S=${BENCH_DURATION}"
elif [[ "$DEBUG_BUILD" == true ]]; then
    FLAGS="$FLAGS -g -O0 -fsanitize=address,undefined -DDEBUG"
    echo "⚠  Debug build: AddressSanitizer + UBSan + -DDEBUG enabled"
else
    FLAGS="$FLAGS -g -O0"
fi

if [[ -n "$STRESS_RECTS" ]]; then
    FLAGS="$FLAGS -DSTRESS_RECTS=${STRESS_RECTS}"
    echo "🔧 Stress mode: STRESS_RECTS=${STRESS_RECTS}"
fi

# ══════ Build ════════════════════════════════════════════════════════════════

OUTPUT="./build/game"

# Argument order: flags → output → sources → libraries.
# Libraries last: the linker resolves symbols in order.
clang $FLAGS -o "$OUTPUT" $SOURCES $LIBS

echo "Build successful! Run with: $OUTPUT"

# ══════ Optional Auto-Run ════════════════════════════════════════════════════

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Asteroids ═══"
    echo ""
    exec "$OUTPUT"
fi
