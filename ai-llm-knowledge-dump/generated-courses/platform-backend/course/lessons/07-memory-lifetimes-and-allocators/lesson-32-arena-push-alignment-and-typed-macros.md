# Lesson 32: `arena_push`, Alignment, and Typed Macros

## Frontmatter

- Module: `07-memory-lifetimes-and-allocators`
- Lesson: `32`
- Status: Required
- Target files:
  - `src/utils/arena.h`
- Target symbols:
  - `arena_push_impl`
  - `arena_push_size`
  - `arena_push_zero`
  - `ARENA_PUSH_STRUCT`
  - `ARENA_PUSH_ARRAY`
  - `ARENA_PUSH_ZERO_STRUCT`
  - `ARENA_PUSH_ZERO_ARRAY`
  - `arena_push_string`

## Observable Outcome

By the end of this lesson, you should be able to trace one push allocation from alignment calculation to returned pointer, and explain why the typed push macros are safer and clearer than hand-written size math at call sites.

## Prerequisite Bridge

Lesson 31 built the arena's first block.

Now we can inspect the allocation path that runs every time the runtime says:

```c
ARENA_PUSH_ZERO_STRUCT(level, GameLevelState)
```

That path lives in `arena_push_impl(...)`.

## Step 1: Know the Two Public Allocation Modes

The arena exposes two main byte-level entry points:

```c
static inline void *arena_push_size(Arena *arena, size_t size, size_t align)
static inline void *arena_push_zero(Arena *arena, size_t size, size_t align)
```

Both call the shared engine:

```c
arena_push_impl(arena, size, align, mode)
```

The difference is only initialization mode:

- raw path: unspecified contents, debug-filled with `0xCD` in debug builds
- zero path: explicitly zeroed

That split is deliberate. It keeps the actual allocator logic in one place.

## Step 2: Read the First Assertion as a Contract

At the top of `arena_push_impl(...)`:

```c
assert(align > 0 && (align & (align - 1)) == 0 &&
       "align must be a power of two");
```

This is a very common low-level contract.

Why power-of-two alignment?

Because the later mask logic only works correctly under that assumption.

If you do not understand that contract, the rest of the function looks more magical than it really is.

## Step 3: Walk the Alignment Calculation Slowly

If the arena already has a current block, the function computes the raw next byte:

```c
uintptr_t base = (uintptr_t)((arena->current + 1) + arena->current->used);
uintptr_t mask = align - 1;
```

Then it checks whether `base` is already aligned:

```c
if (base & mask) {
  alignment_offset = align - (base & mask);
}
```

That means:

```text
if base is already on an alignment boundary -> offset = 0
otherwise -> push forward just enough bytes to reach the next boundary
```

## Worked Example: 16-Byte Alignment

Suppose:

- `base = 0x100A`
- `align = 16`
- `mask = 0xF`

Then:

```text
base & mask = 0xA
alignment_offset = 16 - 10 = 6
```

So the allocator burns 6 padding bytes and returns `0x1010`.

That is the entire alignment idea.

## Step 4: Understand the Overflow-Safe Capacity Check

The allocator does not write this:

```text
used + offset + size > capacity
```

directly, because that can overflow `size_t`.

Instead it uses a safer rearrangement:

```c
size_t needed = alignment_offset + size;
int needs_new_block = !arena->current || needed > arena->current->size ||
                      arena->current->used > arena->current->size - needed;
```

This is worth studying carefully.

It avoids accidental wraparound in the check itself.

That is a real engineering detail, not academic polish.

## Step 5: Read the Overflow Path as "Allocate a New Top Block"

If the current block cannot satisfy the request, the allocator grows by linking a new block:

```c
size_t block_size = size > arena->min_block_size ? size : arena->min_block_size;
ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
block->prev = arena->current;
block->size = block_size;
block->used = 0;
arena->current = block;
```

That means the arena is not a single immutable buffer.

It is a stack of blocks, newest first.

The first block may come from guarded bootstrap memory, while later overflow blocks come from `malloc(...)`.

## Step 6: Watch the Result Pointer Get Computed

After padding is consumed, the allocator returns:

