#!/bin/bash
set -e

# ═══════════════════════════════════════════════════════════════════════════
# build-dev.sh — Build script for Memory Management in C course
# ═══════════════════════════════════════════════════════════════════════════
#
# Usage:
#   ./build-dev.sh --lesson=NN [-r|--run] [--bench] [--valgrind] [--all]
#
# Examples:
#   ./build-dev.sh --lesson=01 -r            # Build + run lesson 01 (debug)
#   ./build-dev.sh --lesson=10 --bench -r    # Build + run lesson 10 (bench)
#   ./build-dev.sh --lesson=07 --valgrind -r # Build + run under valgrind
#   ./build-dev.sh --all                     # Build all lessons
# ═══════════════════════════════════════════════════════════════════════════

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build"

# ── Defaults ──────────────────────────────────────────────────────────────
LESSON=""
RUN=0
BENCH=0
VALGRIND=0
MASSIF=0
NO_ASAN=0
BUILD_ALL=0
CC="${CC:-clang}"

# ── Parse arguments ───────────────────────────────────────────────────────
for arg in "$@"; do
  case "$arg" in
    --lesson=*)  LESSON="${arg#--lesson=}" ;;
    -r|--run)    RUN=1 ;;
    --bench)     BENCH=1 ;;
    --valgrind)  VALGRIND=1 ;;
    --massif)    MASSIF=1 ;;
    --no-asan)   NO_ASAN=1 ;;
    --all)       BUILD_ALL=1 ;;
    --help|-h)
      echo "Usage: $0 --lesson=NN [-r|--run] [--bench] [--valgrind] [--all]"
      echo ""
      echo "  --lesson=NN    Build lesson NN (01-30)"
      echo "  --all          Build all lessons"
      echo "  -r, --run      Run the binary after build"
      echo "  --bench        Optimized build with BENCH_MODE (no sanitizers)"
      echo "  --valgrind     Run under valgrind memcheck (disables ASan)"
      echo "  --massif       Run under valgrind massif (disables ASan)"
      echo "  --no-asan      Disable AddressSanitizer"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg"
      echo "Run $0 --help for usage."
      exit 1
      ;;
  esac
done

if [ -z "$LESSON" ] && [ "$BUILD_ALL" -eq 0 ]; then
  echo "Error: specify --lesson=NN or --all"
  echo "Run $0 --help for usage."
  exit 1
fi

mkdir -p "$BUILD_DIR"

# ── Compiler flags ────────────────────────────────────────────────────────
COMMON_FLAGS="-Wall -Wextra -Wno-unused-parameter -std=c11 -I$SRC_DIR"
LIBS="-lm -lpthread"

if [ "$BENCH" -eq 1 ]; then
  # Optimized, no sanitizers
  CFLAGS="$COMMON_FLAGS -O2 -DNDEBUG -DBENCH_MODE"
elif [ "$VALGRIND" -eq 1 ] || [ "$MASSIF" -eq 1 ] || [ "$NO_ASAN" -eq 1 ]; then
  # Debug but no sanitizers (valgrind conflicts with ASan)
  CFLAGS="$COMMON_FLAGS -O0 -g -DDEBUG"
else
  # Full debug with sanitizers
  CFLAGS="$COMMON_FLAGS -O0 -g -DDEBUG -fsanitize=address,undefined"
  LIBS="$LIBS -fsanitize=address,undefined"
fi

# ── Build function ────────────────────────────────────────────────────────
build_lesson() {
  local num="$1"
  local src_file="$SRC_DIR/lessons/lesson_${num}.c"
  local out_file="$BUILD_DIR/lesson_${num}"

  if [ ! -f "$src_file" ]; then
    echo "  SKIP  lesson_${num} (no source file)"
    return 1
  fi

  # Parse BUILD_DEPS from the source file
  local deps=""
  local dep_line
  dep_line=$(grep -m1 'BUILD_DEPS:' "$src_file" 2>/dev/null || true)
  if [ -n "$dep_line" ]; then
    deps=$(echo "$dep_line" | sed 's|.*BUILD_DEPS:||' | tr ',' ' ')
    # Prefix each dep with SRC_DIR if not absolute
    local resolved_deps=""
    for d in $deps; do
      d=$(echo "$d" | xargs)  # trim whitespace
      if [ -n "$d" ]; then
        resolved_deps="$resolved_deps $SRC_DIR/$d"
      fi
    done
    deps="$resolved_deps"
  fi

  # Parse BUILD_LIBS from the source file
  local extra_libs=""
  local libs_line
  libs_line=$(grep -m1 'BUILD_LIBS:' "$src_file" 2>/dev/null || true)
  if [ -n "$libs_line" ]; then
    extra_libs=$(echo "$libs_line" | sed 's|.*BUILD_LIBS:||')
  fi

  echo "  BUILD lesson_${num}"
  $CC $CFLAGS -o "$out_file" "$src_file" $deps $LIBS $extra_libs
  echo "  OK    $out_file"
}

# ── Build ─────────────────────────────────────────────────────────────────
if [ "$BUILD_ALL" -eq 1 ]; then
  echo "=== Building all lessons ==="
  fail_count=0
  pass_count=0
  for f in "$SRC_DIR"/lessons/lesson_*.c; do
    [ -f "$f" ] || continue
    num=$(basename "$f" | sed 's/lesson_//' | sed 's/\.c//')
    if build_lesson "$num"; then
      pass_count=$((pass_count + 1))
    else
      fail_count=$((fail_count + 1))
    fi
  done
  echo "=== Done: $pass_count built, $fail_count skipped ==="
else
  # Zero-pad lesson number to 2 digits
  LESSON=$(printf "%02d" "$((10#$LESSON))")
  build_lesson "$LESSON"

  OUT="$BUILD_DIR/lesson_${LESSON}"

  # ── Run ───────────────────────────────────────────────────────────────
  if [ "$RUN" -eq 1 ]; then
    echo ""
    if [ "$VALGRIND" -eq 1 ]; then
      echo "=== Running under valgrind memcheck ==="
      valgrind --tool=memcheck --leak-check=full --track-origins=yes \
               --show-reachable=yes "$OUT"
    elif [ "$MASSIF" -eq 1 ]; then
      echo "=== Running under valgrind massif ==="
      valgrind --tool=massif "$OUT"
      echo "(massif output written to massif.out.*)"
    else
      echo "=== Running lesson_${LESSON} ==="
      "$OUT"
    fi
  fi
fi
