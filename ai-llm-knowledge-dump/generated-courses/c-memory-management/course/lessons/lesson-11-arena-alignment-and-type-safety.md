# Lesson 11 -- Arena Alignment and Type Safety

> **What you'll build:** An exploration of hardware alignment requirements, the exact bit-manipulation math inside `arena_push_size`, and type-safe macros that make alignment bugs impossible.

## Observable outcome

Run the program and you see alignment requirements for every test type, proof that ARENA_PUSH_STRUCT returns correctly aligned pointers, padding gaps between interleaved char/double allocations, and zero-initialized structs via ARENA_PUSH_ZERO_STRUCT.

## New concepts

| Concept | JS/TS analogy |
|---------|---------------|
| `_Alignof(Type)` | No equivalent -- JS engines hide alignment from you entirely |
| Alignment padding | Like object property padding in V8's hidden classes, but explicit |
| `ARENA_PUSH_STRUCT` macro | Like a generic `new T()` that picks the right alignment automatically |
| `ARENA_PUSH_ZERO_STRUCT` | Like `Object.create(null)` -- guaranteed clean state, no garbage values |

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_11.c` | New | Alignment demo, interleaved types, padding visualization, zero-init |
| `src/lib/arena.h` | Dependency | `arena_push_size` with alignment, type-safe macros |

---

## Background -- why alignment matters

In JavaScript, you never think about where a number sits in memory. The engine handles it. In C, the CPU imposes rules: a 4-byte `int` must start at an address divisible by 4. An 8-byte `double` must start at an address divisible by 8. Break these rules and you get:

- **x86-64**: it works, but reads split across two cache lines (slower)
- **ARM / RISC-V**: hardware trap -- `SIGBUS` -- the program crashes
- **UBSan**: reports `runtime error: misaligned address`

```
  JS world:   const x = 3.14;  // V8 handles alignment invisibly

  C world:    double *d = arena_push_size(&arena, 8, ???);
              // YOU must specify alignment, or pay the price
```

### The alignment formula (line-by-line)

This is the core of `arena_push_size` in `arena.h`. Every allocation passes through this code.

```c
uintptr_t base = (uintptr_t)(arena->current + 1) + arena->current->used;
```

`arena->current` points to the `ArenaBlock` header. `(arena->current + 1)` skips past the header to the data region. Adding `used` gives the current write position as a raw integer address.

```c
uintptr_t mask = align - 1;
```

Because `align` is always a power of 2 (1, 2, 4, 8, 16...), subtracting 1 produces a bitmask that isolates the low bits. For align=8: `mask = 0b0111`.

```c
if (base & mask)
    alignment_offset = align - (base & mask);
```

`base & mask` extracts the remainder when dividing by `align`. If nonzero, the address is misaligned. The padding needed is `align - remainder`.

```
Example: base = 0x...0005, align = 8

  mask = 7 = 0b0111
  base & mask = 5 & 7 = 5      (5 bytes past an 8-byte boundary)
  alignment_offset = 8 - 5 = 3  (need 3 padding bytes)

  Memory:
  [ used data... | X X X X X ]  <-- offset 5 (misaligned for 8)
                        ↑
          3 padding bytes inserted
                        ↓
  [ used data... | X X X X X | P P P | new data... ]
                                       ↑
                             offset 8 (aligned!)
```

The `align_forward` utility in `arena.h` does the same thing in one expression:

```c
static inline size_t align_forward(size_t offset, size_t align) {
    return (offset + align - 1) & ~(align - 1);
}
```

The `& ~(align - 1)` clears the low bits, rounding up to the next multiple of `align`. This is the standard power-of-two alignment trick used everywhere in systems programming.

### Type-safe macros

Manually passing `_Alignof` on every call is error-prone. The macros automate it:

```c
#define ARENA_PUSH_STRUCT(arena, Type) \
    ((Type *)arena_push_size((arena), sizeof(Type), _Alignof(Type)))

#define ARENA_PUSH_ARRAY(arena, Count, Type) \
    ((Type *)arena_push_size((arena), (size_t)(Count) * sizeof(Type), \
                             _Alignof(Type)))
