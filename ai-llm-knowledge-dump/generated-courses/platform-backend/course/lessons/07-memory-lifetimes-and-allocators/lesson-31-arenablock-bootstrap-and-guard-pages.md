# Lesson 31: `ArenaBlock`, Bootstrap, and Guard Pages

## Frontmatter

- Module: `07-memory-lifetimes-and-allocators`
- Lesson: `31`
- Status: Required
- Target files:
  - `src/utils/arena.h`
- Target symbols:
  - `ArenaBlock`
  - `Arena`
  - `arena_alloc`
  - `arena_init_from_block`
  - `arena_bootstrap`
  - `arena_free`

## Observable Outcome

By the end of this lesson, you should be able to explain where an arena's first block comes from, how the usable bytes are laid out, and what the guard pages on the POSIX path are trying to protect against.

## Prerequisite Bridge

Lesson 30 established the three runtime lifetimes.

Now we need to look below that policy layer and ask:

```text
what is an arena actually made of in memory?
```

The answer starts with `ArenaBlock`.

## Step 1: Read `ArenaBlock` as the Physical Chunk Header

In `src/utils/arena.h`:

```c
typedef struct ArenaBlock {
  struct ArenaBlock *prev;
  size_t size;
  size_t used;
} ArenaBlock;
```

This header lives at the beginning of one contiguous allocation.

Its job is simple:

- `prev`: link older blocks
- `size`: usable payload bytes in this block
- `used`: how many of those bytes have already been consumed

The actual payload begins immediately after the header.

That is why the code often uses `(block + 1)` to mean "first usable byte after the header."

## Step 2: Read `Arena` as the Block Stack Handle

The arena itself stores the current top block and some metadata:

```c
typedef struct Arena {
  ArenaBlock *current;
  size_t min_block_size;
  int temp_count;
  void *backing_base;
  size_t backing_total;
  size_t page_size;
  ArenaLifetimeKind lifetime_kind;
  const char *debug_name;
  ArenaStats stats;
} Arena;
```

The key field is `current`.

That is the block the next push allocation will try to use first.

So the high-level structure is:

```text
Arena -> current block -> previous block -> previous block -> ...
```

Newest block first.

## Step 3: Understand the Initial Allocation Problem

Before an arena can serve allocations, it needs its first block.

The course solves that in three layers:

1. `arena_alloc(...)` reserves raw backing memory
2. `arena_init_from_block(...)` writes the first `ArenaBlock` header into it
3. `arena_bootstrap(...)` wires the two together and tags lifetime/debug metadata

That separation keeps each function narrowly focused.

## Step 4: Walk the POSIX `arena_alloc(...)` Path

On non-Windows platforms the allocator does not just call `malloc(...)` for the first block.

It does this:

```c
long ps_long = sysconf(_SC_PAGESIZE);
size_t ps = (ps_long > 0) ? (size_t)ps_long : 4096u;

size_t raw = sizeof(ArenaBlock) + data_size;
size_t usable = ((raw + ps - 1) / ps) * ps;
size_t total = ps + usable + ps;
```

Read those three sizes carefully.

### `raw`

Header plus requested payload.

### `usable`

Rounded up to a page boundary.

### `total`

One guard page before, one usable middle region, one guard page after.

That is the full mapping size.

## Step 5: Understand the Guard-Page Layout

The mapping call is:

```c
void *map = mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
```

So the whole region starts inaccessible.

Then the code opens only the middle portion:

```c
void *start = (uint8_t *)map + ps;
if (mprotect(start, usable, PROT_READ | PROT_WRITE) != 0) {
  munmap(map, total);
  return NULL;
}
```

That means the memory layout is:

```text
[ PROT_NONE guard ][ readable+writable usable region ][ PROT_NONE guard ]
```

If code overruns the usable region badly enough to hit a guard page, it faults instead of silently corrupting nearby memory.

That is the defensive value here.

## Visual: Initial Arena Mapping

```text
map base
  |
  v
+-----------+---------------------------+-----------+
| guard     | ArenaBlock header + data  | guard     |
| PROT_NONE | PROT_READ | PROT_WRITE    | PROT_NONE |
+-----------+---------------------------+-----------+
            ^
            start returned by arena_alloc(...)
```

The returned pointer is not the start of the whole mapping.

