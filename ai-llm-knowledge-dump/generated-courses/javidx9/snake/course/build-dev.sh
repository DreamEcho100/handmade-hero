#!/bin/bash
# =============================================================================
# build-dev.sh — Development Build Script for Snake
# =============================================================================
#
# WHAT IS THIS FILE?
# ──────────────────
# This script compiles the Snake game for development.  It selects the
# rendering/windowing backend, gathers source files, and calls the C compiler
# with developer-friendly flags.
#
# COURSE NOTE: We deviate from the reference source (which uses two separate
# scripts: build_x11.sh and build_raylib.sh) in favour of a single unified
# script with a --backend flag.  This mirrors the Tetris course and shows how
# a small project can support multiple platforms without duplicating 90% of the
# same build logic.  See COURSE-BUILDER-IMPROVEMENTS.md for the full rationale.
#
# HOW TO USE:
#   ./build-dev.sh                      # build with default (raylib) backend
#   ./build-dev.sh --backend=x11        # build with X11 (Linux native) backend
#   ./build-dev.sh -r                   # build AND immediately run the game
#   ./build-dev.sh --debug              # build with address/UB sanitizers
#   ./build-dev.sh --backend=raylib -r  # build raylib backend and run
#   ./build-dev.sh -d -r                # debug build and run
#
# PREREQUISITES — install with apt before first build:
#   Raylib backend:  sudo apt install libraylib-dev
#   X11 backend:     sudo apt install libx11-dev libxkbcommon-dev libasound2-dev
#   (OpenGL/GLX is provided by your GPU driver or mesa)
#
# =============================================================================

# `set -e` — exit immediately if any command fails.
# Without this, the script would continue after a compile error and print
# "Done!" even though the build failed.
# JS equivalent: there's no direct equivalent; it's like having every line
# of a Node script wrapped in a try/catch that re-throws.
set -e

# Create the build output directory.
# `-p` = don't error if it already exists; create parent dirs as needed.
# JS equivalent: fs.mkdirSync('build', { recursive: true })
mkdir -p build

# ══════ Default Configuration ══════════════════════════════════════════════

# Which rendering/windowing backend to use.
# Supported values: "raylib", "x11"
#   raylib — cross-platform (Linux/macOS/Windows), higher-level API
#   x11    — Linux only, raw X11 + OpenGL + ALSA, closer to the metal
BACKEND="raylib"

# Should we run the game immediately after a successful build?
RUN_AFTER_BUILD=false

# Should we enable debug sanitizers?
DEBUG_BUILD=false

# ══════ Argument Parsing ═══════════════════════════════════════════════════

# `$#` = number of arguments passed to the script.
# We loop until all arguments are consumed.
while [[ $# -gt 0 ]]; do
    case "$1" in
        # --backend=x11  or  --backend=raylib
        # `${1#*=}` strips everything up to and including the first `=`,
        # leaving just the value.  Example: "--backend=x11" → "x11".
        --backend=*)
            BACKEND="${1#*=}"
            ;;

        # -r or --run: set flag to run after build
        -r|--run)
            RUN_AFTER_BUILD=true
            ;;

        # -d, --debug: enable address sanitizer + undefined-behaviour sanitizer.
        # These add ~2× runtime overhead but catch:
        #   AddressSanitizer (ASan): buffer overflows, use-after-free, leaks.
        #   UBSan: signed overflow, null pointer dereference, bad casts.
        # Always use during development; never ship with these on.
        -d|--debug)
            DEBUG_BUILD=true
            ;;

        # --help / -h: print usage and exit
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --backend=NAME   Select backend: x11, raylib (default: raylib)"
            echo "  -r, --run        Run the game after a successful build"
            echo "  -d, --debug      Enable AddressSanitizer + UBSan"
            echo "  --help, -h       Show this help message"
            exit 0
            ;;

        # Unknown flag — fail loudly so the user knows what went wrong.
        # `>&2` = write to stderr (standard error stream), not stdout.
        *)
            echo "Unknown option: $1" >&2
            echo "Use --help for usage information" >&2
            exit 1
            ;;
    esac
    # `shift` removes $1 and renumbers remaining args: $2→$1, $3→$2, etc.
    shift
