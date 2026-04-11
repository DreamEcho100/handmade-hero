# Lesson 17 -- Stretchy Buffer (Dynamic Array)

> **What you'll build:** A type-generic dynamic array using the stb-style "stretchy buffer" trick backed by an arena, with growth visualization and benchmarks against naive realloc.

## Observable outcome

Run the program and you see 10 values pushed and printed, a growth table showing the doubling pattern across 1000 pushes (only ~7 growths needed), type genericity with doubles and structs, and the hidden `BufHeader` revealed. With `--bench`, it benchmarks `buf_push` against realloc-every-push and realloc-with-doubling.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| Stretchy buffer | Like a JS `Array` -- push appends, grows automatically |
| Hidden `BufHeader` before the data | Like V8's hidden class storing array metadata before the elements |
| Doubling growth (amortized O(1)) | Exactly like JS Array and `std::vector` -- capacity doubles on overflow |
| Arena-backed growth | Old buffers are abandoned in the arena (no `free` call), unlike `realloc` |
| Type-generic macros | Like TypeScript generics: one `buf_push<T>` works for any element type |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_17.c` | New | Stretchy buffer: usage, growth, type genericity, header trick, benchmark |
| `src/lib/stretchy_buf.h` | Dependency | Header-only stretchy buffer macros |
| `src/lib/arena.h` | Dependency | Arena backing store |

---

## Background -- how stretchy buffers work

Sean Barrett's stb libraries popularized this technique for C dynamic arrays. The trick: store a `BufHeader` (length + capacity) immediately before the element data. The user gets a pointer to `header + 1`, which looks like a normal C array. Macros access the hidden header by casting backward.

```
  Memory layout:

  [ BufHeader: len=3, cap=16 ] [ elem[0] | elem[1] | elem[2] | ... unused ... ]
  ^                              ^
  hidden header                  user-visible pointer (buf)
  (BufHeader *)buf - 1           buf[0], buf[1], buf[2] work normally
```

In JavaScript, `Array` works similarly under the hood. V8 stores array metadata (length, elements kind, map pointer) before the elements backing store. The difference is you never see it. In C, the stretchy buffer makes this explicit.

### The header trick

```c
typedef struct BufHeader {
    size_t len;   // number of elements currently stored
    size_t cap;   // number of elements that fit before growth
} BufHeader;

#define buf__hdr(buf)  ((BufHeader *)(buf) - 1)
#define buf_len(buf)   ((buf) ? buf__hdr(buf)->len : (size_t)0)
#define buf_cap(buf)   ((buf) ? buf__hdr(buf)->cap : (size_t)0)
```

`buf__hdr(buf)` casts the user pointer to `BufHeader *` and subtracts 1, landing on the hidden header. The `NULL` check in `buf_len` / `buf_cap` handles uninitialized buffers (which start as `NULL`).

### Growth strategy

When `buf_push` detects `len >= cap`, it calls `buf__grow`:

```c
size_t new_cap = old_cap ? old_cap * 2 : 16;    // double, minimum 16
size_t alloc_size = sizeof(BufHeader) + new_cap * elem_size;
BufHeader *new_hdr = (BufHeader *)arena_push_size(arena, alloc_size, ...);
new_hdr->len = old_len;
new_hdr->cap = new_cap;
if (buf && old_len > 0)
    memcpy(new_hdr + 1, buf, old_len * elem_size);  // copy old data
