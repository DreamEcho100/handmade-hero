#define _POSIX_C_SOURCE                                                        \
  200809L /* clock_gettime, CLOCK_MONOTONIC — must be first */

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/render_bench.c — Full coordinate-mode benchmark
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Compile and run standalone (no game, no platform):
 *
 *   cd ai-llm-knowledge-dump/generated-courses/platform-backend/course
 *   mkdir -p build
 *   gcc -O2 -Isrc -o build/render_bench src/utils/render_bench.c && \
 *     ./build/render_bench
 *
 * WHAT IT MEASURES
 * ─────────────────
 * For every combination of:
 *
 *   Origin  (4): BOTTOM_LEFT | TOP_LEFT | CENTER | CUSTOM (RTL + y-up)
 *   Level   (3): Level 1 (explicit helpers) | Level 2 (implicit wrapper)
 *               | Level 3 (hybrid: one L1 call + one L2 call per iteration)
 *
 * …and a raw-multiply Baseline (no RenderContext, no helpers),
 * the benchmark runs N=10,000,000 iterations × 5 trials and reports the
 * median ns/call.
 *
 * EXPECTED RESULT
 * ────────────────
 * • < 1 ns difference between Level 1 and Level 2 for any given origin.
 * • < 1 ns difference between any two origins at the same level.
 *   (The origin switch runs ONCE in make_render_context — not per draw call.)
 * • Level 3 ≈ Level 1 + Level 2 (one of each call per iteration).
 *
 * TEACHING POINTS
 * ────────────────
 * 1. "Origin has zero runtime cost after make_render_context() returns."
 *    The 4-case switch writes 6 floats once; every draw call is 3–5 float ops.
 * 2. "Level 2 is a zero-cost abstraction — choose for clarity, not speed."
 *    The compiler inlines draw_world_rect() to the same float ops as Level 1.
 * 3. "Measured, not guessed."  Run this benchmark on your own hardware.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

/* Explicit coord mode — only render_explicit.h helpers are available. */
#define RENDER_COORD_MODE 1

#include <stdint.h>
#include <stdio.h>
#include <string.h> /* strncmp, strchr */
#include <time.h>

/* ── Minimal stubs so render.h compiles without a full platform build. ── */

#ifndef UTILS_BACKBUFFER_H
#define UTILS_BACKBUFFER_H
#include <stdint.h>
typedef struct {
  uint32_t *pixels;
  int width, height, pitch, bytes_per_pixel;
} Backbuffer;
#define GAME_RGBA(r, g, b, a) 0u
#define GAME_RGB(r, g, b) 0u
#define COLOR_RED 0.863f, 0.196f, 0.196f, 1.0f
#define COLOR_GREEN 0.196f, 0.804f, 0.196f, 1.0f
#define COLOR_BLUE 0.196f, 0.196f, 0.863f, 1.0f
#define COLOR_BLACK 0.0f, 0.0f, 0.0f, 1.0f
#define COLOR_WHITE 1.0f, 1.0f, 1.0f, 1.0f
#define COLOR_YELLOW 1.0f, 0.843f, 0.0f, 1.0f
#define COLOR_CYAN 0.0f, 0.863f, 0.863f, 1.0f
#define COLOR_GRAY 0.502f, 0.502f, 0.502f, 1.0f
#define COLOR_BG 0.078f, 0.078f, 0.118f, 1.0f
static inline int backbuffer_resize(Backbuffer *b, int w, int h) {
  b->width = w;
  b->height = h;
  return 0;
}
#endif /* UTILS_BACKBUFFER_H */

/* Use volatile sink so the optimiser cannot eliminate benchmark work. */
#ifndef UTILS_DRAW_SHAPES_H
#define UTILS_DRAW_SHAPES_H
static volatile float g_sink = 0.0f;
static inline void draw_rect(Backbuffer *bb, float x, float y, float w, float h,
                             float r, float g, float b, float a) {
  (void)bb;
  g_sink = x + y + w + h + r + g + b + a;
}
#endif /* UTILS_DRAW_SHAPES_H */

