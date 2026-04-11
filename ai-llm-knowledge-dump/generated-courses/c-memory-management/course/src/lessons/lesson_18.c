/* lesson_18.c — Pool Allocator Fixed-Size
 * BUILD_DEPS: common/bench.c, lib/pool.c
 *
 * Demonstrates a pool allocator for fixed-size objects using an embedded
 * free list.  Simulates a particle system with spawn/despawn, and
 * benchmarks pool_alloc vs malloc.
 *
 * Run:   ./build-dev.sh --lesson=18 -r
 * Bench: ./build-dev.sh --lesson=18 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/pool.h"

/* ── Particle type ─────────────────────────────────────────────────────────*/
typedef struct Particle {
  float x, y;
  float vx, vy;
  float life;
  int   active;
} Particle;

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 18 — Pool Allocator Fixed-Size\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic pool usage ─────────────────────────────────────────────*/
  print_section("Basic Pool Usage");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;

    Pool pool;
    pool_init(&pool, &arena, sizeof(Particle), 10);

    printf("  Pool created: obj_size=%zu  count=%zu  free=%zu\n",
           pool.obj_size, pool.count, pool_count_free(&pool));

    /* Allocate a few particles */
    Particle *p1 = (Particle *)pool_alloc(&pool);
    p1->x = 1.0f; p1->y = 2.0f; p1->life = 1.0f; p1->active = 1;

    Particle *p2 = (Particle *)pool_alloc(&pool);
    p2->x = 3.0f; p2->y = 4.0f; p2->life = 0.8f; p2->active = 1;

    Particle *p3 = (Particle *)pool_alloc(&pool);
    p3->x = 5.0f; p3->y = 6.0f; p3->life = 0.5f; p3->active = 1;

    printf("  Allocated 3 particles.\n");
    printf("  p1=%p  (%.0f, %.0f)  life=%.1f\n",
           (void *)p1, p1->x, p1->y, p1->life);
    printf("  p2=%p  (%.0f, %.0f)  life=%.1f\n",
           (void *)p2, p2->x, p2->y, p2->life);
    printf("  p3=%p  (%.0f, %.0f)  life=%.1f\n",
           (void *)p3, p3->x, p3->y, p3->life);
    printf("  Free slots: %zu\n", pool_count_free(&pool));

    /* Free the middle one */
    pool_free(&pool, p2);
    printf("\n  Freed p2.  Free slots: %zu\n", pool_count_free(&pool));

    /* Reallocate — should reuse p2's slot */
    Particle *p4 = (Particle *)pool_alloc(&pool);
    p4->x = 9.0f; p4->y = 10.0f; p4->life = 1.0f; p4->active = 1;
    printf("  Allocated p4=%p  (reused slot? %s)\n",
           (void *)p4, (p4 == p2) ? "YES" : "no");

    arena_free(&arena);
  }

  /* ── 2. How the free list works ──────────────────────────────────────*/
  print_section("Embedded Free List Visualization");
  {
    Arena arena = {0};
    arena.min_block_size = 4096;

    Pool pool;
    pool_init(&pool, &arena, sizeof(Particle), 5);

    printf("  Pool layout (5 Particle slots, %zu bytes each):\n\n", pool.obj_size);

    /* Allocate all 5 */
    Particle *slots[5];
    for (int i = 0; i < 5; i++) {
      slots[i] = (Particle *)pool_alloc(&pool);
      slots[i]->life = (float)(i + 1);
    }
    printf("  All 5 allocated.  Free=%zu\n", pool_count_free(&pool));

    /* Free slots 1, 3 (non-contiguous) */
    pool_free(&pool, slots[1]);
    pool_free(&pool, slots[3]);
    printf("  Freed slots 1 and 3.  Free=%zu\n\n", pool_count_free(&pool));

    printf("  Slot layout:\n");
    for (int i = 0; i < 5; i++) {
      int is_free = (slots[i] == slots[1] || slots[i] == slots[3]);
      printf("    [%d] %p  %s\n", i, (void *)slots[i],
             is_free ? "FREE (stores next-ptr)" : "USED (particle data)");
    }

    printf("\n  The free list links freed slots together through the\n");
    printf("  same memory that held object data — zero overhead!\n");

    arena_free(&arena);
  }

  /* ── 3. Particle system simulation ───────────────────────────────────*/
  print_section("Particle System Simulation");
  {
    Arena arena = {0};
    arena.min_block_size = 256 * 1024;

    #define MAX_PARTICLES 1000
    Pool pool;
    pool_init(&pool, &arena, sizeof(Particle), MAX_PARTICLES);

    Particle *active[MAX_PARTICLES];
    int num_active = 0;

    printf("  Simulating %d frames of particle spawn/despawn:\n\n", 10);

    /* Simple LCG for deterministic "randomness" */
    unsigned rng = 42;

    for (int frame = 0; frame < 10; frame++) {
      /* Spawn 50-150 particles */
      int to_spawn = 50 + (int)(rng % 100);
      rng = rng * 1103515245 + 12345;
      int spawned = 0;
      for (int i = 0; i < to_spawn && num_active < MAX_PARTICLES; i++) {
        Particle *p = (Particle *)pool_alloc(&pool);
        if (!p) break;
        p->x = (float)(rng % 800); rng = rng * 1103515245 + 12345;
        p->y = (float)(rng % 600); rng = rng * 1103515245 + 12345;
        p->vx = ((float)(rng % 200) - 100.0f) / 100.0f; rng = rng * 1103515245 + 12345;
        p->vy = ((float)(rng % 200) - 100.0f) / 100.0f; rng = rng * 1103515245 + 12345;
        p->life = 1.0f;
        p->active = 1;
        active[num_active++] = p;
        spawned++;
      }

      /* Age and despawn dead particles */
      int despawned = 0;
      for (int i = num_active - 1; i >= 0; i--) {
        active[i]->life -= 0.15f;
        if (active[i]->life <= 0.0f) {
          pool_free(&pool, active[i]);
          active[i] = active[--num_active];
          despawned++;
        }
      }

      printf("    Frame %2d: +%3d spawned  -%3d despawned  "
             "active=%4d  pool_free=%4zu\n",
             frame, spawned, despawned, num_active,
             pool_count_free(&pool));
    }

    /* Clean up remaining */
    for (int i = 0; i < num_active; i++)
      pool_free(&pool, active[i]);

    printf("\n  Final: all particles returned.  pool_free=%zu/%zu\n",
           pool_count_free(&pool), pool.count);

    arena_free(&arena);
  }

  /* ── 4. Benchmark ────────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "pool_alloc+pool_free vs malloc+free (Particle)") {
      /* Pool allocator */
      Arena arena = {0};
      arena.min_block_size = 64 * 1024 * 1024;
      Pool pool;
      pool_init(&pool, &arena, sizeof(Particle), (size_t)N);

      BENCH_CASE(suite, "pool_alloc+free", i, N) {
        void *p = pool_alloc(&pool);
        *(volatile char *)p = 0;
        pool_free(&pool, p);
      }
      BENCH_CASE_END(suite, N);
      arena_free(&arena);

      /* malloc+free */
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(sizeof(Particle));
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);

      /* calloc+free */
      BENCH_CASE(suite, "calloc+free", i, N) {
        void *p = calloc(1, sizeof(Particle));
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_18");
#endif

  printf("\n");
  return 0;
}