done

# ══════ Common Sources & Libraries ═════════════════════════════════════════

# Source files shared by ALL backends (platform-independent game code).
# These never include X11 or Raylib headers — they compile identically
# regardless of which backend we link against.
SOURCES="src/game.c src/audio.c src/utils/draw-shapes.c src/utils/draw-text.c"

# Libraries every backend needs.
#   -lm  C math library (for audio synthesis: we use only basic math here).
#        Unlike JS, C doesn't include math functions automatically — you must
#        explicitly link against libm.
LIBS="-lm"

# ══════ Backend Selection ══════════════════════════════════════════════════

case "$BACKEND" in
    # ── X11 Backend (Linux only) ──────────────────────────────────────────
    # Communicates directly with the X Window System.
    # Libraries:
    #   -lX11       X11 core: open display, create window, receive events
    #   -lxkbcommon XKB keyboard handling (key names, not hardware scan codes)
    #   -lGL        OpenGL rendering (the GPU driver provides this)
    #   -lGLX       GLX: OpenGL context creation inside an X11 window
    #   -lasound    ALSA: Linux low-level audio API (Advanced Linux Sound Arch.)
    x11)
        LIBS="$LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound"
        SOURCES="$SOURCES src/main_x11.c"
        ;;

    # ── Raylib Backend (cross-platform) ──────────────────────────────────
    # Raylib handles windowing, input, rendering, and audio in one library.
    # Platform-specific extra libraries are added via inner case.
    raylib)
        case "$(uname -s)" in
            Darwin) LIBS="$LIBS -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            *)      LIBS="$LIBS -lraylib -lpthread -ldl" ;;
        esac
        SOURCES="$SOURCES src/main_raylib.c"
        ;;

    # ── Unknown backend → fail loudly ────────────────────────────────────
    *)
        echo "Error: Unknown backend '$BACKEND'" >&2
        echo "Available backends: x11, raylib" >&2
        exit 1
        ;;
esac

# ══════ Compiler Flags ══════════════════════════════════════════════════════

# Development flags — these are NOT what you'd use for a release build:
#
#   -Wall       Enable most common warnings (unused vars, missing returns, …)
#   -Wextra     Enable additional warnings (-Wall's more opinionated sibling)
#   -g          Embed debug symbols: file names + line numbers in the binary
#               Without this, debugger shows raw addresses, not source lines
#   -O0         Disable ALL optimisations so debugger step-through works right
#               (Optimised code reorders/eliminates statements — very confusing
#               to single-step through)
FLAGS="-Wall -Wextra -g -O0"

# ── Optional Debug Sanitizers ────────────────────────────────────────────────
# Append sanitizer flags when --debug is passed.
# NOT suitable for release — ~2× runtime overhead and larger binary.
if [[ "$DEBUG_BUILD" == true ]]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -DDEBUG"
    echo "⚠  Debug build: AddressSanitizer + UBSan + -DDEBUG enabled"
fi

# ══════ Build ═══════════════════════════════════════════════════════════════

OUTPUT="./build/snake"

# Invoke the C compiler.
# Argument order matters to the linker (single-pass):
#   flags first → then output path → then source files → then libraries last
# Putting -lm before sources would cause "undefined reference" errors because
# the linker resolves symbols in order; libraries must come after object files.
clang $FLAGS -o "$OUTPUT" $SOURCES $LIBS

echo "Build successful! Run with: $OUTPUT"

# ══════ Optional Auto-Run ════════════════════════════════════════════════════

# If -r / --run was passed, launch the game immediately after a successful build.
# `exec` replaces this shell process with the game process — the shell exits
# cleanly and the game's exit code becomes the script's exit code.
if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Snake ═══"
    echo ""
    exec "$OUTPUT"
fi
