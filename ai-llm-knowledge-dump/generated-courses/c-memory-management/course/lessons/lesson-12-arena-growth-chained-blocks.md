# Lesson 12 -- Arena Growth: Chained Blocks

> **What you'll build:** A demonstration of how the arena grows by chaining multiple memory blocks together, proving that pointers to earlier allocations remain stable even after new blocks are allocated.

## Observable outcome

Run the program and you see 20 allocations with "NEW BLOCK" annotations at block boundaries, a pointer stability proof showing values survive after 100 extra allocations, a visualization of the block chain from newest to oldest, and an explanation of why chaining beats realloc.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| Chained block growth | Like `Array.push` needing a bigger backing store, except old data is never moved |
| Pointer stability | JS arrays invalidate internal buffer pointers on growth; arena chaining does not |
| `ArenaBlock` linked list | Like a linked list of `ArrayBuffer` segments |
| `arena_total_allocated` / `arena_total_used` | Like checking `.byteLength` vs actual written bytes across all segments |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_12.c` | New | Block chaining demo, pointer stability proof, chain visualization |
| `src/lib/arena.h` | Dependency | `arena_push_size` with chained-block growth |

---

## Background -- why chaining matters

A single fixed-size arena is useless for real programs. You would run out of space. The solution is chained blocks: when the current block is full, allocate a new one and link it to the chain.

In JavaScript, when an `Array` outgrows its backing store, V8 allocates a new buffer, copies everything, and frees the old one. Every reference into the old buffer is now dangling. You never see this in JS because the engine hides it behind the array abstraction.

In C with `realloc`, the same thing happens and it is your problem. The arena's chained approach avoids this entirely.

```
  realloc / JS Array growth:
  ┌───────────┐         ┌─────────────────────┐
  │ old data  │ ──copy──> │ old data | new space │
  └───────────┘         └─────────────────────┘
  (freed! all ptrs       (new buffer, new address)
   are invalid)

  Arena chained blocks:
  ┌───────────┐    ┌───────────┐    ┌───────────┐
  │ Block 2   │───>│ Block 1   │───>│ Block 0   │──> NULL
  │ (active)  │prev│ (full)    │prev│ (full)    │prev
  └───────────┘    └───────────┘    └───────────┘
  New data here.    Old data here.   Oldest data here.
  Old blocks NEVER touched.  All pointers remain valid.
```

### How arena_push_size chains a new block

When the current block cannot fit the requested allocation, `arena_push_size` executes this sequence (arena.h lines 96-124):

```c
if (needs_new) {
    size_t block_size = size + align > arena->min_block_size
                      ? size + align
                      : arena->min_block_size;

    ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
    block->prev    = arena->current;   // link to old block
    block->size    = block_size;
    block->used    = 0;
    arena->current = block;            // new block becomes active
}
```

Three things happen: (1) a new block is allocated with `malloc`, (2) it is linked to the previous block via `prev`, (3) it becomes the current active block. The old block is untouched. Its data remains at the same addresses.

---

## Code walkthrough

### Section 1: watching blocks chain (lines 28-61)

```c
Arena arena = {0};
arena.min_block_size = 256;  // tiny blocks to force chaining quickly
```

Setting `min_block_size` to 256 means each block can only hold a few 64-byte allocations before filling up. This forces the arena to chain new blocks frequently, making the behavior visible.

```c
for (int i = 0; i < 20; i++) {
    ptrs[i] = arena_push_size(&arena, 64, 8);
```

Each allocation is 64 bytes with 8-byte alignment. When the address jumps far from the previous one, the code detects a new block:

```c
ptrdiff_t diff = (uint8_t *)ptrs[i] - (uint8_t *)ptrs[i-1];
if (diff < 0 || diff > 256) {
    printf("  <-- NEW BLOCK (jumped %td bytes)", diff);
}
```

A jump larger than the block size means we landed in a completely different `malloc`'d region. After all 20 allocations, the code walks the chain:

```c
ArenaBlock *b = arena.current;
while (b) { block_count++; b = b->prev; }
```

This counts how many blocks the arena used. With 256-byte blocks and 64-byte allocations, each block holds about 3 allocations (accounting for the ArenaBlock header and alignment), so you see roughly 7 blocks.

### Section 2: pointer stability proof (lines 62-104)

This is the most important demonstration. Two values are stored before growth:

```c
int *first  = ARENA_PUSH_STRUCT(&arena, int);  *first = 11111;
int *second = ARENA_PUSH_STRUCT(&arena, int);  *second = 22222;
```

Then 100 more allocations force multiple block growths:

```c
for (int i = 0; i < 100; i++) {
    arena_push_size(&arena, 64, 8);
}
```

Afterward, `*first` still equals `11111` and `*second` still equals `22222`. The old block was never freed or moved. This is impossible with `realloc`, which copies data to a new location and invalidates all old pointers.

In TypeScript terms: imagine if you had an array where you could safely store raw pointers to elements, push millions more elements, and the old pointers would still work. That is what arena chaining gives you.

### Section 3: block chain visualization (lines 106-136)

```c
ArenaBlock *block = arena.current;
while (block) {
    void *data_start = (void *)(block + 1);
    void *data_end   = (uint8_t *)data_start + block->size;
    printf("  Block %d: %p  size=%zu  used=%zu\n", ...);
    block = block->prev;
}
```

This walks from newest to oldest, printing each block's address range, total size, and used bytes. The `(block + 1)` trick skips past the `ArenaBlock` header to reach the data region, the same technique `arena_push_size` uses internally.

### Section 4: why chaining beats realloc (lines 138-153)

The program prints a side-by-side comparison. The key difference: realloc invalidates pointers, chaining does not. The trade-off is that chained blocks are not contiguous in memory, which means you cannot iterate over all arena data as a single array. But arenas are for lifetime management, not iteration. If you need a scannable array, use a stretchy buffer (Lesson 17) or an mmap-backed arena (Lesson 13).

---

## Build and run

```bash
./build-dev.sh --lesson=12 -r
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Only one block ever allocated | `min_block_size` too large for the demo | Set it small (128-512) to observe chaining |
| "Discontiguous blocks hurt cache" | True, but each block internally is contiguous | For cache-critical arrays, use mmap-backed arena (Lesson 13) |
| Trying to iterate all arena data | No single contiguous buffer to scan | Arenas manage lifetimes, not layout; use typed arrays for scannable data |
| `arena_reset` frees extra blocks | By design -- keeps only the first block | Prevents unbounded memory growth after allocation spikes |

---

## Key takeaways

1. When a block fills up, the arena allocates a new block and links it via `prev`. Old blocks are never touched.
2. Pointer stability is the key advantage over `realloc`. Pointers into old blocks remain valid forever.
3. The block chain is newest-first: `arena.current` is the active block, and you follow `prev` to reach older blocks.
4. `arena_reset` frees all blocks except the first, resetting `used` to 0. This recycles memory without losing the initial allocation.
5. Chained blocks trade contiguity for stability. If you need contiguous memory, use the mmap-backed arena from Lesson 13.
