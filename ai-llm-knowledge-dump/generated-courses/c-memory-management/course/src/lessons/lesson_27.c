/* lesson_27.c — SoA vs AoS (Structure of Arrays vs Array of Structures)
 * BUILD_DEPS: common/bench.c
 *
 * Implements a particle system two ways:
 *   AoS: struct Particle { float x, y, vx, vy, life; }; Particle particles[N];
 *   SoA: struct Particles { float *x, *y, *vx, *vy, *life; };
 *
 * Benchmarks update_positions (touches x,y,vx,vy only) and
 * update_all (touches all fields).
 *
 * Run:   ./build-dev.sh --lesson=27 -r
 * Bench: ./build-dev.sh --lesson=27 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ── Particle count ───────────────────────────────────────────────────────*/
#define PARTICLE_COUNT (1024 * 1024)  /* 1M particles */

/* ── AoS layout ───────────────────────────────────────────────────────────
 *
 * Each particle is a contiguous struct:
 *   [x][y][vx][vy][life] [x][y][vx][vy][life] [x][y][vx][vy][life] ...
 *
 * sizeof(ParticleAoS) = 20 bytes.  When we only need x,y,vx,vy (16 bytes),
 * we still load the 'life' field into cache.  With 1M particles, that's
 * 4 MB of wasted bandwidth.
 */
typedef struct ParticleAoS {
  float x, y, vx, vy, life;
} ParticleAoS;

/* ── SoA layout ───────────────────────────────────────────────────────────
 *
 * Each field is a separate contiguous array:
 *   x:    [x0][x1][x2][x3]...
 *   y:    [y0][y1][y2][y3]...
 *   vx:   [vx0][vx1][vx2]...
 *   vy:   [vy0][vy1][vy2]...
 *   life: [l0][l1][l2][l3]...
 *
 * When we only update positions (x,y,vx,vy), the 'life' array is never
 * touched — it stays out of cache entirely.  Perfect spatial locality
 * for the fields we DO access.
 */
typedef struct ParticlesSoA {
  float *x;
  float *y;
  float *vx;
  float *vy;
  float *life;
} ParticlesSoA;

/* ── AoS operations ───────────────────────────────────────────────────────*/
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

static void aos_init(ParticleAoS *p, int n) {
  for (int i = 0; i < n; i++) {
    p[i].x    = (float)(i % 1000);
    p[i].y    = (float)(i / 1000);
    p[i].vx   = 1.0f;
    p[i].vy   = 0.5f;
    p[i].life  = 100.0f;
  }
}

static void aos_update_positions(ParticleAoS *p, int n, float dt) {
  for (int i = 0; i < n; i++) {
    p[i].x += p[i].vx * dt;
    p[i].y += p[i].vy * dt;
  }
}

static void aos_update_all(ParticleAoS *p, int n, float dt) {
  for (int i = 0; i < n; i++) {
    p[i].x    += p[i].vx * dt;
    p[i].y    += p[i].vy * dt;
    p[i].life -= dt;
  }
}

/* ── SoA operations ───────────────────────────────────────────────────────*/

static ParticlesSoA soa_create(int n) {
  ParticlesSoA s;
  s.x    = malloc((size_t)n * sizeof(float));
  s.y    = malloc((size_t)n * sizeof(float));
  s.vx   = malloc((size_t)n * sizeof(float));
  s.vy   = malloc((size_t)n * sizeof(float));
  s.life = malloc((size_t)n * sizeof(float));
  return s;
}

static void soa_free(ParticlesSoA *s) {
  free(s->x);  free(s->y);
  free(s->vx); free(s->vy);
  free(s->life);
}

static void soa_init(ParticlesSoA *s, int n) {
  for (int i = 0; i < n; i++) {
    s->x[i]    = (float)(i % 1000);
    s->y[i]    = (float)(i / 1000);
    s->vx[i]   = 1.0f;
    s->vy[i]   = 0.5f;
    s->life[i] = 100.0f;
  }
}

static void soa_update_positions(ParticlesSoA *s, int n, float dt) {
  for (int i = 0; i < n; i++) {
    s->x[i] += s->vx[i] * dt;
    s->y[i] += s->vy[i] * dt;
  }
}

