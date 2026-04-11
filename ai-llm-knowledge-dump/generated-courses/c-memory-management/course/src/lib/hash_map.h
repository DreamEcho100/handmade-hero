#ifndef LIB_HASH_MAP_H
#define LIB_HASH_MAP_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/hash_map.h — Arena-backed hash map (open addressing, linear probing)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * All storage lives on an Arena.  No individual frees needed.
 * Keys are C strings (copied onto the arena).  Values are uint64_t.
 *
 * Lesson 29: demonstrates arena-backed data structures.
 *
 * Limitations (by design, for teaching):
 *   - No automatic resize (caller picks capacity upfront)
 *   - Delete uses tombstones (doesn't shrink)
 *   - Keys are NUL-terminated C strings
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>
#include <stdint.h>
#include "lib/arena.h"

typedef struct HashMapEntry {
  const char *key;     /* NULL = empty, TOMBSTONE = deleted */
  uint64_t    value;
} HashMapEntry;

typedef struct HashMap {
  HashMapEntry *entries;
  size_t        capacity;
  size_t        count;    /* live entries (not counting tombstones) */
  Arena        *arena;    /* arena for key storage */
} HashMap;

/* Create a hash map with the given capacity.  All memory on the arena. */
HashMap hashmap_create(Arena *arena, size_t capacity);

/* Insert or update a key-value pair.  Returns 1 on success, 0 if full. */
int hashmap_set(HashMap *map, const char *key, uint64_t value);

/* Lookup a key.  Returns pointer to value, or NULL if not found. */
uint64_t *hashmap_get(const HashMap *map, const char *key);

/* Delete a key (tombstone).  Returns 1 if found and deleted, 0 if not found. */
int hashmap_delete(HashMap *map, const char *key);

/* Number of live entries. */
size_t hashmap_count(const HashMap *map);

/* Iterate all live entries.  callback returns 0 to stop, nonzero to continue. */
void hashmap_iterate(const HashMap *map,
                     int (*callback)(const char *key, uint64_t value, void *ctx),
                     void *ctx);

#endif /* LIB_HASH_MAP_H */
