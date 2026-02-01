#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════════
# HANDMADE HERO - BUILD SCRIPT
# ═══════════════════════════════════════════════════════════════════════════════
#
# Usage:
#   ./build.sh                    # Build everything
#   ./build.sh -r                 # Build and run
#   ./build.sh --build-game       # Build game libraries only
#   ./build.sh --build-platform   # Build platform executable only
#   ./build.sh --backend=raylib   # Use specific backend
#   ./build.sh --sanitize         # Enable sanitizers
#   ./build.sh --sanitize -r      # Build with sanitizers and run
#
# ═══════════════════════════════════════════════════════════════════════════════

set -e

# ───────────────────────────────────────────────────────────────────────────────
# IMPORT ENGINE CONFIGURATION
# ───────────────────────────────────────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../engine/build-common.sh"

# ───────────────────────────────────────────────────────────────────────────────
# PROJECT CONFIGURATION
# ───────────────────────────────────────────────────────────────────────────────

BUILD_DIR="$SCRIPT_DIR/build"
OUT_DIR="$SCRIPT_DIR/out"

# Library names
GAME_MAIN_LIB="main"
GAME_STARTUP_LIB="startup"
GAME_INIT_LIB="init"
PLATFORM_EXE="game"

# ───────────────────────────────────────────────────────────────────────────────
# PARSE ARGUMENTS
# ───────────────────────────────────────────────────────────────────────────────

BACKEND="auto"
ENABLE_SANITIZERS=false
BUILD_GAME=false
BUILD_PLATFORM=false
RUN_AFTER_BUILD=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        --sanitize|--DE100_SANITIZE_WAVE_1_MEMORY=1)
            ENABLE_SANITIZERS=true
        ;;
        --build-game)
            BUILD_GAME=true
        ;;
        --build-platform)
            BUILD_PLATFORM=true
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        -R|--run-only)
            RUN_AFTER_BUILD=true
            BUILD_GAME=false
            BUILD_PLATFORM=false
        ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -r, --run         Run the game after building"
            echo "  -R, --run-only    Run the game without building"
            echo "  --backend=NAME    Set backend (x11, macos, win32, raylib, auto)"
            echo "  --sanitize        Enable address/leak/undefined sanitizers"
            echo "  --build-game      Build game libraries only"
            echo "  --build-platform  Build platform executable only"
            echo "  -h, --help        Show this help"
            echo ""
            echo "Examples:"
            echo "  $0                    # Build everything"
            echo "  $0 -r                 # Build and run"
            echo "  $0 --sanitize -r      # Build with sanitizers and run"
            echo "  $0 --build-game -r    # Build game libs and run"
            exit 0
        ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
        ;;
    esac
    shift
done

# Default: build everything if no specific target requested
if [[ "$BUILD_GAME" == false && "$BUILD_PLATFORM" == false ]]; then
    BUILD_GAME=true
    BUILD_PLATFORM=true
fi

# ───────────────────────────────────────────────────────────────────────────────
# SETUP BUILD ENVIRONMENT
# ───────────────────────────────────────────────────────────────────────────────

mkdir -p "$BUILD_DIR" "$OUT_DIR"

DE100_INTERNAL=1

# Set backend
de100_set_backend "$BACKEND" $DE100_INTERNAL || exit 1

# ───────────────────────────────────────────────────────────────────────────────
# COMPILER FLAGS
# ───────────────────────────────────────────────────────────────────────────────

# Common flags for this project
COMMON_FLAGS="$DE100_BASE_FLAGS"
COMMON_FLAGS="$COMMON_FLAGS -I$SCRIPT_DIR/src"
COMMON_FLAGS="$COMMON_FLAGS -g -O0"
COMMON_FLAGS="$COMMON_FLAGS -fno-omit-frame-pointer"  # Better stack traces
COMMON_FLAGS="$COMMON_FLAGS -D_DEBUG"         # Define _DEBUG like MSVC does
COMMON_FLAGS="$COMMON_FLAGS -Wall -Wextra -Werror"
COMMON_FLAGS="$COMMON_FLAGS -DDE100_INTERNAL=$DE100_INTERNAL -DDE100_SLOW=1"

# Sanitizer flags
SANITIZE_FLAGS=""
if [[ "$ENABLE_SANITIZERS" == true ]]; then
    case "$DE100_OS" in
        linux|macos|freebsd)
            SANITIZE_FLAGS="-fsanitize=address,leak,undefined"
            SANITIZE_FLAGS="$SANITIZE_FLAGS -DDE100_SANITIZE_WAVE_1_MEMORY=1"
            echo "Sanitizers: ENABLED"
        ;;
        windows)
            echo "Warning: Sanitizers have limited support on Windows, skipping"
        ;;
    esac
fi

# ───────────────────────────────────────────────────────────────────────────────
# OUTPUT PATHS
# ───────────────────────────────────────────────────────────────────────────────

MAIN_LIB_PATH="$BUILD_DIR/$(de100_shared_name "$GAME_MAIN_LIB")"
STARTUP_LIB_PATH="$BUILD_DIR/$(de100_shared_name "$GAME_STARTUP_LIB")"
INIT_LIB_PATH="$BUILD_DIR/$(de100_shared_name "$GAME_INIT_LIB")"
PLATFORM_EXE_PATH="$BUILD_DIR/$(de100_exe_name "$PLATFORM_EXE")"

