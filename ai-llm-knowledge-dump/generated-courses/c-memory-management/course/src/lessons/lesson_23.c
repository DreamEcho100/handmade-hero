/* lesson_23.c — Valgrind Memcheck Deep Dive
 * BUILD_DEPS: common/bench.c
 *
 * Intentionally buggy programs to teach reading Valgrind output.
 * Each sub-program triggers a specific class of memory error.
 * Run under Valgrind to see the diagnostic output.
 *
 * Run:   ./build-dev.sh --lesson=23 --no-asan -r
 * Usage: ./build/lesson_23 <test>
 *   Tests: leak, uaf, uninit, overlap, invalid-write, double-free
 *
 * Valgrind: ./build-dev.sh --lesson=23 --valgrind -r
 * (must build with --no-asan since ASan and Valgrind conflict)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Sub-program: memory leak ──────────────────────────────────────────────
 *
 * Expected Valgrind output:
 *   ==PID== HEAP SUMMARY:
 *   ==PID==     in use at exit: 1,024 bytes in 1 blocks
 *   ==PID==   total heap usage: 1 allocs, 0 frees, 1,024 bytes allocated
 *   ==PID==
 *   ==PID== 1,024 bytes in 1 blocks are definitely lost in loss record 1 of 1
 *   ==PID==    at 0x...: malloc (vg_replace_malloc.c:...)
 *   ==PID==    by 0x...: demo_leak (lesson_23.c:NN)
 *   ==PID==    by 0x...: main (lesson_23.c:NN)
 */
static void demo_leak(void) {
  printf("  Allocating 1024 bytes and 'forgetting' to free...\n");
  char *buf = malloc(1024);
  if (!buf) return;

  /* We write to it so the allocation isn't optimized away */
  memset(buf, 'A', 1024);
  printf("  buf[0] = '%c'\n", buf[0]);
  printf("  Returning WITHOUT free(buf) — this is a leak.\n");
  printf("\n  Valgrind will report:\n");
  printf("    \"1,024 bytes in 1 blocks are definitely lost\"\n");
  printf("    with a stack trace pointing to this function.\n");
  /* Intentionally no free(buf) */
}

/* ── Sub-program: use-after-free ───────────────────────────────────────────
 *
 * Expected Valgrind output:
 *   ==PID== Invalid read of size 1
 *   ==PID==    at 0x...: demo_uaf (lesson_23.c:NN)
 *   ==PID==  Address 0x... is 0 bytes inside a block of size 64 free'd
 *   ==PID==    at 0x...: free (vg_replace_malloc.c:...)
 *   ==PID==    by 0x...: demo_uaf (lesson_23.c:NN)
 */
static void demo_uaf(void) {
  printf("  Allocating 64 bytes, freeing, then reading...\n");
  char *buf = malloc(64);
  if (!buf) return;

  memset(buf, 'X', 64);
  printf("  Before free: buf[0] = '%c'\n", buf[0]);

  free(buf);
  printf("  Called free(buf).\n");

  /* Use-after-free: reading freed memory */
  volatile char val = buf[0];
  printf("  After free: buf[0] = 0x%02x (use-after-free!)\n",
         (unsigned char)val);
  printf("\n  Valgrind will report:\n");
  printf("    \"Invalid read of size 1\"\n");
  printf("    \"Address 0x... is 0 bytes inside a block of size 64 free'd\"\n");
  printf("    It shows BOTH the free site and the original alloc site.\n");
}

/* ── Sub-program: uninitialized read ───────────────────────────────────────
 *
 * Expected Valgrind output:
 *   ==PID== Conditional jump or move depends on uninitialised value(s)
 *   ==PID==    at 0x...: demo_uninit (lesson_23.c:NN)
 *   ==PID==  Uninitialised value was created by a heap allocation
 *   ==PID==    at 0x...: malloc (vg_replace_malloc.c:...)
 *   ==PID==    by 0x...: demo_uninit (lesson_23.c:NN)
 */
static void demo_uninit(void) {
  printf("  Allocating 32 ints with malloc (NOT calloc — no zeroing)...\n");
  int *arr = malloc(32 * sizeof(int));
  if (!arr) return;

  /* Don't initialize arr — then branch on its content */
  printf("  Branching on uninitialized arr[7]...\n");
  if (arr[7] > 0) {
    printf("  arr[7] was positive: %d\n", arr[7]);
  } else {
    printf("  arr[7] was non-positive: %d\n", arr[7]);
  }

  printf("\n  Valgrind will report:\n");
  printf("    \"Conditional jump or move depends on uninitialised value(s)\"\n");
  printf("    With --track-origins=yes, it also shows WHERE the\n");
  printf("    uninitialized memory was allocated.\n");

  free(arr);
}

/* ── Sub-program: overlapping memcpy ───────────────────────────────────────
 *
 * Expected Valgrind output:
 *   ==PID== Source and destination overlap in memcpy(0x..., 0x..., 20)
 *   ==PID==    at 0x...: memcpy@@... (vg_replace_strmem.c:...)
 *   ==PID==    by 0x...: demo_overlap (lesson_23.c:NN)
 */
