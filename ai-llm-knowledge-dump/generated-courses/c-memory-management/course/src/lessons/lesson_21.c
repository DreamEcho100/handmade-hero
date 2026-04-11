/* lesson_21.c — Slab Allocator
 * BUILD_DEPS: common/bench.c, lib/slab.c
 *
 * Demonstrates a slab allocator: pre-allocated pages of fixed-size objects
 * with per-slab free lists.  Like a pool that grows automatically.
 * Allocate/free many fixed-size objects in mixed patterns.
 *
 * Run:   ./build-dev.sh --lesson=21 -r
 * Bench: ./build-dev.sh --lesson=21 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/slab.h"

/* ── Example types ─────────────────────────────────────────────────────────*/
typedef struct Entity {
  int   id;
  float x, y, z;
  int   health;
  int   active;
} Entity;

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 21 — Slab Allocator\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic slab usage ─────────────────────────────────────────────*/
  print_section("Basic Slab Usage");
  {
    Slab slab;
    slab_init(&slab, sizeof(Entity), 4);  /* 4 objects per page (tiny for demo) */

    printf("  Slab: obj_size=%zu  objs_per_slab=%zu\n",
           slab.obj_size, slab.objs_per_slab);

    Entity *e1 = (Entity *)slab_alloc(&slab);
    e1->id = 1; e1->x = 10.0f; e1->health = 100;

    Entity *e2 = (Entity *)slab_alloc(&slab);
    e2->id = 2; e2->x = 20.0f; e2->health = 80;

    printf("  Allocated e1 (id=%d) at %p\n", e1->id, (void *)e1);
    printf("  Allocated e2 (id=%d) at %p\n", e2->id, (void *)e2);
    printf("  Outstanding: %zu\n", slab.total_allocs);

    slab_free(&slab, e1);
    printf("\n  Freed e1.  Outstanding: %zu\n", slab.total_allocs);

    Entity *e3 = (Entity *)slab_alloc(&slab);
    e3->id = 3; e3->health = 50;
    printf("  Allocated e3 at %p (reused e1's slot? %s)\n",
           (void *)e3, (e3 == e1) ? "YES" : "no");

    slab_destroy(&slab);
  }

  /* ── 2. Automatic page growth ────────────────────────────────────────*/
  print_section("Automatic Page Growth");
  {
    Slab slab;
    slab_init(&slab, sizeof(Entity), 8);

    printf("  Slab with 8 objects per page.  Allocating 25 objects:\n\n");

    Entity *entities[25];
    int page_count = 0;
    SlabPage *last_pages = NULL;

    for (int i = 0; i < 25; i++) {
      entities[i] = (Entity *)slab_alloc(&slab);
      entities[i]->id = i;

      /* Detect new page */
      if (slab.pages != last_pages) {
        page_count++;
        last_pages = slab.pages;
        printf("    alloc[%2d]: NEW PAGE #%d created\n", i, page_count);
      }
    }

    printf("\n  25 objects across %d pages (ceil(25/8) = 4 expected)\n",
           page_count);
    printf("  Outstanding: %zu\n", slab.total_allocs);

    /* Count pages */
    int counted = 0;
    SlabPage *p = slab.pages;
    while (p) { counted++; p = p->next; }
    printf("  Pages linked: %d\n", counted);

    slab_destroy(&slab);
  }

  /* ── 3. Mixed alloc/free pattern ─────────────────────────────────────*/
  print_section("Mixed Alloc/Free Pattern (Entity Simulation)");
  {
    Slab slab;
    slab_init(&slab, sizeof(Entity), 64);

    #define MAX_ENTITIES 500
    Entity *active[MAX_ENTITIES];
    int num_active = 0;

    unsigned rng = 12345;

    printf("  Simulating 20 frames of entity spawn/despawn:\n\n");
    for (int frame = 0; frame < 20; frame++) {
      /* Spawn 10-30 entities */
      int to_spawn = 10 + (int)(rng % 20);
      rng = rng * 1103515245 + 12345;
      int spawned = 0;
      for (int i = 0; i < to_spawn && num_active < MAX_ENTITIES; i++) {
        Entity *e = (Entity *)slab_alloc(&slab);
        if (!e) break;
        e->id = frame * 1000 + i;
        e->health = 100;
        e->active = 1;
        active[num_active++] = e;
        spawned++;
      }

      /* Despawn entities with probability ~30% */
      int despawned = 0;
      for (int i = num_active - 1; i >= 0; i--) {
        rng = rng * 1103515245 + 12345;
        if ((rng % 100) < 30) {
          slab_free(&slab, active[i]);
          active[i] = active[--num_active];
          despawned++;
        }
      }

      if (frame % 5 == 0 || frame == 19) {
        printf("    Frame %2d: +%2d spawned  -%2d despawned  "
               "active=%3d  slab_total=%3zu\n",
               frame, spawned, despawned, num_active, slab.total_allocs);
      }
    }

    /* Clean up */
    for (int i = 0; i < num_active; i++)
      slab_free(&slab, active[i]);

    printf("\n  All entities freed.  Outstanding: %zu\n", slab.total_allocs);
    slab_destroy(&slab);
  }

  /* ── 4. Slab vs Pool vs malloc ───────────────────────────────────────*/
  print_section("Slab vs Pool vs malloc");
  printf("  ┌───────────────────┬───────────────┬───────────────┬───────────────┐\n");
  printf("  │ Feature           │ Slab          │ Pool          │ malloc        │\n");
  printf("  ├───────────────────┼───────────────┼───────────────┼───────────────┤\n");
  printf("  │ Object size       │ Fixed         │ Fixed         │ Any           │\n");
  printf("  │ Grows             │ Yes (pages)   │ No (fixed)    │ Yes           │\n");
  printf("  │ Alloc speed       │ O(1)*         │ O(1)          │ O(1) amortiz. │\n");
  printf("  │ Free speed        │ O(1)          │ O(1)          │ O(1) amortiz. │\n");
  printf("  │ Fragmentation     │ None internal │ None          │ Yes           │\n");
  printf("  │ Overhead/object   │ 0 bytes       │ 0 bytes       │ 16-32 bytes   │\n");
  printf("  └───────────────────┴───────────────┴───────────────┴───────────────┘\n");
  printf("  * O(n) when searching for a page with free space (rare path).\n");

  /* ── 5. Benchmark ────────────────────────────────────────────────────*/
#ifdef BENCH_MODE
  {
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "slab_alloc+free vs malloc+free (Entity)") {
      /* Slab allocator */
      Slab slab;
      slab_init(&slab, sizeof(Entity), 4096);
      BENCH_CASE(suite, "slab alloc+free", i, N) {
        void *p = slab_alloc(&slab);
        *(volatile char *)p = 0;
        slab_free(&slab, p);
      }
      BENCH_CASE_END(suite, N);
      slab_destroy(&slab);

      /* malloc+free */
      BENCH_CASE(suite, "malloc+free", i, N) {
        void *p = malloc(sizeof(Entity));
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);

      /* calloc+free */
      BENCH_CASE(suite, "calloc+free", i, N) {
        void *p = calloc(1, sizeof(Entity));
        *(volatile char *)p = 0;
        free(p);
      }
      BENCH_CASE_END(suite, N);
    }
  }
#else
  bench_skip_notice("lesson_21");
#endif

  printf("\n");
  return 0;
}