buf = (void *)(new_hdr + 1);                         // update user pointer
```

The old buffer is simply abandoned in the arena. No `free`, no `realloc`. The arena will reclaim it when reset or freed. This wastes some arena space but eliminates all the complexity of realloc.

Doubling means N pushes need only O(log N) growths. For 1000 pushes: 16, 32, 64, 128, 256, 512, 1024 = 7 growths.

---

## Code walkthrough

### Section 1: basic usage (lines 27-59)

```c
int *nums = NULL;  // starts empty -- no allocation yet
```

A stretchy buffer starts as `NULL`. This is the zero-initialized state. `buf_len(NULL)` returns 0, `buf_cap(NULL)` returns 0.

```c
for (int i = 1; i <= 10; i++) {
    buf_push(&arena, nums, i * 10);
}
```

`buf_push` checks if `len >= cap`. On the first push, `cap` is 0, so it grows (allocates header + 16 slots). Then it stores the value at `buf[len]` and increments `len`.

```c
printf("  Last element: %d\n", buf_last(nums));
```

`buf_last(buf)` is `buf[buf__hdr(buf)->len - 1]`. It asserts the buffer is non-empty.

```c
buf_pop(nums);   // decrements len, does NOT free memory
buf_free(nums);  // sets nums = NULL, arena still holds the memory
```

`buf_pop` just decrements `len`. `buf_free` sets the pointer to NULL. Neither actually frees memory -- the arena handles that.

### Section 2: growth pattern (lines 61-93)

```c
arena.min_block_size = 64 * 1024;
int *buf = NULL;
for (int i = 0; i < 1000; i++) {
    buf_push(&arena, buf, i);
    if (buf_cap(buf) != prev_cap) {
        // log the growth event
    }
}
```

The output shows a table of capacity changes: 0 to 16, 16 to 32, 32 to 64, ... up to 1024. Only 7 growths for 1000 pushes. Each growth copies all existing data to the new buffer, but because the arena just bumps a pointer for the allocation, the copy is the only real cost.

### Section 3: type genericity (lines 95-123)

The same `buf_push` macro works with any type because `sizeof(*(buf))` infers the element size:

```c
double *dbls = NULL;
buf_push(&arena, dbls, 3.14);    // sizeof(*(dbls)) == sizeof(double) == 8

typedef struct { float x, y; } Vec2;
Vec2 *points = NULL;
buf_push(&arena, points, ((Vec2){1.0f, 2.0f}));  // sizeof(*(points)) == 8
```

This is the C equivalent of TypeScript generics. One macro definition, any element type. The `((Vec2){...})` double-parentheses are needed because the compound literal contains a comma, which the preprocessor would otherwise treat as a macro argument separator.

### Section 4: the header trick revealed (lines 125-147)

```c
BufHeader *hdr = buf__hdr(buf);
printf("  User pointer (buf):    %p\n", (void *)buf);
printf("  Hidden header (hdr):   %p\n", (void *)hdr);
printf("  hdr + 1 == buf?        %s\n",
       ((void *)(hdr + 1) == (void *)buf) ? "YES" : "no");
```

This confirms that the header sits exactly `sizeof(BufHeader)` bytes before the user pointer. The memory looks like:

```
  [BufHeader: len=2 cap=16] [42] [99] [... unused slots ...]
  ^                          ^
  hdr                        buf (= hdr + 1)
```

---

## Build and run

```bash
./build-dev.sh --lesson=17 -r
./build-dev.sh --lesson=17 --bench -r   # with benchmarks
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Crash on `buf_push` | `buf` is not an lvalue (e.g., function parameter by value) | `buf_push` reassigns the pointer on growth; use a local variable or pass by pointer |
| Old pointer dangling after push | Growth allocated a new chunk; cached pointer is stale | Always access via `buf` directly -- never cache `&buf[i]` across a push |
| Arena grows unboundedly | Old abandoned buffers accumulate | Set `min_block_size` large enough; reset the arena when done with all buffers |
| Compound literal comma error | `buf_push(&a, pts, (Vec2){1,2})` -- comma splits macro args | Double-parenthesize: `buf_push(&a, pts, ((Vec2){1,2}))` |

---

## Key takeaways

1. Stretchy buffers store a hidden `BufHeader` (len + cap) before the user-visible array pointer.
2. `buf_push` doubles capacity on overflow (amortized O(1)). Growth allocates from the arena; the old buffer is abandoned.
3. The same macros work with any element type via `sizeof(*(buf))` -- C's version of generics.
4. `buf` must be an lvalue because growth reassigns the pointer. Never cache element addresses across a push.
5. Old buffers are wasted arena space. This is acceptable because the arena frees everything at once -- no per-buffer tracking needed.
