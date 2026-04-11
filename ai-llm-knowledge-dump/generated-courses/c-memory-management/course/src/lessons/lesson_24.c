#define _POSIX_C_SOURCE 200809L
/* lesson_24.c — ASan, UBSan, MSan, TSan: Compiler Sanitizers
 * BUILD_DEPS: common/bench.c
 *
 * Sub-programs demonstrating bugs caught by different sanitizers.
 * Build with specific sanitizer flags to see each in action.
 *
 * Run:   ./build-dev.sh --lesson=24 -r
 * Usage: ./build/lesson_24 <test>
 *   Tests: asan, ubsan, race
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Sub-program: ASan — heap buffer overflow ──────────────────────────────
 *
 * AddressSanitizer catches:
 *   - Heap/stack/global buffer overflow
 *   - Use-after-free
 *   - Use-after-return (with flag)
 *   - Double free
 *   - Memory leaks (with leak sanitizer)
 *
 * Build: clang -fsanitize=address -g lesson_24.c
 *
 * Expected ASan output:
 *   ==PID==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
 *   READ of size 4 at 0x... thread T0
 *     #0 0x... in demo_asan lesson_24.c:NN
 *   0x... is located 0 bytes to the right of 40-byte region [0x..., 0x...)
 *   allocated by thread T0 here:
 *     #0 0x... in malloc
 *     #1 0x... in demo_asan lesson_24.c:NN
 */
static void demo_asan(void) {
  printf("  ASan: Heap buffer overflow demo\n\n");
  printf("  Allocating int[10] (40 bytes), reading index 10 (out of bounds).\n\n");

  int *arr = malloc(10 * sizeof(int));
  if (!arr) return;

  for (int i = 0; i < 10; i++) arr[i] = i * 100;

  /* Read one past the end — ASan will catch this */
  printf("  arr[9] = %d (valid)\n", arr[9]);
  printf("  arr[10] = ... (heap buffer overflow!)\n");
  volatile int oob = arr[10];
  printf("  Read arr[10] = %d\n", oob);

  printf("\n  ASan reports:\n");
  printf("    \"heap-buffer-overflow on address 0x...\"\n");
  printf("    Shows the read location relative to the allocated region,\n");
  printf("    plus stack traces for both the access and the allocation.\n");

  free(arr);
}

/* ── Sub-program: UBSan — signed integer overflow ─────────────────────────
 *
 * UndefinedBehaviorSanitizer catches:
 *   - Signed integer overflow
 *   - Null pointer dereference
 *   - Misaligned access
 *   - Shift past bit width
 *   - Division by zero
 *   - Unreachable code
 *
 * Build: clang -fsanitize=undefined -g lesson_24.c
 *
 * Expected UBSan output:
 *   lesson_24.c:NN: runtime error: signed integer overflow:
 *   2147483647 + 1 cannot be represented in type 'int'
 */
static void demo_ubsan(void) {
  printf("  UBSan: Signed integer overflow demo\n\n");
  printf("  INT_MAX = 2147483647.  Adding 1 is undefined behavior.\n\n");

  volatile int x = 2147483647;  /* INT_MAX */
  printf("  x = %d (INT_MAX)\n", (int)x);

  /* Signed overflow — undefined behavior, UBSan catches it */
  volatile int y = x + 1;
  printf("  x + 1 = %d (signed overflow — undefined behavior!)\n", (int)y);

  printf("\n  UBSan reports:\n");
  printf("    \"runtime error: signed integer overflow:\"\n");
  printf("    \"2147483647 + 1 cannot be represented in type 'int'\"\n");
  printf("\n  Why this matters: the compiler is ALLOWED to assume signed\n");
  printf("  overflow never happens.  It can optimize based on that assumption,\n");
  printf("  leading to loops that never terminate or branches that vanish.\n");
}