# ───────────────────────────────────────────────────────────────────────────────
# BUILD GAME LIBRARIES
# ───────────────────────────────────────────────────────────────────────────────

build_game_lib() {
    local name="$1"
    local source="$2"
    local output="$3"
    
    echo "Building: $name"
    
    rm -f "$output"
    
    $DE100_CC "$source" -o "$output" \
    $COMMON_FLAGS \
    $SANITIZE_FLAGS \
    $DE100_SHARED_FLAGS \
    -lm
    
    if [[ -f "$output" ]]; then
        touch "$output"  # Force update modification time
        echo "  -> $output"
        
        # Show exported symbols on supported platforms
        case "$DE100_OS" in
            linux|freebsd)
                echo "  Symbols: $(nm -D "$output" 2>/dev/null | grep -c ' T ' || echo 0) exported"
            ;;
            macos)
                echo "  Symbols: $(nm "$output" 2>/dev/null | grep -c ' T ' || echo 0) exported"
            ;;
        esac
    else
        echo "Error: Failed to build $name"
        exit 1
    fi
}

if [[ "$BUILD_GAME" == true ]]; then
    echo ""
    echo "═══ Building Game Libraries ═══"
    echo ""
    
    build_game_lib "lib-main"    "$SCRIPT_DIR/src/main.c"    "$MAIN_LIB_PATH"
    build_game_lib "lib-startup" "$SCRIPT_DIR/src/startup.c" "$STARTUP_LIB_PATH"
    build_game_lib "lib-init"    "$SCRIPT_DIR/src/init.c"    "$INIT_LIB_PATH"
fi

# ───────────────────────────────────────────────────────────────────────────────
# BUILD PLATFORM EXECUTABLE
# ───────────────────────────────────────────────────────────────────────────────

if [[ "$BUILD_PLATFORM" == true ]]; then
    echo ""
    echo "═══ Building Platform Executable ═══"
    echo ""
    
    # Library path defines for C code
    LIB_DEFINES=$(de100_get_lib_defines "$BUILD_DIR" "$GAME_MAIN_LIB" "$GAME_STARTUP_LIB" "$GAME_INIT_LIB")
    
    # Platform-specific linker flags
    PLATFORM_LINK_FLAGS="$DE100_LINKER_FLAGS"
    case "$DE100_OS" in
        linux|freebsd)
            PLATFORM_LINK_FLAGS="$PLATFORM_LINK_FLAGS -rdynamic"
            PLATFORM_LINK_FLAGS="$PLATFORM_LINK_FLAGS -Wl,-Map=$BUILD_DIR/game.map"
            PLATFORM_LINK_FLAGS="$PLATFORM_LINK_FLAGS -Wl,-rpath,$BUILD_DIR"
        ;;
        macos)
            PLATFORM_LINK_FLAGS="$PLATFORM_LINK_FLAGS -Wl,-rpath,@executable_path"
        ;;
    esac
    
    # Get all source files
    PLATFORM_SOURCES=$(de100_get_platform_sources)
    
    echo "Compiling platform..."
    echo "  Sources: $(echo $PLATFORM_SOURCES | wc -w) files"
    
    $DE100_CC $PLATFORM_SOURCES -o "$PLATFORM_EXE_PATH" \
    $COMMON_FLAGS \
    $SANITIZE_FLAGS \
    $LIB_DEFINES \
    $PLATFORM_LINK_FLAGS \
    -L"$BUILD_DIR" \
    -lm \
    $DE100_BACKEND_LIBS
    
    if [[ -f "$PLATFORM_EXE_PATH" ]]; then
        echo "  -> $PLATFORM_EXE_PATH"
        [[ -f "$BUILD_DIR/game.map" ]] && echo "  -> $BUILD_DIR/game.map"
    else
        echo "Error: Failed to build platform executable"
        exit 1
    fi
fi

# ───────────────────────────────────────────────────────────────────────────────
# BUILD SUMMARY
# ───────────────────────────────────────────────────────────────────────────────

echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "Build Complete!"
echo ""
echo "  OS:         $DE100_OS"
echo "  Compiler:   $DE100_CC"
echo "  Backend:    $DE100_BACKEND"
echo "  Sanitizers: $( [[ "$ENABLE_SANITIZERS" == true ]] && echo "enabled" || echo "disabled" )"
echo ""
if [[ "$BUILD_GAME" == true ]]; then
    echo "  Game Libraries:"
    echo "    $MAIN_LIB_PATH"
    echo "    $STARTUP_LIB_PATH"
    echo "    $INIT_LIB_PATH"
fi
if [[ "$BUILD_PLATFORM" == true ]]; then
    echo "  Platform:"
    echo "    $PLATFORM_EXE_PATH"
fi
echo ""
echo "═══════════════════════════════════════════════════════════════════════════"

# ───────────────────────────────────────────────────────────────────────────────
# RUN (if requested)
# ───────────────────────────────────────────────────────────────────────────────

if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "═══ Running Game ═══"
    echo ""
    exec "$PLATFORM_EXE_PATH"
fi
