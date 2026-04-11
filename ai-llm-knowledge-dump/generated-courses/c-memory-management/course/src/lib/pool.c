/* ═══════════════════════════════════════════════════════════════════════════
 * lib/pool.c — Fixed-size pool allocator implementation
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "lib/pool.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

void pool_init(Pool *pool, Arena *backing, size_t obj_size, size_t count) {
  /* Each slot must be large enough to hold a PoolFreeNode pointer */
  if (obj_size < sizeof(PoolFreeNode))
    obj_size = sizeof(PoolFreeNode);

  /* Align object size to pointer alignment */
  size_t align = _Alignof(max_align_t);
  obj_size = (obj_size + align - 1) & ~(align - 1);

  /* Overflow check: obj_size * count must not wrap */
  assert(count == 0 || obj_size <= SIZE_MAX / count);

  pool->obj_size  = obj_size;
  pool->count     = count;
  pool->used      = 0;
  pool->memory    = arena_push_size(backing, obj_size * count, align);
  pool->free_head = NULL;

  if (!pool->memory) {
    assert(0 && "pool_init: arena allocation failed");
    return;
  }

  memset(pool->memory, 0, obj_size * count);

  /* Thread all slots onto the free list */
  uint8_t *base = (uint8_t *)pool->memory;
  for (size_t i = 0; i < count; i++) {
    PoolFreeNode *node = (PoolFreeNode *)(base + i * obj_size);
    node->next = pool->free_head;
    pool->free_head = node;
  }
}

void *pool_alloc(Pool *pool) {
  if (!pool->free_head) return NULL;  /* pool exhausted */

  PoolFreeNode *node = pool->free_head;
  pool->free_head = node->next;
  pool->used++;

  /* Zero the slot before returning */
  memset(node, 0, pool->obj_size);
  return (void *)node;
}

void pool_free(Pool *pool, void *ptr) {
  if (!ptr) return;

  /* Debug: verify ptr is within pool bounds */
  assert((uint8_t *)ptr >= (uint8_t *)pool->memory);
  assert((uint8_t *)ptr < (uint8_t *)pool->memory + pool->obj_size * pool->count);
  assert(((uint8_t *)ptr - (uint8_t *)pool->memory) % pool->obj_size == 0);

  PoolFreeNode *node = (PoolFreeNode *)ptr;
  node->next = pool->free_head;
  pool->free_head = node;
  pool->used--;
}

size_t pool_count_free(const Pool *pool) {
  return pool->count - pool->used;
}
