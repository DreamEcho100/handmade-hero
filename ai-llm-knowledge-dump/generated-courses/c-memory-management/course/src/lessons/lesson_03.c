/* lesson_03.c — Heap: malloc, calloc, realloc, free
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates the four core heap allocation functions in C.
 * Shows zeroing behavior, realloc pointer stability, and common patterns.
 *
 * Run:  ./build-dev.sh --lesson=03 -r
 * Bench: ./build-dev.sh --lesson=03 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Helper: print first N bytes of a buffer ──────────────────────────────*/

static void print_bytes(const char *label, const void *ptr, size_t n) {
  const uint8_t *bytes = (const uint8_t *)ptr;
  printf("  %s: ", label);
  for (size_t i = 0; i < n && i < 32; i++) {
    printf("%02x ", bytes[i]);
  }
  if (n > 32) printf("...");
  printf("\n");
}

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 03 — Heap: malloc, calloc, realloc, free\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. malloc — uninitialized memory ───────────────────────────────
   *
   * malloc(size) returns a pointer to `size` bytes of UNINITIALIZED memory.
   * The contents are indeterminate — could be anything.
   * Returns NULL on failure.
   */
  print_section("malloc — Uninitialized Memory");
  {
    int *arr = (int *)malloc(5 * sizeof(int));
    if (!arr) { perror("malloc"); return 1; }

    printf("  malloc(5 * sizeof(int)) = %p\n", (void *)arr);
    printf("  Contents (BEFORE writing):\n");
    for (int i = 0; i < 5; i++) {
      printf("    arr[%d] = %d  (garbage!)\n", i, arr[i]);
    }

    /* Write values */
    for (int i = 0; i < 5; i++) arr[i] = (i + 1) * 10;
    printf("  Contents (AFTER writing):\n");
    for (int i = 0; i < 5; i++) {
      printf("    arr[%d] = %d\n", i, arr[i]);
    }
    free(arr);
    printf("  free(arr) — memory returned to the heap.\n");
  }

  /* ── 2. calloc — zero-initialized memory ────────────────────────────
   *
   * calloc(count, size) allocates count*size bytes, ALL ZEROED.
   * Two advantages over malloc + memset:
   *   - Overflow check: calloc checks count*size for overflow
   *   - OS optimization: may use zero-pages without touching memory
   */
  print_section("calloc — Zero-Initialized Memory");
  {
    int *arr = (int *)calloc(5, sizeof(int));
    if (!arr) { perror("calloc"); return 1; }

    printf("  calloc(5, sizeof(int)) = %p\n", (void *)arr);
    printf("  Contents (guaranteed zero):\n");
    for (int i = 0; i < 5; i++) {
      printf("    arr[%d] = %d\n", i, arr[i]);
    }

    /* Show raw bytes are zero */
    print_bytes("Raw bytes", arr, 20);

    free(arr);
  }

  /* ── 3. malloc vs calloc — content comparison ───────────────────────*/
  print_section("malloc vs calloc — Content Comparison");
  {
    void *m = malloc(64);
    void *c = calloc(1, 64);

    printf("  malloc(64) raw bytes:\n");
    print_bytes("  malloc", m, 64);
    printf("  calloc(1, 64) raw bytes:\n");
    print_bytes("  calloc", c, 64);
    printf("\n  calloc is guaranteed zero; malloc contents are undefined.\n");

    free(m);
    free(c);
  }

  /* ── 4. realloc — resize an allocation ──────────────────────────────
   *
   * realloc(ptr, new_size) resizes the allocation:
   *   - If it can grow in place → returns SAME pointer
   *   - If it can't → allocates new block, copies data, frees old
   *   - realloc(NULL, size) ≡ malloc(size)
   *   - realloc(ptr, 0) — implementation-defined (may free or return non-NULL)
   *
   * CRITICAL: realloc may return a DIFFERENT pointer!
   * Always use: ptr = realloc(ptr, new_size);
   * But beware: if realloc fails (returns NULL), the old ptr is still valid!
   */
  print_section("realloc — Resize an Allocation");
  {
    int *arr = (int *)malloc(3 * sizeof(int));
    arr[0] = 100; arr[1] = 200; arr[2] = 300;
    void *old_ptr = arr;

    printf("  Initial: %p  [%d, %d, %d]\n", (void *)arr,
           arr[0], arr[1], arr[2]);

    /* Grow the array */
    arr = (int *)realloc(arr, 6 * sizeof(int));
    printf("  After realloc to 6: %p  [%d, %d, %d, ?, ?, ?]\n",
           (void *)arr, arr[0], arr[1], arr[2]);

    if ((void *)arr == old_ptr) {
      printf("  Pointer UNCHANGED (grew in place).\n");
    } else {
      printf("  Pointer CHANGED (data was copied to new location).\n");
    }
    printf("  Old data preserved: arr[0]=%d, arr[1]=%d, arr[2]=%d\n",
           arr[0], arr[1], arr[2]);

    /* Fill new slots */
    arr[3] = 400; arr[4] = 500; arr[5] = 600;

    /* Shrink */
    old_ptr = arr;
    arr = (int *)realloc(arr, 2 * sizeof(int));
    printf("  After realloc to 2: %p  [%d, %d]\n",
           (void *)arr, arr[0], arr[1]);

    free(arr);
  }

  /* ── 5. Safe realloc pattern ────────────────────────────────────────*/
  print_section("Safe realloc Pattern");
  {
    printf("  WRONG (leaks on failure):\n");
    printf("    ptr = realloc(ptr, new_size);  // if NULL, old ptr is lost!\n\n");
    printf("  CORRECT:\n");
    printf("    void *tmp = realloc(ptr, new_size);\n");
    printf("    if (!tmp) { /* handle error, ptr is still valid */ }\n");
    printf("    ptr = tmp;\n");

    /* Demonstrate */
    int *data = (int *)malloc(10 * sizeof(int));
    for (int i = 0; i < 10; i++) data[i] = i;

    void *tmp = realloc(data, 20 * sizeof(int));
    if (tmp) {
      data = (int *)tmp;
      printf("\n  Realloc succeeded: data=%p, data[9]=%d\n",
             (void *)data, data[9]);
    }
    free(data);
  }

  /* ── 6. Common allocation sizes ─────────────────────────────────────*/
  print_section("Allocation Size and Alignment");
  {
    void *p1 = malloc(1);
    void *p2 = malloc(7);
    void *p3 = malloc(16);
    void *p4 = malloc(100);

    printf("  malloc(1)   = %p  (aligned to %zu)\n",
           p1, (uintptr_t)p1 % 16);
    printf("  malloc(7)   = %p  (aligned to %zu)\n",
           p2, (uintptr_t)p2 % 16);
    printf("  malloc(16)  = %p  (aligned to %zu)\n",
           p3, (uintptr_t)p3 % 16);
    printf("  malloc(100) = %p  (aligned to %zu)\n",
           p4, (uintptr_t)p4 % 16);
    printf("\n  malloc guarantees alignment suitable for any basic type\n");
    printf("  (typically 16-byte aligned on 64-bit systems).\n");

    free(p1); free(p2); free(p3); free(p4);
  }

  /* ── 7. Benchmark: malloc+free vs calloc+free ───────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "malloc+free vs calloc+free (64 bytes)") {
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(64);
        *(volatile char *)p = 0;  /* prevent optimization */
        free(p);
      }
      BENCH_CASE_END(suite, N);

      BENCH_CASE(suite, "calloc+free", i, N) {
        void *p = calloc(1, 64);
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_03");
#endif

  printf("\n");
  return 0;
}
