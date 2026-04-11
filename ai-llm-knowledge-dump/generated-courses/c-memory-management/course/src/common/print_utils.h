#ifndef COMMON_PRINT_UTILS_H
#define COMMON_PRINT_UTILS_H

/* ═══════════════════════════════════════════════════════════════════════════
 * common/print_utils.h — Memory visualization helpers
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Inline utilities for printing memory addresses, hex dumps, and
 * pointer relationships. Used across multiple lessons.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

/* ── Print a labeled pointer address ──────────────────────────────────────*/

static inline void print_ptr(const char *label, const void *ptr) {
  printf("  %-30s %p\n", label, ptr);
}

/* ── Print a labeled pointer with its value ───────────────────────────────*/

static inline void print_ptr_val(const char *label, const void *ptr,
                                  size_t size) {
  printf("  %-30s %p  (", label, ptr);
  const uint8_t *bytes = (const uint8_t *)ptr;
  for (size_t i = 0; i < size && i < 8; i++) {
    if (i > 0) printf(" ");
    printf("%02x", bytes[i]);
  }
  if (size > 8) printf(" ...");
  printf(")\n");
}

/* ── Hex dump of a memory region ──────────────────────────────────────────
 *
 * Output format (16 bytes per line):
 *   0x7fff1234:  48 65 6c 6c 6f 20 57 6f  72 6c 64 00 00 00 00 00  |Hello World.....|
 */

static inline void hex_dump(const void *ptr, size_t len) {
  const uint8_t *data = (const uint8_t *)ptr;
  for (size_t off = 0; off < len; off += 16) {
    printf("  %p:  ", (const void *)(data + off));

    /* Hex bytes */
    for (size_t i = 0; i < 16; i++) {
      if (off + i < len)
        printf("%02x ", data[off + i]);
      else
        printf("   ");
      if (i == 7) printf(" ");
    }

    /* ASCII */
    printf(" |");
    for (size_t i = 0; i < 16 && off + i < len; i++) {
      uint8_t c = data[off + i];
      printf("%c", (c >= 32 && c < 127) ? (char)c : '.');
    }
    printf("|\n");
  }
}

/* ── Section header ───────────────────────────────────────────────────────*/

static inline void print_section(const char *title) {
  printf("\n── %s ", title);
  size_t len = strlen(title) + 4;
  while (len++ < 66) printf("─");
  printf("\n\n");
}

/* ── Print address distance between two pointers ─────────────────────────*/

static inline void print_distance(const char *label,
                                   const void *a, const void *b) {
  ptrdiff_t diff = (const uint8_t *)b - (const uint8_t *)a;
  printf("  %-30s %td bytes (%td KB)\n", label, diff, diff / 1024);
}

#endif /* COMMON_PRINT_UTILS_H */
