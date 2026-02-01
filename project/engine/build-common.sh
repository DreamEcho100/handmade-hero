#!/bin/bash

# ═══════════════════════════════════════════════════════════════════════════════
# DE100 ENGINE - COMMON BUILD CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════
#
# This file provides shared build configuration for all projects using the
# DE100 engine. Source this file from your project's build.sh.
#
# Usage:
#   source "$SCRIPT_DIR/../engine/build-common.sh"
#
# ═══════════════════════════════════════════════════════════════════════════════

set -e

# ───────────────────────────────────────────────────────────────────────────────
# PATHS
# ───────────────────────────────────────────────────────────────────────────────

DE100_ENGINE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ───────────────────────────────────────────────────────────────────────────────
# OS DETECTION
# ───────────────────────────────────────────────────────────────────────────────

de100_detect_os() {
    local os_name
    os_name="$(uname -s)"

    case "$os_name" in
        Linux*)     echo "linux" ;;
        Darwin*)    echo "macos" ;;
        FreeBSD*)   echo "freebsd" ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *)
            echo "Warning: Unknown OS '$os_name', assuming POSIX-like" >&2
            echo "posix"
            ;;
    esac
}

DE100_OS="$(de100_detect_os)"

# ───────────────────────────────────────────────────────────────────────────────
# COMPILER DETECTION
# ───────────────────────────────────────────────────────────────────────────────

de100_detect_compiler() {
    if command -v clang &> /dev/null; then
        echo "clang"
        return 0
    fi

    echo "Error: clang not found. Please install LLVM/Clang." >&2
    echo "" >&2
    case "$DE100_OS" in
        linux)
            echo "  Ubuntu/Debian: sudo apt install clang" >&2
            echo "  Fedora:        sudo dnf install clang" >&2
            echo "  Arch:          sudo pacman -S clang" >&2
            ;;
        macos)
            echo "  Install Xcode Command Line Tools: xcode-select --install" >&2
            ;;
        freebsd)
            echo "  FreeBSD: pkg install llvm" >&2
            ;;
        windows)
            echo "  Windows: Download from https://releases.llvm.org/" >&2
            echo "           Or: winget install LLVM.LLVM" >&2
            ;;
    esac
    return 1
}

DE100_CC="$(de100_detect_compiler)" || exit 1

# ───────────────────────────────────────────────────────────────────────────────
# PLATFORM-SPECIFIC FILE EXTENSIONS
# ───────────────────────────────────────────────────────────────────────────────

case "$DE100_OS" in
    windows)
        DE100_EXE_EXT=".exe"
        DE100_SHARED_EXT=".dll"
        DE100_SHARED_PREFIX=""
        ;;
    macos)
        DE100_EXE_EXT=""
        DE100_SHARED_EXT=".dylib"
        DE100_SHARED_PREFIX="lib"
        ;;
    *)
        DE100_EXE_EXT=""
        DE100_SHARED_EXT=".so"
        DE100_SHARED_PREFIX="lib"
        ;;
esac

# ───────────────────────────────────────────────────────────────────────────────
# COMPILER FLAGS
# ───────────────────────────────────────────────────────────────────────────────

# Base flags (always applied)
DE100_BASE_FLAGS="-std=c11"

case "$DE100_OS" in
    linux|freebsd|posix)
        DE100_BASE_FLAGS+=" -ffunction-sections -fdata-sections"
        DE100_BASE_FLAGS+=" -D_POSIX_C_SOURCE=200809L"
        ;;
    macos)
        DE100_BASE_FLAGS+=" -ffunction-sections -fdata-sections"
        ;;
    windows)
        DE100_BASE_FLAGS+=" -D_CRT_SECURE_NO_WARNINGS"
        ;;
esac

# Linker flags
case "$DE100_OS" in
    linux|freebsd|posix)
        DE100_LINKER_FLAGS="-Wl,--gc-sections"
        ;;
    macos)
        DE100_LINKER_FLAGS="-Wl,-dead_strip"
        ;;
    windows)
        DE100_LINKER_FLAGS=""
        ;;
