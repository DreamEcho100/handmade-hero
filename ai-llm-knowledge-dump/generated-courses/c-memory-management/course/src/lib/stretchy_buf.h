#ifndef LIB_STRETCHY_BUF_H
#define LIB_STRETCHY_BUF_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/stretchy_buf.h — Type-generic dynamic array (stb-style stretchy buffer)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Uses an arena as backing store.  The trick: store a BufHeader struct
 * immediately BEFORE the array pointer the user sees.
 *
 *   [ BufHeader | elem[0] | elem[1] | ... | elem[cap-1] ]
 *   ^            ^
 *   header       user-visible pointer (buf)
 *
 * Access the header via:  ((BufHeader *)buf - 1)
 *
 * Macros:
 *   buf_push(arena, buf, val) — append value, grow if needed
 *   buf_len(buf)              — current length (0 if NULL)
 *   buf_cap(buf)              — current capacity (0 if NULL)
 *   buf_pop(buf)              — remove last element
 *   buf_last(buf)             — access last element
 *   buf_free(buf)             — set to NULL (arena handles actual memory)
 *
 * IMPORTANT: buf must be an lvalue (a variable, not an expression) because
 * buf_push may reassign it when growing.
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>
#include <string.h>
#include "lib/arena.h"

typedef struct BufHeader {
  size_t len;
  size_t cap;
} BufHeader;

/* ── Internal: access header from user pointer ───────────────────────────*/
#define buf__hdr(buf) ((BufHeader *)(buf) - 1)

/* ── Public API ──────────────────────────────────────────────────────────*/

#define buf_len(buf) ((buf) ? buf__hdr(buf)->len : (size_t)0)
#define buf_cap(buf) ((buf) ? buf__hdr(buf)->cap : (size_t)0)

/* buf_last: precondition — buf != NULL && len > 0 (asserted in debug) */
#define buf_last(buf) \
  (assert((buf) && buf__hdr(buf)->len > 0 && "buf_last: empty buffer"), \
   (buf)[buf__hdr(buf)->len - 1])

#define buf_pop(buf) \
  ((buf) && buf__hdr(buf)->len > 0 ? --buf__hdr(buf)->len : (size_t)0)

#define buf_free(buf) ((buf) = NULL)

/* ── Growth: double capacity, minimum 16 ─────────────────────────────────
 *
 * When we grow, we allocate a NEW chunk from the arena (the old one is
 * just wasted arena space — acceptable because the arena frees everything
 * at once).  We memcpy old data into the new chunk.
 */
#define buf__grow(arena_ptr, buf, elem_size)                                 \
  do {                                                                       \
    size_t old_len = buf_len(buf);                                           \
    size_t old_cap = buf_cap(buf);                                           \
    size_t new_cap = old_cap ? old_cap * 2 : 16;                             \
    size_t alloc_size = sizeof(BufHeader) + new_cap * (elem_size);           \
    BufHeader *new_hdr = (BufHeader *)arena_push_size(                       \
        (arena_ptr), alloc_size, _Alignof(max_align_t));                     \
    new_hdr->len = old_len;                                                  \
    new_hdr->cap = new_cap;                                                  \
    if ((buf) && old_len > 0) {                                              \
      memcpy(new_hdr + 1, (buf), old_len * (elem_size));                     \
    }                                                                        \
    (buf) = (void *)(new_hdr + 1);                                           \
  } while (0)

#define buf_push(arena_ptr, buf, val)                                        \
  do {                                                                       \
    if (buf_len(buf) >= buf_cap(buf)) {                                      \
      buf__grow((arena_ptr), (buf), sizeof(*(buf)));                         \
    }                                                                        \
    (buf)[buf__hdr(buf)->len++] = (val);                                     \
  } while (0)

#endif /* LIB_STRETCHY_BUF_H */