static void demo_overlap(void) {
  printf("  Creating overlapping memcpy...\n");
  char buf[64];
  memset(buf, 'A', sizeof(buf));

  /* memcpy with overlapping src and dst (undefined behavior) */
  /* memmove would be correct here */
  printf("  memcpy(buf + 5, buf, 20) — src and dst overlap!\n");
  memcpy(buf + 5, buf, 20);

  printf("  Result: %.30s\n", buf);
  printf("\n  Valgrind will report:\n");
  printf("    \"Source and destination overlap in memcpy\"\n");
  printf("    Use memmove() instead when regions can overlap.\n");
}

/* ── Sub-program: invalid write (heap buffer overflow) ─────────────────────
 *
 * Expected Valgrind output:
 *   ==PID== Invalid write of size 1
 *   ==PID==    at 0x...: demo_invalid_write (lesson_23.c:NN)
 *   ==PID==  Address 0x... is 0 bytes after a block of size 16 alloc'd
 */
static void demo_invalid_write(void) {
  printf("  Allocating 16 bytes, writing 20...\n");
  char *buf = malloc(16);
  if (!buf) return;

  /* Write past the end */
  for (int i = 0; i < 20; i++) {
    buf[i] = (char)('A' + i);
  }
  printf("  Wrote 20 bytes into a 16-byte buffer.\n");
  printf("\n  Valgrind will report:\n");
  printf("    \"Invalid write of size 1\"\n");
  printf("    \"Address 0x... is 0 bytes after a block of size 16 alloc'd\"\n");
  printf("    The 'after a block' tells you it's a buffer overflow.\n");

  free(buf);
}

/* ── Sub-program: double free ──────────────────────────────────────────────
 *
 * Expected Valgrind output:
 *   ==PID== Invalid free() / delete / delete[] / realloc()
 *   ==PID==    at 0x...: free (vg_replace_malloc.c:...)
 *   ==PID==    by 0x...: demo_double_free (lesson_23.c:NN)
 *   ==PID==  Address 0x... is 0 bytes inside a block of size 32 free'd
 *   ==PID==    at 0x...: free (vg_replace_malloc.c:...)
 *   ==PID==    by 0x...: demo_double_free (lesson_23.c:NN)
 */
static void demo_double_free(void) {
  printf("  Allocating 32 bytes, freeing twice...\n");
  char *buf = malloc(32);
  if (!buf) return;

  memset(buf, 'Z', 32);
  free(buf);
  printf("  First free(buf) — OK.\n");

  printf("  Second free(buf) — DOUBLE FREE!\n");
  free(buf);

  printf("\n  Valgrind will report:\n");
  printf("    \"Invalid free() / delete / delete[] / realloc()\"\n");
  printf("    It shows the first free site and the second free site.\n");
}

/* ── Usage ─────────────────────────────────────────────────────────────────*/

static void print_usage(const char *prog) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 23 — Valgrind Memcheck Deep Dive\n");
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("\n");
  printf("  Usage: %s <test>\n\n", prog);
  printf("  Available tests:\n");
  printf("    leak          — Memory leak (definitely lost)\n");
  printf("    uaf           — Use-after-free (invalid read)\n");
  printf("    uninit        — Uninitialized memory read\n");
  printf("    overlap       — Overlapping memcpy\n");
  printf("    invalid-write — Heap buffer overflow (write past end)\n");
  printf("    double-free   — Double free\n");
  printf("\n");
  printf("  Build WITHOUT sanitizers, then run under Valgrind:\n");
  printf("    ./build-dev.sh --lesson=23 --no-asan\n");
  printf("    valgrind --leak-check=full --track-origins=yes ./build/lesson_23 leak\n");
  printf("\n");
  printf("  Key Valgrind flags:\n");
  printf("    --leak-check=full       Show details of each leak\n");
  printf("    --track-origins=yes     Trace where uninit memory came from\n");
  printf("    --show-reachable=yes    Show still-reachable blocks too\n");
  printf("    -v                      Verbose (more detail)\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 0;
  }

  const char *test = argv[1];
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 23 — Valgrind Memcheck: \"%s\"\n", test);
  printf("═══════════════════════════════════════════════════════════════\n\n");

  if (strcmp(test, "leak") == 0)              demo_leak();
  else if (strcmp(test, "uaf") == 0)          demo_uaf();
  else if (strcmp(test, "uninit") == 0)       demo_uninit();
  else if (strcmp(test, "overlap") == 0)      demo_overlap();
  else if (strcmp(test, "invalid-write") == 0) demo_invalid_write();
  else if (strcmp(test, "double-free") == 0)  demo_double_free();
  else {
    fprintf(stderr, "  Unknown test: '%s'\n\n", test);
    print_usage(argv[0]);
    return 1;
  }

  printf("\n");
  return 0;
}
