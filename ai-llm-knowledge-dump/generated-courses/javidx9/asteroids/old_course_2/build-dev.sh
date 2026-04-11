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
# COURSE NOTE: The reference source uses two separate scripts (build_x11.sh and
# build_raylib.sh).  We replace them with a single unified script and a
# --backend flag, matching the architecture students see in the Snake course.
# Trade-off: slightly more complex argument parsing in exchange for eliminating
# 90% duplicated build logic and a consistent workflow across all course games.
# See COURSE-BUILDER-IMPROVEMENTS.md for the full rationale.
#
# HOW TO USE:
#   ./build-dev.sh                      # build with default (raylib) backend
#   ./build-dev.sh --backend=x11        # X11/OpenGL/ALSA (Linux native)
#   ./build-dev.sh -r                   # build AND immediately run
#   ./build-dev.sh --debug              # AddressSanitizer + UBSan
#   ./build-dev.sh --backend=x11 -r     # build X11 and run
#   ./build-dev.sh -d -r                # debug build and run
#
# PREREQUISITES:
#   Raylib backend:  sudo apt install libraylib-dev
#   X11 backend:     sudo apt install libx11-dev libxkbcommon-dev libasound2-dev
#   (OpenGL/GLX is provided by your GPU driver or mesa)
# =============================================================================

# `set -e` — exit immediately if any command fails.
# Without this, the script would continue after a compile error and print
# "Done!" even though the build failed.
# JS equivalent: there's no direct equivalent; it's like having every line
# wrapped in a try/catch that re-throws.
set -e

mkdir -p build

# ══════ Default Configuration ══════════════════════════════════════════════

BACKEND="raylib"
RUN_AFTER_BUILD=false
DEBUG_BUILD=false

# ══════ Argument Parsing ═══════════════════════════════════════════════════

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
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --backend=NAME   Select backend: x11, raylib (default: raylib)"
            echo "  -r, --run        Run the game after a successful build"
            echo "  -d, --debug      Enable AddressSanitizer + UBSan (-DDEBUG)"
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

# ══════ Common Sources & Libraries ═════════════════════════════════════════

# Source files shared by ALL backends (platform-independent game code).
# None of these files include X11 or Raylib headers.
#
# Compile order matters to the linker: list object files before libraries.
SOURCES="src/game/game.c src/game/audio.c src/utils/draw-shapes.c src/utils/draw-text.c"

# -lm: C math library (sinf, cosf, fabsf, fmodf used in game and audio code).
# Unlike JS, C's math functions are NOT built-in — you must explicitly link.
LIBS="-lm"

# ══════ Backend Selection ══════════════════════════════════════════════════

case "$BACKEND" in
    # ── X11 Backend (Linux only) ──────────────────────────────────────────
    # Communicates directly with the X Window System (Xlib).
    #   -lX11       Xlib: open display, create window, receive events
    #   -lxkbcommon XKB keyboard handling (key names vs hardware scan codes)
    #   -lGL        OpenGL (GPU driver or mesa provides this)
    #   -lGLX       GLX: OpenGL context creation inside an X11 window
    #   -lasound    ALSA: Advanced Linux Sound Architecture (audio output)
    x11)
        LIBS="$LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound"
        SOURCES="$SOURCES src/platforms/x11/main.c"
    ;;

    # ── Raylib Backend (cross-platform) ──────────────────────────────────
    # Raylib handles windowing, input, rendering, and audio in one library.
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

# Development flags (NOT for release):
#   -Wall              Most common warnings (unused vars, missing returns, …)
#   -Wextra            Additional warnings
#   -g                 Debug symbols (file names + line numbers in binary)
#   -O0                Disable optimisations (debugger step-through works correctly)
#   -DRENDER_COORD_MODE=1  Explicit coordinate mode only (no implicit helpers)
FLAGS="-Wall -Wextra -g -O0 -DRENDER_COORD_MODE=1"

if [[ "$DEBUG_BUILD" == true ]]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -DDEBUG"
    echo "⚠  Debug build: AddressSanitizer + UBSan + -DDEBUG enabled"
fi

# ══════ Build ═══════════════════════════════════════════════════════════════

OUTPUT="./build/game"

# Argument order matters to the linker (single-pass):
# flags → output path → source files → libraries last.
# Libraries after sources: the linker resolves symbols in order; placing
# -lm before object files would cause "undefined reference" errors.
clang $FLAGS -o "$OUTPUT" $SOURCES $LIBS

echo "Build successful! Run with: $OUTPUT"

# ══════ Optional Auto-Run ════════════════════════════════════════════════════

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Asteroids ═══"
    echo ""
    exec "$OUTPUT"
fi