esac

# Shared library flags
case "$DE100_OS" in
    linux|freebsd|posix)
        DE100_SHARED_FLAGS="-shared -fPIC -fvisibility=default"
        ;;
    macos)
        DE100_SHARED_FLAGS="-dynamiclib -fPIC -fvisibility=default"
        ;;
    windows)
        DE100_SHARED_FLAGS="-shared -fvisibility=default"
        ;;
esac

# ───────────────────────────────────────────────────────────────────────────────
# ENGINE SOURCE FILES
# ───────────────────────────────────────────────────────────────────────────────

DE100_SRC_MAIN="$DE100_ENGINE_DIR/main.c"

DE100_SRC_COMMON=(
    "$DE100_ENGINE_DIR/engine.c"
    "$DE100_ENGINE_DIR/_common/debug-file-io.c"
    "$DE100_ENGINE_DIR/_common/dll.c"
    "$DE100_ENGINE_DIR/_common/file.c"
    "$DE100_ENGINE_DIR/_common/memory.c"
    "$DE100_ENGINE_DIR/_common/path.c"
    "$DE100_ENGINE_DIR/_common/time.c"
)

DE100_SRC_GAME=(
    "$DE100_ENGINE_DIR/game/audio.c"
    "$DE100_ENGINE_DIR/game/base.c"
    "$DE100_ENGINE_DIR/game/config.c"
    "$DE100_ENGINE_DIR/game/game-loader.c"
    "$DE100_ENGINE_DIR/game/input.c"
    "$DE100_ENGINE_DIR/game/memory.c"
)

DE100_SRC_PLATFORM_COMMON=(
    "$DE100_ENGINE_DIR/platform/_common/input-recording.c"
    "$DE100_ENGINE_DIR/platform/_common/adaptive-fps.c"
    "$DE100_ENGINE_DIR/platform/_common/frame-timing.c"
)

# ───────────────────────────────────────────────────────────────────────────────
# BACKEND CONFIGURATION
# ───────────────────────────────────────────────────────────────────────────────

# Backend source files (set by de100_set_backend)
DE100_SRC_BACKEND=()
DE100_BACKEND_LIBS=""
DE100_BACKEND=""

de100_set_backend() {
    local backend="$1"
    local DE100_INTERNAL="$2"

    if [[ "$DE100_INTERNAL" == "1" ]]; then
        DE100_SRC_PLATFORM_COMMON+=(
            "$DE100_ENGINE_DIR/platform/_common/frame-stats.c"
        )
    fi

    # Auto-select if not specified
    if [[ -z "$backend" || "$backend" == "auto" ]]; then
        case "$DE100_OS" in
            linux|freebsd) backend="x11" ;;
            macos)         backend="macos" ;;
            windows)       backend="win32" ;;
            *)             backend="raylib" ;;
        esac
        echo "Auto-selected backend: $backend"
    fi

    local backend_dir="$DE100_ENGINE_DIR/platform/$backend"

    case "$backend" in
        x11)
            DE100_SRC_BACKEND=(
                "$backend_dir/audio.c"
                "$backend_dir/backend.c"
                "$backend_dir/inputs/joystick.c"
                "$backend_dir/inputs/keyboard.c"
            )
            DE100_BACKEND_LIBS="-lX11 -lXrandr -lGL -lGLX -lasound -ldl"
            ;;
        macos)
            DE100_SRC_BACKEND=(
                "$backend_dir/audio.c"
                "$backend_dir/backend.c"
                "$backend_dir/inputs/joystick.c"
                "$backend_dir/inputs/keyboard.c"
            )
            DE100_BACKEND_LIBS="-framework Cocoa -framework OpenGL -framework AudioToolbox -framework IOKit"
            ;;
        win32)
            DE100_SRC_BACKEND=(
                "$backend_dir/audio.c"
                "$backend_dir/backend.c"
                "$backend_dir/inputs/joystick.c"
                "$backend_dir/inputs/keyboard.c"
            )
            DE100_BACKEND_LIBS="-luser32 -lgdi32 -lopengl32 -lwinmm -lole32"
            ;;
        raylib)
            DE100_SRC_BACKEND=(
                "$backend_dir/audio.c"
                "$backend_dir/backend.c"
                "$backend_dir/inputs/joystick.c"
                "$backend_dir/inputs/keyboard.c"
            )
            case "$DE100_OS" in
                windows) DE100_BACKEND_LIBS="-lraylib -lpthread" ;;
                macos)   DE100_BACKEND_LIBS="-lraylib -lpthread -framework Cocoa -framework IOKit" ;;
                *)       DE100_BACKEND_LIBS="-lraylib -lpthread -ldl" ;;
            esac
            ;;
        *)
            echo "Error: Unknown backend '$backend'" >&2
            echo "Available: x11, macos, win32, raylib, auto" >&2
            return 1
            ;;
    esac

    DE100_BACKEND="$backend"
    return 0
}

