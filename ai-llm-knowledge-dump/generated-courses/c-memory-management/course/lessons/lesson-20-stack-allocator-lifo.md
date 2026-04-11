# Lesson 20 -- Stack Allocator (LIFO)

> **What you'll build:** A LIFO stack allocator that supports individual pop (most recent only) via per-allocation headers, demonstrated with nested computation buffers and an overhead comparison against a plain arena.

## Observable outcome

Run the program and you see three allocations pushed (int, double, char*) then popped in reverse order with the offset returning to zero, a header overhead comparison showing the cost vs a plain arena, nested float buffers for a three-level computation that unwinds cleanly, a comparison table of stack allocator vs arena trade-offs, and (with `--bench`) stack push+pop vs arena push vs malloc+free.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| LIFO stack allocator | Like the JS call stack -- each function call "pushes" a frame, return "pops" it |
| `StackHeader` per allocation | Like a stack frame's saved return address -- records where to rewind to |
| `stack_alloc_pop` | Like `return` from a function -- restores the stack pointer to before the call |
| Header overhead trade-off | No JS equivalent -- JS hides all overhead. Here you see the exact cost. |
| Nested scratch buffers | Like nested function scopes where each level has its own local variables |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_20.c` | New | Push/pop demo, overhead analysis, nested buffers, comparison table, benchmark |
| `src/lib/stack_alloc.h` | Dependency | `StackAlloc`, `stack_alloc_push`, `stack_alloc_pop`, `stack_alloc_reset` |
| `src/lib/stack_alloc.c` | Dependency | Implementation with StackHeader per allocation |
| `src/lib/arena.h` | Dependency | Arena for overhead comparison |

---

## Background -- adding pop to a bump allocator

An arena allocates by bumping a pointer forward. It cannot free individual allocations -- only bulk reset. The stack allocator adds one capability: popping the most recent allocation. It achieves this by storing a small header before each allocation that records where the previous stack top was.

```
  Memory layout per push:

  [ StackHeader: prev_offset ] [ padding ] [ user data ......... ]
  ^                                         ^
  internal (not returned)                   returned to caller

  After three pushes:
  [ Hdr0 ][ int ][ Hdr1 ][ pad ][ double ][ Hdr2 ][ char[32] ]
  ^                                                             ^
  base                                                   offset (stack top)
```

### How push works (stack_alloc.c lines 18-49)

```c
void *stack_alloc_push(StackAlloc *sa, size_t size, size_t align) {
    // 1. Align position for the StackHeader
    size_t hdr_offset = sa->offset;
    size_t hdr_padding = (hdr_align - (hdr_offset & (hdr_align - 1))) & (hdr_align - 1);
    hdr_offset += hdr_padding;

    // 2. Position after the header
    size_t data_start = hdr_offset + sizeof(StackHeader);

    // 3. Align for user data
    size_t data_padding = (align - (data_start & (align - 1))) & (align - 1);
    data_start += data_padding;

    // 4. Check for overflow
    if (data_start + size > sa->size) return NULL;

    // 5. Write the header (saves previous stack top)
    StackHeader *hdr = (StackHeader *)(sa->base + hdr_offset);
    hdr->prev_offset = sa->prev_offset;

    // 6. Update allocator state
    sa->prev_offset = hdr_offset;
    sa->offset = data_start + size;
    sa->alloc_count++;

    return sa->base + data_start;
}
```

Two alignment passes: one for the header itself, one for the user data. The header stores `prev_offset`, which is the offset of the previous header. This forms a linked chain through the stack -- not via pointers, but via offsets from the base address.

### How pop works (stack_alloc.c lines 52-62)

```c
void stack_alloc_pop(StackAlloc *sa) {
    StackHeader *hdr = (StackHeader *)(sa->base + sa->prev_offset);
    sa->offset      = sa->prev_offset;       // rewind to before this header
    sa->prev_offset = hdr->prev_offset;      // follow chain to previous header
    sa->alloc_count--;
}
```

Pop reads the current top's header, restores `offset` to `prev_offset` (rewinding past the header and data), then follows the chain to the next header down. O(1), no search.

### The cost: header overhead

Every allocation stores a `StackHeader` (8 bytes on 64-bit). For a single `int` (4 bytes), that is 200% overhead. For a 400-byte float buffer, it is 2% overhead. The rule: use a stack allocator for larger scratch buffers where the overhead is negligible. Use a plain arena for many small allocations.

---

## Code walkthrough

### Section 1: basic push and pop (lines 26-67)

```c
uint8_t buffer[4096];
StackAlloc sa;
stack_alloc_init(&sa, buffer, sizeof(buffer));
```

The stack allocator operates on a pre-allocated buffer. Unlike the arena (which mallocs blocks), you give it memory upfront. This makes it useful for embedded systems or when you want to avoid any heap allocation.

```c
int *a = (int *)stack_alloc_push(&sa, sizeof(int), _Alignof(int));
*a = 111;
```

Push returns a pointer into the buffer, after the hidden header. The offset advances past the header + padding + data.

```c
stack_alloc_pop(&sa);  // removes char*
stack_alloc_pop(&sa);  // removes double
stack_alloc_pop(&sa);  // removes int, offset back to 0
```

Pops must happen in reverse order (LIFO). After all three pops, `offset` returns to 0. The buffer is fully reclaimed.

### Section 2: header overhead analysis (lines 69-102)

```c
printf("  StackHeader size: %zu bytes per allocation\n", sizeof(StackHeader));
```

On a typical 64-bit system, `sizeof(StackHeader)` is 8 bytes. The demo pushes 10 ints on both a stack allocator and an arena, then compares:

```
  10 ints (40 bytes of data):
    Stack allocator used: ~200 bytes (includes headers + alignment padding)
    Arena used:           ~40 bytes  (data only, minimal padding)
