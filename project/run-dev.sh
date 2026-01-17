#!/usr/bin/env bash

BACKEND="x11"
HANDMADE_SANITIZE_WAVE_1_MEMORY="0"

FLAGS=""
# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
            BACKEND_SET=1
            ;;
        --HANDMADE_SANITIZE_WAVE_1_MEMORY=*)
            HANDMADE_SANITIZE_WAVE_1_MEMORY="${1#*=}"
            if [ -n "$HANDMADE_SANITIZE_WAVE_1_MEMORY" ]; then
                HANDMADE_SANITIZE_WAVE_1_MEMORY=$HANDMADE_SANITIZE_WAVE_1_MEMORY
            fi
            ;;
        *)
            ;;
    esac
    shift
done

FLAGS="--backend=$BACKEND --HANDMADE_SANITIZE_WAVE_1_MEMORY=$HANDMADE_SANITIZE_WAVE_1_MEMORY"

echo "Running with FLAGS: $FLAGS"

mkdir -p build out

./build.sh $FLAGS && ./build/game
