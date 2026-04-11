#define _POSIX_C_SOURCE 200809L
/* lesson_16.c — Arena String Handling
 * BUILD_DEPS: common/bench.c, lib/str.c
 *
 * Demonstrates arena-backed string utilities that eliminate the
 * "who frees this string?" problem.
 *
 * Run:   ./build-dev.sh --lesson=16 -r
 * Bench: ./build-dev.sh --lesson=16 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/str.h"

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 16 — Arena String Handling\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  Arena arena = {0};
  arena.min_block_size = 4096;

  /* ── 1. Basic string operations ─────────────────────────────────────*/
  print_section("Basic Arena Strings");
  {
    /* Push a copy of a C string literal */
    Str hello = str_push_cstr(&arena, "Hello, World!");
    printf("  str_push_cstr: \"%.*s\" (len=%zu)\n",
           (int)hello.len, hello.data, hello.len);

    /* Push from a Str literal */
    Str name = str_push(&arena, STR_LIT("Ryan Fleury"));
    printf("  str_push:      \"%.*s\" (len=%zu)\n",
           (int)name.len, name.data, name.len);

    /* Printf into arena */
    Str formatted = str_pushf(&arena, "Arena used: %zu bytes",
                               arena_total_used(&arena));
    printf("  str_pushf:     \"%.*s\" (len=%zu)\n",
           (int)formatted.len, formatted.data, formatted.len);
  }

  /* ── 2. Concatenation ───────────────────────────────────────────────*/
  print_section("String Concatenation");
  {
    Str first = STR_LIT("Memory ");
    Str last  = STR_LIT("Management");
    Str full  = str_concat(&arena, first, last);
    printf("  concat: \"%.*s\" + \"%.*s\" = \"%.*s\"\n",
           (int)first.len, first.data,
           (int)last.len,  last.data,
           (int)full.len,  full.data);
  }

  /* ── 3. Join ────────────────────────────────────────────────────────*/
  print_section("String Join");
  {
    Str words[] = {
      STR_LIT("arena"),
      STR_LIT("pool"),
      STR_LIT("stack"),
      STR_LIT("slab"),
    };
    Str joined = str_join(&arena, words, 4, STR_LIT(", "));
    printf("  join: \"%.*s\"\n", (int)joined.len, joined.data);
  }

  /* ── 4. Comparison ──────────────────────────────────────────────────*/
  print_section("String Comparison");
  {
    Str a = str_push_cstr(&arena, "hello");
    Str b = str_push_cstr(&arena, "hello");
    Str c = str_push_cstr(&arena, "world");

    printf("  \"hello\" == \"hello\": %s\n", str_eq(a, b) ? "true" : "false");
    printf("  \"hello\" == \"world\": %s\n", str_eq(a, c) ? "true" : "false");
  }

  /* ── 5. The key insight: no free() needed ───────────────────────────*/
  print_section("Why Arena Strings Matter");
  printf("  Traditional C strings (strdup + free):\n");
  printf("    char *s = strdup(\"hello\");  // who frees this?\n");
  printf("    char *t = concat(s, \"!\");   // now who frees s AND t?\n");
  printf("    // Must track ownership, call free() for each.\n");
  printf("    // Easy to leak or double-free.\n\n");
  printf("  Arena strings (str_push + arena_reset):\n");
  printf("    Str s = str_push_cstr(&arena, \"hello\");\n");
  printf("    Str t = str_concat(&arena, s, STR_LIT(\"!\"));\n");
  printf("    // Nobody frees individual strings.\n");
  printf("    // arena_reset() or arena_end_temp() frees them ALL.\n\n");

  printf("  Arena used for all strings above: %zu bytes\n",
         arena_total_used(&arena));
  arena_reset(&arena);
  printf("  After arena_reset: %zu bytes (all strings gone)\n",
         arena_total_used(&arena));

  /* ── 6. Real-world: build a formatted report ────────────────────────*/
  print_section("Real-World: Build a Report");
  {
    TempMemory temp = arena_begin_temp(&arena);

    Str header = str_pushf(&arena, "=== Memory Report ===\n");
    Str line1  = str_pushf(&arena, "  Total allocators: %d\n", 5);
    Str line2  = str_pushf(&arena, "  Arena size: %zu KB\n", (size_t)64);
    Str line3  = str_pushf(&arena, "  Peak usage: %.1f%%\n", 73.2);
    Str footer = str_pushf(&arena, "=====================\n");

    Str parts[] = { header, line1, line2, line3, footer };
    Str report = str_join(&arena, parts, 5, STR_LIT(""));

    printf("  Built report (%zu bytes):\n\n%.*s\n",
           report.len, (int)report.len, report.data);

    arena_end_temp(temp);  /* entire report freed */
    printf("  Report freed via arena_end_temp.\n");
  }

  /* ── 7. Benchmark ───────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 1000000;

    BENCH_SUITE(suite, "Arena str_push vs strdup+free") {
      /* strdup + free */
      BENCH_CASE(suite, "strdup+free", i, N) {
        char *s = strdup("Hello, arena strings are fast!");
        *(volatile char *)s = s[0];
        free(s);
      }
      BENCH_CASE_END(suite, N);

      /* Arena str_push */
      Arena bench_arena = {0};
      bench_arena.min_block_size = 1024 * 1024;
      BENCH_CASE(suite, "str_push (arena)", i, N) {
        Str s = str_push_cstr(&bench_arena, "Hello, arena strings are fast!");
        *(volatile char *)(char *)s.data = s.data[0];
        if (i % 10000 == 9999) arena_reset(&bench_arena);
      }
      BENCH_CASE_END(suite, N);
      arena_free(&bench_arena);
    }
  }
#else
  bench_skip_notice("lesson_16");
#endif

  arena_free(&arena);
  printf("\n");
  return 0;
}
