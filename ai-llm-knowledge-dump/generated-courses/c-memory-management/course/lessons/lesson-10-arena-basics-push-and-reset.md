# Lesson 10 -- Arena Basics: Push and Reset

> **What you will build:** A minimal arena allocator that replaces malloc/free
> with two operations -- push (bump a pointer) and reset (free everything at
> once) -- and benchmark it against the system allocator.

## Observable outcome

When you run the program you see arena allocations happening as simple pointer
bumps, arrays allocated without individual tracking, a per-frame game loop
pattern that reuses memory with zero leaks, and a benchmark showing arena push
running significantly faster than malloc+free.

## New concepts

- Arena as a bump allocator: O(1) allocation via pointer arithmetic
- `arena_reset` as bulk deallocation -- one call frees all allocations
- Zero Is Initialization (ZII): `Arena arena = {0}` is a valid empty arena
- The per-frame scratch pattern for game loops and request handlers

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_10.c` | New | Arena basics demo with push, reset, per-frame pattern, and benchmark |
| `src/lib/arena.h` | Dependency | Arena implementation: `arena_push_size`, `arena_reset`, `arena_free_all` |
| `src/common/bench.h` | Dependency | Benchmarking harness |

---

## Background

### JS/TS analogy -- the hidden allocator you already use

Every React render cycle does something surprisingly similar to an arena:

```js
function MyComponent({ items }) {
    // Each render creates temporary objects:
    const filtered = items.filter(i => i.active);   // new array
    const mapped = filtered.map(i => i.name);        // new array
    const joined = mapped.join(', ');                 // new string

    return <div>{joined}</div>;
    // After render, all temporaries become garbage -> GC collects them
}
```

Each render allocates temporary data, uses it, and then the GC reclaims it all.
An arena does the same thing but explicitly and without GC pauses:

```c
// Each frame:
arena_reset(&frame_arena);             // "GC" -- instant, O(1)
float *positions = ARENA_PUSH_ARRAY(&frame_arena, 100, float);
char *debug_str = arena_push_size(&frame_arena, 128, 1);
// ... use them ...
// Next frame: reset again, all freed instantly
```

The difference: React's GC might pause for 5-50ms to collect those temporaries.
The arena reset is a single pointer assignment -- sub-nanosecond.

### The core insight

Ryan Fleury describes it:

> "Arena is just taking the stack-like allocation pattern and applying it to a
> larger category of lifetimes."

The call stack already does bump allocation -- every function call pushes a
frame, every return pops it. An arena generalizes this: you push allocations
forward, and when you are done with the whole batch, you reset to the beginning.

```
Arena internals (single block):

  +------------------+-------------------+-----------------+
  | ArenaBlock header | used data...      | free space...   |
  +------------------+-------------------+-----------------+
  ^                   ^                   ^                 ^
  malloc'd ptr        data start          alloc ptr         end
                                          (bumps forward)

  arena_push_size:   move alloc ptr forward by 'size'
  arena_reset:       move alloc ptr back to data start
```

### Why arenas beat malloc

Compare what happens on each allocation:

```
malloc(64):                        arena_push_size(64):
  1. Lock the allocator              1. Add 64 to offset
  2. Search free lists               2. Return pointer
  3. Split a block if needed         Done.
  4. Update metadata headers
  5. Unlock the allocator
  6. Return pointer
```

The arena does ONE operation (pointer bump). Malloc does FIVE. This is why arena
push is 5-20x faster than malloc in benchmarks.

The trade-off is that you cannot free individual allocations -- you free
everything at once. This is not a limitation; it is a design choice. Most
allocations in real programs share a lifetime:
- One frame of a game: all per-frame data resets at frame end
- One HTTP request: all request data is freed when the response is sent
- One file parse: all parse nodes are freed when parsing completes

Arenas make that lifetime explicit.

### The arena vs malloc comparison

```
+-------------------+------------------+------------------+
| Operation         | malloc/free      | Arena            |
+-------------------+------------------+------------------+
| Allocate          | Search free list | Bump pointer     |
| Free one          | Update metadata  | N/A (or noop)    |
| Free all          | N free() calls   | One reset()      |
| Per-alloc overhead| 16-32 bytes      | 0 bytes          |
| Thread safety     | Locks/atomics    | One arena/thread |
| Fragmentation     | Yes              | No               |
+-------------------+------------------+------------------+
```

---

## Code walkthrough

### Creating and using an arena

The arena starts zero-initialized -- no constructor needed:

```c
Arena arena = {0};  // ZII: zero-initialized is a valid empty arena
arena.min_block_size = 4096;  // 4 KB for demo purposes
```

In JS, `const map = new Map()` calls a constructor. In C with ZII (Zero Is
Initialization), `= {0}` produces a valid empty state with no function call.
The first `arena_push_size` call detects `current == NULL` and allocates the
first block automatically.

### Pushing values

Each push is a single pointer addition:

```c
int *a = ARENA_PUSH_STRUCT(&arena, int);
*a = 42;

int *b = ARENA_PUSH_STRUCT(&arena, int);
*b = 99;

