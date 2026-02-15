#!/bin/bash
# Format all C source files, excluding build and _ignore directories

find engine games -name '*.c' -o -name '*.h' | grep -v -E '/(build|_ignore)/' | xargs clang-format -i

echo "✅ Formatted all .c and .h files"
