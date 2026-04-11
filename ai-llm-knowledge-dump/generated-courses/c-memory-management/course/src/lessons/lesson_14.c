/* lesson_14.c — Arena TempMemory Checkpoints
 * BUILD_DEPS: common/bench.c
 *
 * Demonstrates TempMemory begin/end for scoped allocation lifetimes.
 * Nested temp regions and arena_check for mismatch detection.
 *
 * Run:  ./build-dev.sh --lesson=14 -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 14 — Arena TempMemory Checkpoints\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  Arena arena = {0};
  arena.min_block_size = 4096;

  /* ── 1. Basic TempMemory usage ──────────────────────────────────────
   *
   * Ryan Fleury: "Begin a scratch arena scope, call into a function,
   * look through the parse tree, then pop."
   *
   * TempMemory saves a checkpoint.  arena_end_temp rolls back to it.
   */
  print_section("Basic TempMemory Usage");
  {
    /* Permanent allocation — survives the temp scope */
    int *permanent = ARENA_PUSH_STRUCT(&arena, int);
    *permanent = 42;
    printf("  Permanent alloc: %p → %d\n", (void *)permanent, *permanent);
    printf("  Arena used: %zu bytes\n\n", arena_total_used(&arena));

    /* Begin temp scope */
    TempMemory temp = arena_begin_temp(&arena);
    printf("  --- TempMemory BEGIN ---\n");

    /* Temp allocations */
    float *scratch_buf = ARENA_PUSH_ARRAY(&arena, 100, float);
    for (int i = 0; i < 100; i++) scratch_buf[i] = (float)i;

    char *temp_str = (char *)arena_push_size(&arena, 256, 1);
    snprintf(temp_str, 256, "This string is temporary!");

    printf("  Scratch buf: %p (100 floats)\n", (void *)scratch_buf);
    printf("  Temp string: \"%s\"\n", temp_str);
    printf("  Arena used: %zu bytes\n\n", arena_total_used(&arena));

    /* End temp scope — all temp allocations freed */
    arena_end_temp(temp);
    printf("  --- TempMemory END ---\n");
    printf("  Arena used: %zu bytes (back to before temp scope)\n",
           arena_total_used(&arena));
    printf("  Permanent still valid: %p → %d\n",
           (void *)permanent, *permanent);
  }

  /* ── 2. Nested TempMemory ───────────────────────────────────────────*/
  print_section("Nested TempMemory Scopes");
  {
    arena_reset(&arena);

    int *base = ARENA_PUSH_STRUCT(&arena, int);
    *base = 1;
    size_t used_0 = arena_total_used(&arena);
    printf("  Level 0: base=%d  used=%zu\n", *base, used_0);

    TempMemory t1 = arena_begin_temp(&arena);
    {
      int *level1 = ARENA_PUSH_ARRAY(&arena, 50, int);
      level1[0] = 2;
      size_t used_1 = arena_total_used(&arena);
      printf("  Level 1: level1[0]=%d  used=%zu\n", level1[0], used_1);

      TempMemory t2 = arena_begin_temp(&arena);
      {
        int *level2 = ARENA_PUSH_ARRAY(&arena, 100, int);
        level2[0] = 3;
        size_t used_2 = arena_total_used(&arena);
        printf("  Level 2: level2[0]=%d  used=%zu\n", level2[0], used_2);

        /* End inner scope — level2 freed */
        arena_end_temp(t2);
        printf("  After end_temp(t2): used=%zu (level2 gone)\n",
               arena_total_used(&arena));
      }

      /* level1 still alive */
      printf("  level1[0] still = %d\n", level1[0]);

      /* End outer scope — level1 freed */
      arena_end_temp(t1);
      printf("  After end_temp(t1): used=%zu (level1 gone)\n",
             arena_total_used(&arena));
    }

    /* base still alive */
    printf("  base still = %d\n", *base);
    printf("  Nesting works like function call stack scopes.\n");
  }

  /* ── 3. arena_keep_temp — conditional commit ────────────────────────*/
  print_section("arena_keep_temp — Conditional Commit");
  {
    arena_reset(&arena);
    printf("  Use case: load an asset.  Keep it if valid, roll back if not.\n\n");

    for (int trial = 0; trial < 3; trial++) {
      TempMemory t = arena_begin_temp(&arena);

      /* Simulate asset loading */
      int *asset = ARENA_PUSH_ARRAY(&arena, 256, int);
      for (int i = 0; i < 256; i++) asset[i] = i + trial;

      /* Simulate validation (fail on trial 1) */
      int valid = (trial != 1);

      if (valid) {
        arena_keep_temp(t);  /* commit — keep the allocation */
        printf("  Trial %d: VALID → kept (used=%zu)\n",
               trial, arena_total_used(&arena));
      } else {
        arena_end_temp(t);   /* rollback — free the allocation */
        printf("  Trial %d: INVALID → rolled back (used=%zu)\n",
               trial, arena_total_used(&arena));
      }
    }
  }

  /* ── 4. arena_check — mismatch detection ────────────────────────────*/
  print_section("arena_check — Catch Mismatched begin/end");
  {
    arena_reset(&arena);
    printf("  arena_check asserts temp_count == 0.\n");
    printf("  Call it at frame/request boundaries to catch leaks.\n\n");

    TempMemory t = arena_begin_temp(&arena);
    arena_push_size(&arena, 100, 1);
    arena_end_temp(t);
    arena_check(&arena);  /* should pass */
    printf("  arena_check passed (no orphaned temp scopes).\n");

    printf("\n  If you forget arena_end_temp, arena_check will assert.\n");
    printf("  This catches the arena equivalent of a memory leak.\n");
  }

  /* ── 5. Per-frame scratch pattern (complete) ────────────────────────*/
  print_section("Complete Per-Frame Pattern");
  {
    Arena scratch = {0};
    scratch.min_block_size = 64 * 1024;

    printf("  for each frame:\n");
    printf("    TempMemory frame = arena_begin_temp(&scratch);\n");
    printf("    // ... allocate freely ...\n");
    printf("    arena_end_temp(frame);\n");
    printf("    arena_check(&scratch);\n\n");

    for (int frame = 0; frame < 5; frame++) {
      TempMemory f = arena_begin_temp(&scratch);

      /* Frame work */
      char *msg = (char *)arena_push_size(&scratch, 64, 1);
      snprintf(msg, 64, "Frame %d: scratch used=%zu", frame,
               arena_total_used(&scratch));
      printf("    %s\n", msg);

      arena_end_temp(f);
      arena_check(&scratch);
    }
    printf("\n  Each frame cleans up perfectly.  Zero leaks by design.\n");

    arena_free(&scratch);
  }

  arena_free(&arena);
  printf("\n");
  return 0;
}