double *c = ARENA_PUSH_STRUCT(&arena, double);
*c = 3.14159;
```

`ARENA_PUSH_STRUCT` is a macro that expands to
`arena_push_size(&arena, sizeof(int), _Alignof(int))`. It handles alignment
automatically -- the `double` allocation bumps the pointer forward to an
8-byte-aligned address, potentially wasting a few padding bytes (just like
struct padding from Lesson 04).

In JS, you would write `const a = 42; const b = 99; const c = 3.14159;` and
V8 would handle all allocation internally. In C with an arena, you have the
same simplicity -- just push and use. No `free` calls needed.

### Array allocation

Arrays work identically to single values:

```c
float *positions = ARENA_PUSH_ARRAY(&arena, 1000, float);
for (int i = 0; i < 1000; i++) positions[i] = (float)i * 0.1f;

int *indices = ARENA_PUSH_ARRAY(&arena, 500, int);
for (int i = 0; i < 500; i++) indices[i] = i * 2;
```

`ARENA_PUSH_ARRAY` is just `arena_push_size(arena, count * sizeof(Type), ...)`.
The arena does not distinguish between a single int and an array of 1000
floats -- it is all just bytes with an offset bump.

To free both arrays:

```c
arena_reset(&arena);  // both arrays freed in one call
```

In JS, both arrays would need to become unreachable for the GC to collect them.
With an arena, one reset frees everything regardless of how many allocations
were made.

### The per-frame pattern

This is the core game development pattern that makes arenas transformative:

```c
Arena frame_arena = {0};
frame_arena.min_block_size = 64 * 1024;  // 64 KB

for (int frame = 0; frame < 5; frame++) {
    arena_reset(&frame_arena);

    int num_entities = 100 + frame * 50;
    float *entity_distances = ARENA_PUSH_ARRAY(
        &frame_arena, num_entities, float);
    for (int i = 0; i < num_entities; i++)
        entity_distances[i] = (float)(i * frame);

    char *debug_str = (char *)arena_push_size(
        &frame_arena, 128, 1);
    snprintf(debug_str, 128, "Frame %d: %d entities",
             frame, num_entities);

    printf("    Frame %d: %s  (arena used: %zu bytes)\n",
           frame, debug_str, arena_total_used(&frame_arena));
}
```

Every frame:
1. Reset the arena (O(1) -- just set offset back to zero)
2. Push whatever temporary data this frame needs
3. Use it throughout the frame
4. Next iteration: reset again

No `malloc`/`free` calls. No leaks. No use-after-free. No double-free. The
entire category of memory bugs from Lessons 07-08 disappears.

In React terms, this is like if `useEffect`'s cleanup function could instantly
free all memory allocated during the render -- no GC scan, no pause, just done.

### Benchmark

The bench mode compares 5 million iterations of arena push vs malloc+free:

```c
BENCH_CASE(suite, "malloc+free", i, N) {
    void *p = malloc(64);
    *(volatile char *)p = 0;
    free(p);
}

Arena arena = {0};
arena.min_block_size = 1024 * 1024;
BENCH_CASE(suite, "arena_push", i, N) {
    void *p = arena_push_size(&arena, 64, 8);
    *(volatile char *)p = 0;
    if (i % 10000 == 9999) arena_reset(&arena);
}
```

The arena resets every 10,000 allocations to avoid running out of memory while
still demonstrating the speed advantage. Typical results show arena push at
5-20x faster than malloc+free.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Segfault on first push | Forgot to set `min_block_size` and it defaulted to 0 on an older version | The implementation defaults to 1 MB if `min_block_size` is 0; ensure you use the provided `arena.h` |
| Use-after-free after `arena_reset` | Dereferencing a pointer obtained before `arena_reset` | All pointers from before the reset are invalid; re-push after reset |
| Arena memory grows without bound | Never calling `arena_reset` in a loop | Reset at the top of each frame/iteration |
| "Why not just use the stack?" | Stack is limited (~8 MB) and tied to call scope | Arenas can hold megabytes and their lifetime is controlled by you, not the call stack |

---

## Build and run

```bash
./build-dev.sh --lesson=10 -r
./build-dev.sh --lesson=10 --bench -r   # with benchmarks
```

---

## Key takeaways

1. An arena allocator replaces malloc/free with push (bump a pointer) and reset
   (free everything). Allocation is O(1) -- a single pointer addition.

2. The per-frame pattern eliminates the entire category of memory bugs from
   Lessons 07-08. No leaks, no use-after-free, no double-free. Just push, use,
   reset.

3. Arena push is 5-20x faster than malloc+free in benchmarks because it skips
   free-list searching, metadata updates, and locking.

4. ZII (Zero Is Initialization) means `Arena arena = {0}` is a valid empty
   arena with no constructor. The first push auto-allocates the backing block.

5. Arenas are the single most important memory management pattern for game
   development, request handling, and any workload where allocations share a
   lifetime. Ryan Fleury: "When I learned about these, all of the traditional
   concerns about memory management in C went away."