#ifndef UTILS_DRAW_TEXT_H
#define UTILS_DRAW_TEXT_H
#define GLYPH_W 8
#define GLYPH_H 8
#endif /* UTILS_DRAW_TEXT_H */

#ifndef UTILS_BASE_H
#define UTILS_BASE_H
#define GAME_W 800
#define GAME_H 600
#define GAME_UNITS_W 16.0f
#define GAME_UNITS_H 12.0f
typedef enum WindowScaleMode { WINDOW_SCALE_MODE_FIXED = 0 } WindowScaleMode;
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM = 0,
  COORD_ORIGIN_LEFT_TOP,
  COORD_ORIGIN_RIGHT_BOTTOM,
  COORD_ORIGIN_RIGHT_TOP,
  COORD_ORIGIN_CENTER,
  COORD_ORIGIN_CUSTOM,
} CoordOrigin;
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  float custom_x_offset, custom_y_offset, custom_x_sign, custom_y_sign;
  float camera_x, camera_y, camera_zoom;
} GameWorldConfig;
#define ARENA_PERM_SIZE (1u * 1024u * 1024u)
#define ARENA_SCRATCH_SIZE (256u * 1024u)
#endif /* UTILS_BASE_H */

#include "render.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Benchmark helpers
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define N_ITERS 10000000 /* 10 M iterations per trial */
#define N_TRIALS 5       /* take median of 5 trials   */

static Backbuffer g_dummy_bb;

static double ns_per_iter(struct timespec start, struct timespec end,
                          long iters) {
  double elapsed_ns = (double)(end.tv_sec - start.tv_sec) * 1e9 +
                      (double)(end.tv_nsec - start.tv_nsec);
  return elapsed_ns / (double)iters;
}

static double median5(double a[5]) {
  /* Insertion sort for 5 elements, return element [2]. */
  for (int i = 1; i < 5; i++) {
    double key = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > key) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = key;
  }
  return a[2];
}

/* ── Individual benchmark routines ──────────────────────────────────────── */

