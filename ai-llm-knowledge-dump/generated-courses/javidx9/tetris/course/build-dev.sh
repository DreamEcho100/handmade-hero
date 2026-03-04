#!/bin/bash
# =============================================================================
# build-dev.sh — Development Build Script for Tetris
# =============================================================================
#
# This script compiles the Tetris game for development (not production).
# It selects the rendering/windowing backend, collects source files, and
# invokes the C compiler with developer-friendly flags.
#
# HOW TO USE:
#   ./build-dev.sh                        # build with default (raylib) backend
#   ./build-dev.sh --backend=x11          # build with X11 (Linux native) backend
#   ./build-dev.sh -r                     # build AND immediately run
#   ./build-dev.sh --debug                # build with address/UB sanitizers
#   ./build-dev.sh --backend=raylib -r    # build raylib backend and run it
#
# =============================================================================

# `set -e` tells bash: "exit immediately if ANY command fails".
# Without this, the script would silently continue even if `clang` returned
# an error — you'd see "Done!" even after a compile failure.
set -e

# Create the build output directory.
# `-p` means "don't error if it already exists, create parents too".
# JS equivalent: fs.mkdirSync('build', { recursive: true })
mkdir -p build

# ══════ Default Configuration ══════

# Which rendering/windowing backend to use.
# Supported values: "raylib", "x11"
# - raylib: Cross-platform (Windows, macOS, Linux). Uses raylib library.
# - x11:    Linux-only. Uses raw X11, OpenGL, and ALSA (audio).
BACKEND="raylib"

# Should we launch the game immediately after a successful build?
RUN_AFTER_BUILD=false

# Should we enable sanitizers (extra runtime checks for bugs)?
# Enabled by passing --debug flag.
DEBUG_BUILD=false

# ══════ Argument Parsing ══════

# `$#` is the number of command-line arguments passed to this script.
# `[[ $# -gt 0 ]]` means "while there are still arguments left to process".
while [[ $# -gt 0 ]]; do
    case "$1" in
        # --backend=x11 or --backend=raylib
        # `${1#*=}` strips everything up to and including the `=` sign,
        # leaving just the value (e.g. "x11" or "raylib").
        --backend=*)
            BACKEND="${1#*=}"
        ;;

        # -r or --run: set the run flag
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;

        # --debug: enable address sanitizer + undefined-behaviour sanitizer.
        # These are runtime checkers that catch:
        #   - AddressSanitizer (ASan): buffer overflows, use-after-free, etc.
        #   - UndefinedBehaviorSanitizer (UBSan): signed overflow, null deref, etc.
        # These add ~2× overhead but catch bugs that would otherwise be silent.
        --debug)
            DEBUG_BUILD=true
        ;;

        # --help / -h: print usage and exit cleanly
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --backend=NAME   Select backend: x11, raylib (default: raylib)"
            echo "  -r, --run        Run the game after a successful build"
            echo "  --debug          Enable AddressSanitizer + UBSan (slower but safer)"
            echo "  --help, -h       Show this help message"
        ;;

        # Anything else is unknown — fail loudly.
        # `>&2` redirects the error message to stderr (standard error stream),
        # keeping stdout clean for scripts that might parse our output.
        *)
            echo "Unknown option: $1" >&2
            echo "Use --help for usage information" >&2
            exit 1
        ;;
    esac
    # `shift` discards $1 and moves $2→$1, $3→$2, etc.
    # This is how bash loops consume arguments one-by-one.
    shift
done

# ══════ Common Libraries & Sources ══════

# Libraries that every backend needs:
#   -lm  links the C math library (sin, cos, sqrt, etc.)
#        In C, the math functions are NOT automatically linked — you must ask for them.
#        JS has Math built in; C splits it into a separate library.
DE100_BACKEND_LIBS="-lm"

# Source files that are always compiled regardless of backend.
# These are the platform-independent game logic and utility files.
SOURCES="src/game.c src/audio.c src/utils/draw-shapes.c src/utils/draw-text.c"

# ══════ Backend Selection ══════

