/* ═══════════════════════════════════════════════════════════════════════════
 * lib/freelist.c — Free list overlay on an arena
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "lib/freelist.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

void freelist_init(FreeList *fl, Arena *arena) {
  assert(fl && arena && "freelist_init: NULL parameter");
  fl->free_head = NULL;
  fl->arena     = arena;
}

void *freelist_alloc(FreeList *fl, size_t size, size_t align) {
  if (size == 0) return NULL;

  /* Ensure we can fit a FreeListNode in every allocation for later reuse */
  size_t min_size = sizeof(FreeListNode);
  if (size < min_size) size = min_size;

  /* First-fit search through the free list.
   * Check alignment: a freed block might not meet the requested alignment. */
  FreeListNode **prev_next = &fl->free_head;
  FreeListNode  *node      = fl->free_head;

  while (node) {
    if (node->size >= size &&
        ((uintptr_t)node % align == 0)) { /* alignment check */
      /* Found a fit with correct alignment — remove from free list */
      *prev_next = node->next;
      void *result = (void *)node;
      memset(result, 0, size);
      return result;
    }
    prev_next = &node->next;
    node = node->next;
  }

  /* Nothing on the free list fits — bump the arena */
  void *result = arena_push_zero(fl->arena, size, align);
  return result;
}

void freelist_free(FreeList *fl, void *ptr, size_t size) {
  if (!ptr) return;

  /* Ensure minimum size for the node */
  size_t min_size = sizeof(FreeListNode);
  if (size < min_size) size = min_size;

  /* Push this block onto the free list head */
  FreeListNode *node = (FreeListNode *)ptr;
  node->size = size;
  node->next = fl->free_head;
  fl->free_head = node;
}
