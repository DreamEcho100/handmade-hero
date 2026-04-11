# Lesson 04 -- Alignment and sizeof

> **What you will build:** A program that reveals struct padding, compares
> naive vs optimally-ordered struct layouts, demonstrates packed structs and
> `alignas`, and shows the manual alignment math used in every arena allocator.

## What you'll learn

- `sizeof` and `_Alignof` for every basic C type
- Why the compiler inserts invisible padding bytes between struct fields
- How to reorder struct fields to eliminate wasted space (largest-first rule)
- `__attribute__((packed))` for zero-padding structs (and its dangers)
- `alignas` for forcing cache-line alignment
- The alignment formula `(ptr + align - 1) & ~(align - 1)` -- the most
  important bit trick in systems programming
- Why 1 million entities with bad struct layout waste 8 MB of RAM

## Prerequisites

- Lesson 01 (memory layout)
- Lesson 03 (heap allocation -- you know about malloc's alignment guarantee)

---

## Step 1 -- Why alignment exists (the JS bridge)

In JavaScript, you have actually encountered alignment before, even if you did
not realize it:

```js
const sab = new SharedArrayBuffer(64);
const bad = new Float64Array(sab, 3); // RangeError: start offset must be
                                      // a multiple of 8
```

A `Float64Array` requires 8-byte alignment. You cannot create one at an
odd offset. This is the same problem that exists in C -- CPUs access memory
most efficiently when data sits at an address that is a multiple of the data's
natural size.

```
Memory bus (8 bytes wide on 64-bit CPU):
+--------+--------+--------+--------+
| addr 0 | addr 8 | addr 16| addr 24|
|  0-7   |  8-15  | 16-23  | 24-31  |
+--------+--------+--------+--------+

Aligned double at address 8:    ONE bus transaction
Misaligned double at address 5: TWO bus transactions (crosses 0-7 and 8-15)
```

On x86, misaligned access is slow (extra bus cycles, cache line splits). On
ARM and SPARC, it can be an **illegal operation** that causes a hardware fault
-- your program crashes. The C compiler prevents this by inserting padding.

In JavaScript, V8 handles object layout internally. You never see padding.
In C, the compiler inserts invisible **padding bytes** between struct fields
to satisfy alignment requirements, and you need to understand this to write
memory-efficient code.

> **Why?** If you are building a game engine with 1 million entities, and each
> entity struct wastes 8 bytes of padding, that is 8 MB of wasted RAM. On a
> console with 8 GB of shared memory, that matters. On cache-limited CPUs,
> extra padding means fewer entities fit in the L1 cache per frame, which
> directly impacts performance.

---

## Step 2 -- sizeof and _Alignof for every basic type

Let's start by printing the size and alignment of every fundamental C type.
This gives us the numbers we need to understand struct padding.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"

int main(void) {
    printf("=== Lesson 04 -- Alignment and sizeof ===\n");

    print_section("Basic Type Sizes and Alignments");
    printf("  %-16s  %-6s  %-6s\n", "Type", "Size", "Align");
    printf("  ----------------  ------  ------\n");
    printf("  %-16s  %-6zu  %-6zu\n", "char",      sizeof(char),      _Alignof(char));
    printf("  %-16s  %-6zu  %-6zu\n", "short",     sizeof(short),     _Alignof(short));
    printf("  %-16s  %-6zu  %-6zu\n", "int",       sizeof(int),       _Alignof(int));
    printf("  %-16s  %-6zu  %-6zu\n", "long",      sizeof(long),      _Alignof(long));
    printf("  %-16s  %-6zu  %-6zu\n", "long long", sizeof(long long), _Alignof(long long));
    printf("  %-16s  %-6zu  %-6zu\n", "float",     sizeof(float),     _Alignof(float));
    printf("  %-16s  %-6zu  %-6zu\n", "double",    sizeof(double),    _Alignof(double));
    printf("  %-16s  %-6zu  %-6zu\n", "void *",    sizeof(void *),    _Alignof(void *));
```

On a typical 64-bit Linux system, you will see:

```
  Type              Size    Align
  ----------------  ------  ------
  char              1       1
  short             2       2
  int               4       4
  long              8       8
  long long         8       8
  float             4       4
  double            8       8
  void *            8       8
```

The pattern: **alignment equals size** for fundamental types. A 4-byte `int`
must be at an address divisible by 4. An 8-byte `double` must be at an
address divisible by 8. A 1-byte `char` can be anywhere.

`sizeof` gives you the size in bytes. `_Alignof` (C11) gives you the
required alignment in bytes. In JavaScript, you never think about these numbers
because V8 handles layout. In C, they determine struct layout and array
stride.

> **Why?** These numbers are the building blocks of struct padding. Once you
> know that `int` needs 4-byte alignment and `double` needs 8-byte alignment,
> you can predict exactly where the compiler will insert padding. This is
> not guesswork -- it follows deterministic rules.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=04 -r
```

---

## Step 3 -- Struct padding: the naive layout

Now let's see what happens when you declare a struct with fields in an
unfortunate order. Define these three structs above `main`:

```c
typedef struct {
    char   a;    /* 1 byte  -- alignment: 1 */
    int    b;    /* 4 bytes -- alignment: 4 */
    char   c;    /* 1 byte  -- alignment: 1 */
    double d;    /* 8 bytes -- alignment: 8 */
} Padded;
```

Now add the helper macro to print field offsets:

```c
#define PRINT_OFFSET(type, member) \
    printf("    %-10s offset=%-4zu  size=%-4zu\n", \
           #member, offsetof(type, member), \
           sizeof(((type *)0)->member))
```

`offsetof(type, member)` is a standard C macro that returns the byte offset
of a member within a struct. `sizeof(((type *)0)->member)` gives the size
of that member. The `#member` stringizes the member name for printing.

Print the Padded struct's layout:

```c
    print_section("Struct Padding");
    printf("  Padded struct (naive field order):\n");
    printf("    sizeof(Padded)   = %zu\n", sizeof(Padded));
    printf("    _Alignof(Padded) = %zu\n", _Alignof(Padded));
    PRINT_OFFSET(Padded, a);
    PRINT_OFFSET(Padded, b);
    PRINT_OFFSET(Padded, c);
    PRINT_OFFSET(Padded, d);
```

You will see:

```
  Padded struct (naive field order):
    sizeof(Padded)   = 24
    _Alignof(Padded) = 8
    a          offset=0     size=1
    b          offset=4     size=4
    c          offset=8     size=1
    d          offset=16    size=8
```

Wait -- the actual data is `1 + 4 + 1 + 8 = 14` bytes, but the struct is 24
bytes! Where did 10 bytes go? They are **padding**:

```
Padded struct layout (24 bytes total):

Byte:  0    1  2  3    4  5  6  7    8    9 10 11 12 13 14 15   16 17 18 19 20 21 22 23
       +----+--------+------------+----+-----------------------+------------------------+
       | a  | PAD*3  |     b      | c  |       PAD*7           |          d             |
       |1B  | 3 pad  |    4B      |1B  |    7 bytes padding    |         8B             |
       +----+--------+------------+----+-----------------------+------------------------+

Why the padding:
- After 'a' (offset 0, 1 byte): 'b' needs 4-byte alignment.
  Next offset divisible by 4 is offset 4. Insert 3 padding bytes.
- After 'c' (offset 8, 1 byte): 'd' needs 8-byte alignment.
  Next offset divisible by 8 is offset 16. Insert 7 padding bytes.
- After 'd' (offset 16, 8 bytes): end is offset 24.
  Struct alignment is 8 (largest member). 24 is divisible by 8. No tail padding needed.
```

10 bytes wasted out of 24. That is 42% overhead.

> **Why?** In JavaScript, `{ a: 'x', b: 42, c: 'y', d: 3.14 }` -- V8
> decides the layout, and you never care. In C, the order you declare fields
> directly determines how much memory the struct uses. Declaring fields in the
> wrong order can nearly double the struct's size. With 1 million instances,
> that is the difference between 14 MB and 24 MB.

---

## Step 4 -- The compact layout: sort by alignment

Same four fields, different order:

```c
typedef struct {
    double d;    /* 8 bytes -- alignment: 8 */
    int    b;    /* 4 bytes -- alignment: 4 */
    char   a;    /* 1 byte  -- alignment: 1 */
    char   c;    /* 1 byte  -- alignment: 1 */
} Compact;
```

Print it:

```c
    printf("\n  Compact struct (sorted by alignment):\n");
    printf("    sizeof(Compact)  = %zu\n", sizeof(Compact));
    PRINT_OFFSET(Compact, d);
    PRINT_OFFSET(Compact, b);
    PRINT_OFFSET(Compact, a);
    PRINT_OFFSET(Compact, c);

    printf("\n  Savings: %zu bytes per instance (%zu vs %zu)\n",
           sizeof(Padded) - sizeof(Compact),
           sizeof(Padded), sizeof(Compact));
    printf("  For 1 million instances: %zu KB saved!\n",
           (sizeof(Padded) - sizeof(Compact)) * 1000000 / 1024);
```

Output:

```
  Compact struct (sorted by alignment):
    sizeof(Compact)  = 16
    d          offset=0     size=8
    b          offset=8     size=4
    a          offset=12    size=1
    c          offset=13    size=1

  Savings: 8 bytes per instance (24 vs 16)
  For 1 million instances: 7812 KB saved!
```

The layout:

```
Compact struct layout (16 bytes total):

Byte:  0  1  2  3  4  5  6  7    8  9 10 11   12   13   14 15
       +------------------------+------------+----+----+------+
       |          d             |     b      | a  | c  |PAD*2 |
       |         8B             |    4B      |1B  |1B  | 2B   |
       +------------------------+------------+----+----+------+

Why it works:
- 'd' (8B, align 8) at offset 0. Perfect.
- 'b' (4B, align 4) at offset 8. 8 is divisible by 4. No padding.
- 'a' (1B, align 1) at offset 12. No padding needed.
- 'c' (1B, align 1) at offset 13. No padding needed.
- Tail padding: 2 bytes to make total size (16) divisible by struct alignment (8).
```

Only 2 bytes of padding, down from 10. The struct shrunk from 24 to 16 bytes.

**The rule of thumb:** Sort struct fields from largest alignment to smallest.
This minimizes internal padding because each field naturally falls on a
properly aligned offset after the previous (larger) field.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=04 -r
```

---

## Step 5 -- Packed structs: zero padding, zero safety

`__attribute__((packed))` tells the compiler to remove ALL padding:

```c
typedef struct __attribute__((packed)) {
    char   a;
    int    b;
    char   c;
    double d;
} Packed;
```

Print it:

```c
    print_section("Packed Struct (No Padding)");
    printf("  sizeof(Packed) = %zu  (sum of fields = %zu)\n",
           sizeof(Packed),
           sizeof(char) + sizeof(int) + sizeof(char) + sizeof(double));
    PRINT_OFFSET(Packed, a);
    PRINT_OFFSET(Packed, b);
    PRINT_OFFSET(Packed, c);
    PRINT_OFFSET(Packed, d);
    printf("\n  WARNING: Packed structs cause misaligned access!\n");
    printf("  Use only for: wire protocols, file formats, hardware regs.\n");
```

Output:

```
  sizeof(Packed) = 14  (sum of fields = 14)
    a          offset=0     size=1
    b          offset=1     size=4
    c          offset=5     size=1
    d          offset=6     size=8
```

The struct is exactly 14 bytes -- the sum of its fields, no padding at all.
But look at the offsets:

```
Packed struct layout (14 bytes, NO padding):

Byte:  0    1  2  3  4    5    6  7  8  9 10 11 12 13
       +----+-----------+----+------------------------+
       | a  |     b     | c  |          d             |
       |1B  |    4B     |1B  |         8B             |
       +----+-----------+----+------------------------+
            ^                 ^
            |                 |
       b at offset 1:     d at offset 6:
       NOT 4-byte         NOT 8-byte
       aligned!           aligned!
```

Field `b` (an `int`) is at offset 1, which is NOT a multiple of 4. Field `d`
(a `double`) is at offset 6, which is NOT a multiple of 8. Accessing these
fields is:

- **On x86**: Slow. The CPU must do extra work to split the access across
  cache line boundaries.
- **On ARM/SPARC**: A hardware fault. Your program crashes.

> **Why?** Packed structs exist for one purpose: when the byte layout must
> match an external specification exactly. Network protocol headers, binary
> file formats, hardware register maps -- these have fixed layouts that do
> not care about C alignment rules. For everything else, use the Compact
> approach (reorder fields) instead of packing.

---

## Step 6 -- alignas for cache-line alignment

Sometimes you need MORE alignment than the compiler would naturally give you.
The most common case is cache-line alignment for multithreaded code:

```c
typedef struct {
    alignas(64) char cache_line_data[64];
    int count;
} CacheAligned;
```

`alignas(64)` forces the first field to a 64-byte boundary. Since the first
field determines the struct's starting alignment, the entire struct starts at a
64-byte boundary.

```c
    print_section("Explicit Alignment (alignas)");
    CacheAligned ca;
    printf("  sizeof(CacheAligned) = %zu\n", sizeof(CacheAligned));
    printf("  _Alignof(CacheAligned) = %zu\n", _Alignof(CacheAligned));
    printf("  &ca.cache_line_data = %p  (mod 64 = %zu)\n",
           (void *)ca.cache_line_data,
           (uintptr_t)ca.cache_line_data % 64);
```

Output:

```
  sizeof(CacheAligned)   = 128
  _Alignof(CacheAligned) = 64
  &ca.cache_line_data = 0x7fffffffe000  (mod 64 = 0)
```

The struct is 128 bytes (64 for the data array + 4 for count + 60 tail
padding to reach the next 64-byte boundary). It starts at an address
divisible by 64.

Why 64 bytes? Because a CPU cache line is typically 64 bytes on modern x86
processors. In multithreaded code, if two threads write to different variables
that happen to share the same cache line, the CPU must bounce the cache line
between cores on every write. This is called **false sharing** and it
destroys performance. Aligning each thread's data to a 64-byte boundary
guarantees they are on separate cache lines.

> **Why?** If you are building a multithreaded game engine where the physics
> thread and the rendering thread each maintain their own counters, putting
> those counters on the same cache line causes the CPUs to fight over it. A
> simple `alignas(64)` on each thread's data solves this. In JavaScript,
> `SharedArrayBuffer` with `Atomics` has the same underlying problem, but
> the engine handles cache alignment for you.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=04 -r
```

---

## Step 7 -- Manual alignment math: the bit trick

This is the single most important formula in systems programming. Every arena
allocator, every custom memory manager, and most memory-mapped I/O code uses
it:

```c
uintptr_t mask = align - 1;
uintptr_t aligned = (raw + mask) & ~mask;
```

This rounds `raw` UP to the next multiple of `align` (which must be a power
of 2).

Add this demo:

```c
    print_section("Manual Alignment Math");
    printf("  Formula: aligned = (ptr + align - 1) & ~(align - 1)\n\n");

    uintptr_t raw = 0x1003;
    uintptr_t align = 16;
    uintptr_t mask = align - 1;
    uintptr_t aligned = (raw + mask) & ~mask;
    printf("  Example: raw=0x%lx  align=%zu  -> aligned=0x%lx\n",
           (unsigned long)raw, (size_t)align, (unsigned long)aligned);
    printf("  Wasted: %zu bytes of padding\n", (size_t)(aligned - raw));
```

Let's trace through the math step by step:

```
raw   = 0x1003 = 0001 0000 0000 0011 (binary)
align = 16     = 0001 0000 (binary)
mask  = 15     = 0000 1111 (binary)
~mask          = 1111 0000 (binary)

Step 1: raw + mask = 0x1003 + 0x000F = 0x1012
        0001 0000 0001 0010

Step 2: (raw + mask) & ~mask
        0001 0000 0001 0010
      & 1111 1111 1111 0000
        -------------------
        0001 0000 0001 0000 = 0x1010

Result: aligned = 0x1010 (the next multiple of 16 after 0x1003)
Wasted: 0x1010 - 0x1003 = 13 bytes of padding
```

How it works:
1. Adding `mask` (align - 1) pushes the value past the next alignment
   boundary (unless it was already aligned).
2. ANDing with `~mask` clears the low bits, snapping down to the alignment
   boundary.

The result: the smallest multiple of `align` that is >= `raw`.

```
Number line showing alignment to 16:

... 0x0FF0  0x1000  0x1010  0x1020  0x1030 ...
      |       |       |       |       |
           aligned   aligned  aligned
           boundary  boundary boundary

raw = 0x1003 falls between 0x1000 and 0x1010
Rounded UP to 0x1010
```

> **Why?** In JavaScript, you never manually align pointers -- the engine
> handles it. In C, this formula appears every time you build a custom
> allocator. In the arena allocator you will build in Lesson 13, every
> allocation call uses this formula to ensure the returned pointer is properly
> aligned. It also appears in GPU buffer uploads, SIMD data alignment, and
> memory-mapped hardware register access. Memorize it.

---

## Step 8 -- malloc alignment and over-aligned allocations

The source shows that regular `malloc` returns 16-byte-aligned memory:

```c
    print_section("Alignment from malloc");
    for (int i = 0; i < 5; i++) {
        void *p = malloc(1);
        printf("  malloc(1) = %p  (mod 16 = %zu)\n", p, (uintptr_t)p % 16);
        free(p);
    }
    printf("\n  malloc returns memory aligned for any standard type.\n");
    printf("  Typically 16-byte aligned on 64-bit systems.\n");
```

All five addresses should show `mod 16 = 0`. This 16-byte guarantee means
you can safely cast a `malloc`'d pointer to any standard type:

```c
void *buf = malloc(100);
double *d = (double *)buf;   /* OK: 16 >= 8 */
int *i = (int *)buf;         /* OK: 16 >= 4 */
```

But what if you need 64-byte alignment for SIMD or cache lines? Regular
`malloc` cannot guarantee that. Use `aligned_alloc`:

```c
/* aligned_alloc(alignment, size) -- C11 */
void *p = aligned_alloc(64, 1024);
/* p is guaranteed to be 64-byte aligned */
free(p);  /* free works normally */
```

Or `posix_memalign` on older systems:

```c
void *p;
posix_memalign(&p, 64, 1024);
free(p);
```

### Build and run checkpoint

```bash
./build-dev.sh --lesson=04 -r
```

With alignment benchmark:

```bash
./build-dev.sh --lesson=04 --bench -r
```

The benchmark shows aligned vs misaligned `int` access. On x86, misaligned
access works but is measurably slower due to cache line splits.

---

## Build and run

Normal run:

```bash
./build-dev.sh --lesson=04 -r
```

With benchmarks (aligned vs misaligned int access):

```bash
./build-dev.sh --lesson=04 --bench -r
```

---

## Expected output

```
=== Basic Type Sizes and Alignments ===
  Type              Size    Align
  ----------------  ------  ------
  char              1       1
  short             2       2
  int               4       4
  long              8       8
  double            8       8
  void *            8       8

=== Struct Padding ===
  Padded struct (naive field order):
    sizeof(Padded)   = 24
    a          offset=0     size=1
    b          offset=4     size=4
    c          offset=8     size=1
    d          offset=16    size=8

  Compact struct (sorted by alignment):
    sizeof(Compact)  = 16
    d          offset=0     size=8
    b          offset=8     size=4
    a          offset=12    size=1
    c          offset=13    size=1

  Savings: 8 bytes per instance (24 vs 16)
  For 1 million instances: 7812 KB saved!

=== Packed Struct (No Padding) ===
  sizeof(Packed) = 14  (sum of fields = 14)
    a          offset=0     size=1
    b          offset=1     size=4
    c          offset=5     size=1
    d          offset=6     size=8
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `sizeof(MyStruct)` is larger than the sum of its fields | Padding between fields and at the end (tail padding) | Reorder fields largest-alignment-first |
| Crash on ARM when reading a packed struct field | Misaligned access is a hardware fault on strict-alignment CPUs | Use `memcpy` to read fields from packed structs portably |
| `_Alignof` not recognized | Compiling as C89/C90 | Use C11 mode (`-std=c11`) or the GCC extension `__alignof__` |
| `alignas(64)` not working with `malloc` | `malloc` only guarantees `_Alignof(max_align_t)` alignment | Use `aligned_alloc(64, size)` or `posix_memalign` for over-aligned heap allocations |
| Alignment formula gives wrong result | `align` is not a power of 2 | The bit trick ONLY works for power-of-2 alignments. All hardware alignments are powers of 2 |

---

## Exercises

1. **Design a game entity struct.** An entity has: `uint8_t type`, `uint8_t
   flags`, `float x`, `float y`, `float z`, `double health`, `uint32_t id`.
   Write it in naive order and compute `sizeof`. Then reorder for minimal
   padding and compute again. How many bytes did you save?

2. **Prove tail padding exists.** Create `struct { double d; char c; }`. The
   sum of fields is 9 bytes, but `sizeof` will be 16 (8 bytes of tail padding
   to maintain 8-byte struct alignment). Why? Because an array of these structs
   needs every element to be 8-byte aligned.

3. **Test packed struct portability.** Write a packed struct with `{char, int,
   double}`. On x86, reading the `double` field "works" but is slow. If you
   have access to an ARM system (Raspberry Pi, phone via SSH), test it there.
   It will crash.

4. **Implement aligned_alloc manually.** Allocate `size + align` bytes with
   `malloc`, apply the alignment formula to get an aligned pointer within the
   buffer, and store the original pointer just before the aligned pointer (so
   you can `free` it later). This is how `aligned_alloc` works internally.

5. **Benchmark struct iteration.** Create an array of 10 million Padded
   structs and an array of 10 million Compact structs. Iterate over each
   array summing a field. The Compact version should be faster because more
   structs fit in each cache line.

---

## Connection to your work

In game engines:

- **Entity structs are the most common hot data.** You might have millions of
  particles, tens of thousands of entities, or thousands of bones. Saving 8
  bytes per struct through field reordering saves megabytes.

- **Cache-line alignment for threads.** Physics, audio, rendering -- each
  subsystem running on its own thread needs its shared counters and flags on
  separate cache lines. `alignas(64)` prevents false sharing.

- **The alignment formula is in every allocator.** When you build the arena
  allocator in Lesson 13, the `(ptr + align - 1) & ~(align - 1)` formula will
  appear in the core allocation path. It is also in every GPU buffer upload
  (vertex buffers need 4-byte alignment, uniform buffers need 256-byte
  alignment).

- **Packed structs for save files and network packets.** When you serialize
  game state to disk or send it over the network, the byte layout must be
  exact. Packed structs (plus explicit endianness handling) give you that
  control.

---

## What's next

In Lesson 05, we go beneath the allocator and learn about **virtual memory
and pages** -- the OS mechanism that makes all of this possible. You will use
`mmap` to allocate memory directly from the OS, learn the reserve-then-commit
pattern used by production arena allocators, and build guard pages that catch
buffer overflows instantly.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_04.c`. It contains the
type table, all three struct definitions, the alignment math demo, and the
aligned vs misaligned benchmark.
