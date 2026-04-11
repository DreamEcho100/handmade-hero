#ifndef LIB_SLAB_H
#define LIB_SLAB_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/slab.h — Slab allocator (multi-page pool for fixed-size objects)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Like a pool allocator but with automatic growth: when the current slab
 * page fills up, a new one is allocated (via malloc).  Each slab page has
 * its own embedded free list.
 *
 * Great for allocating many fixed-size objects with unpredictable lifetimes,
 * e.g., scene nodes, hash map entries, connection handles.
 *
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>

/* Each free slot stores a pointer to the next free slot. */
typedef union SlabFreeNode {
  union SlabFreeNode *next;
} SlabFreeNode;

/* A single slab page — one contiguous allocation of objs_per_slab objects. */
typedef struct SlabPage {
  struct SlabPage *next;       /* linked list of all pages                  */
  SlabFreeNode    *free_head;  /* per-page free list                        */
  size_t           used;       /* number of allocated objects in this page  */
} SlabPage;

typedef struct Slab {
  SlabPage *pages;          /* linked list of all slab pages              */
  size_t    obj_size;       /* size of each object (aligned)              */
  size_t    objs_per_slab;  /* number of objects per slab page            */
  size_t    total_allocs;   /* total outstanding allocations              */
} Slab;

void  slab_init(Slab *slab, size_t obj_size, size_t objs_per_slab);
void *slab_alloc(Slab *slab);
void  slab_free(Slab *slab, void *ptr);
void  slab_destroy(Slab *slab);

#endif /* LIB_SLAB_H */
