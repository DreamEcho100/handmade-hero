/* lesson_02.c — Stack Allocation Mechanics
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates how the C stack works: frame creation/destruction,
 * growth direction, alloca, and stack overflow.
 *
 * Run:  ./build-dev.sh --lesson=02 -r
 * Overflow demo:  ./build-dev.sh --lesson=02 --no-asan -r  (pass "overflow" arg)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <alloca.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Stack growth observation ─────────────────────────────────────────────
 *
 * We use volatile to prevent the compiler from optimizing away our
 * address observations.  Each function prints the address of its local
 * variable, showing the stack growing downward (on x86-64).
 */

__attribute__((noinline))
void depth_3(void) {
  volatile int local = 3;
  printf("  depth_3:  &local = %p  (value=%d)\n", (void *)&local, local);
}

__attribute__((noinline))
void depth_2(void) {
  volatile int local = 2;
  printf("  depth_2:  &local = %p  (value=%d)\n", (void *)&local, local);
  depth_3();
}

__attribute__((noinline))
void depth_1(void) {
  volatile int local = 1;
  printf("  depth_1:  &local = %p  (value=%d)\n", (void *)&local, local);
  depth_2();
}

/* ── Stack frame size demonstration ───────────────────────────────────────*/

__attribute__((noinline))
void small_frame(void) {
  volatile char buf[16];
  buf[0] = 'A';
  printf("  small_frame (16 B):  &buf = %p\n", (void *)buf);
}

__attribute__((noinline))
void large_frame(void) {
  volatile char buf[4096];
  buf[0] = 'B';
  printf("  large_frame (4 KB):  &buf = %p\n", (void *)buf);
}

/* ── alloca demonstration ─────────────────────────────────────────────────
 *
 * alloca allocates memory on the stack.  It's automatically freed when
 * the function returns — no free() needed.  But it's dangerous:
 * - No overflow check
 * - Can blow the stack silently
 * - Not in the C standard (POSIX extension)
 */

__attribute__((noinline))
void alloca_demo(size_t n) {
  printf("\n  alloca(%zu bytes):\n", n);
  volatile char *buf = (char *)alloca(n);
  buf[0] = 'C';
  buf[n - 1] = 'D';
  printf("    &buf = %p  (first=%c, last=%c)\n",
         (void *)buf, buf[0], buf[n - 1]);
  printf("    NOTE: This memory lives ON THE STACK.\n");
  printf("    It vanishes when this function returns.\n");
}

/* ── Stack overflow demonstration ─────────────────────────────────────────
 *
 * Each recursive call pushes a frame.  Eventually we exceed the stack
 * limit (typically 8 MB on Linux) and get a SIGSEGV / stack overflow.
 *
 * WARNING: This will crash!  Run with --no-asan to see the raw crash.
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
__attribute__((noinline))
int recursive_overflow(int depth) {
  volatile char buf[1024];  /* 1 KB per frame */
  buf[0] = (char)depth;
  if (depth % 1000 == 0) {
    printf("  depth=%d  &buf=%p\n", depth, (void *)buf);
  }
  return recursive_overflow(depth + 1);
}
#pragma clang diagnostic pop

/* ── Variable-Length Array (VLA) demonstration ─────────────────────────────
 *
 * VLAs allocate on the stack at runtime (like alloca but standard C99/C11).
 * Same risks: stack overflow if n is large.
 */

__attribute__((noinline))
void vla_demo(int n) {
  printf("\n  VLA with %d elements:\n", n);
  int arr[n];  /* variable-length array on the stack */
  for (int i = 0; i < n; i++) arr[i] = i * i;
  printf("    &arr[0]    = %p\n", (void *)&arr[0]);
  printf("    &arr[%d] = %p\n", n - 1, (void *)&arr[n - 1]);
  printf("    Total size: %zu bytes\n", (size_t)n * sizeof(int));
  printf("    Sum: %d (sanity check)\n", arr[0] + arr[n - 1]);
}

int main(int argc, char *argv[]) {
  int do_overflow = (argc > 1 && strcmp(argv[1], "overflow") == 0);

  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 02 — Stack Allocation Mechanics\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Stack growth direction ──────────────────────────────────────*/
  print_section("Stack Growth Direction (nested calls)");
  printf("  Calling depth_1 → depth_2 → depth_3:\n\n");
  volatile int main_local = 0;
  printf("  main:     &local = %p  (value=%d)\n", (void *)&main_local, main_local);
  depth_1();
  printf("\n  Observation: addresses DECREASE with deeper calls.\n");
  printf("  The stack grows DOWNWARD on x86-64.\n");

  /* ── 2. Frame sizes ─────────────────────────────────────────────────*/
  print_section("Frame Size vs Local Arrays");
  small_frame();
  large_frame();
  printf("\n  Larger local arrays push the stack pointer further down.\n");

  /* ── 3. alloca ──────────────────────────────────────────────────────*/
  print_section("alloca() — Dynamic Stack Allocation");
  alloca_demo(64);
  alloca_demo(1024);
  printf("\n  alloca is fast (just bumps the stack pointer) but risky:\n");
  printf("  - No size limit check\n");
  printf("  - Freed automatically on function return\n");
  printf("  - Not portable (not in C standard)\n");
  printf("  Prefer arenas (Lesson 10+) for dynamic allocation needs.\n");

  /* ── 4. VLA ─────────────────────────────────────────────────────────*/
  print_section("Variable-Length Arrays (VLA)");
  vla_demo(10);
  vla_demo(100);
  printf("\n  VLAs are in the C99/C11 standard but share alloca's risks.\n");
  printf("  Many codebases (Linux kernel, CERT C) forbid VLAs.\n");

  /* ── 5. Stack limits ────────────────────────────────────────────────*/
  print_section("Stack Size Limits");
  printf("  Default stack size on Linux: typically 8 MB (ulimit -s)\n");
  printf("  Each function call pushes: return addr + saved regs + locals\n");
  printf("  A 1 KB local buffer × 8000 recursions = 8 MB → overflow!\n");

  /* ── 6. Stack overflow demo (opt-in) ────────────────────────────────*/
  if (do_overflow) {
    print_section("Stack Overflow Demo (WILL CRASH)");
    printf("  Recursing with 1 KB frames until crash...\n\n");
    recursive_overflow(0);
    /* Never reaches here */
  } else {
    print_section("Stack Overflow Demo (skipped)");
    printf("  Pass \"overflow\" as argument to trigger:\n");
    printf("    ./build/lesson_02 overflow\n");
    printf("  Build with --no-asan to see the raw SIGSEGV.\n");
  }

  printf("\n");
  return 0;
}
