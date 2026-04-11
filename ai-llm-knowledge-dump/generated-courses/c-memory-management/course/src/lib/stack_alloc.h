#ifndef LIB_STACK_ALLOC_H
#define LIB_STACK_ALLOC_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/stack_alloc.h — Stack (LIFO) allocator with per-allocation headers
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Like an arena but supports individual pop (most-recent only).  Each
 * allocation stores a small header recording the previous stack top,
 * allowing pop to restore the allocator state.
 *
 * Memory layout per allocation:
 *
 *   [ StackHeader: prev_offset | padding ] [ user data ........... ]
 *   ^                                       ^
 *   header (internal)                       returned pointer
 *
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct StackHeader {
  size_t prev_offset;  /* offset of the previous StackHeader from base */
} StackHeader;

typedef struct StackAlloc {
  uint8_t *base;        /* start of the memory block                   */
  size_t   size;        /* total size of the memory block              */
  size_t   offset;      /* current allocation offset (stack top)       */
  size_t   prev_offset; /* offset of the most recent StackHeader       */
  size_t   alloc_count; /* number of active allocations                */
} StackAlloc;

void  stack_alloc_init(StackAlloc *sa, void *mem, size_t size);
void *stack_alloc_push(StackAlloc *sa, size_t size, size_t align);
void  stack_alloc_pop(StackAlloc *sa);
void  stack_alloc_reset(StackAlloc *sa);

#endif /* LIB_STACK_ALLOC_H */
