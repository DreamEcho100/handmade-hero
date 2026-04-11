/* lesson_04.c — Alignment and sizeof
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates sizeof, _Alignof, struct padding, packed structs,
 * and how alignment affects performance.
 *
 * Run:   ./build-dev.sh --lesson=04 -r
 * Bench: ./build-dev.sh --lesson=04 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Struct with padding ──────────────────────────────────────────────────
 *
 * The compiler inserts padding bytes to satisfy alignment requirements.
 * Each member must be at an offset that is a multiple of its alignment.
 *
 * Layout of Padded (likely):
 *   offset 0: a (char, 1 byte) + 3 bytes padding
 *   offset 4: b (int, 4 bytes)
 *   offset 8: c (char, 1 byte) + 7 bytes padding
 *   offset 16: d (double, 8 bytes)
 *   Total: 24 bytes (not 14!)
 */
typedef struct {
  char   a;    /* 1 byte */
  int    b;    /* 4 bytes */
  char   c;    /* 1 byte */
  double d;    /* 8 bytes */
} Padded;

/* ── Same fields, reordered to minimize padding ──────────────────────────
 *
 * Rule: sort fields by alignment (largest first).
 * Layout of Compact:
 *   offset 0: d (double, 8 bytes)
 *   offset 8: b (int, 4 bytes)
 *   offset 12: a (char, 1 byte)
 *   offset 13: c (char, 1 byte) + 2 bytes tail padding
 *   Total: 16 bytes (saved 8 bytes!)
 */
typedef struct {
  double d;    /* 8 bytes */
  int    b;    /* 4 bytes */
  char   a;    /* 1 byte */
  char   c;    /* 1 byte */
} Compact;

/* ── Packed struct (no padding) ───────────────────────────────────────────
 *
 * __attribute__((packed)) removes ALL padding.
 * This can cause misaligned access — slow on x86, crash on ARM/SPARC.
 * Use only for wire protocols, file formats, or hardware registers.
 */
typedef struct __attribute__((packed)) {
  char   a;
  int    b;
  char   c;
  double d;
} Packed;

/* ── Struct with explicit alignment ───────────────────────────────────────*/
typedef struct {
  alignas(64) char cache_line_data[64];  /* force 64-byte alignment */
  int count;
} CacheAligned;

/* ── Helper: print struct member offsets ───────────────────────────────────*/

#define PRINT_OFFSET(type, member) \
  printf("    %-10s offset=%-4zu  size=%-4zu\n", \
         #member, \
         offsetof(type, member), \
         sizeof(((type *)0)->member))

/* ── Helper: print field layout as a visual bar ───────────────────────────*/

