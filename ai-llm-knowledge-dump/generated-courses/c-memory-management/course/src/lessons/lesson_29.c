#define _POSIX_C_SOURCE 200809L
/* lesson_29.c — Hash Map with Arena Backing
 * BUILD_DEPS: common/bench.c, lib/hash_map.c
 *
 * Demonstrates the arena-backed hash map:
 *   - Insert 100K entries, lookup, iterate, delete some
 *   - Benchmark arena-backed vs simple malloc-backed version
 *
 * Run:   ./build-dev.sh --lesson=29 -r
 * Bench: ./build-dev.sh --lesson=29 --bench -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"
#include "lib/hash_map.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Simple malloc-backed hash map for comparison
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Same open-addressing algorithm, but each key is individually malloc'd
 * and must be individually freed.
 */

typedef struct MallocHashEntry {
  char     *key;    /* NULL = empty, (char*)1 = tombstone */
  uint64_t  value;
} MallocHashEntry;

typedef struct MallocHashMap {
  MallocHashEntry *entries;
  size_t           capacity;
  size_t           count;
} MallocHashMap;

static uint64_t mhm_hash(const char *key) {
  uint64_t h = 14695981039346656037ULL;
  while (*key) { h ^= (uint64_t)(unsigned char)*key++; h *= 1099511628211ULL; }
  return h;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static MallocHashMap mhm_create(size_t capacity) {
  MallocHashMap m;
  m.capacity = capacity;
  m.count    = 0;
  m.entries  = calloc(capacity, sizeof(MallocHashEntry));
  return m;
}

static void mhm_set(MallocHashMap *m, const char *key, uint64_t value) {
  size_t idx = (size_t)(mhm_hash(key) % m->capacity);
  for (size_t i = 0; i < m->capacity; i++) {
    size_t slot = (idx + i) % m->capacity;
    MallocHashEntry *e = &m->entries[slot];
    if (e->key == NULL || e->key == (char *)1) {
      e->key = strdup(key);
      e->value = value;
      m->count++;
      return;
    }
    if (strcmp(e->key, key) == 0) {
      e->value = value;
      return;
    }
  }
}

static uint64_t *mhm_get(const MallocHashMap *m, const char *key) {
  size_t idx = (size_t)(mhm_hash(key) % m->capacity);
  for (size_t i = 0; i < m->capacity; i++) {
    size_t slot = (idx + i) % m->capacity;
    MallocHashEntry *e = &m->entries[slot];
    if (e->key == NULL) return NULL;
    if (e->key == (char *)1) continue;
    if (strcmp(e->key, key) == 0) return &e->value;
  }
  return NULL;
}

static void mhm_destroy(MallocHashMap *m) {
  for (size_t i = 0; i < m->capacity; i++) {
    if (m->entries[i].key && m->entries[i].key != (char *)1)
      free(m->entries[i].key);
  }
  free(m->entries);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Iteration callback for demo
 * ═══════════════════════════════════════════════════════════════════════════
 */
typedef struct IterCtx {
  int printed;
  int max_print;
} IterCtx;

static int print_entry(const char *key, uint64_t value, void *ctx) {
  IterCtx *ic = (IterCtx *)ctx;
  if (ic->printed < ic->max_print) {
    printf("    %-20s => %lu\n", key, (unsigned long)value);
    ic->printed++;
  }
  return 1;
}
#pragma clang diagnostic pop

int main(void) {
  printf("═══════════════════════════════════════════════════════════════\n");
  printf("  Lesson 29 — Hash Map with Arena Backing\n");
  printf("═══════════════════════════════════════════════════════════════\n");

  /* ── 1. Basic usage demo ─────────────────────────────────────────────── */
  print_section("Basic Hash Map Usage");
  {
    Arena arena = {0};
    arena.min_block_size = 64 * 1024;

    HashMap map = hashmap_create(&arena, 32);

    hashmap_set(&map, "health", 100);
    hashmap_set(&map, "mana", 50);
    hashmap_set(&map, "stamina", 75);
    hashmap_set(&map, "gold", 1234);
    hashmap_set(&map, "level", 7);

    printf("  Inserted 5 key-value pairs.\n");
    printf("  Count: %zu / %zu capacity\n\n", hashmap_count(&map), (size_t)32);

    /* Lookup */
    uint64_t *val = hashmap_get(&map, "health");
    printf("  get(\"health\")  = %lu\n", val ? (unsigned long)*val : 0);
    val = hashmap_get(&map, "gold");
    printf("  get(\"gold\")    = %lu\n", val ? (unsigned long)*val : 0);
    val = hashmap_get(&map, "missing");
    printf("  get(\"missing\") = %s\n\n", val ? "found" : "NULL (not found)");

    /* Update */
    hashmap_set(&map, "health", 80);
    val = hashmap_get(&map, "health");
    printf("  After set(\"health\", 80): %lu\n\n", val ? (unsigned long)*val : 0);

    /* Delete */
    int deleted = hashmap_delete(&map, "mana");
    printf("  delete(\"mana\"): %s\n", deleted ? "deleted" : "not found");
    val = hashmap_get(&map, "mana");
    printf("  get(\"mana\") after delete: %s\n\n", val ? "found" : "NULL (gone)");

    /* Iterate */
    printf("  All entries:\n");
    IterCtx ctx = { 0, 10 };
    hashmap_iterate(&map, print_entry, &ctx);

    printf("\n  Arena used: %zu bytes for entire hash map\n",
           arena_total_used(&arena));
    printf("  No individual frees needed — one arena_free() does it.\n");

    arena_free(&arena);
  }

  /* ── 2. Large-scale test ─────────────────────────────────────────────── */
  print_section("100K Entry Test");
  {
    Arena arena = {0};
    arena.min_block_size = 8 * 1024 * 1024;  /* 8 MB */

    /* Capacity with ~60% load factor */
    size_t N = 100000;
    size_t cap = 200003;  /* prime-ish for better distribution */
    HashMap map = hashmap_create(&arena, cap);

    /* Insert 100K entries */
    char key_buf[32];
    for (size_t i = 0; i < N; i++) {
      snprintf(key_buf, sizeof(key_buf), "key_%06zu", i);
      hashmap_set(&map, key_buf, i * 17);
    }
    printf("  Inserted %zu entries.\n", hashmap_count(&map));

    /* Verify some lookups */
    int misses = 0;
    for (size_t i = 0; i < N; i += 1000) {
      snprintf(key_buf, sizeof(key_buf), "key_%06zu", i);
      uint64_t *v = hashmap_get(&map, key_buf);
      if (!v || *v != i * 17) misses++;
    }
    printf("  Spot-check 100 lookups: %d mismatches\n", misses);

    /* Delete 10K entries */
    for (size_t i = 0; i < N; i += 10) {
      snprintf(key_buf, sizeof(key_buf), "key_%06zu", i);
      hashmap_delete(&map, key_buf);
    }
    printf("  Deleted every 10th entry: count now %zu\n", hashmap_count(&map));

    /* Verify deletes worked */
    snprintf(key_buf, sizeof(key_buf), "key_%06d", 0);
    uint64_t *v = hashmap_get(&map, key_buf);
    printf("  get(\"key_000000\") after delete: %s\n", v ? "found (BUG)" : "NULL (correct)");

    snprintf(key_buf, sizeof(key_buf), "key_%06d", 1);
    v = hashmap_get(&map, key_buf);
    printf("  get(\"key_000001\") (not deleted): %lu\n",
           v ? (unsigned long)*v : 0);

    printf("\n  Arena total used: %zu bytes (%.1f MB)\n",
           arena_total_used(&arena),
           (double)arena_total_used(&arena) / (1024.0 * 1024.0));
    printf("  One arena_free() frees all 100K entries + their keys.\n");

    arena_free(&arena);
  }

  /* ── 3. Benchmarks ──────────────────────────────────────────────────── */
#ifdef BENCH_MODE
  {
    size_t N = 100000;
    size_t cap = 200003;
    long iters = 5;
    char key_buf[32];

    /* Pre-generate keys to avoid snprintf overhead in hot loop */
    char (*keys)[32] = malloc(N * 32);
    if (!keys) { fprintf(stderr, "OOM\n"); return 1; }
    for (size_t i = 0; i < N; i++)
      snprintf(keys[i], 32, "key_%06zu", i);

    /* ── Insert benchmark ── */
    BenchSuite suite1;
    BENCH_SUITE(suite1, "100K inserts: arena-backed vs malloc-backed") {
      BENCH_CASE(suite1, "arena hashmap", iter, iters) {
        Arena arena = {0};
        arena.min_block_size = 8 * 1024 * 1024;
        HashMap map = hashmap_create(&arena, cap);
        for (size_t i = 0; i < N; i++)
          hashmap_set(&map, keys[i], i);
        arena_free(&arena);
      }
      BENCH_CASE_END(suite1, iters);

      BENCH_CASE(suite1, "malloc hashmap", iter, iters) {
        MallocHashMap map = mhm_create(cap);
        for (size_t i = 0; i < N; i++)
          mhm_set(&map, keys[i], i);
        mhm_destroy(&map);
      }
      BENCH_CASE_END(suite1, iters);
    }

    /* ── Lookup benchmark ── */
    BenchSuite suite2;
    Arena arena = {0};
    arena.min_block_size = 8 * 1024 * 1024;
    HashMap amap = hashmap_create(&arena, cap);
    MallocHashMap mmap = mhm_create(cap);
    for (size_t i = 0; i < N; i++) {
      hashmap_set(&amap, keys[i], i);
      mhm_set(&mmap, keys[i], i);
    }

    BENCH_SUITE(suite2, "100K lookups: arena-backed vs malloc-backed") {
      BENCH_CASE(suite2, "arena hashmap", iter, iters) {
        for (size_t i = 0; i < N; i++) {
          volatile uint64_t *v = hashmap_get(&amap, keys[i]);
          (void)v;
        }
      }
      BENCH_CASE_END(suite2, iters);

      BENCH_CASE(suite2, "malloc hashmap", iter, iters) {
        for (size_t i = 0; i < N; i++) {
          volatile uint64_t *v = mhm_get(&mmap, keys[i]);
          (void)v;
        }
      }
      BENCH_CASE_END(suite2, iters);
    }

    arena_free(&arena);
    mhm_destroy(&mmap);
    free(keys);

    printf("  Arena-backed wins on insert (bulk alloc, no per-key malloc).\n");
    printf("  Lookup should be similar (same algorithm, just key storage differs).\n");
    printf("  The real win: cleanup is O(1) with arena vs O(N) with malloc.\n\n");

    (void)key_buf;
  }
#else
  bench_skip_notice("lesson_29");
#endif

  printf("\n");
  return 0;
}
