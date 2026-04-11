/* lesson_07.c — Memory Bugs: Leaks and Dangling Pointers
 *
 * Demonstrates three classic memory bugs and their fixes:
 *   "leak"   — allocate memory, never free it (ASan reports the leak)
 *   "uaf"    — use-after-free: free then read (ASan catches it)
 *   "double" — double-free: free the same pointer twice (ASan catches it)
 *   "fixed"  — corrected versions of all three bugs
 *
 * Run:  ./build-dev.sh --lesson=07 -r
 *       (then manually: ./build/lesson_07 leak)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Sub-program: Memory Leak ────────────────────────────────────────────*/

static void demo_leak(void) {
  printf("\n── Demo: Memory Leak ──────────────────────────────────────────\n\n");
  printf("  Allocating 256 bytes with malloc... and never freeing them.\n");
  printf("  ASan (with detect_leaks=1) will report this at exit.\n\n");

  char *buf = malloc(256);
  if (!buf) { fprintf(stderr, "  malloc failed\n"); return; }

  /* Write something so the allocation isn't optimized away. */
  memset(buf, 'A', 256);
  printf("  buf[0] = '%c'  (allocation is alive, but we never call free)\n", buf[0]);

  /* Deliberately NOT calling free(buf) — this is the bug. */
  printf("\n  [BUG] Exiting without free(buf). Check ASan output below.\n");
}

/* ── Sub-program: Use-After-Free ─────────────────────────────────────────*/

static void demo_use_after_free(void) {
  printf("\n── Demo: Use-After-Free ───────────────────────────────────────\n\n");
  printf("  Allocating 128 bytes, writing a message, then freeing.\n");
  printf("  After free, we read from the pointer — undefined behavior!\n\n");

  char *msg = malloc(128);
  if (!msg) { fprintf(stderr, "  malloc failed\n"); return; }

  strcpy(msg, "Hello from the heap!");
  printf("  Before free: msg = \"%s\"\n", msg);

  free(msg);
  printf("  free(msg) called.\n");

  /* BUG: reading freed memory */
  printf("  [BUG] Reading after free: msg[0] = 0x%02x\n",
         (unsigned char)msg[0]);
  printf("\n  ASan should have caught this use-after-free above.\n");
}

/* ── Sub-program: Double-Free ────────────────────────────────────────────*/

static void demo_double_free(void) {
  printf("\n── Demo: Double-Free ──────────────────────────────────────────\n\n");
  printf("  Allocating 64 bytes, freeing once, then freeing again.\n");
  printf("  The second free is undefined behavior — ASan catches it.\n\n");

  char *data = malloc(64);
  if (!data) { fprintf(stderr, "  malloc failed\n"); return; }

  memset(data, 'X', 64);
  printf("  First free(data)... OK.\n");
  free(data);

  printf("  [BUG] Second free(data)...\n");
  free(data);   /* BUG: double free */

  printf("\n  ASan should have aborted before reaching this line.\n");
}

/* ── Sub-program: Fixed Versions ─────────────────────────────────────────*/

static void demo_fixed(void) {
  printf("\n── Demo: Fixed Versions ───────────────────────────────────────\n\n");

  /* Fix 1: Always free what you malloc. */
  printf("  Fix 1 — No Leak:\n");
  printf("    Allocate, use, then free.\n");
  char *buf = malloc(256);
  if (!buf) { fprintf(stderr, "  malloc failed\n"); return; }
  memset(buf, 'A', 256);
  printf("    buf[0] = '%c'\n", buf[0]);
  free(buf);
  buf = NULL;   /* Set to NULL after free — good habit. */
  printf("    free(buf); buf = NULL;  -- no leak.\n\n");

  /* Fix 2: Never use a pointer after freeing it. */
  printf("  Fix 2 — No Use-After-Free:\n");
  printf("    Copy the data you need BEFORE freeing.\n");
  char *msg = malloc(128);
  if (!msg) { fprintf(stderr, "  malloc failed\n"); return; }
  strcpy(msg, "Hello from the heap!");
  char saved_char = msg[0];    /* Save what we need before free. */
  printf("    Before free: msg = \"%s\"\n", msg);
  free(msg);
  msg = NULL;
  printf("    After free: saved_char = '%c' (read from local copy)\n", saved_char);
  printf("    msg is NULL — we won't accidentally use it.\n\n");

  /* Fix 3: Set pointer to NULL after free; check before freeing. */
  printf("  Fix 3 — No Double-Free:\n");
  printf("    Set pointer to NULL after free, guard future frees.\n");
  char *data = malloc(64);
  if (!data) { fprintf(stderr, "  malloc failed\n"); return; }
  memset(data, 'X', 64);
  free(data);
  data = NULL;
  printf("    free(data); data = NULL;\n");
  /* Safe: freeing NULL is a no-op per the C standard. */
  free(data);
  printf("    free(data) again — data is NULL, so this is a safe no-op.\n");

  printf("\n  All three bugs avoided. ASan is happy.\n");
}

/* ── Main: dispatch on argv[1] ───────────────────────────────────────────*/

static void print_menu(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 07 — Memory Bugs: Leaks and Dangling Pointers\n");
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("\n");
  printf("  Usage: ./build/lesson_07 <demo>\n\n");
  printf("  Available demos:\n");
  printf("    leak     Allocate memory, never free (ASan reports leak)\n");
  printf("    uaf      Use-after-free: read from freed pointer\n");
  printf("    double   Double-free the same pointer\n");
  printf("    fixed    Corrected versions of all three bugs\n");
  printf("\n");
  printf("  Build: ./build-dev.sh --lesson=07 -r\n");
  printf("  Then:  ./build/lesson_07 leak\n");
  printf("\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_menu();
    return 0;
  }

  const char *demo = argv[1];

  if (strcmp(demo, "leak") == 0) {
    demo_leak();
  } else if (strcmp(demo, "uaf") == 0) {
    demo_use_after_free();
  } else if (strcmp(demo, "double") == 0) {
    demo_double_free();
  } else if (strcmp(demo, "fixed") == 0) {
    demo_fixed();
  } else {
    fprintf(stderr, "  Unknown demo: '%s'\n", demo);
    print_menu();
    return 1;
  }

  printf("\n");
  return 0;
}