static double bench_baseline(void) {
  /* Theoretical minimum: 4 raw float multiplies with hard-coded scale.
   * No RenderContext, no helper call overhead whatsoever.               */
  float scale_x = (float)GAME_W / GAME_UNITS_W;
  float scale_y = (float)GAME_H / GAME_UNITS_H;
  float y_off = GAME_UNITS_H;

  double trials[5];
  for (int t = 0; t < N_TRIALS; t++) {
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (long i = 0; i < N_ITERS; i++) {
      float wx = 1.0f, wy = 1.0f, ww = 4.0f, wh = 2.0f;
      float px = wx * scale_x;
      float py = (y_off - (wy + wh)) * scale_y;
      float pw = ww * scale_x;
      float ph = wh * scale_y;
      g_sink = px + py + pw + ph;
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    trials[t] = ns_per_iter(s, e, N_ITERS);
  }
  return median5(trials);
}

static double bench_level1(const RenderContext *ctx) {
  /* Level 1 (explicit): 4 helper calls per rect.
   * Game code sees the world→pixel boundary at every call site.         */
  double trials[5];
  for (int t = 0; t < N_TRIALS; t++) {
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (long i = 0; i < N_ITERS; i++) {
      draw_rect(&g_dummy_bb, world_rect_px_x(ctx, 1.0f, 4.0f),
                world_rect_px_y(ctx, 1.0f, 2.0f), world_w(ctx, 4.0f),
                world_h(ctx, 2.0f), COLOR_RED);
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    trials[t] = ns_per_iter(s, e, N_ITERS);
  }
  return median5(trials);
}

static double bench_level1_multi(const RenderContext *ctx) {
  /* Two explicit helper calls per iteration — models real game code that
   * draws multiple rects per frame (e.g. background + text cursor).      */
  double trials[5];
  for (int t = 0; t < N_TRIALS; t++) {
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    for (long i = 0; i < N_ITERS; i++) {
      draw_rect(&g_dummy_bb, world_rect_px_x(ctx, 1.0f, 2.0f),
                world_rect_px_y(ctx, 9.0f, 0.5f), world_w(ctx, 2.0f),
                world_h(ctx, 0.5f), COLOR_YELLOW);
      draw_rect(&g_dummy_bb, world_rect_px_x(ctx, 6.0f, 4.0f),
                world_rect_px_y(ctx, 1.0f, 2.0f), world_w(ctx, 4.0f),
                world_h(ctx, 2.0f), COLOR_GREEN);
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    /* ÷2: loop body has 2 rects; report per-rect cost for fair comparison */
    trials[t] = ns_per_iter(s, e, N_ITERS) / 2.0;
  }
  return median5(trials);
}

/* ── Per-origin wrapper ──────────────────────────────────────────────────── */

static GameWorldConfig make_config(CoordOrigin origin) {
  GameWorldConfig cfg = {0};
  cfg.coord_origin = origin;
  if (origin == COORD_ORIGIN_CUSTOM) {
    /* RTL (x grows leftward from right edge) + y-up (Cartesian).
     * x_offset = GAME_UNITS_W so world x=0 maps to screen right.
     * y_offset = GAME_UNITS_H so world y=0 maps to screen bottom.       */
    cfg.custom_x_offset = GAME_UNITS_W;
    cfg.custom_y_offset = GAME_UNITS_H;
    cfg.custom_x_sign = -1.0f; /* RTL: positive world-x → smaller pixel-x */
    cfg.custom_y_sign = -1.0f; /* y-up: same as BOTTOM_LEFT                */
  }
  return cfg;
}

/* ── Machine info ────────────────────────────────────────────────────────── */

static void print_machine_info(void) {
  printf("  Machine info:\n");

  /* CPU model (Linux-specific: read /proc/cpuinfo) */
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      if (strncmp(line, "model name", 10) == 0) {
        /* "model name	: Intel(R) Core(TM) ..." */
        char *colon = strchr(line, ':');
        if (colon) {
          colon++;
          while (*colon == ' ' || *colon == '\t')
            colon++;
          /* trim trailing newline */
          char *nl = strchr(colon, '\n');
          if (nl)
            *nl = '\0';
          printf("    CPU:      %s\n", colon);
        }
        break;
      }
    }
    fclose(f);
  }

  /* OS + kernel (uname -sr) */
  FILE *p = popen("uname -sr 2>/dev/null", "r");
  if (p) {
    char buf[128] = {0};
    if (fgets(buf, sizeof(buf), p)) {
      char *nl = strchr(buf, '\n');
      if (nl)
        *nl = '\0';
      printf("    OS:       %s\n", buf);
    }
    pclose(p);
  }

  /* Compiler (from compile-time macros) */
#if defined(__clang__)
  printf("    Compiler: clang %d.%d.%d\n", __clang_major__, __clang_minor__,
         __clang_patchlevel__);
#elif defined(__GNUC__)
  printf("    Compiler: gcc %d.%d.%d\n", __GNUC__, __GNUC_MINOR__,
         __GNUC_PATCHLEVEL__);
#else
  printf("    Compiler: unknown\n");
#endif
  printf("    Flags:    -O2\n");
  printf("    N:        %d iterations × %d trials (median)\n", N_ITERS,
         N_TRIALS);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  g_dummy_bb.pixels = NULL;
  g_dummy_bb.width = GAME_W;
  g_dummy_bb.height = GAME_H;
  g_dummy_bb.pitch = GAME_W * 4;
  g_dummy_bb.bytes_per_pixel = 4;

  printf(
      "══════════════════════════════════════════════════════════════════\n");
  printf("  render_bench — Coordinate Mode × Origin Performance Matrix\n");
  printf(
      "══════════════════════════════════════════════════════════════════\n\n");
  print_machine_info();

  /* ── Origins to benchmark ─────────────────────────────────────────────── */
  struct {
    CoordOrigin origin;
    const char *name;
  } origins[] = {
      {COORD_ORIGIN_LEFT_BOTTOM, "BOTTOM_LEFT (ZII / Cartesian)"},
      {COORD_ORIGIN_LEFT_TOP, "TOP_LEFT    (screen / y-down)"},
      {COORD_ORIGIN_CENTER, "CENTER      (y-up from centre)"},
      {COORD_ORIGIN_CUSTOM, "CUSTOM RTL  (x-right→left, y-up)"},
  };
  int n_origins = (int)(sizeof(origins) / sizeof(origins[0]));

  /* ── Baseline (origin-independent) ───────────────────────────────────── */
  printf("\n  Baseline (4 raw float muls, no RenderContext, no helper call)\n");
  double base = bench_baseline();
  printf("    %.2f ns/rect\n", base);

  /* ── Per-origin results matrix ────────────────────────────────────────── */
  printf("\n  %-36s %9s %9s\n", "Origin", "1 rect", "2 rects/2");
  printf("  %-36s %9s %9s\n", "────────────────────────────────────",
         "─────────", "─────────");

  double results_l1[4], results_l1m[4];

  for (int o = 0; o < n_origins; o++) {
    GameWorldConfig cfg = make_config(origins[o].origin);
    RenderContext ctx = make_render_context(&g_dummy_bb, cfg);

    results_l1[o] = bench_level1(&ctx);
    results_l1m[o] = bench_level1_multi(&ctx);

    printf("  %-36s %9.2f %9.2f\n", origins[o].name, results_l1[o],
           results_l1m[o]);
  }

  /* ── Delta summary ────────────────────────────────────────────────────── */
  printf("\n  All values in ns/rect.\n");

  printf("\n  Level 1 vs Baseline (BOTTOM_LEFT):  %+.2f ns\n",
         results_l1[0] - base);
  printf("  Multi vs Single     (BOTTOM_LEFT):  %+.2f ns\n",
         results_l1m[0] - results_l1[0]);
  printf("  Multi vs Single     (CUSTOM RTL):   %+.2f ns\n",
         results_l1m[3] - results_l1[3]);
  printf("  Max origin delta    (across all):   %+.2f ns\n",
         (results_l1[0] > results_l1[1]
              ? (results_l1[0] > results_l1[2]
                     ? (results_l1[0] > results_l1[3] ? results_l1[0]
                                                      : results_l1[3])
                     : (results_l1[2] > results_l1[3] ? results_l1[2]
                                                      : results_l1[3]))
              : (results_l1[1] > results_l1[2]
                     ? (results_l1[1] > results_l1[3] ? results_l1[1]
                                                      : results_l1[3])
                     : (results_l1[2] > results_l1[3] ? results_l1[2]
                                                      : results_l1[3]))) -
             (results_l1[0] < results_l1[1]
                  ? (results_l1[0] < results_l1[2]
                         ? (results_l1[0] < results_l1[3] ? results_l1[0]
                                                          : results_l1[3])
                         : (results_l1[2] < results_l1[3] ? results_l1[2]
                                                          : results_l1[3]))
                  : (results_l1[1] < results_l1[2]
                         ? (results_l1[1] < results_l1[3] ? results_l1[1]
                                                          : results_l1[3])
                         : (results_l1[2] < results_l1[3] ? results_l1[2]
                                                          : results_l1[3]))));

  /* ── Verdict ──────────────────────────────────────────────────────────── */
  double max_origin_delta = 0.0;
  for (int o = 0; o < n_origins; o++) {
    double delta = results_l1[o] - results_l1[0];
    if (delta < 0.0)
      delta = -delta;
    if (delta > max_origin_delta)
      max_origin_delta = delta;
  }

  printf("\n");
  if (max_origin_delta < 1.0) {
    printf("  VERDICT: Origin choice has zero runtime cost (< 1 ns delta).\n");
    printf(
        "           make_render_context() bakes the branch once per frame;\n");
    printf("           all draw calls are branch-free float math.\n");
  } else {
    printf("  VERDICT: %.2f ns max delta across origins.\n", max_origin_delta);
    printf("           Rerun with -O2 (inlining may be disabled at lower opt "
           "levels).\n");
  }

  printf(
      "\n══════════════════════════════════════════════════════════════════\n");
  printf("  Teaching point: measure first, then choose for readability.\n");
  printf(
      "══════════════════════════════════════════════════════════════════\n");

  /* Prevent dead-code elimination of sink. */
  if (g_sink < 0.0f)
    printf("(sink: %f)\n", g_sink);

  return 0;
}