case "$BACKEND" in
    # ── X11 Backend (Linux only) ──────────────────────────────────────────
    # Communicates directly with the X Window System on Linux.
    # Libraries required:
    #   -lX11       X11 core library: creates windows, handles events
    #   -lxkbcommon XKB keyboard layout handling (key names, not scan codes)
    #   -lGL        OpenGL: hardware-accelerated 2D/3D rendering
    #   -lGLX       GLX: bridge between OpenGL and X11 (creates GL context in X11)
    #   -lasound    ALSA (Advanced Linux Sound Architecture): audio playback
    x11)
        DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lX11 -lxkbcommon -lGL -lGLX -lasound"
        SOURCES="$SOURCES src/main_x11.c"
    ;;

    # ── Raylib Backend (cross-platform) ──────────────────────────────────
    # Raylib is a higher-level library that handles windowing, input, audio,
    # and rendering on Windows, macOS, and Linux with one API.
    # Libraries required on each platform:
    #   -lraylib   The raylib library itself
    #   -lpthread  POSIX threads (raylib uses background threads internally)
    #   -ldl       Dynamic linking (dlopen/dlsym — raylib loads GL at runtime)
    #
    # macOS extras:
    #   -framework Cocoa    macOS windowing and UI framework
    #   -framework IOKit    Hardware I/O (gamepad, HID devices)
    raylib)
        case "$DE100_OS" in
            windows) DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lraylib -lpthread -ldl" ;;
            macos)   DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lraylib -lpthread -framework Cocoa -framework IOKit" ;;
            # Default covers Linux and anything else
            *)       DE100_BACKEND_LIBS="$DE100_BACKEND_LIBS -lraylib -lpthread -ldl" ;;
        esac
        SOURCES="$SOURCES src/main_raylib.c"
    ;;

    # ── Unknown backend → fail loudly ────────────────────────────────────
    *)
        echo "Error: Unknown backend '$BACKEND'" >&2
        echo "Available: x11, raylib" >&2
        exit 1
    ;;
esac

# ══════ Compiler Flags ══════

# Core developer flags — these are intentionally NOT the flags you'd use in
# a release build. Here's what each one does:
#
#   -Wall       "Warn all": enable the most common warnings.
#               Catches things like unused variables, missing return values.
#               JS equivalent: ESLint with "recommended" ruleset.
#
#   -Wextra     "Warn extra": enable additional warnings beyond -Wall.
#               Catches things like unused parameters, sign comparison issues.
#               JS equivalent: ESLint with stricter rules enabled.
#
#   -g          "Generate debug info": embed variable names, line numbers, and
#               source file paths into the binary so debuggers (GDB, LLDB) and
#               tools like AddressSanitizer can show you exactly where a bug
#               occurred in your source code.
#               Without -g, error messages say "crash at 0x4012a3"; with -g
#               they say "crash at draw_rect in draw-shapes.c line 18".
#
#   -O0         "Optimisation level 0": disable ALL compiler optimisations.
#               This is critical during development because optimisations can
#               reorder, inline, or eliminate code — making it very hard to
#               step through with a debugger.
#               Release builds use -O2 or -O3 for speed. Never use those while
#               developing, as they break debugger step-through.
FLAGS="-Wall -Wextra -g -O0"

# ── Debug Sanitizers (optional) ──────────────────────────────────────────────
# If --debug was passed, append sanitizer flags.
#
# -fsanitize=address        AddressSanitizer: detects heap buffer overflow,
#                           stack buffer overflow, use-after-free, double-free.
#                           Inserts runtime "shadow memory" checks around every
#                           allocation and access.
#
# -fsanitize=undefined      UndefinedBehaviorSanitizer: detects C undefined
#                           behaviour at runtime: signed integer overflow,
#                           null pointer dereference, misaligned access, etc.
#
# These are NOT suitable for shipping — they add ~2× runtime overhead and
# significantly increase binary size. Use them during development only.
if [[ "$DEBUG_BUILD" == true ]]; then
    FLAGS="$FLAGS -fsanitize=address,undefined"
    echo "⚠  Debug build: AddressSanitizer + UBSan enabled"
fi

# ══════ Build ══════

BINARY="game"
PLATFORM_EXE_PATH="./build/${BINARY}"

# Invoke the compiler.
# clang is our C compiler (an alternative to gcc with better error messages).
# Argument order matters: flags first, then output path, then sources, then libs.
# Libraries MUST come LAST — the linker is single-pass and resolves symbols
# in order. Putting -lm before sources would cause "undefined reference" errors.
clang $FLAGS -o "$PLATFORM_EXE_PATH" $SOURCES $DE100_BACKEND_LIBS
echo "Done! Run with: $PLATFORM_EXE_PATH"

# ══════ Optional Auto-Run ══════

# If -r / --run was passed, launch the game immediately after a successful build.
# `exec` replaces the shell process with the game process (instead of forking a
# child). This means the shell exits cleanly and the game's exit code becomes
# the script's exit code — useful for CI pipelines.
if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Game ═══"
    echo ""
    exec "$PLATFORM_EXE_PATH"
fi