static void print_struct_layout(const char *name, size_t size) {
  printf("  %s: [", name);
  for (size_t i = 0; i < size; i++) {
    if (i > 0 && i % 8 == 0) printf("|");
    printf(".");
  }
  printf("] = %zu bytes\n", size);
}

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 04 — Alignment and sizeof\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic type sizes and alignments ─────────────────────────────*/
  print_section("Basic Type Sizes and Alignments");
  printf("  %-16s  %-6s  %-6s\n", "Type", "Size", "Align");
  printf("  ────────────────  ──────  ──────\n");
  printf("  %-16s  %-6zu  %-6zu\n", "char",      sizeof(char),      _Alignof(char));
  printf("  %-16s  %-6zu  %-6zu\n", "short",     sizeof(short),     _Alignof(short));
  printf("  %-16s  %-6zu  %-6zu\n", "int",       sizeof(int),       _Alignof(int));
  printf("  %-16s  %-6zu  %-6zu\n", "long",      sizeof(long),      _Alignof(long));
  printf("  %-16s  %-6zu  %-6zu\n", "long long", sizeof(long long), _Alignof(long long));
  printf("  %-16s  %-6zu  %-6zu\n", "float",     sizeof(float),     _Alignof(float));
  printf("  %-16s  %-6zu  %-6zu\n", "double",    sizeof(double),    _Alignof(double));
  printf("  %-16s  %-6zu  %-6zu\n", "void *",    sizeof(void *),    _Alignof(void *));
  printf("  %-16s  %-6zu  %-6zu\n", "size_t",    sizeof(size_t),    _Alignof(size_t));
  printf("  %-16s  %-6zu  %-6zu\n", "intptr_t",  sizeof(intptr_t),  _Alignof(intptr_t));

  /* ── 2. Struct padding demonstration ────────────────────────────────*/
  print_section("Struct Padding");
  printf("  Padded struct (naive field order):\n");
  printf("    sizeof(Padded)  = %zu\n", sizeof(Padded));
  printf("    _Alignof(Padded) = %zu\n", _Alignof(Padded));
  PRINT_OFFSET(Padded, a);
  PRINT_OFFSET(Padded, b);
  PRINT_OFFSET(Padded, c);
  PRINT_OFFSET(Padded, d);
  print_struct_layout("Padded", sizeof(Padded));

  printf("\n  Compact struct (sorted by alignment):\n");
  printf("    sizeof(Compact) = %zu\n", sizeof(Compact));
  printf("    _Alignof(Compact) = %zu\n", _Alignof(Compact));
  PRINT_OFFSET(Compact, d);
  PRINT_OFFSET(Compact, b);
  PRINT_OFFSET(Compact, a);
  PRINT_OFFSET(Compact, c);
  print_struct_layout("Compact", sizeof(Compact));

  printf("\n  Savings: %zu bytes per instance (%zu vs %zu)\n",
         sizeof(Padded) - sizeof(Compact),
         sizeof(Padded), sizeof(Compact));
  printf("  For 1 million instances: %zu KB saved!\n",
         (sizeof(Padded) - sizeof(Compact)) * 1000000 / 1024);

  /* ── 3. Packed struct ───────────────────────────────────────────────*/
  print_section("Packed Struct (No Padding)");
  printf("  sizeof(Packed) = %zu  (sum of fields = %zu)\n",
         sizeof(Packed),
         sizeof(char) + sizeof(int) + sizeof(char) + sizeof(double));
  PRINT_OFFSET(Packed, a);
  PRINT_OFFSET(Packed, b);
  PRINT_OFFSET(Packed, c);
  PRINT_OFFSET(Packed, d);
  printf("\n  WARNING: Packed structs cause misaligned access!\n");
  printf("  Use only for: wire protocols, file formats, hardware regs.\n");

  /* ── 4. Explicit alignment with alignas ─────────────────────────────*/
  print_section("Explicit Alignment (alignas)");
  CacheAligned ca;
  printf("  sizeof(CacheAligned) = %zu\n", sizeof(CacheAligned));
  printf("  _Alignof(CacheAligned) = %zu\n", _Alignof(CacheAligned));
  printf("  &ca.cache_line_data = %p  (mod 64 = %zu)\n",
         (void *)ca.cache_line_data,
         (uintptr_t)ca.cache_line_data % 64);

  /* ── 5. Alignment and malloc ────────────────────────────────────────*/
  print_section("Alignment from malloc");
  for (int i = 0; i < 5; i++) {
    void *p = malloc(1);
    printf("  malloc(1) = %p  (mod 16 = %zu)\n", p, (uintptr_t)p % 16);
    free(p);
  }
  printf("\n  malloc returns memory aligned for any standard type.\n");
  printf("  Typically 16-byte aligned on 64-bit systems.\n");

  /* ── 6. Manual alignment calculation ────────────────────────────────*/
  print_section("Manual Alignment Math");
  printf("  To align a pointer `p` to `align` bytes (power of 2):\n\n");
  printf("    uintptr_t mask = align - 1;\n");
  printf("    uintptr_t aligned = (p + mask) & ~mask;\n\n");

  uintptr_t raw = 0x1003;
  uintptr_t align = 16;
  uintptr_t mask = align - 1;
  uintptr_t aligned = (raw + mask) & ~mask;
  printf("  Example: raw=0x%lx  align=%zu  → aligned=0x%lx\n",
         (unsigned long)raw, (size_t)align, (unsigned long)aligned);
  printf("  Wasted: %zu bytes of padding\n", (size_t)(aligned - raw));

  /* ── 7. Benchmark: aligned vs unaligned access ──────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 10000000;

    /* Allocate a large buffer and sum ints at aligned vs offset positions */
    size_t buf_size = N * sizeof(int) + 64;
    char *raw_buf = (char *)malloc(buf_size);
    memset(raw_buf, 1, buf_size);

    /* Aligned: ints at natural alignment */
    int *aligned_arr = (int *)(((uintptr_t)raw_buf + 15) & ~(uintptr_t)15);

    /* Misaligned: ints offset by 1 byte */
    int *misaligned_arr = (int *)(((uintptr_t)raw_buf + 15 + 1) & ~(uintptr_t)0);
    /* Actually force misalignment */
    misaligned_arr = (int *)((char *)aligned_arr + 1);

    volatile long sum = 0;

    BENCH_SUITE(suite, "Aligned vs Misaligned int access") {
      sum = 0;
      BENCH_CASE(suite, "aligned (4B boundary)", i, N) {
        sum += aligned_arr[i % (N / 2)];
      }
      BENCH_CASE_END(suite, N);

      sum = 0;
      /* For misaligned, use memcpy to avoid UB */
      BENCH_CASE(suite, "misaligned (memcpy)", i, N) {
        int val;
        memcpy(&val, (char *)misaligned_arr + (i % (N / 2)) * sizeof(int), sizeof(int));
        sum += val;
      }
      BENCH_CASE_END(suite, N);
    }
    (void)sum;
    free(raw_buf);
  }
#else
  bench_skip_notice("lesson_04");
#endif

  printf("\n");
  return 0;
}
