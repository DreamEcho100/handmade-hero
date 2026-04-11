#ifndef LIB_STR_H
#define LIB_STR_H

/* ═══════════════════════════════════════════════════════════════════════════
 * lib/str.h — Arena-backed string utilities
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Lesson 16: Strings that live on an arena.  No free() needed.
 *
 * Str is a non-owning fat pointer: { data, length }.
 * All allocation goes through an Arena, so strings are freed in bulk
 * when the arena is reset/freed.
 * ═══════════════════════════════════════════════════════════════════════════
  * NOT thread-safe. Use one instance per thread or external synchronization.
 */

#include <stddef.h>
#include "lib/arena.h"

/* ── Str — length-delimited string (NOT null-terminated by default) ───────*/
typedef struct Str {
  const char *data;
  size_t      len;
} Str;

/* ── Create a Str from a C string literal ─────────────────────────────────*/
#define STR_LIT(s)  ((Str){ (s), sizeof(s) - 1 })

/* ── Create a Str from a char* and length ─────────────────────────────────*/
static inline Str str_make(const char *data, size_t len) {
  return (Str){ data, len };
}

/* ── Push a copy of a Str onto the arena (null-terminated for convenience) */
Str str_push(Arena *arena, Str s);

/* ── Push a copy of a C string onto the arena ─────────────────────────────*/
Str str_push_cstr(Arena *arena, const char *cstr);

/* ── Printf into the arena and return a Str ───────────────────────────────*/
Str str_pushf(Arena *arena, const char *fmt, ...)
  __attribute__((format(printf, 2, 3)));

/* ── Concatenate two Strs onto the arena ──────────────────────────────────*/
Str str_concat(Arena *arena, Str a, Str b);

/* ── Join an array of Strs with a separator ───────────────────────────────*/
Str str_join(Arena *arena, const Str *strs, size_t count, Str sep);

/* ── Compare two Strs ─────────────────────────────────────────────────────*/
static inline int str_eq(Str a, Str b) {
  if (a.len != b.len) return 0;
  for (size_t i = 0; i < a.len; i++) {
    if (a.data[i] != b.data[i]) return 0;
  }
  return 1;
}

/* ── Get a null-terminated C string (pushes onto arena if needed) ─────────*/
const char *str_to_cstr(Arena *arena, Str s);

#endif /* LIB_STR_H */