/* ── Sub-program: TSan — data race ─────────────────────────────────────────
 *
 * ThreadSanitizer catches:
 *   - Data races (concurrent unsynchronized access)
 *   - Lock order inversions (potential deadlocks)
 *
 * Build: clang -fsanitize=thread -g lesson_24.c -lpthread
 *
 * NOTE: TSan conflicts with ASan.  You must build with ONLY -fsanitize=thread.
 * The default build uses ASan+UBSan, so this demo shows the CONCEPT
 * and documents what TSan would report.  To actually trigger TSan:
 *   clang -fsanitize=thread -g -O1 -o lesson_24_tsan lesson_24.c -lpthread
 *   ./lesson_24_tsan race
 *
 * Expected TSan output:
 *   WARNING: ThreadSanitizer: data race (pid=...)
 *     Write of size 4 at 0x... by thread T1:
 *       #0 thread_func lesson_24.c:NN
 *     Previous write of size 4 at 0x... by thread T2:
 *       #0 thread_func lesson_24.c:NN
 */
static volatile int g_shared_counter = 0;

static void *thread_increment(void *arg) {
  (void)arg;
  for (int i = 0; i < 100000; i++) {
    g_shared_counter++;  /* DATA RACE: no synchronization */
  }
  return NULL;
}

static void demo_race(void) {
  printf("  TSan: Data race demo\n\n");
  printf("  Two threads increment a shared counter without synchronization.\n\n");

  g_shared_counter = 0;

  pthread_t t1, t2;
  pthread_create(&t1, NULL, thread_increment, NULL);
  pthread_create(&t2, NULL, thread_increment, NULL);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  printf("  Expected: 200000\n");
  printf("  Got:      %d\n", g_shared_counter);
  printf("  Lost:     %d increments\n\n", 200000 - g_shared_counter);

  printf("  TSan would report:\n");
  printf("    \"WARNING: ThreadSanitizer: data race\"\n");
  printf("    Shows both threads' stack traces and the conflicting accesses.\n");
  printf("\n  NOTE: TSan conflicts with ASan.  To run with TSan:\n");
  printf("    clang -fsanitize=thread -g -O1 -o lesson_24_tsan \\\n");
  printf("      src/lessons/lesson_24.c src/common/bench.c -lpthread\n");
  printf("    ./lesson_24_tsan race\n");
}

/* ── Usage ─────────────────────────────────────────────────────────────────*/

static void print_usage(const char *prog) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 24 — Compiler Sanitizers: ASan, UBSan, TSan\n");
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("\n");
  printf("  Usage: %s <test>\n\n", prog);
  printf("  Available tests:\n");
  printf("    asan   — Heap buffer overflow (ASan catches)\n");
  printf("    ubsan  — Signed integer overflow (UBSan catches)\n");
  printf("    race   — Data race between threads (TSan catches)\n");
  printf("\n");
  printf("  Sanitizer build flags:\n");
  printf("    ASan:  -fsanitize=address          (heap/stack/global OOB, UAF)\n");
  printf("    UBSan: -fsanitize=undefined        (signed overflow, null deref)\n");
  printf("    MSan:  -fsanitize=memory           (uninitialized reads)\n");
  printf("    TSan:  -fsanitize=thread            (data races)\n");
  printf("\n");
  printf("  IMPORTANT: ASan and TSan conflict — use only one at a time.\n");
  printf("  MSan and ASan also conflict.\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 0;
  }

  const char *test = argv[1];
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 24 — Sanitizer Demo: \"%s\"\n", test);
  printf("═══════════════════════════════════════════════════════════════\n\n");

  if (strcmp(test, "asan") == 0)       demo_asan();
  else if (strcmp(test, "ubsan") == 0) demo_ubsan();
  else if (strcmp(test, "race") == 0)  demo_race();
  else {
    fprintf(stderr, "  Unknown test: '%s'\n\n", test);
    print_usage(argv[0]);
    return 1;
  }

  printf("\n");
  return 0;
}