```

The stack allocator uses roughly 5x more memory for small objects. For larger allocations (hundreds of bytes each), the overhead becomes negligible.

### Section 3: nested computation buffers (lines 104-146)

This is the practical use case. Three levels of temporary float buffers, each depending on the previous:

```c
float *outer = (float *)stack_alloc_push(&sa, 100 * sizeof(float), _Alignof(float));
for (int i = 0; i < 100; i++) outer[i] = (float)i * 0.5f;

float *inner = (float *)stack_alloc_push(&sa, 50 * sizeof(float), _Alignof(float));
for (int i = 0; i < 50; i++) inner[i] = outer[i] * 2.0f;

float *deep = (float *)stack_alloc_push(&sa, 25 * sizeof(float), _Alignof(float));
for (int i = 0; i < 25; i++) deep[i] = inner[i] + 1.0f;
```

Then unwinding:

```c
float result = deep[0] + deep[24];
stack_alloc_pop(&sa);   // free deep

result += inner[0] + inner[49];
stack_alloc_pop(&sa);   // free inner

result += outer[0] + outer[99];
stack_alloc_pop(&sa);   // free outer
```

Each level reads its data, then pops. The parent level's data is still valid (it was pushed earlier). This models real computation patterns: parse an expression tree depth-first, allocating scratch buffers at each level and freeing them on the way back up.

### Section 4: comparison table (lines 148-160)

```
  ┌─────────────────────┬──────────────────┬──────────────────┐
  | Feature             | Stack Allocator  | Arena            |
  ├─────────────────────┼──────────────────┼──────────────────┤
  | Alloc               | O(1) bump+header | O(1) bump        |
  | Free individual     | O(1) pop (LIFO)  | No               |
  | Free all            | O(1) reset       | O(1) reset       |
  | Per-alloc overhead  | 8 bytes header   | 0 bytes          |
  | Free order          | LIFO only        | N/A              |
  | Use case            | Scratch buffers  | Lifetime groups  |
  └─────────────────────┴──────────────────┴──────────────────┘
```

The stack allocator and the arena's `TempMemory` (Lesson 14) solve similar problems. The difference: `TempMemory` checkpoints a position and frees everything after it in one call. The stack allocator pops individual allocations in reverse order. Use `TempMemory` for scoped lifetimes (per-frame, per-request). Use the stack allocator when you need fine-grained LIFO control within a single scope.

---

## Build and run

```bash
./build-dev.sh --lesson=20 -r
./build-dev.sh --lesson=20 --bench -r   # with benchmarks
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Pop frees the wrong allocation | Popping out of order (not LIFO) | Stack requires reverse-order pops; use a free list for arbitrary order |
| High overhead percentage | Pushing many small objects (single ints) | Use a plain arena for small uniform allocations |
| NULL returned from push | Stack buffer is full (`data_start + size > sa->size`) | Allocate a larger backing buffer, or use `stack_alloc_reset` to reclaim |
| Pop on empty stack | `stack_alloc_pop` when `alloc_count == 0` | Assert fires in debug; check `alloc_count > 0` before popping |

---

## Key takeaways

1. The stack allocator adds individual LIFO pop to a bump allocator by storing a `StackHeader` before each allocation.
2. Pop is O(1): read the header, restore the offset, follow the chain to the previous header.
3. The trade-off is `sizeof(StackHeader)` bytes of overhead per allocation. Worth it for large scratch buffers, wasteful for many small objects.
4. The stack allocator operates on a pre-allocated buffer -- no malloc needed. Good for embedded systems and fixed-budget scenarios.
5. For scoped lifetimes (per-frame, per-request), prefer the arena's `TempMemory` from Lesson 14. For fine-grained LIFO within a scope, use the stack allocator.
