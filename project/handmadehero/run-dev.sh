#!/usr/bin/env bash

BACKEND="x11"
HANDMADE_SANITIZE_WAVE_1_MEMORY="0"
SHOULD_BUILD_GAME=false
SHOULD_BUILD_PLATFORM=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
            ;;
        --HANDMADE_SANITIZE_WAVE_1_MEMORY=*)
            HANDMADE_SANITIZE_WAVE_1_MEMORY="${1#*=}"
            ;;
        --build-game)
            SHOULD_BUILD_GAME=true
            ;;
        --build-platform)
            SHOULD_BUILD_PLATFORM=true
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
    shift
done

FLAGS="--backend=$BACKEND --HANDMADE_SANITIZE_WAVE_1_MEMORY=$HANDMADE_SANITIZE_WAVE_1_MEMORY"

if [ "$SHOULD_BUILD_GAME" = true ]; then
    FLAGS="$FLAGS --build-game"
fi
if [ "$SHOULD_BUILD_PLATFORM" = true ]; then
    FLAGS="$FLAGS --build-platform"
fi

echo "Running with FLAGS: $FLAGS"

mkdir -p build out

./build.sh $FLAGS && ./build/game
