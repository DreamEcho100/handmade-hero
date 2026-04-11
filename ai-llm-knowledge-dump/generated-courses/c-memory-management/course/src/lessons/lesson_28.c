/* lesson_28.c — Batch Allocation and Memory Pools
 * BUILD_DEPS: common/bench.c
 *
 * Benchmarks three approaches for entity spawn/despawn in a hot loop:
 *   1. Per-object malloc/free
 *   2. Batch malloc (allocate N at once, use indices)
 *   3. Pool allocator (free-list based, constant-time alloc/free)
 *
 * Run:   ./build-dev.sh --lesson=28 -r
 * Bench: ./build-dev.sh --lesson=28 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Entity — a simple game entity for benchmarking
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct Entity {
  float x, y, vx, vy;
  int   hp;
  int   active;
  int   type;
  int   _pad;  /* pad to 32 bytes for clean alignment */
} Entity;

/* ═══════════════════════════════════════════════════════════════════════════
 * Pool Allocator — inline, fixed-size block pool
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Free list threaded through the blocks themselves:
 *   - Each free block's first sizeof(void*) bytes store a pointer to
 *     the next free block.
 *   - Allocation: pop from free list (O(1))
 *   - Free: push onto free list (O(1))
 *   - No fragmentation for same-sized objects.
 */

typedef struct Pool {
  void   *memory;        /* backing allocation */
  void   *free_list;     /* head of free list */
  size_t  block_size;    /* size of each block (>= sizeof(void*)) */
  size_t  capacity;      /* total blocks */
  size_t  active;        /* currently allocated blocks */
} Pool;

static int pool_init(Pool *pool, size_t block_size, size_t capacity) {
  if (block_size < sizeof(void *))
    block_size = sizeof(void *);

  pool->memory     = malloc(block_size * capacity);
  if (!pool->memory) return 0;

  pool->block_size = block_size;
  pool->capacity   = capacity;
  pool->active     = 0;

  /* Thread the free list through all blocks */
  pool->free_list = pool->memory;
  uint8_t *p = (uint8_t *)pool->memory;
  for (size_t i = 0; i < capacity - 1; i++) {
    void *next = p + block_size;
    memcpy(p, &next, sizeof(void *));
    p += block_size;
  }
  /* Last block points to NULL */
  void *null_ptr = NULL;
  memcpy(p, &null_ptr, sizeof(void *));

  return 1;
}

static void *pool_alloc(Pool *pool) {
  if (!pool->free_list) return NULL;  /* pool exhausted */

  void *block = pool->free_list;
  /* Pop: free_list = block->next */
  memcpy(&pool->free_list, block, sizeof(void *));
  pool->active++;
  return block;
}

static void pool_free(Pool *pool, void *block) {
  if (!block) return;
  /* Push: block->next = free_list; free_list = block */
  memcpy(block, &pool->free_list, sizeof(void *));
  pool->free_list = block;
  pool->active--;
}

