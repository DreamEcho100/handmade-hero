#ifndef LIB_POOL_H
#define LIB_POOL_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/pool.h — Fixed-size pool allocator with embedded free list
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Allocates fixed-size objects from a contiguous block of arena memory.
 * Free slots store a pointer to the next free slot (union trick).
 *
 * O(1) alloc and free — just pop/push from the free list head.
 *
 * Usage:
 *   Pool pool;
 *   pool_init(&pool, &arena, sizeof(Particle), 1024);
 *   Particle *p = pool_alloc(&pool);
 *   pool_free(&pool, p);
 *
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>
#include "lib/arena.h"

/* Each free slot stores a pointer to the next free slot. */
typedef union PoolFreeNode {
  union PoolFreeNode *next;
} PoolFreeNode;

typedef struct Pool {
  PoolFreeNode *free_head;   /* head of the embedded free list           */
  void         *memory;      /* start of the pool slab                   */
  size_t        obj_size;    /* size of each object (>= sizeof(void*))   */
  size_t        count;       /* total number of objects in the pool      */
  size_t        used;        /* number of currently allocated objects     */
} Pool;

void   pool_init(Pool *pool, Arena *backing, size_t obj_size, size_t count);
void  *pool_alloc(Pool *pool);
void   pool_free(Pool *pool, void *ptr);
size_t pool_count_free(const Pool *pool);

#endif /* LIB_POOL_H */
