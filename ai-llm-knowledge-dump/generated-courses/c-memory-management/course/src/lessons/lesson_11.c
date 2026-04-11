/* lesson_11.c — Arena Alignment and Type Safety
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates how alignment works in the arena allocator and why
 * the ARENA_PUSH_STRUCT/ARRAY macros matter.
 *
 * Run:  ./build-dev.sh --lesson=11 -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

/* Test structs with different alignment requirements */
typedef struct { char c; } Tiny;                    /* align 1 */
typedef struct { int x; int y; } Point;             /* align 4 */
typedef struct { double x; double y; } PointD;      /* align 8 */
typedef struct { char c; double d; int i; } Mixed;  /* align 8 (double) */

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 11 — Arena Alignment and Type Safety\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  Arena arena = {0};
  arena.min_block_size = 4096;

  /* ── 1. Alignment requirements of common types ──────────────────────*/
  print_section("Alignment Requirements");
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "char",   sizeof(char),   _Alignof(char));
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "int",    sizeof(int),    _Alignof(int));
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "double", sizeof(double), _Alignof(double));
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "Tiny",   sizeof(Tiny),   _Alignof(Tiny));
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "Point",  sizeof(Point),  _Alignof(Point));
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "PointD", sizeof(PointD), _Alignof(PointD));
  printf("  %-12s  size=%-4zu  align=%-4zu\n", "Mixed",  sizeof(Mixed),  _Alignof(Mixed));

  /* ── 2. Demonstrate alignment in arena ──────────────────────────────*/
  print_section("Aligned Allocations via ARENA_PUSH_STRUCT");
  {
    /* Push different types — each automatically aligned */
    Tiny   *t = ARENA_PUSH_STRUCT(&arena, Tiny);
    Point  *p = ARENA_PUSH_STRUCT(&arena, Point);
    PointD *d = ARENA_PUSH_STRUCT(&arena, PointD);
    Mixed  *m = ARENA_PUSH_STRUCT(&arena, Mixed);

    printf("  Tiny   at %p  (mod %zu = %zu)  ✓\n",
           (void *)t, _Alignof(Tiny),   (uintptr_t)t % _Alignof(Tiny));
    printf("  Point  at %p  (mod %zu = %zu)  ✓\n",
           (void *)p, _Alignof(Point),  (uintptr_t)p % _Alignof(Point));
    printf("  PointD at %p  (mod %zu = %zu)  ✓\n",
           (void *)d, _Alignof(PointD), (uintptr_t)d % _Alignof(PointD));
    printf("  Mixed  at %p  (mod %zu = %zu)  ✓\n",
           (void *)m, _Alignof(Mixed),  (uintptr_t)m % _Alignof(Mixed));

    /* Verify all are properly aligned */
    int all_ok = 1;
    if ((uintptr_t)t % _Alignof(Tiny)   != 0) all_ok = 0;
    if ((uintptr_t)p % _Alignof(Point)  != 0) all_ok = 0;
    if ((uintptr_t)d % _Alignof(PointD) != 0) all_ok = 0;
    if ((uintptr_t)m % _Alignof(Mixed)  != 0) all_ok = 0;
    printf("\n  All aligned correctly: %s\n", all_ok ? "YES" : "NO");
  }

  /* ── 3. Interleaved types show padding ──────────────────────────────*/
  print_section("Interleaved Allocations Show Padding");
  arena_reset(&arena);
  {
    printf("  Pushing: char, double, char, int, double...\n\n");
    for (int i = 0; i < 5; i++) {
      /* Alternate types to force alignment padding */
      char   *c  = (char *)arena_push_size(&arena, 1, 1);
      double *d  = (double *)arena_push_size(&arena, sizeof(double), _Alignof(double));
      char   *c2 = (char *)arena_push_size(&arena, 1, 1);
      int    *n  = (int *)arena_push_size(&arena, sizeof(int), _Alignof(int));
      *c = 'A'; *d = 1.0; *c2 = 'B'; *n = 42;

      printf("  [%d] char@%p  double@%p  char@%p  int@%p\n",
             i, (void *)c, (void *)d, (void *)c2, (void *)n);
    }
    printf("\n  Note: addresses between char and double have gaps.\n");
    printf("  Those gaps are alignment padding — wasted but necessary.\n");
    printf("  Arena used: %zu bytes (includes padding)\n",
           arena_total_used(&arena));
  }

  /* ── 4. ARENA_PUSH_ZERO_STRUCT ──────────────────────────────────────*/
  print_section("Zero-Initialized Allocation");
  arena_reset(&arena);
  {
    Point *p = ARENA_PUSH_ZERO_STRUCT(&arena, Point);
    printf("  ARENA_PUSH_ZERO_STRUCT: x=%d, y=%d (guaranteed zero)\n",
           p->x, p->y);

    float *arr = ARENA_PUSH_ZERO_ARRAY(&arena, 10, float);
    printf("  ARENA_PUSH_ZERO_ARRAY(10 floats):\n    ");
    for (int i = 0; i < 10; i++) printf("%.1f ", arr[i]);
    printf("\n  All zero — safe for structs with pointer/count fields.\n");
  }

  /* ── 5. What happens without alignment (the wrong way) ─────────────*/
  print_section("Why Alignment Matters (Misalignment Demo)");
  printf("  Without proper alignment:\n");
  printf("    - x86-64: works but may be slower (split cache lines)\n");
  printf("    - ARM: may cause SIGBUS (hardware trap)\n");
  printf("    - UBSan reports: 'misaligned address'\n\n");
  printf("  The arena's alignment parameter prevents this.\n");
  printf("  ARENA_PUSH_STRUCT uses _Alignof(Type) automatically.\n");
  printf("  Never call arena_push_size with align=1 for typed data.\n");

  arena_free(&arena);
  printf("\n");
  return 0;
}
