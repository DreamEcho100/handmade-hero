/* lib/debug_alloc.c — Debug allocator implementation
 *
 * Memory layout for each allocation:
 *
 *   [ 4-byte canary | user_size bytes (filled 0xCD) | 4-byte canary ]
 *   ^               ^                                ^
 *   real_ptr        user_ptr                         trailing canary
 *
 * We also store the allocation size in a header before the leading canary
 * so that debug_free knows how many bytes to check/fill.
 *
 * Full layout:
 *   [ size_t size | 4-byte canary | user data ... | 4-byte canary ]
 */

#include "lib/debug_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── Constants ───────────────────────────────────────────────────────────*/

#define CANARY_VALUE  0xDEADBEEFu
#define CANARY_SIZE   4           /* bytes */
#define FILL_ALLOC    0xCD        /* "clean data" — freshly allocated */
#define FILL_FREE     0xDD        /* "dead data"  — freed memory */

/* ── Global stats ────────────────────────────────────────────────────────*/

static size_t g_total_allocs    = 0;   /* lifetime allocation count */
static size_t g_total_frees     = 0;   /* lifetime free count */
static size_t g_active_allocs   = 0;   /* currently live allocations */
static size_t g_total_bytes     = 0;   /* lifetime bytes requested */
static size_t g_active_bytes    = 0;   /* currently live bytes */

/* ── Header stored before the leading canary ─────────────────────────────*/

typedef struct {
  size_t user_size;         /* number of bytes the caller requested */
  const char *alloc_file;   /* source file of the allocation */
  int alloc_line;           /* source line of the allocation */
} DebugHeader;

/* ── Helpers: compute offsets ────────────────────────────────────────────*/

static size_t total_block_size(size_t user_size) {
  return sizeof(DebugHeader) + CANARY_SIZE + user_size + CANARY_SIZE;
}

static DebugHeader *header_from_user(void *user_ptr) {
  uint8_t *p = (uint8_t *)user_ptr;
  return (DebugHeader *)(p - CANARY_SIZE - sizeof(DebugHeader));
}

static uint8_t *leading_canary_ptr(DebugHeader *hdr) {
  return (uint8_t *)hdr + sizeof(DebugHeader);
}

static uint8_t *user_ptr_from_header(DebugHeader *hdr) {
  return (uint8_t *)hdr + sizeof(DebugHeader) + CANARY_SIZE;
}

static uint8_t *trailing_canary_ptr(DebugHeader *hdr) {
  return user_ptr_from_header(hdr) + hdr->user_size;
}

/* ── Canary read/write ───────────────────────────────────────────────────*/

static void write_canary(uint8_t *dest) {
  uint32_t val = CANARY_VALUE;
  memcpy(dest, &val, CANARY_SIZE);
}

static int check_canary(const uint8_t *src) {
  uint32_t val;
  memcpy(&val, src, CANARY_SIZE);
  return val == CANARY_VALUE;
}

/* ── Public API ──────────────────────────────────────────────────────────*/

void *debug_alloc(size_t size, const char *file, int line) {
  if (size == 0) size = 1;  /* avoid zero-size edge case */

  size_t block_size = total_block_size(size);
  void *raw = malloc(block_size);
  if (!raw) {
    fprintf(stderr, "[debug_alloc] malloc(%zu) failed at %s:%d\n",
            size, file, line);
    return NULL;
  }

  /* Set up header */
  DebugHeader *hdr = (DebugHeader *)raw;
  hdr->user_size  = size;
  hdr->alloc_file = file;
  hdr->alloc_line = line;

  /* Write canaries */
  write_canary(leading_canary_ptr(hdr));
  write_canary(trailing_canary_ptr(hdr));

  /* Fill user region with 0xCD */
  uint8_t *user = user_ptr_from_header(hdr);
  memset(user, FILL_ALLOC, size);

  /* Update stats */
  g_total_allocs++;
  g_active_allocs++;
  g_total_bytes += size;
  g_active_bytes += size;

  return user;
}

void debug_free(void *ptr, const char *file, int line) {
  if (!ptr) return;  /* free(NULL) is a no-op */

  DebugHeader *hdr = header_from_user(ptr);

  /* Check leading canary */
  if (!check_canary(leading_canary_ptr(hdr))) {
    fprintf(stderr,
            "[debug_free] LEADING CANARY CORRUPTED!\n"
            "  Free called at: %s:%d\n"
            "  Originally allocated at: %s:%d (%zu bytes)\n"
            "  Someone wrote before the start of the allocation.\n",
            file, line, hdr->alloc_file, hdr->alloc_line, hdr->user_size);
  }

  /* Check trailing canary */
  if (!check_canary(trailing_canary_ptr(hdr))) {
    fprintf(stderr,
            "[debug_free] TRAILING CANARY CORRUPTED!\n"
            "  Free called at: %s:%d\n"
            "  Originally allocated at: %s:%d (%zu bytes)\n"
            "  Someone wrote past the end of the allocation.\n",
            file, line, hdr->alloc_file, hdr->alloc_line, hdr->user_size);
  }

  /* Fill user region with 0xDD so use-after-free is visible */
  memset(ptr, FILL_FREE, hdr->user_size);

  /* Update stats */
  g_total_frees++;
  if (g_active_allocs > 0) g_active_allocs--;
  if (g_active_bytes >= hdr->user_size) g_active_bytes -= hdr->user_size;

  /* Actually release memory */
  free(hdr);
}

void debug_alloc_report(void) {
  printf("\n");
  printf("══════════════════════════════════════════════════════════════\n");
  printf("  Debug Allocator Report\n");
  printf("══════════════════════════════════════════════════════════════\n");
  printf("\n");
  printf("  Total allocations:   %zu\n", g_total_allocs);
  printf("  Total frees:         %zu\n", g_total_frees);
  printf("  Active allocations:  %zu\n", g_active_allocs);
  printf("  Total bytes alloc'd: %zu\n", g_total_bytes);
  printf("  Active bytes:        %zu\n", g_active_bytes);
  printf("\n");

  if (g_active_allocs > 0) {
    printf("  WARNING: %zu allocation(s) still live — possible leak!\n\n",
           g_active_allocs);
  } else {
    printf("  All allocations freed. No leaks detected.\n\n");
  }
}
