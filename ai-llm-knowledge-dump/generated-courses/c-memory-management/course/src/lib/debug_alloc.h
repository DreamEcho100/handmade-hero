/* lib/debug_alloc.h — Debug allocator with canaries, fill patterns, and stats
 *
 * Wraps malloc/free to catch common bugs:
 *   - Fills new allocations with 0xCD ("clean data")
 *   - Fills freed memory with 0xDD ("dead data")
 *   - Places 4-byte canary 0xDEADBEEF before and after each allocation
 *   - On free: checks canaries for corruption, reports if broken
 *   - Tracks total allocations, active allocations, and total bytes
 *
 * Usage:
 *   void *p = DALLOC(64);      // debug_alloc(64, __FILE__, __LINE__)
 *   DFREE(p);                  // debug_free(p, __FILE__, __LINE__)
 *   debug_alloc_report();      // print allocation statistics
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#ifndef LIB_DEBUG_ALLOC_H
#define LIB_DEBUG_ALLOC_H

#include <stddef.h>

/* ── Core API ────────────────────────────────────────────────────────────*/

void *debug_alloc(size_t size, const char *file, int line);
void  debug_free(void *ptr, const char *file, int line);
void  debug_alloc_report(void);

/* ── Convenience macros ──────────────────────────────────────────────────*/

#define DALLOC(size)  debug_alloc((size), __FILE__, __LINE__)
#define DFREE(ptr)    debug_free((ptr), __FILE__, __LINE__)

#endif /* LIB_DEBUG_ALLOC_H */
