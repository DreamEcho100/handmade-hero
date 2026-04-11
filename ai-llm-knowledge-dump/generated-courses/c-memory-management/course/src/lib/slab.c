/* ═══════════════════════════════════════════════════════════════════════════
 * lib/slab.c — Slab allocator implementation
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "lib/slab.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── Internal: allocate and initialize a new slab page ────────────────────*/
static SlabPage *slab_new_page(Slab *slab) {
  /* Layout: [SlabPage header] [obj0] [obj1] ... [objN-1] */
  size_t page_size = sizeof(SlabPage) + slab->obj_size * slab->objs_per_slab;
  SlabPage *page = (SlabPage *)malloc(page_size);
  if (!page) return NULL;

  memset(page, 0, page_size);
  page->next = NULL;
  page->used = 0;

  /* Thread all slots onto the per-page free list */
  uint8_t *base = (uint8_t *)(page + 1);
  page->free_head = NULL;
  for (size_t i = 0; i < slab->objs_per_slab; i++) {
    SlabFreeNode *node = (SlabFreeNode *)(base + i * slab->obj_size);
    node->next = page->free_head;
    page->free_head = node;
  }

  return page;
}

/* ── Internal: find which page a pointer belongs to ───────────────────────*/
static SlabPage *slab_find_page(const Slab *slab, const void *ptr) {
  SlabPage *page = slab->pages;
  while (page) {
    uint8_t *base = (uint8_t *)(page + 1);
    uint8_t *end  = base + slab->obj_size * slab->objs_per_slab;
    if ((const uint8_t *)ptr >= base && (const uint8_t *)ptr < end) {
      return page;
    }
    page = page->next;
  }
  return NULL;
}

void slab_init(Slab *slab, size_t obj_size, size_t objs_per_slab) {
  /* Ensure each slot can hold a free list pointer */
  if (obj_size < sizeof(SlabFreeNode))
    obj_size = sizeof(SlabFreeNode);

  /* Align object size */
  size_t align = _Alignof(max_align_t);
  obj_size = (obj_size + align - 1) & ~(align - 1);

  slab->obj_size      = obj_size;
  slab->objs_per_slab = objs_per_slab;
  slab->total_allocs  = 0;
  slab->pages         = NULL;
}

void *slab_alloc(Slab *slab) {
  /* Find a page with free slots */
  SlabPage *page = slab->pages;
  while (page && !page->free_head) {
    page = page->next;
  }

  /* No page with free space — allocate a new one */
  if (!page) {
    page = slab_new_page(slab);
    if (!page) return NULL;
    page->next = slab->pages;
    slab->pages = page;
  }

  /* Pop from the page's free list */
  SlabFreeNode *node = page->free_head;
  page->free_head = node->next;
  page->used++;
  slab->total_allocs++;

  memset(node, 0, slab->obj_size);
  return (void *)node;
}

void slab_free(Slab *slab, void *ptr) {
  if (!ptr) return;

  SlabPage *page = slab_find_page(slab, ptr);
  assert(page && "slab_free: pointer does not belong to this slab");

  /* Push back onto the page's free list */
  SlabFreeNode *node = (SlabFreeNode *)ptr;
  node->next = page->free_head;
  page->free_head = node;
  page->used--;
  slab->total_allocs--;
}

void slab_destroy(Slab *slab) {
  SlabPage *page = slab->pages;
  while (page) {
    SlabPage *next = page->next;
    free(page);
    page = next;
  }
  slab->pages = NULL;
  slab->total_allocs = 0;
}
