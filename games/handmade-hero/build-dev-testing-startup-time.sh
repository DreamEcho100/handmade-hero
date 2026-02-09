#!/bin/bash

echo "=== Testing startup time ==="
echo ""

echo "1. Build time:"
time ./build-dev.sh --build-game --build-platform 2>/dev/null

echo ""
echo "2. Game startup time (run for 1 second then kill):"
timeout 1s ./build/game 2>&1 | head -30

echo ""
echo "3. Direct game execution time:"
time (./build/game &
    PID=$!
    sleep 0.5
kill $PID 2>/dev/null)
