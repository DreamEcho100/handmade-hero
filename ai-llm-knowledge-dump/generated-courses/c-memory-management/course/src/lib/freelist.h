#ifndef LIB_FREELIST_H
#define LIB_FREELIST_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/freelist.h — Free list overlay on an arena for variable-size dealloc
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The arena gives us fast bump-pointer allocation, but no individual frees.
 * This free list sits on top: freed blocks go onto a linked list.  On alloc,
 * we first-fit search the free list; if nothing fits, we bump the arena.
 *
 * Each free node stores its size and a next pointer:
 *
 *   [ FreeListNode: size | next ] [ ......... usable ......... ]
 *
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>
#include "lib/arena.h"

typedef struct FreeListNode {
  size_t               size;  /* usable size (not including this header) */
  struct FreeListNode *next;
} FreeListNode;

typedef struct FreeList {
  FreeListNode *free_head;   /* head of free list (first-fit search)    */
  Arena        *arena;       /* backing arena for new allocations       */
} FreeList;

void  freelist_init(FreeList *fl, Arena *arena);
void *freelist_alloc(FreeList *fl, size_t size, size_t align);
void  freelist_free(FreeList *fl, void *ptr, size_t size);

#endif /* LIB_FREELIST_H */
