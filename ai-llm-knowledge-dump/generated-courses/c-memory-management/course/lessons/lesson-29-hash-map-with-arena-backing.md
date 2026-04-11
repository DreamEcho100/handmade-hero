# Lesson 29 -- Hash Map with Arena Backing

> **What you'll build:** An arena-backed hash map using open addressing and linear probing, benchmarked against a malloc-backed equivalent to show the arena's advantage in allocation speed and O(1) cleanup.

## Observable outcome

The program inserts 5 key-value pairs, looks them up, updates one, deletes one (using a tombstone), and iterates all entries. It then scales to 100K entries, verifies lookups, deletes every 10th entry, and reports total arena usage. The benchmark shows arena-backed insert is faster and cleanup is O(1) vs O(N).

## New concepts

- Open addressing with linear probing for hash collision resolution
- FNV-1a hash function for string keys
- Tombstone-based deletion in open-addressed hash tables
- Arena-backed key storage: keys are `arena_push`'d, never individually freed
- Load factor and its effect on probe chain length

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_29.c` | New | Hash map demos, malloc-backed comparison, benchmarks |
| `src/lib/hash_map.h` | Dependency | Arena-backed `HashMap` API definition |
| `src/lib/hash_map.c` | Dependency | `hashmap_create`, `set`, `get`, `delete`, `iterate` implementation |
| `src/lib/arena.h` | Dependency | Arena allocator for key and entry storage |

---

## Background -- from JS Map to C hash maps

In JavaScript, `Map` handles everything for you: hashing, collision resolution, resizing, and garbage collection. In C, you build each of these pieces yourself. This lesson builds a hash map that uses an arena for all its memory, demonstrating the "arena-backed data structure" pattern.

### Open addressing vs chaining

There are two main approaches to collision resolution:

```
  Chaining (separate lists):          Open addressing (flat array):
  +-----+                             +-----+-----+-----+-----+-----+
  |  0  |-->["a",1]-->["d",4]         |"a",1|"b",2| --- |"d",4|"c",3|
  |  1  |-->["b",2]                   +-----+-----+-----+-----+-----+
  |  2  |                              slot 0  1     2     3     4
  |  3  |-->["c",3]
  +-----+
```

Our hash map uses **open addressing** with **linear probing**: the entry array is one flat allocation, and collisions are resolved by scanning forward to the next empty slot. This is more cache-friendly than chaining because entries are contiguous in memory.

### Why arena-backing matters

In a malloc-backed hash map, every string key needs its own `strdup`/`malloc` call:

```c
  // Malloc-backed: N inserts = N mallocs for keys
  entry->key = strdup(key);     // malloc + strlen + memcpy

  // Cleanup: must iterate ALL slots to free each key
  for (size_t i = 0; i < capacity; i++) {
    if (entries[i].key) free(entries[i].key);  // O(capacity)
  }
  free(entries);
```

In the arena-backed version, keys are pushed onto the arena:

```c
  // Arena-backed: N inserts = N pointer bumps (no system calls)
  entry->key = arena_push_string(arena, key);  // bump pointer + memcpy

  // Cleanup: one call frees everything
  arena_free(&arena);                          // O(1)
```

The arena win is threefold: faster insertion (pointer bump vs malloc), faster cleanup (O(1) vs O(N)), and zero chance of leaking individual keys.

### The FNV-1a hash function

The hash map uses FNV-1a, a simple and fast hash for short strings:

```c
uint64_t h = 14695981039346656037ULL;    /* FNV offset basis */
while (*key) {
  h ^= (uint64_t)(unsigned char)*key++;
  h *= 1099511628211ULL;                  /* FNV prime */
}
```

It XORs each byte with the running hash, then multiplies by a prime. This produces good distribution for typical string keys. The slot index is `h % capacity`.

### Tombstones for deletion

```
  Before delete("b"):
  +-------+-------+-------+-------+-------+
  | "a",1 | "b",2 | "c",3 | ---   | ---   |
  +-------+-------+-------+-------+-------+

  After delete("b"):
  +-------+---------+-------+-------+-------+
  | "a",1 | TOMB    | "c",3 | ---   | ---   |
  +-------+---------+-------+-------+-------+
           ^
           tombstone (not NULL!)
```

Why not set the deleted slot to NULL? Because linear probing relies on the **probe chain** being unbroken. If "c" was inserted after "b" (due to a collision), setting "b"'s slot to NULL would break the chain -- a lookup for "c" would stop at the NULL and report "not found."

A tombstone (sentinel value `(char *)1`) says "this slot is empty, but the chain continues." `get` skips tombstones. `set` can reuse them for new entries.

---

## Code walkthrough

### HashMap structure (lib/hash_map.h)

```c
typedef struct HashMapEntry {
  const char *key;      /* NULL = empty, (char*)1 = tombstone */
  uint64_t    value;
} HashMapEntry;