It is the start of the middle usable region.

## Step 6: Read `arena_init_from_block(...)` as the First Header Write

After raw bytes exist, the initializer turns them into a real arena:

```c
ArenaBlock *block = (ArenaBlock *)mem;
block->prev = NULL;
block->size = total_bytes - sizeof(ArenaBlock);
block->used = 0;
arena->current = block;
arena->min_block_size = min_block_size;
```

This is where the abstract concept of an arena becomes concrete.

The very first bytes of the usable region become the first `ArenaBlock` header.

Everything after that header becomes payload space.

## Step 7: `arena_bootstrap(...)` Is the Real Public Entry Point

The bootstrap helper is the path `platform_game_props_init(...)` actually calls:

```c
mem = arena_alloc(arena, data_size);
if (!mem)
  return -1;
arena_init_from_block(arena, mem, data_size, min_block_size);
arena_debug_configure(arena, lifetime_kind, debug_name);
```

This helper matters because it standardizes three things at once:

- zero-initialize the `Arena` handle first
- allocate the initial backing region
- attach lifetime/debug metadata consistently

That avoids boilerplate at every call site.

## Step 8: Why `backing_base`, `backing_total`, and `page_size` Exist

Those fields in `Arena` are not needed for ordinary push allocations.

They exist so `arena_free(...)` can later release the original mapping correctly.

On the POSIX path, `arena_free(...)` reconstructs the usable-region pointer from:

- `backing_base`
- `page_size`

Then it skips freeing that initial block as if it were an overflow `malloc(...)` block, and finally `munmap(...)`s the whole original mapping.

That is why the arena stores these bookkeeping fields.

## Step 9: Notice the Cross-Platform Split

On Windows, the initial path falls back to a simpler `malloc(...)`-backed block.

On non-Windows, it uses `mmap(...)` plus guard pages.

The important teaching point is not the platform API difference itself.

It is that the arena API above this layer stays the same.

Higher-level code still just says:

```c
arena_bootstrap(...)
ARENA_PUSH_ZERO_STRUCT(...)
arena_reset(...)
arena_free(...)
```

That is exactly what a good platform abstraction should do.

## Practical Exercises

### Exercise 1: Explain the Three Sizes

Why does `arena_alloc(...)` compute `raw`, `usable`, and `total` instead of just one size?

Expected answer:

```text
because it must account for the header, round the usable region to page boundaries, and then add guard pages around that usable region
```

### Exercise 2: Explain the Returned Pointer

Why does `arena_alloc(...)` return `start` instead of `map` on the POSIX path?

Expected answer:

```text
because `map` points at the whole guard-page mapping, while `start` points at the middle readable+writable usable region where the ArenaBlock should live
```

### Exercise 3: Explain Bootstrap

What extra value does `arena_bootstrap(...)` provide over calling `arena_alloc(...)` directly?

Expected answer:

```text
it turns raw backing memory into a configured arena by initializing the first block and attaching lifetime/debug metadata in one standard path
```

## Common Mistakes

### Mistake 1: Thinking `ArenaBlock` is separate from the allocation payload

It is part of the same contiguous allocation. The header sits at the front.

### Mistake 2: Forgetting that the returned usable pointer is offset past the first guard page

That matters for understanding cleanup and layout.

### Mistake 3: Assuming guard pages replace all debugging needs

They help catch some overruns, but the arena also uses stats, debug names, temp checks, and fill patterns.

## Troubleshooting Your Understanding

### "Why bother page-aligning the usable size?"

Because page protection APIs work at page granularity. The middle region must align to that reality.

### "Do overflow blocks also use guard pages?"

Not in the current implementation. Overflow blocks are later allocated with plain `malloc(...)` inside `arena_push_impl(...)`.

## Recap

You now understand the initial arena construction path:

1. `arena_alloc(...)` reserves guarded backing memory
2. `arena_init_from_block(...)` writes the first block header
3. `arena_bootstrap(...)` exposes the standard setup path
4. `arena_free(...)` uses stored backing metadata to release the mapping safely

## Next Lesson

Lesson 32 moves from arena construction to the hot allocation path: alignment, overflow-safe capacity checks, overflow block growth, typed push macros, and why the debug fill modes differ between raw and zeroed allocations.
