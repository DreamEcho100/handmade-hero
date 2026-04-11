/* lesson_01.c — Memory Layout Exploration
 * BUILD_DEPS: common/bench.c
 *
 * Prints the addresses of variables in each memory segment:
 * text (code), data (initialized globals), BSS (zero-initialized globals),
 * heap (malloc), and stack (local variables).
 *
 * Run:  ./build-dev.sh --lesson=01 -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Text segment ─────────────────────────────────────────────────────────
 * Function code lives in the text (code) segment.  It's typically at the
 * lowest addresses and is read-only + executable.
 */
void text_segment_function(void) {
  /* This function exists so we can print the address of code. */
}

/* ── Data segment ─────────────────────────────────────────────────────────
 * Initialized global/static variables go in the data segment.
 * The compiler embeds their initial values into the executable.
 */
int global_initialized = 42;
static int static_initialized = 99;

/* ── BSS segment ──────────────────────────────────────────────────────────
 * Uninitialized (or zero-initialized) globals go in BSS.
 * BSS stands for "Block Started by Symbol" — the OS zeros this memory
 * at program start, so the executable doesn't store all those zeros.
 */
int global_uninitialized;
static int static_uninitialized;
static int global_zero = 0;

/* ── Demonstrate the memory layout ────────────────────────────────────────*/

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 01 — C Memory Layout Exploration\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── Stack segment ──────────────────────────────────────────────────
   * Local variables live on the stack.  The stack typically occupies the
   * highest user-space addresses and grows downward.
   */
  int stack_var_a = 10;
  int stack_var_b = 20;
  char stack_array[64];
  stack_array[0] = 'A';  /* touch it so compiler doesn't optimize away */

  /* ── Heap segment ───────────────────────────────────────────────────
   * Dynamic allocations from malloc live on the heap.  The heap is
   * between BSS and the stack, growing upward.
   */
  void *heap_ptr_a = malloc(64);
  void *heap_ptr_b = malloc(1024);
  void *heap_ptr_c = malloc(64 * 1024);

  /* ── Print all addresses ────────────────────────────────────────────*/

  print_section("TEXT segment (code)");
  print_ptr("main()",                  (void *)(uintptr_t)main);
  print_ptr("text_segment_function()", (void *)(uintptr_t)text_segment_function);
  print_ptr("printf()",               (void *)(uintptr_t)printf);

  print_section("DATA segment (initialized globals)");
  print_ptr("global_initialized",  &global_initialized);
  print_ptr("static_initialized",  &static_initialized);

  print_section("BSS segment (uninitialized/zero globals)");
  print_ptr("global_uninitialized", &global_uninitialized);
  print_ptr("static_uninitialized", &static_uninitialized);
  print_ptr("global_zero",          &global_zero);
  printf("\n  (BSS values are zero: global_uninitialized=%d, global_zero=%d)\n",
         global_uninitialized, global_zero);

  print_section("HEAP segment (malloc)");
  print_ptr("heap_ptr_a (64 B)",     heap_ptr_a);
  print_ptr("heap_ptr_b (1 KB)",     heap_ptr_b);
  print_ptr("heap_ptr_c (64 KB)",    heap_ptr_c);

  print_section("STACK segment (local variables)");
  print_ptr("stack_var_a",   &stack_var_a);
  print_ptr("stack_var_b",   &stack_var_b);
  print_ptr("stack_array",   stack_array);

  /* ── Address ordering summary ───────────────────────────────────────*/

  print_section("Address Ordering Summary");
  printf("  Typical Linux x86-64 layout (low → high):\n\n");
  printf("    TEXT   (code)              ~ %p\n", (void *)(uintptr_t)main);
  printf("    DATA   (init globals)      ~ %p\n", (void *)&global_initialized);
  printf("    BSS    (zero globals)      ~ %p\n", (void *)&global_uninitialized);
  printf("    HEAP   (malloc, grows ↑)   ~ %p\n", heap_ptr_a);
  printf("    ...    (unmapped gap)\n");
  printf("    STACK  (locals, grows ↓)   ~ %p\n", (void *)&stack_var_a);

  print_section("Stack Growth Direction");
  /* Call a nested function to observe stack growth */
  volatile int outer_var = 0;
  printf("  &outer_var (main):    %p\n", (void *)&outer_var);

  /* We can't easily call a nested function inline, but we can show
   * that stack_var_a > stack_var_b in address (both in same frame) and
   * that the stack grows downward by comparing frame addresses.       */
  printf("  &stack_var_a:         %p\n", (void *)&stack_var_a);
  printf("  &stack_var_b:         %p\n", (void *)&stack_var_b);

  ptrdiff_t stack_diff = (uint8_t *)&stack_var_a - (uint8_t *)&stack_var_b;
  printf("  Difference (a - b):   %td bytes\n", stack_diff);
  printf("  (Variables declared earlier may have higher addresses on\n");
  printf("   the stack, but this is compiler-dependent. The key insight\n");
  printf("   is that deeper function calls use LOWER addresses.)\n");

  /* ── Heap growth demo ───────────────────────────────────────────────*/

  print_section("Heap Growth Direction");
  printf("  heap_ptr_a: %p\n", heap_ptr_a);
  printf("  heap_ptr_b: %p\n", heap_ptr_b);
  printf("  heap_ptr_c: %p\n", heap_ptr_c);
  ptrdiff_t heap_diff = (uint8_t *)heap_ptr_b - (uint8_t *)heap_ptr_a;
  printf("  Difference (b - a):   %td bytes\n", heap_diff);
  printf("  (Heap generally grows upward, but malloc may use different\n");
  printf("   arenas/bins, so addresses aren't always strictly increasing.)\n");

  /* ── /proc/self/maps (Linux only) ───────────────────────────────────*/

  print_section("Process Memory Map (Linux: /proc/self/maps)");
  FILE *maps = fopen("/proc/self/maps", "r");
  if (maps) {
    char line[256];
    int count = 0;
    printf("  (Showing first 20 lines)\n\n");
    while (fgets(line, sizeof(line), maps) && count < 20) {
      printf("  %s", line);
      count++;
    }
    if (!feof(maps)) printf("  ... (truncated)\n");
    fclose(maps);
  } else {
    printf("  /proc/self/maps not available (not on Linux?).\n");
    printf("  On macOS, try: vmmap <pid>\n");
  }

  /* ── Cleanup ────────────────────────────────────────────────────────*/
  free(heap_ptr_a);
  free(heap_ptr_b);
  free(heap_ptr_c);

  printf("\n");
  return 0;
}