typedef struct HashMap {
  HashMapEntry *entries;
  size_t        capacity;
  size_t        count;     /* live entries (not counting tombstones) */
  Arena        *arena;     /* arena for key storage */
} HashMap;
```

The entry array is allocated on the arena. Keys are also stored on the arena. The `count` field tracks live entries only -- tombstones do not count toward the load factor.

### Section 1: basic usage

```c
Arena arena = {0};
arena.min_block_size = 64 * 1024;

HashMap map = hashmap_create(&arena, 32);   /* 32 slots */

hashmap_set(&map, "health", 100);
hashmap_set(&map, "mana", 50);
hashmap_set(&map, "stamina", 75);

uint64_t *val = hashmap_get(&map, "health");
// val points to 100

hashmap_set(&map, "health", 80);            /* update in place */
hashmap_delete(&map, "mana");                /* tombstone */
```

`hashmap_create` pushes the entry array onto the arena using `ARENA_PUSH_ZERO_ARRAY`. The entries start as all-zeros, which means all keys are NULL (empty). `hashmap_set` hashes the key, probes for an empty slot (NULL or tombstone), copies the key onto the arena via `arena_push_string`, and stores the value.

### Section 2: 100K entry test

```c
Arena arena = {0};
arena.min_block_size = 8 * 1024 * 1024;     /* 8 MB block */

size_t N = 100000;
size_t cap = 200003;    /* ~50% load factor */
HashMap map = hashmap_create(&arena, cap);

char key_buf[32];
for (size_t i = 0; i < N; i++) {
  snprintf(key_buf, sizeof(key_buf), "key_%06zu", i);
  hashmap_set(&map, key_buf, i * 17);
}
```

The capacity is chosen as roughly 2x the expected entry count to keep the load factor around 50%. At 50% load, average probe length is about 1.5 -- nearly O(1). At 90% load, probe length degrades badly.

After inserting 100K entries, the demo deletes every 10th entry and verifies that deleted keys return NULL while non-deleted keys remain accessible. The arena report shows total memory used: all 100K keys and the entry table live on the arena.

### Malloc-backed comparison (for benchmarking)

The lesson includes a `MallocHashMap` using the same algorithm but `strdup` for keys:

```c
static void mhm_set(MallocHashMap *m, const char *key, uint64_t value) {
  /* ... find slot via probing ... */
  e->key = strdup(key);    /* malloc + strlen + memcpy */
  e->value = value;
}

static void mhm_destroy(MallocHashMap *m) {
  for (size_t i = 0; i < m->capacity; i++) {
    if (m->entries[i].key && m->entries[i].key != (char *)1)
      free(m->entries[i].key);    /* must free each key individually */
  }
  free(m->entries);
}
```

The `mhm_destroy` must iterate all slots -- O(capacity) work even if most slots are empty. The arena version: `arena_free(&arena)` -- done.

### Benchmark results

Two benchmarks compare arena vs malloc:

1. **100K inserts**: arena wins because `arena_push_string` is a pointer bump, while `strdup` calls `malloc` + `strlen` + `memcpy`
2. **100K lookups**: similar performance because the lookup algorithm is identical -- only key storage differs

The real win is in cleanup. Arena cleanup is O(1) regardless of entry count. Malloc cleanup is O(capacity).

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Lookup returns NULL for a key that was inserted | Load factor too high (>75%), causing excessive probing | Use a larger capacity; rule of thumb is 2x expected entries |
| Deleted key still found after `hashmap_delete` | Tombstone not being skipped correctly in `get` | Tombstone slots must `continue` the probe, not `return NULL` |
| Segfault when using map after `arena_free` | The map's entries and keys live on the arena | Freeing the arena invalidates everything; use before freeing |
| Arena uses more memory than expected | No automatic resize; initial capacity may be too large | Size capacity to ~2x expected entry count; arena grows but does not shrink |

---

## Build and run

```bash
./build-dev.sh --lesson=29 -r              # demos and 100K test
./build-dev.sh --lesson=29 --bench -r      # arena vs malloc benchmarks
```

---

## Key takeaways

1. An arena-backed hash map eliminates per-key malloc/free calls. Insertion is a pointer bump. Cleanup is O(1) regardless of entry count. Keys cannot leak because the arena owns them all.
2. Open addressing with linear probing stores entries in a flat array -- good cache locality for probing. The trade-off is that deletions require tombstones to preserve probe chains.
3. Load factor determines performance. Below 50%, probing is nearly O(1). Above 75%, probe chains grow long. The hash map does not auto-resize (by design for teaching), so choose capacity carefully.
4. FNV-1a is a simple, fast hash function suitable for string keys. For production, you might want xxHash or SipHash, but FNV-1a is perfect for understanding the mechanics.
5. The "arena-backed data structure" pattern applies to any collection: hash maps, trees, graphs, linked lists. The arena provides the storage; the data structure provides the organization. Cleanup is always one call.
