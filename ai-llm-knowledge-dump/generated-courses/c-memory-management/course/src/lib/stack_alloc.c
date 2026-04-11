/* ═══════════════════════════════════════════════════════════════════════════
 * lib/stack_alloc.c — Stack (LIFO) allocator implementation
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "lib/stack_alloc.h"
#include <assert.h>
#include <string.h>

void stack_alloc_init(StackAlloc *sa, void *mem, size_t size) {
  sa->base        = (uint8_t *)mem;
  sa->size        = size;
  sa->offset      = 0;
  sa->prev_offset = 0;
  sa->alloc_count = 0;
}

void *stack_alloc_push(StackAlloc *sa, size_t size, size_t align) {
  /* Align the position for the StackHeader first */
  size_t hdr_align = _Alignof(StackHeader);
  size_t hdr_offset = sa->offset;
  size_t hdr_padding = (hdr_align - (hdr_offset & (hdr_align - 1))) & (hdr_align - 1);
  hdr_offset += hdr_padding;

  /* Position after the header */
  size_t data_start = hdr_offset + sizeof(StackHeader);

  /* Align for user data */
  size_t data_padding = (align - (data_start & (align - 1))) & (align - 1);
  data_start += data_padding;

  /* Check for overflow */
  if (data_start + size > sa->size) {
    return NULL;  /* out of memory */
  }

  /* Write the header */
  StackHeader *hdr = (StackHeader *)(sa->base + hdr_offset);
  hdr->prev_offset = sa->prev_offset;

  /* Update allocator state */
  sa->prev_offset = hdr_offset;
  sa->offset = data_start + size;
  sa->alloc_count++;

  /* Zero the returned memory */
  void *result = sa->base + data_start;
  memset(result, 0, size);
  return result;
}

void stack_alloc_pop(StackAlloc *sa) {
  assert(sa->alloc_count > 0 && "stack_alloc_pop: nothing to pop");

  /* Read the header of the most recent allocation */
  StackHeader *hdr = (StackHeader *)(sa->base + sa->prev_offset);

  /* Restore state to before this allocation */
  sa->offset      = sa->prev_offset;
  sa->prev_offset = hdr->prev_offset;
  sa->alloc_count--;
}

void stack_alloc_reset(StackAlloc *sa) {
  sa->offset      = 0;
  sa->prev_offset = 0;
  sa->alloc_count = 0;
}