# ───────────────────────────────────────────────────────────────────────────────
# HELPER FUNCTIONS
# ───────────────────────────────────────────────────────────────────────────────

# Get executable filename with platform extension
de100_exe_name() {
    echo "${1}${DE100_EXE_EXT}"
}

# Get shared library filename with platform prefix/extension
de100_shared_name() {
    echo "${DE100_SHARED_PREFIX}${1}${DE100_SHARED_EXT}"
}

# Get all platform source files as a single string
de100_get_platform_sources() {
    echo "$DE100_SRC_MAIN ${DE100_SRC_COMMON[*]} ${DE100_SRC_GAME[*]} ${DE100_SRC_PLATFORM_COMMON[*]} ${DE100_SRC_BACKEND[*]}"
}

# Generate -D flags for library paths (passed to C code)
de100_get_lib_defines() {
    local build_dir="$1"
    local main_lib="$2"
    local startup_lib="$3"
    local init_lib="$4"

    local defines=""

    # Library names (just the name, no path)
    defines+=" -DDE100_GAME_MAIN_LIB_NAME=\"$main_lib\""
    defines+=" -DDE100_GAME_STARTUP_LIB_NAME=\"$startup_lib\""
    defines+=" -DDE100_GAME_INIT_LIB_NAME=\"$init_lib\""

    # Full paths
    defines+=" -DDE100_GAME_MAIN_LIB_PATH=\"$build_dir/$(de100_shared_name "$main_lib")\""
    defines+=" -DDE100_GAME_STARTUP_LIB_PATH=\"$build_dir/$(de100_shared_name "$startup_lib")\""
    defines+=" -DDE100_GAME_INIT_LIB_PATH=\"$build_dir/$(de100_shared_name "$init_lib")\""

    # Temp library paths (for hot-reload)
    defines+=" -DDE100_GAME_MAIN_TMP_LIB_PATH=\"$build_dir/$(de100_shared_name "${main_lib}_tmp")\""
    defines+=" -DDE100_GAME_STARTUP_TMP_LIB_PATH=\"$build_dir/$(de100_shared_name "${startup_lib}_tmp")\""
    defines+=" -DDE100_GAME_INIT_TMP_LIB_PATH=\"$build_dir/$(de100_shared_name "${init_lib}_tmp")\""

    echo "$defines"
}

# Print build configuration summary
de100_print_config() {
    echo ""
    echo "DE100 Build Configuration:"
    echo "  OS:       $DE100_OS"
    echo "  Compiler: $DE100_CC"
    echo "  Backend:  $DE100_BACKEND"
    echo ""
}

# ───────────────────────────────────────────────────────────────────────────────
# INITIALIZATION MESSAGE
# ───────────────────────────────────────────────────────────────────────────────

echo "DE100 Engine: Detected OS=$DE100_OS, Compiler=$DE100_CC"