static void soa_update_all(ParticlesSoA *s, int n, float dt) {
  for (int i = 0; i < n; i++) {
    s->x[i]    += s->vx[i] * dt;
    s->y[i]    += s->vy[i] * dt;
    s->life[i] -= dt;
  }
}

#pragma clang diagnostic pop

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 27 — SoA vs AoS (Structure of Arrays)\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Layout comparison ────────────────────────────────────────────── */
  print_section("Memory Layout Comparison");
  {
    printf("  AoS (Array of Structures):\n");
    printf("    struct Particle { float x, y, vx, vy, life; };\n");
    printf("    Particle particles[N];\n\n");
    printf("    Memory: [x0 y0 vx0 vy0 l0] [x1 y1 vx1 vy1 l1] ...\n");
    printf("    sizeof = %zu bytes per particle\n\n", sizeof(ParticleAoS));

    printf("  SoA (Structure of Arrays):\n");
    printf("    struct Particles { float *x, *y, *vx, *vy, *life; };\n\n");
    printf("    Memory:\n");
    printf("      x:    [x0 x1 x2 x3 ...]\n");
    printf("      y:    [y0 y1 y2 y3 ...]\n");
    printf("      vx:   [vx0 vx1 vx2 ...]\n");
    printf("      vy:   [vy0 vy1 vy2 ...]\n");
    printf("      life: [l0 l1 l2 l3 ...]\n\n");
    printf("    5 separate arrays, each %zu bytes for %d particles\n",
           (size_t)PARTICLE_COUNT * sizeof(float), PARTICLE_COUNT);
  }

  /* ── 2. When SoA wins ───────────────────────────────────────────────── */
  print_section("When Does SoA Win?");
  {
    printf("  SoA wins when you access a SUBSET of fields.\n\n");
    printf("  update_positions touches: x, y, vx, vy  (4 of 5 fields)\n");
    printf("    AoS: loads 20 bytes/particle, uses 16 — 80%% utilization\n");
    printf("    SoA: loads exactly 16 bytes/particle — 100%% utilization\n\n");
    printf("  A hypothetical 'is_alive' check touching only 'life':\n");
    printf("    AoS: loads 20 bytes/particle, uses 4 — 20%% utilization\n");
    printf("    SoA: loads exactly 4 bytes/particle — 100%% utilization\n\n");
    printf("  AoS is simpler and fine for small N or when you always\n");
    printf("  access all fields together.\n");
  }

  /* ── 3. Benchmarks ──────────────────────────────────────────────────── */
#ifdef BENCH_MODE
  {
    int N = PARTICLE_COUNT;
    float dt = 0.016f;
    long iters = 10;

    /* Allocate both layouts */
    ParticleAoS *aos = malloc((size_t)N * sizeof(ParticleAoS));
    ParticlesSoA soa = soa_create(N);
    if (!aos || !soa.x) { fprintf(stderr, "OOM\n"); return 1; }
    aos_init(aos, N);
    soa_init(&soa, N);

    /* ── Benchmark: update_positions ── */
    BenchSuite suite1;
    BENCH_SUITE(suite1, "update_positions (x,y,vx,vy only)") {
      BENCH_CASE(suite1, "AoS", i, iters) {
        aos_update_positions(aos, N, dt);
      }
      BENCH_CASE_END(suite1, iters);

      BENCH_CASE(suite1, "SoA", i, iters) {
        soa_update_positions(&soa, N, dt);
      }
      BENCH_CASE_END(suite1, iters);
    }

    /* Re-init to avoid float drift affecting comparison */
    aos_init(aos, N);
    soa_init(&soa, N);

    /* ── Benchmark: update_all ── */
    BenchSuite suite2;
    BENCH_SUITE(suite2, "update_all (x,y,vx,vy,life)") {
      BENCH_CASE(suite2, "AoS", i, iters) {
        aos_update_all(aos, N, dt);
      }
      BENCH_CASE_END(suite2, iters);

      BENCH_CASE(suite2, "SoA", i, iters) {
        soa_update_all(&soa, N, dt);
      }
      BENCH_CASE_END(suite2, iters);
    }

    free(aos);
    soa_free(&soa);

    printf("  SoA should be faster for update_positions (less data loaded).\n");
    printf("  For update_all, the gap narrows since all fields are accessed.\n\n");
  }
#else
  bench_skip_notice("lesson_27");
#endif

  printf("\n");
  return 0;
}