```c
void *result = (uint8_t *)(arena->current + 1) + arena->current->used;
arena->current->used += size;
```

This is the simplest and most important arena operation in the whole module:

```text
start at first usable byte after the header
skip however many bytes are already used
return that address
advance used by the requested size
```

That is why arenas are fast on the hot path. There is no free-list search here.

## Step 7: Notice the Stats Are Updated Along the Way

The function also updates:

- current used bytes
- peak used bytes
- total requested bytes
- total alignment padding
- allocation count
- overflow block count when needed

This matters because allocator observability is part of the course, not an afterthought.

The runtime wants memory usage to be explainable.

## Step 8: Understand the Two Initialization Modes

At the end of the function:

```c
if (mode == ARENA_INIT_ZERO) {
  memset(result, 0, size);
}
#ifndef NDEBUG
else {
  memset(result, ARENA_DEBUG_FILL_ALLOC, size);
}
#endif
```

So zeroed allocations are deterministic.

Raw allocations are debug-poisoned in debug builds to catch uninitialized reads more aggressively.

That is why the API exposes both zeroed and raw push helpers.

## Step 9: Typed Macros Remove Repeated Size Math

The byte-level functions are not what most game code should call directly.

Instead the code usually uses:

```c
#define ARENA_PUSH_STRUCT(arena, Type) ...
#define ARENA_PUSH_ARRAY(arena, Count, Type) ...
#define ARENA_PUSH_ZERO_STRUCT(arena, Type) ...
#define ARENA_PUSH_ZERO_ARRAY(arena, Count, Type) ...
```

These macros buy you three things:

### Correct size

No repeated `sizeof(Type)` arithmetic at every call site.

### Correct alignment

They pass `_Alignof(Type)` automatically.

### Better readability

`ARENA_PUSH_ZERO_STRUCT(level, GameLevelState)` says what is being allocated much more clearly than a raw byte count.

## Step 10: `arena_push_string(...)` Shows a Nice Specialized Wrapper

The string helper is:

```c
size_t len = strlen(str) + 1;
char *copy = (char *)arena_push_size(arena, len, 1);
if (copy)
  memcpy(copy, str, len);
```

That is a good example of building small convenience helpers on top of the general arena primitive.

It keeps call sites cleaner while still exposing the real lifetime choice through the arena argument.

## Practical Exercises

### Exercise 1: Explain Alignment

Why does the allocator compute `alignment_offset` before writing the result pointer?

Expected answer:

```text
so the returned pointer lands on the required alignment boundary for the requested type or data
```

### Exercise 2: Explain the Macros

Why is `ARENA_PUSH_ZERO_STRUCT(level, GameLevelState)` better than manually writing a byte count?

Expected answer:

```text
because it automatically uses the right size and alignment for the type and makes the allocation intent readable at the call site
```

### Exercise 3: Explain Overflow Blocks

When does `arena_push_impl(...)` allocate a new block?

Expected answer:

```text
when the current block is missing or does not have enough remaining capacity for the requested size plus alignment padding
```

## Common Mistakes

### Mistake 1: Thinking alignment is optional detail

Incorrect alignment can break code even when the byte count looks right.

### Mistake 2: Assuming the arena can never grow beyond its first block

It can. Later overflow blocks are linked through `prev`.

### Mistake 3: Using raw pushes when zeroed memory is actually required

That can leave important fields uninitialized.

## Troubleshooting Your Understanding

### "Why not always zero memory?"

Because the API wants both options. Some callers want explicit zeroing; others want raw storage and, in debug builds, aggressive poison values that expose accidental reads.

### "Why does the new overflow block size use `max(requested_size, min_block_size)`?"

So tiny allocations do not create tiny overflow blocks forever, while very large one-off allocations can still fit.

## Recap

You now understand the hot push path:

1. validate alignment contract
2. compute padding
3. perform an overflow-safe capacity check
4. allocate a new top block if needed
5. return the next aligned byte range
6. prefer typed macros at call sites

## Next Lesson

Lesson 33 turns the scratch arena into a rewindable tool through `TempMemory`, nested checkpoints, and the per-frame begin/end pattern that both platform backends enforce.
