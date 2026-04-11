/* lib/hash_map.c — Arena-backed hash map implementation */

#include "lib/hash_map.h"
#include <assert.h>
#include <string.h>

/* ── Sentinel for deleted entries ─────────────────────────────────────────*/
static const char TOMBSTONE_SENTINEL = '\xff';
#define TOMBSTONE ((const char *)&TOMBSTONE_SENTINEL)

/* ── FNV-1a hash ──────────────────────────────────────────────────────────
 *
 * Simple, fast, good distribution.  Used in many hash map implementations.
 */
static uint64_t fnv1a(const char *key) {
  uint64_t hash = 14695981039346656037ULL;  /* FNV offset basis */
  while (*key) {
    hash ^= (uint64_t)(unsigned char)*key++;
    hash *= 1099511628211ULL;               /* FNV prime */
  }
  return hash;
}

/* ── Push a copy of a C string onto the arena ────────────────────────────*/
static const char *push_key(Arena *arena, const char *key) {
  size_t len = strlen(key);
  char *buf = (char *)arena_push_size(arena, len + 1, 1);
  if (!buf) return NULL;
  memcpy(buf, key, len + 1);
  return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════
 */

HashMap hashmap_create(Arena *arena, size_t capacity) {
  HashMap map = {0};
  assert(capacity > 0 && "hashmap_create: capacity must be > 0");
  if (capacity == 0) return map; /* prevents division by zero in set/get */
  map.arena    = arena;
  map.capacity = capacity;
  map.count    = 0;
  map.entries  = ARENA_PUSH_ZERO_ARRAY(arena, capacity, HashMapEntry);
  return map;
}

int hashmap_set(HashMap *map, const char *key, uint64_t value) {
  if (!key) return 0;

  /* Refuse if load factor > 70% (open addressing needs headroom) */
  if (map->count * 10 >= map->capacity * 7)
    return 0;

  uint64_t hash = fnv1a(key);
  size_t idx = (size_t)(hash % map->capacity);

  /* Linear probe */
  for (size_t i = 0; i < map->capacity; i++) {
    size_t slot = (idx + i) % map->capacity;
    HashMapEntry *e = &map->entries[slot];

    if (e->key == NULL || e->key == TOMBSTONE) {
      /* Empty or tombstone: insert here */
      const char *keycopy = push_key(map->arena, key);
      if (!keycopy) return 0; /* OOM */
      e->key   = keycopy;
      e->value = value;
      map->count++;
      return 1;
    }

    if (strcmp(e->key, key) == 0) {
      /* Key already exists: update value */
      e->value = value;
      return 1;
    }
  }

  return 0;  /* table full (shouldn't happen with load factor check) */
}

uint64_t *hashmap_get(const HashMap *map, const char *key) {
  if (!key) return NULL;

  uint64_t hash = fnv1a(key);
  size_t idx = (size_t)(hash % map->capacity);

  for (size_t i = 0; i < map->capacity; i++) {
    size_t slot = (idx + i) % map->capacity;
    HashMapEntry *e = &map->entries[slot];

    if (e->key == NULL) return NULL;  /* empty: not found */
    if (e->key == TOMBSTONE) continue; /* skip tombstones */
    if (strcmp(e->key, key) == 0) return &e->value;
  }

  return NULL;
}

int hashmap_delete(HashMap *map, const char *key) {
  if (!key) return 0;

  uint64_t hash = fnv1a(key);
  size_t idx = (size_t)(hash % map->capacity);

  for (size_t i = 0; i < map->capacity; i++) {
    size_t slot = (idx + i) % map->capacity;
    HashMapEntry *e = &map->entries[slot];

    if (e->key == NULL) return 0;
    if (e->key == TOMBSTONE) continue;
    if (strcmp(e->key, key) == 0) {
      e->key = TOMBSTONE;
      e->value = 0;
      map->count--;
      return 1;
    }
  }

  return 0;
}

size_t hashmap_count(const HashMap *map) {
  return map->count;
}

void hashmap_iterate(const HashMap *map,
                     int (*callback)(const char *key, uint64_t value, void *ctx),
                     void *ctx) {
  for (size_t i = 0; i < map->capacity; i++) {
    const HashMapEntry *e = &map->entries[i];
    if (e->key != NULL && e->key != TOMBSTONE) {
      if (!callback(e->key, e->value, ctx)) return;
    }
  }
}