```

The cast to `(Type *)` gives you a typed pointer. `_Alignof(Type)` is resolved at compile time. The macro handles size, alignment, and casting in one call. In TypeScript terms, this is like a generic function `push<T>()` that infers everything from the type parameter.

---

## Code walkthrough

### Section 1: alignment requirements (lines 31-39)

```c
printf("  %-12s  size=%-4zu  align=%-4zu\n", "char",   sizeof(char),   _Alignof(char));
printf("  %-12s  size=%-4zu  align=%-4zu\n", "Mixed",  sizeof(Mixed),  _Alignof(Mixed));
```

The `Mixed` struct is `{ char c; double d; int i; }`. Its alignment is 8 (because it contains a `double`), and its size is 24 -- not 13. The compiler inserts 7 bytes of internal padding after `c` so that `d` sits on an 8-byte boundary. This is the same principle the arena applies externally between allocations.

```
  Mixed struct layout (24 bytes):
  [ c ][ pad x7 ][ d (8 bytes) ][ i (4 bytes) ][ pad x4 ]
   1B    7B         8B              4B             4B (for array alignment)
```

### Section 2: ARENA_PUSH_STRUCT proof (lines 42-66)

```c
Tiny   *t = ARENA_PUSH_STRUCT(&arena, Tiny);
Point  *p = ARENA_PUSH_STRUCT(&arena, Point);
PointD *d = ARENA_PUSH_STRUCT(&arena, PointD);
Mixed  *m = ARENA_PUSH_STRUCT(&arena, Mixed);
```

Each pointer is verified with `(uintptr_t)ptr % _Alignof(Type)`. If the result is 0, the pointer is correctly aligned. The program asserts all four pass. This is the proof that the arena's alignment math works for every type.

### Section 3: interleaved allocations show padding (lines 69-88)

```c
char   *c  = (char *)arena_push_size(&arena, 1, 1);
double *d  = (double *)arena_push_size(&arena, sizeof(double), _Alignof(double));
```

After a 1-byte char, the next double needs 8-byte alignment. That creates up to 7 bytes of padding. The output shows address gaps between allocations, making the padding concrete. The total `arena_total_used` includes this padding.

### Section 4: zero-initialized allocation (lines 91-102)

```c
Point *p = ARENA_PUSH_ZERO_STRUCT(&arena, Point);
// p->x == 0, p->y == 0 -- guaranteed
```

`ARENA_PUSH_ZERO_STRUCT` calls `arena_push_zero`, which does `memset(p, 0, size)` after the push. In JS, every object starts with defined properties. In C, uninitialized memory contains garbage. Use zero-init for structs with pointer fields (NULL) or count fields (0) where garbage would cause crashes.

### Section 5: why alignment matters (lines 104-112)

The program prints a summary of what happens without alignment on different architectures. The takeaway: never call `arena_push_size` with `align=1` for typed data. Always use `ARENA_PUSH_STRUCT` or pass `_Alignof(Type)`.

---

## Build and run

```bash
./build-dev.sh --lesson=11 -r
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `SIGBUS` on ARM/RISC-V | `arena_push_size` called with `align=1` for a `double*` | Always use `ARENA_PUSH_STRUCT` or pass `_Alignof(Type)` |
| Mysterious data corruption | Reading a `double` from a misaligned address (undefined behavior) | Compile with `-fsanitize=undefined` to catch immediately |
| "Arena wastes too much memory" | Interleaving small and large types causes padding gaps | Group allocations by alignment: push all chars, then all doubles |
| Zero-init overhead | Calling `ARENA_PUSH_ZERO_ARRAY` on a million-element array | Only zero-init when needed; use `ARENA_PUSH_ARRAY` for raw buffers |

---

## Key takeaways

1. Every C type has an alignment requirement. The CPU enforces it, sometimes violently.
2. The arena's alignment math uses `(base & mask)` to detect misalignment and inserts padding bytes.
3. `ARENA_PUSH_STRUCT` and `ARENA_PUSH_ARRAY` automate both size and alignment -- use them always.
4. `ARENA_PUSH_ZERO_STRUCT` is the C equivalent of "initialize to safe defaults" -- use it for structs with pointers or counts.
5. Padding is the cost of alignment. It is wasted space but it prevents crashes and undefined behavior.
