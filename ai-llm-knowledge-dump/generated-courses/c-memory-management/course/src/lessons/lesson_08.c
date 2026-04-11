/* lesson_08.c — Buffer Overflow and Uninitialized Reads
 *
 * Demonstrates four classic memory-access bugs:
 *   "heap-overflow"   — write past the end of a heap allocation
 *   "stack-overflow"  — write past the end of a stack buffer
 *   "uninitialized"   — read uninitialized malloc memory, branch on it
 *   "off-by-one"      — classic off-by-one in a loop
 *
 * Run:  ./build-dev.sh --lesson=08 -r
 *       (then manually: ./build/lesson_08 heap-overflow)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Sub-program: Heap Buffer Overflow ───────────────────────────────────*/

static void demo_heap_overflow(void) {
  printf("\n── Demo: Heap Buffer Overflow ──────────────────────────────────\n\n");
  printf("  Allocating 16 bytes on the heap, then writing 32 bytes into it.\n");
  printf("  This corrupts memory past our allocation — ASan catches it.\n\n");

  char *buf = malloc(16);
  if (!buf) { fprintf(stderr, "  malloc failed\n"); return; }

  printf("  buf = malloc(16);  // only 16 bytes allocated\n");

  /* BUG: writing 32 bytes into a 16-byte buffer */
  printf("  [BUG] memset(buf, 'A', 32);  // writing 32 bytes!\n");
  memset(buf, 'A', 32);

  printf("  ASan should have caught the heap-buffer-overflow above.\n");

  free(buf);
}

/* ── Sub-program: Stack Buffer Overflow ──────────────────────────────────*/

static void demo_stack_overflow(void) {
  printf("\n── Demo: Stack Buffer Overflow ─────────────────────────────────\n\n");
  printf("  Declaring a 16-byte stack array, then writing 32 bytes into it.\n");
  printf("  This corrupts the stack frame — ASan catches it.\n\n");

  char buf[16];

  printf("  char buf[16];  // 16 bytes on the stack\n");

  /* BUG: writing 32 bytes into a 16-byte stack buffer */
  printf("  [BUG] memset(buf, 'B', 32);  // writing 32 bytes!\n");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfortify-source"
  memset(buf, 'B', 32);
#pragma clang diagnostic pop

  /* Use buf so the compiler doesn't optimize it away. */
  printf("  buf[0] = 0x%02x\n", (unsigned char)buf[0]);
  printf("  ASan should have caught the stack-buffer-overflow above.\n");
}

/* ── Sub-program: Uninitialized Read ─────────────────────────────────────*/

static void demo_uninitialized(void) {
  printf("\n── Demo: Uninitialized Memory Read ────────────────────────────\n\n");
  printf("  malloc does NOT zero memory. Reading before writing is UB.\n");
  printf("  Branching on uninitialized data makes program behavior random.\n\n");

  int *values = malloc(5 * sizeof(int));
  if (!values) { fprintf(stderr, "  malloc failed\n"); return; }

  printf("  int *values = malloc(5 * sizeof(int));  // not zeroed!\n\n");

  /* BUG: reading uninitialized memory and branching on it */
  printf("  [BUG] Reading uninitialized values:\n");
  for (int i = 0; i < 5; i++) {
    printf("    values[%d] = %d (0x%08x)\n", i, values[i], (unsigned)values[i]);
  }

  printf("\n  [BUG] Branching on uninitialized data:\n");
  if (values[0] > 0) {
    printf("    values[0] > 0 — took TRUE branch (unpredictable!)\n");
  } else {
    printf("    values[0] <= 0 — took FALSE branch (unpredictable!)\n");
  }

  printf("\n  Fix: use calloc() to zero-initialize, or memset after malloc:\n");
  int *clean = calloc(5, sizeof(int));
  if (!clean) { free(values); fprintf(stderr, "  calloc failed\n"); return; }
  printf("  int *clean = calloc(5, sizeof(int));\n");
  for (int i = 0; i < 5; i++) {
    printf("    clean[%d] = %d  (zeroed by calloc)\n", i, clean[i]);
  }

  free(values);
  free(clean);
}

/* ── Sub-program: Off-By-One ─────────────────────────────────────────────*/

static void demo_off_by_one(void) {
  printf("\n── Demo: Off-By-One Error ──────────────────────────────────────\n\n");
  printf("  Allocating array of 10 ints, but looping with <= instead of <.\n");
  printf("  The last iteration writes one past the end of the array.\n\n");

  int *arr = malloc(10 * sizeof(int));
  if (!arr) { fprintf(stderr, "  malloc failed\n"); return; }

  printf("  int *arr = malloc(10 * sizeof(int));  // indices 0..9\n");

  /* BUG: loop condition uses <= instead of < */
  printf("  [BUG] for (int i = 0; i <= 10; i++) arr[i] = i;\n");
  for (int i = 0; i <= 10; i++) {
    arr[i] = i;    /* When i == 10, this writes past the allocation. */
  }

  printf("  arr[10] was written — one past the end!\n");
  printf("  ASan should have caught the heap-buffer-overflow above.\n\n");

  printf("  Fix: use i < 10 as the loop condition.\n");
  printf("  Always remember: an array of N elements has indices 0..N-1.\n");

  free(arr);
}

/* ── Main: dispatch on argv[1] ───────────────────────────────────────────*/

static void print_menu(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 08 — Buffer Overflow and Uninitialized Reads\n");
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("\n");
  printf("  Usage: ./build/lesson_08 <demo>\n\n");
  printf("  Available demos:\n");
  printf("    heap-overflow    Write past end of heap allocation\n");
  printf("    stack-overflow   Write past end of stack buffer\n");
  printf("    uninitialized    Read uninitialized malloc memory\n");
  printf("    off-by-one       Classic off-by-one in a loop\n");
  printf("\n");
  printf("  Build: ./build-dev.sh --lesson=08 -r\n");
  printf("  Then:  ./build/lesson_08 heap-overflow\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_menu();
    return 0;
  }

  const char *demo = argv[1];

  if (strcmp(demo, "heap-overflow") == 0) {
    demo_heap_overflow();
  } else if (strcmp(demo, "stack-overflow") == 0) {
    demo_stack_overflow();
  } else if (strcmp(demo, "uninitialized") == 0) {
    demo_uninitialized();
  } else if (strcmp(demo, "off-by-one") == 0) {
    demo_off_by_one();
  } else {
    fprintf(stderr, "  Unknown demo: '%s'\n", demo);
    print_menu();
    return 1;
  }

  printf("\n");
  return 0;
}