static void pool_destroy(Pool *pool) {
  free(pool->memory);
  pool->memory = NULL;
  pool->free_list = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 28 — Batch Allocation and Memory Pools\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. The problem ──────────────────────────────────────────────────── */
  print_section("The Problem: Entity Spawn/Despawn");
  {
    printf("  In a game loop, entities are constantly spawned and despawned.\n");
    printf("  Each entity might live for only a few frames (bullets, particles).\n\n");
    printf("  Three allocation strategies:\n\n");
    printf("  1. Per-object malloc/free:\n");
    printf("     + Simple\n");
    printf("     - Each call searches free lists, may call sbrk/mmap\n");
    printf("     - Memory fragmentation over time\n\n");
    printf("  2. Batch malloc (pre-allocate N slots):\n");
    printf("     + One allocation for many entities\n");
    printf("     + Good cache locality (contiguous)\n");
    printf("     - Must manage slot reuse yourself\n\n");
    printf("  3. Pool allocator (free-list of fixed-size blocks):\n");
    printf("     + O(1) alloc and free\n");
    printf("     + Zero fragmentation\n");
    printf("     + Excellent cache locality\n");
    printf("     - Fixed block size (one pool per entity type)\n");
  }

  /* ── 2. Pool allocator demo ──────────────────────────────────────────── */
  print_section("Pool Allocator Demo");
  {
    Pool pool;
    pool_init(&pool, sizeof(Entity), 8);

    printf("  Pool: %zu slots of %zu bytes each\n\n", pool.capacity, pool.block_size);

    /* Allocate some entities */
    Entity *e1 = pool_alloc(&pool);
    Entity *e2 = pool_alloc(&pool);
    Entity *e3 = pool_alloc(&pool);
    if (e1) { e1->x = 1.0f; e1->hp = 100; e1->active = 1; }
    if (e2) { e2->x = 2.0f; e2->hp = 50;  e2->active = 1; }
    if (e3) { e3->x = 3.0f; e3->hp = 75;  e3->active = 1; }

    printf("  Allocated 3 entities: active=%zu\n", pool.active);
    printf("    e1: x=%.1f hp=%d\n", e1->x, e1->hp);
    printf("    e2: x=%.1f hp=%d\n", e2->x, e2->hp);
    printf("    e3: x=%.1f hp=%d\n", e3->x, e3->hp);

    /* Free e2 and reuse the slot */
    pool_free(&pool, e2);
    printf("\n  Freed e2: active=%zu\n", pool.active);

    Entity *e4 = pool_alloc(&pool);
    if (e4) { e4->x = 4.0f; e4->hp = 200; e4->active = 1; }
    printf("  Allocated e4 (reuses e2's slot): x=%.1f hp=%d\n", e4->x, e4->hp);
    printf("  e4 == e2? %s\n", (e4 == e2) ? "yes (slot reused!)" : "no");

    pool_free(&pool, e1);
    pool_free(&pool, e3);
    pool_free(&pool, e4);
    pool_destroy(&pool);
  }

  /* ── 3. Benchmarks ──────────────────────────────────────────────────── */
#ifdef BENCH_MODE
  {
    #define BENCH_N 100000
    long spawn_despawn_cycles = 50;

    /* ── 3a. Per-object malloc/free ── */
    BenchSuite suite;
    BENCH_SUITE(suite, "Entity spawn/despawn patterns") {

      /* Per-object malloc/free */
      BENCH_CASE(suite, "malloc/free each", cycle, spawn_despawn_cycles) {
        Entity *entities[BENCH_N];
        for (int i = 0; i < BENCH_N; i++) {
          entities[i] = malloc(sizeof(Entity));
          entities[i]->x = (float)i;
          entities[i]->active = 1;
        }
        /* Despawn: free every other one */
        for (int i = 0; i < BENCH_N; i += 2) {
          free(entities[i]);
          entities[i] = NULL;
        }
        /* Respawn into freed slots */
        for (int i = 0; i < BENCH_N; i += 2) {
          entities[i] = malloc(sizeof(Entity));
          entities[i]->x = (float)(i + 1000);
          entities[i]->active = 1;
        }
        /* Cleanup */
        for (int i = 0; i < BENCH_N; i++) {
          free(entities[i]);
        }
      }
      BENCH_CASE_END(suite, spawn_despawn_cycles);

      /* Batch: single malloc, index-based reuse */
      BENCH_CASE(suite, "batch (index reuse)", cycle, spawn_despawn_cycles) {
        Entity *batch = malloc(BENCH_N * sizeof(Entity));
        int *free_stack = malloc(BENCH_N * sizeof(int));
        int free_top = 0;

        /* Spawn all */
        for (int i = 0; i < BENCH_N; i++) {
          batch[i].x = (float)i;
          batch[i].active = 1;
        }
        /* Despawn every other */
        for (int i = 0; i < BENCH_N; i += 2) {
          batch[i].active = 0;
          free_stack[free_top++] = i;
        }
        /* Respawn */
        for (int i = 0; i < free_top; i++) {
          int idx = free_stack[--free_top];
          batch[idx].x = (float)(idx + 1000);
          batch[idx].active = 1;
          i--;  /* compensate since free_top changed */
        }
        free(batch);
        free(free_stack);
      }
      BENCH_CASE_END(suite, spawn_despawn_cycles);

      /* Pool allocator */
      BENCH_CASE(suite, "pool allocator", cycle, spawn_despawn_cycles) {
        Pool pool;
        pool_init(&pool, sizeof(Entity), BENCH_N);

        Entity *entities[BENCH_N];
        /* Spawn */
        for (int i = 0; i < BENCH_N; i++) {
          entities[i] = pool_alloc(&pool);
          entities[i]->x = (float)i;
          entities[i]->active = 1;
        }
        /* Despawn every other */
        for (int i = 0; i < BENCH_N; i += 2) {
          pool_free(&pool, entities[i]);
          entities[i] = NULL;
        }
        /* Respawn */
        for (int i = 0; i < BENCH_N; i += 2) {
          entities[i] = pool_alloc(&pool);
          entities[i]->x = (float)(i + 1000);
          entities[i]->active = 1;
        }

        pool_destroy(&pool);
      }
      BENCH_CASE_END(suite, spawn_despawn_cycles);
    }

    printf("  Pool allocator should be fastest: O(1) alloc/free,\n");
    printf("  no system calls, excellent cache locality.\n");
    printf("  Batch is middle ground: one malloc, but manual index tracking.\n\n");
  }
#else
  bench_skip_notice("lesson_28");
#endif

  printf("\n");
  return 0;
}
