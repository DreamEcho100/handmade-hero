# Lesson 16 -- Arena String Handling

> **What you'll build:** An arena-backed string library with `Str` (a non-owning fat pointer), push/concat/join/format operations, and a benchmark showing arena strings outperforming `strdup` + `free`.

## Observable outcome

Run the program and you see arena-backed string creation via `str_push_cstr`, `str_push`, and `str_pushf`, concatenation and join producing new strings without manual memory management, a comparison of `str_eq`, a formatted report built from 5 parts and freed in one call, and (with `--bench`) arena `str_push` vs `strdup` + `free` performance.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| `Str` fat pointer (data + len) | Like a JS string -- carries its own length, but here you see the internals |
| `str_push` / `str_push_cstr` | Like `structuredClone(str)` but into arena memory |
| `str_concat` / `str_join` | Like `a + b` and `arr.join(sep)`, but allocation is explicit via arena |
| `STR_LIT("hello")` | Like a string literal -- zero allocation, computed at compile time |
| Bulk string cleanup | Like all strings in a request scope being GC'd at once, but instant |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_16.c` | New | Arena string demo: push, concat, join, format, report, benchmark |
| `src/lib/str.h` | New header | `Str` type, `str_push`, `str_push_cstr`, `str_pushf`, `str_concat`, `str_join`, `str_eq` |
| `src/lib/str.c` | New impl | String operation implementations |
| `src/lib/arena.h` | Dependency | Arena allocator |

---

## Background -- the string ownership problem

Strings are the #1 source of memory bugs in C. Every `strdup` needs a matching `free`. Every `sprintf` into a `malloc`'d buffer raises the question: who owns this buffer? Who frees it? When?

In JavaScript, you never think about this. Strings are immutable, garbage-collected values. Concatenation with `+` creates a new string and the old ones are freed automatically. In C, you must track every string's owner.

Arena strings eliminate this entire category of bugs.

### The Str type

```c
typedef struct Str {
    const char *data;   // pointer to character data (may or may not be null-terminated)
    size_t      len;    // byte count (NOT including any null terminator)
} Str;
```

`Str` is a fat pointer: it carries both the data pointer and the length. This solves two problems:

1. **Ownership**: `Str` does not own the memory -- the arena does. When the arena resets, all strings are freed together.
2. **Length**: C strings use `strlen` (O(n) scan for null). `Str` carries its length (O(1) access). It can also contain embedded null bytes.

```
  Traditional C string:
  [ h | e | l | l | o | \0 ]
  You must call strlen() to find the length (scans all bytes).
  You must call free() when done (but who calls it?).

  Str (fat pointer):
  { .data = ──> [ h | e | l | l | o | \0 ], .len = 5 }
  Length is O(1).  Arena owns the memory.  No free() needed.
```

### STR_LIT -- zero-cost string literals

```c
#define STR_LIT(s)  ((Str){ (s), sizeof(s) - 1 })
```

`sizeof("hello")` is 6 (5 chars + null terminator), evaluated at compile time. `sizeof(s) - 1` gives 5. This creates a `Str` pointing directly at the string literal in the binary's read-only data section. No allocation, no copy, no runtime cost.

**Warning**: `STR_LIT` only works with string literals. Using it with a `char *` variable gives `sizeof(char *)` (8 on 64-bit), not the string length.

---

## Code walkthrough

### Section 1: basic string operations (lines 29-47)

Three ways to create arena strings:

```c
Str hello = str_push_cstr(&arena, "Hello, World!");
```

`str_push_cstr` (str.c lines 16-19) measures the string with `strlen`, then calls `str_push` which allocates `len + 1` bytes on the arena, copies the data, and null-terminates for C interop:

```c
Str str_push(Arena *arena, Str s) {
    char *buf = (char *)arena_push_size(arena, s.len + 1, 1);
    memcpy(buf, s.data, s.len);
    buf[s.len] = '\0';
    return (Str){ buf, s.len };
}
```

The `+1` and null terminator are a convenience so you can pass `s.data` to C functions that expect null-terminated strings. The `len` field does NOT include the null.

```c
Str name = str_push(&arena, STR_LIT("Ryan Fleury"));
```

`STR_LIT` creates a `Str` from a literal at compile time. `str_push` copies it onto the arena.

```c
Str formatted = str_pushf(&arena, "Arena used: %zu bytes", arena_total_used(&arena));
```

`str_pushf` (str.c lines 21-40) uses a two-pass approach: first `vsnprintf(NULL, 0, ...)` to measure the needed size, then allocate and format. This avoids over-allocating.

### Section 2: concatenation (lines 49-59)

```c
Str full = str_concat(&arena, STR_LIT("Memory "), STR_LIT("Management"));
```

`str_concat` (str.c lines 42-50) allocates `a.len + b.len + 1` bytes, copies both halves with `memcpy`, and null-terminates. The arena owns the result. The original strings are untouched.

### Section 3: join (lines 61-72)

```c
Str words[] = { STR_LIT("arena"), STR_LIT("pool"), STR_LIT("stack"), STR_LIT("slab") };
Str joined = str_join(&arena, words, 4, STR_LIT(", "));
// Result: "arena, pool, stack, slab"
```

`str_join` (str.c lines 52-81) first calculates the total length (with overflow checks), allocates once, then copies all parts with separators using a cursor. One allocation, one pass. In JS, `Array.prototype.join` does the same thing internally.

### Section 4: comparison (lines 74-83)

```c
str_eq(a, b)  // true if same length AND same bytes
```

`str_eq` (str.h lines 51-57) checks length first (O(1) fast path for different lengths), then compares byte by byte. This avoids scanning past the end of a string, unlike `strcmp` which relies on null terminators.

### Section 5: building a report with TempMemory (lines 104-123)

```c
TempMemory temp = arena_begin_temp(&arena);

Str header = str_pushf(&arena, "=== Memory Report ===\n");
Str line1  = str_pushf(&arena, "  Total allocators: %d\n", 5);
// ... more lines ...
Str parts[] = { header, line1, line2, line3, footer };
Str report = str_join(&arena, parts, 5, STR_LIT(""));

// ... use the report ...
arena_end_temp(temp);  // ALL 6 strings freed in one call
```

Six string allocations, zero `free` calls. The temp scope frees everything. This is the pattern for building responses, reports, or log messages from parts.

---

## Build and run

```bash
./build-dev.sh --lesson=16 -r
./build-dev.sh --lesson=16 --bench -r   # with benchmarks
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Str` data is garbage after arena reset | The `Str` pointed into arena memory that was freed | Copy to a longer-lived arena before resetting |
| Passing `Str` to `printf("%s")` | `Str` may not be null-terminated in general | Use `%.*s` with `(int)s.len, s.data`, or call `str_to_cstr` |
| `STR_LIT` on a `char *` variable | `sizeof(ptr)` gives 8 (pointer size), not string length | Use `str_make(ptr, len)` for runtime variables |
| Benchmark shows strdup winning | Arena block too small, causing frequent new block allocations | Increase `min_block_size` or use mmap-backed arena |

---

## Key takeaways

1. `Str` is a fat pointer (data + length) that does not own its memory. The arena owns it.
2. `str_push`, `str_concat`, `str_join`, and `str_pushf` allocate on the arena. No individual `free` needed.
3. `STR_LIT("literal")` creates a `Str` at compile time with zero allocation -- use it for constants and separators.
4. Arena strings eliminate the entire "who frees this string?" problem. When the arena resets or the temp scope ends, all strings are freed together.
5. The two-pass `str_pushf` technique (measure then allocate) is a standard C pattern for printf-into-buffer without over-allocating.
