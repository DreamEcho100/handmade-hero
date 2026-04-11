/* lesson_09.c — Debug Allocator
 * BUILD_DEPS: lib/debug_alloc.c
 *
 * Demonstrates a custom debug allocator that wraps malloc with:
 *   - 0xCD fill for new allocations ("clean data")
 *   - 0xDD fill for freed memory ("dead data")
 *   - 4-byte canary (0xDEADBEEF) before and after each allocation
 *   - Canary checking on free to detect buffer overflows
 *   - Allocation statistics tracking
 *
 * Run:  ./build-dev.sh --lesson=09 -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "lib/debug_alloc.h"

/* ── Demo 1: Normal allocation and free (canaries intact) ────────────────*/

static void demo_normal(void) {
  print_section("Demo 1: Normal Allocation and Free");

  printf("  Allocating 32 bytes with DALLOC...\n\n");
  uint8_t *buf = DALLOC(32);
  if (!buf) return;

  printf("  Fresh allocation filled with 0xCD (\"clean data\"):\n");
  hex_dump(buf, 32);

  printf("\n  Writing \"Hello, debug allocator!\" into the buffer...\n");
  strcpy((char *)buf, "Hello, debug allocator!");
  printf("  buf = \"%s\"\n\n", (char *)buf);

  printf("  Freeing with DFREE (canaries should be intact)...\n");
  DFREE(buf);
  printf("  Free succeeded — no canary corruption detected.\n");
}

/* ── Demo 2: Deliberate buffer overflow corrupts trailing canary ─────────*/

static void demo_overflow(void) {
  print_section("Demo 2: Buffer Overflow Corrupts Trailing Canary");

  printf("  Allocating 16 bytes with DALLOC...\n");
  uint8_t *buf = DALLOC(16);
  if (!buf) return;

  printf("  Writing 20 bytes into a 16-byte buffer (4-byte overflow)...\n");
  printf("  This will overwrite the trailing canary.\n\n");

  /* BUG: write 4 bytes past the 16-byte allocation.
   * This stomps on the trailing canary (0xDEADBEEF). */
  memset(buf, 'X', 20);

  printf("  Now calling DFREE — it should detect the corruption:\n\n");
  DFREE(buf);

  printf("\n  The debug allocator caught the overflow via canary check!\n");
}

/* ── Demo 3: Reading freed memory shows 0xDD pattern ─────────────────────*/

static void demo_freed_pattern(void) {
  print_section("Demo 3: Freed Memory Shows 0xDD Pattern");

  printf("  Allocating 32 bytes, writing data, then freeing...\n\n");
  uint8_t *buf = DALLOC(32);
  if (!buf) return;

  strcpy((char *)buf, "This will be overwritten");
  printf("  Before free:\n");
  hex_dump(buf, 32);

  /* Save a pointer to the same address so we can peek after free.
   * NOTE: In real code this is use-after-free (UB). We do it here
   * purely to demonstrate the 0xDD fill pattern. We build this lesson
   * with --no-asan to avoid ASan catching this peek. */
  uint8_t *peek = buf;

  DFREE(buf);

  printf("\n  After free (peeking at same address — UB in real code!):\n");
  printf("  Every byte should be 0xDD (\"dead data\"):\n");
  hex_dump(peek, 32);

  int all_dd = 1;
  for (int i = 0; i < 32; i++) {
    if (peek[i] != 0xDD) { all_dd = 0; break; }
  }
  printf("\n  All bytes 0xDD? %s\n", all_dd ? "YES — freed memory is poisoned." :
         "NO — something unexpected happened.");
}

/* ── Demo 4: Final allocation stats report ───────────────────────────────*/

static void demo_stats(void) {
  print_section("Demo 4: Allocation Statistics");

  printf("  Making several allocations of different sizes...\n\n");

  void *a = DALLOC(64);
  void *b = DALLOC(128);
  void *c = DALLOC(256);
  void *d = DALLOC(512);

  printf("  Allocated: 64 + 128 + 256 + 512 = 960 bytes in 4 blocks.\n");

  printf("  Freeing a, b, c (keeping d alive)...\n");
  DFREE(a);
  DFREE(b);
  DFREE(c);
  /* Deliberately NOT freeing d — to show leak detection in the report. */

  debug_alloc_report();

  printf("  (d is still live — the report shows 1 active allocation.)\n\n");
  printf("  Freeing d...\n");
  DFREE(d);

  debug_alloc_report();
}

/* ── Main ────────────────────────────────────────────────────────────────*/

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 09 — Debug Allocator\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  demo_normal();
  demo_overflow();
  demo_freed_pattern();
  demo_stats();

  printf("\n");
  return 0;
}
