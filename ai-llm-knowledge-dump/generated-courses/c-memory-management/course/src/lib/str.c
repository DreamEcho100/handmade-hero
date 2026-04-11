/* lib/str.c — Arena-backed string utilities implementation */

#include "lib/str.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

Str str_push(Arena *arena, Str s) {
  char *buf = (char *)arena_push_size(arena, s.len + 1, 1);
  if (!buf) return (Str){0};
  memcpy(buf, s.data, s.len);
  buf[s.len] = '\0';
  return (Str){ buf, s.len };
}

Str str_push_cstr(Arena *arena, const char *cstr) {
  size_t len = strlen(cstr);
  return str_push(arena, str_make(cstr, len));
}

Str str_pushf(Arena *arena, const char *fmt, ...) {
  /* First pass: measure */
  va_list args;
  va_start(args, fmt);
  int needed = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (needed < 0) return (Str){0};

  /* Second pass: write */
  size_t len = (size_t)needed;
  char *buf = (char *)arena_push_size(arena, len + 1, 1);
  if (!buf) return (Str){0};

  va_start(args, fmt);
  vsnprintf(buf, len + 1, fmt, args);
  va_end(args);

  return (Str){ buf, len };
}

Str str_concat(Arena *arena, Str a, Str b) {
  size_t total = a.len + b.len;
  char *buf = (char *)arena_push_size(arena, total + 1, 1);
  if (!buf) return (Str){0};
  memcpy(buf, a.data, a.len);
  memcpy(buf + a.len, b.data, b.len);
  buf[total] = '\0';
  return (Str){ buf, total };
}

Str str_join(Arena *arena, const Str *strs, size_t count, Str sep) {
  if (count == 0) return (Str){ "", 0 };

  /* Calculate total length (overflow-safe) */
  size_t total = 0;
  for (size_t i = 0; i < count; i++) {
    if (total > SIZE_MAX - strs[i].len) return (Str){0}; /* overflow */
    total += strs[i].len;
    if (i > 0) {
      if (total > SIZE_MAX - sep.len) return (Str){0};
      total += sep.len;
    }
  }

  char *buf = (char *)arena_push_size(arena, total + 1, 1);
  if (!buf) return (Str){0};

  char *cursor = buf;
  for (size_t i = 0; i < count; i++) {
    if (i > 0 && sep.len > 0) {
      memcpy(cursor, sep.data, sep.len);
      cursor += sep.len;
    }
    memcpy(cursor, strs[i].data, strs[i].len);
    cursor += strs[i].len;
  }
  *cursor = '\0';

  return (Str){ buf, total };
}

const char *str_to_cstr(Arena *arena, Str s) {
  /* Always copy — we can't safely check s.data[s.len] because the byte
   * past the string may not be allocated. The old code read past the end
   * which is undefined behavior. */
  Str copy = str_push(arena, s);
  return copy.data;
}
