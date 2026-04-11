# Lesson 13 -- Arena Growth: Virtual Memory (mmap)

> **What you will build:** An mmap-backed arena that reserves a large virtual
> address range upfront, commits physical pages on demand, and uses guard pages
> to catch buffer overflows -- eliminating chained blocks entirely.

## What you'll learn

- How virtual memory works at the OS level (reserve vs commit)
- How `mmap` with `PROT_NONE` reserves address space at zero physical cost
- How `mprotect` commits pages so the CPU can actually read/write them
- How guard pages catch buffer overflows with hardware enforcement
- How `arena_init_mmap` combines all of this into one call
- How `arena_free` handles both mmap and malloc arenas automatically

## Prerequisites

- Lesson 12 (arena growth with chained blocks)
- Comfort with pointers and the Arena struct from Lessons 10-12
- Linux or macOS (mmap is POSIX; Windows uses `VirtualAlloc` instead)

---

## Step 1 -- Understand what virtual memory actually is

Before you touch any code, you need to understand the split between virtual
addresses and physical RAM. In JavaScript, you never see this. When Node.js
allocates an object, V8 asks the OS for memory, the OS hands back an address,
and physical RAM is mapped behind the scenes. You have zero control over this
process.

In C on Linux, you control both layers directly. Here is the mental model:

```
  Your program's address space (virtual):

  0x0000000000000000 ─────────────────── 0x00007FFFFFFFFFFF
  │                                                        │
  │  [ code ]  [ heap ]  [ stack ]  ... vast empty gap ... │
  │                                                        │
  └────────────────────────────────────────────────────────┘

  Physical RAM (what the CPU cache and DRAM actually hold):

  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
  │ 4 KB │ 4 KB │ 4 KB │ 4 KB │ 4 KB │ 4 KB │ 4 KB │ 4 KB │
  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘

  The OS page table maps virtual addresses -> physical pages.
  Most virtual addresses map to NOTHING (no physical RAM behind them).
```

On a 64-bit system, you have 48 bits of virtual address space -- that is 256 TB.
Your machine might only have 16 GB of physical RAM. The gap is enormous. You can
claim huge chunks of virtual address space without using any real memory at all.

> **Why?** In JavaScript, `new ArrayBuffer(16 * 1024 * 1024)` immediately
> reserves 16 MB of physical memory. With mmap and `PROT_NONE`, you can reserve
> 16 MB of *addresses* without touching a single byte of physical RAM. The OS
> only maps real memory when you actually write to a page. This is called
> "demand paging."

Open `src/lessons/lesson_13.c` and add the includes and main function skeleton:

```c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "common/print_utils.h"
#include "common/bench.h"
#include "lib/arena.h"

int main(void) {
    printf("Lesson 13 -- Arena Growth: Virtual Memory (mmap)\n\n");

    long page_size = sysconf(_SC_PAGESIZE);
    printf("  Page size: %ld bytes\n", page_size);

    return 0;
}
```

Build and run:

```bash
./build-dev.sh --lesson=13 -r
```

You should see your system's page size printed (almost certainly 4096).

> **Why?** `sysconf(_SC_PAGESIZE)` asks the OS how big one page of memory is.
> Pages are the smallest unit the OS can map. Everything in virtual memory works
> in page-sized chunks. In JS, you never think about this -- the engine handles
> it. In C, you need to know the page size to align your mmap calls correctly.

---

## Step 2 -- Reserve address space with mmap + PROT_NONE

The first layer of the mmap trick is *reservation*. You ask the OS: "Give me a
range of 16 million addresses, but do not back them with physical RAM yet."

The call is `mmap(NULL, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)`.
Let's break down every argument:

| Argument | Value | Meaning |
|----------|-------|---------|
| `addr` | `NULL` | "You pick the address, OS" |
| `length` | `total` | How many bytes of address space to reserve |
| `prot` | `PROT_NONE` | No read, no write, no execute -- just reserve addresses |
| `flags` | `MAP_PRIVATE \| MAP_ANONYMOUS` | Private to this process, not backed by a file |
| `fd` | `-1` | No file descriptor (anonymous mapping) |
| `offset` | `0` | No file offset |

```
  After mmap(..., PROT_NONE, ...):

  ┌────────────────────────────────────────────────────────┐
  │                  PROT_NONE region                       │
  │         (addresses reserved, NO physical RAM)           │
  │                                                        │
  │  Any read or write here triggers SIGSEGV immediately   │
  └────────────────────────────────────────────────────────┘
  ^                                                        ^
  map                                               map + total
```

This is free. The OS just marks a range in its internal page tables. No physical
RAM is allocated. You could reserve 1 GB and it would cost effectively nothing.

> **Why?** In JavaScript terms, this is like creating a `SharedArrayBuffer` that
> has not been initialized yet. The addresses exist, but reading or writing them
> will crash. The key insight for C memory management: virtual address space is
> nearly unlimited on 64-bit systems, so reserving large ranges upfront is the
> standard technique for arena allocators.

---

## Step 3 -- Commit pages with mprotect

Now you have a huge reserved range, but you cannot use it -- any access is a
segfault. To make part of it usable, you *commit* pages with `mprotect`:

```c
void *start = (uint8_t *)map + ps;          // skip the leading guard page
mprotect(start, usable, PROT_READ | PROT_WRITE);  // NOW this region is usable
```

`mprotect` changes the permissions on a range of pages. After this call, the
middle section of your mapping can be read and written. The OS will allocate
physical RAM on demand -- the first time you write to a page, the OS traps the
fault, maps a physical page, and resumes your program. This is called a *soft
page fault* and it is invisible to your code.

```
  After mprotect on the middle section:

  ┌─────────┬──────────────────────────────────┬─────────┐
  │ GUARD   │  PROT_READ | PROT_WRITE          │ GUARD   │
  │ 4 KB    │  (committed, usable region)       │ 4 KB    │
  │ PROT_   │                                   │ PROT_   │
  │ NONE    │  Write here -> OS maps phys RAM   │ NONE    │
  └─────────┴──────────────────────────────────┴─────────┘
  ^         ^                                   ^         ^
  map    map+ps                          map+ps+usable  map+total
```

> **Why?** In JavaScript, when you write `const buf = new ArrayBuffer(1024)`,
> the engine asks the OS for 1024 bytes and they are immediately usable. With
> mmap + mprotect, you separate the "claim the addresses" step from the "make
> them usable" step. This lets you reserve far more than you need, then commit
> only what you actually use. Physical RAM is only consumed for pages you write
> to.

---

## Step 4 -- Guard pages catch overflows at hardware speed

The first and last pages of the mapping stay `PROT_NONE`. These are *guard
pages*. They exist solely to crash your program instantly if you read or write
past the usable region.

```
  Guard page behavior:

  ┌─────────┬──────────────────────────────────┬─────────┐
  │ GUARD   │ committed usable region           │ GUARD   │
  │ 4 KB    │ PROT_READ | PROT_WRITE            │ 4 KB    │
  │ PROT_   │                                    │ PROT_   │
  │ NONE    │ [ArenaBlock hdr][data data data...]│ NONE    │
  └─────────┴──────────────────────────────────┴─────────┘
  ↑ SIGSEGV                                      ↑ SIGSEGV
    on underflow                                   on overflow
```

If your arena pushes past the committed region and touches the trailing guard
page, the CPU raises `SIGSEGV` immediately. No bounds-checking code needed. No
runtime overhead during normal operation. The hardware enforces it for free.

This is strictly better than the chained-block approach from Lesson 12, where
overflow just triggers a new `malloc` call. With guard pages, overflow is a
hard crash -- you catch the bug immediately in development.

> **Why?** In JavaScript, array bounds are checked in software -- V8 inserts
> bounds checks before every array access. Guard pages do the same thing but at
> the hardware level. The CPU's memory management unit (MMU) checks permissions
> on every memory access as part of the address translation it already does.
> The cost is literally zero additional instructions.

---

## Step 5 -- Read arena_init_mmap in the library

Now look at how `arena_init_mmap` in `src/lib/arena.h` combines all three steps
(reserve, commit, guard) into one call:

```c
static inline void *arena_init_mmap(Arena *arena, size_t data_size) {
    long ps_long = sysconf(_SC_PAGESIZE);        // 1. query page size
    size_t ps = (ps_long > 0) ? (size_t)ps_long : 4096u;

    size_t raw = sizeof(ArenaBlock) + data_size;  // 2. header + user data
    size_t usable = ((raw + ps - 1) / ps) * ps;  // 3. round up to page boundary
    size_t total = ps + usable + ps;              // 4. guard + usable + guard

    // Step A: reserve entire range (PROT_NONE = no physical RAM)
    void *map = mmap(NULL, total, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) return NULL;

    // Step B: commit the usable middle section
    void *start = (uint8_t *)map + ps;
    if (mprotect(start, usable, PROT_READ | PROT_WRITE) != 0) {
        munmap(map, total);
        return NULL;
    }

    // Step C: initialize the ArenaBlock at the start of usable region
    ArenaBlock *block = (ArenaBlock *)start;
    block->prev = NULL;
    block->size = usable - sizeof(ArenaBlock);
    block->used = 0;
    arena->current = block;

    // Store backing info for arena_free
    arena->backing_base  = map;
    arena->backing_total = total;
    arena->page_size     = ps;
    arena->min_block_size = data_size;
    arena->temp_count    = 0;

    return start;
}
```

The size math at the top is worth studying:

```
  Size calculation:

  raw    = sizeof(ArenaBlock) + data_size
         = 24 + 16,777,216 = 16,777,240  (for 16 MB data_size)

  usable = round_up_to_page(raw)
         = 16,781,312  (next multiple of 4096)

  total  = page_size + usable + page_size
         = 4096 + 16,781,312 + 4096 = 16,789,504

  Layout:
  [guard 4KB][ArenaBlock hdr + 16MB usable data][guard 4KB]
```

> **Why?** The rounding formula `((raw + ps - 1) / ps) * ps` is a standard
> trick for rounding up to a multiple. In JavaScript, you would write
> `Math.ceil(raw / ps) * ps`. The C version avoids floating point entirely.
> mmap requires page-aligned sizes, so this rounding is mandatory.

---

## Step 6 -- Use the mmap-backed arena

Now use `arena_init_mmap` in your lesson code. Add this after the page size
print:

```c
size_t reserve_size = 16 * 1024 * 1024;  // 16 MB
Arena arena = {0};
void *mem = arena_init_mmap(&arena, reserve_size);
if (!mem) {
    printf("  arena_init_mmap failed!\n");
    return 1;
}

printf("  Reserved:  %zu MB virtual address space\n",
       reserve_size / (1024 * 1024));
printf("  Backing:   %p (mmap base, includes guard pages)\n",
       arena.backing_base);
printf("  Usable:    %p\n", mem);
printf("  Guard pages on both sides.\n\n");
```

Build and run:

```bash
./build-dev.sh --lesson=13 -r
```

You should see the reserved size, the backing address (the full mmap region
including guards), and the usable address (one page past the guard).

> **Why?** The difference between `backing_base` and the usable pointer is
> exactly one page size (4096 bytes on most systems). That first page is the
> leading guard page. If you ever see a crash at an address near `backing_base`,
> you know you underflowed the arena.

---

## Step 7 -- Push 1000 allocations into contiguous memory

Now push 1000 integers. Unlike the chained-block arena from Lesson 12, these
all land in a single contiguous region:

```c
int *values[1000];
for (int i = 0; i < 1000; i++) {
    values[i] = ARENA_PUSH_STRUCT(&arena, int);
    *values[i] = i * i;
}
printf("  Pushed 1000 ints.\n");
printf("  values[0]   = %d (at %p)\n", *values[0],   (void *)values[0]);
printf("  values[999] = %d (at %p)\n", *values[999], (void *)values[999]);
printf("  All in one contiguous block (no chaining needed).\n");
printf("  Used: %zu bytes of %zu available\n",
       arena_total_used(&arena), arena_total_allocated(&arena));
```

Build and run. Compare the two addresses. They should be close together (about
4000 bytes apart -- 1000 ints * 4 bytes each). No "NEW BLOCK" jumps like Lesson
12. Everything is contiguous.

```
  Chained-block arena (Lesson 12):
  Block 1: [hdr][int int int ... int]──> Block 2: [hdr][int int ...]──> ...
  Pointers JUMP between blocks. Cache misses on block boundaries.

  mmap-backed arena (this lesson):
  [guard][hdr][int int int int int int int ... all 1000 ints ...][guard]
  ALL pointers within one contiguous region. Perfect cache locality.
```

> **Why?** Cache locality matters enormously for performance. When the CPU reads
> `values[0]`, it pulls a whole cache line (64 bytes, typically 16 ints) into
> L1 cache. If `values[1]` through `values[15]` are adjacent in memory, they
> are already in cache -- zero additional memory latency. With chained blocks,
> crossing a block boundary is a cache miss. In JavaScript terms, this is like
> the difference between iterating a typed array (contiguous) vs iterating a
> Map (pointer-chasing through hash buckets).

---

## Step 8 -- Clean up with arena_free

Release the entire mapping with one call:

```c
arena_free(&arena);
printf("  arena_free -- entire region released.\n");
```

Internally, `arena_free` detects the mmap backing via `arena->backing_base != NULL`.
It first frees any overflow blocks that were malloc'd (if the arena grew past the
mmap region), then calls `munmap(arena->backing_base, arena->backing_total)` to
release the entire virtual mapping back to the OS. After this call, the Arena is
zero-initialized (ZII) again.

```
  arena_free decision tree:

  arena->backing_base != NULL ?
  ├── YES: mmap-backed
  │   1. Free any overflow blocks (malloc'd by arena_push_size)
  │   2. munmap(backing_base, backing_total)  -- releases everything
  └── NO: malloc-backed
      1. Walk the block linked list
      2. free() each block
```

> **Why?** `arena_free` handles both backends transparently. You do not need to
> track which kind of arena you have. This is the C equivalent of a polymorphic
> destructor -- one function call that does the right thing based on the arena's
> internal state.

---

## Step 9 -- Compare malloc-backed vs mmap-backed arenas

Add a comparison table so you can see the tradeoffs side by side:

```c
printf("  ┌────────────────────┬──────────────────┬──────────────────┐\n");
printf("  | Feature            | malloc-backed    | mmap-backed      |\n");
printf("  ├────────────────────┼──────────────────┼──────────────────┤\n");
printf("  | Growth             | Chained blocks   | Commit more pages|\n");
printf("  | Contiguous memory  | No (per-block)   | Yes              |\n");
printf("  | Guard pages        | No               | Yes              |\n");
printf("  | Min granularity    | Any size         | Page size (4KB)  |\n");
printf("  | Portability        | Everywhere       | POSIX/Windows    |\n");
printf("  | Typical use        | Libraries, small | Application-wide |\n");
printf("  └────────────────────┴──────────────────┴──────────────────┘\n");
```

Build and run:

```bash
./build-dev.sh --lesson=13 -r
```

The rule of thumb: use malloc-backed arenas for libraries and small allocators
that need maximum portability. Use mmap-backed arenas for application-wide
permanent and scratch arenas where contiguity and overflow detection matter.

> **Why?** In a real game engine (like the ones Casey Muratori and Ryan Fleury
> build), you typically reserve a huge mmap region at startup -- sometimes
> gigabytes of virtual address space -- and never call malloc again. The arena
> serves every allocation for the lifetime of the program. The chained-block
> approach is useful for smaller, more portable allocators that might run in a
> library embedded in someone else's application.

---

## Step 10 -- Benchmark mmap arena vs malloc arena vs raw malloc

Add the benchmark section (only runs with `--bench`):

```c
#ifdef BENCH_MODE
{
    BenchSuite suite;
    long N = 5000000;

    BENCH_SUITE(suite, "mmap arena vs malloc arena vs raw malloc") {
        /* mmap-backed arena */
        {
            size_t sz = 256 * 1024 * 1024;
            Arena arena = {0};
            void *mem = arena_init_mmap(&arena, sz);
            (void)mem;
            BENCH_CASE(suite, "mmap arena push", i, N) {
                void *p = arena_push_size(&arena, 64, 8);
                *(volatile char *)p = 0;
                if (i % 100000 == 99999) arena_reset(&arena);
            }
            BENCH_CASE_END(suite, N);
            arena_free(&arena);
        }

        /* malloc-backed arena (chained blocks) */
        {
            Arena arena = {0};
            arena.min_block_size = 1024 * 1024;
            BENCH_CASE(suite, "malloc arena push", i, N) {
                void *p = arena_push_size(&arena, 64, 8);
                *(volatile char *)p = 0;
                if (i % 100000 == 99999) arena_reset(&arena);
            }
            BENCH_CASE_END(suite, N);
            arena_free(&arena);
        }

        /* Raw malloc+free */
        BENCH_CASE(suite, "malloc+free", i, N) {
            void *p = malloc(64);
            *(volatile char *)p = 0;
            free(p);
        }
        BENCH_CASE_END(suite, N);
    }
}
#else
    bench_skip_notice("lesson_13");
#endif
```

Build and run the benchmark:

```bash
./build-dev.sh --lesson=13 --bench -r
```

The arena approaches are typically 5-20x faster than raw malloc because they
avoid per-allocation bookkeeping and syscalls. The mmap arena is slightly faster
than the malloc arena because it never needs to chain blocks.

> **Why?** The `*(volatile char *)p = 0;` write forces the compiler to actually
> use the allocation (otherwise it might optimize the whole thing away). The
> `volatile` keyword tells the compiler "this memory access has side effects,
> do not remove it." In JS, you do not need this -- the JIT is not aggressive
> enough to remove allocations. C compilers absolutely will.

---

## Build and run

```bash
./build-dev.sh --lesson=13 -r
./build-dev.sh --lesson=13 --bench -r   # with benchmarks
```

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `arena_init_mmap` returns NULL | `mmap` failed (address space limit or permissions) | Check return value; fall back to malloc-backed arena |
| SIGSEGV during normal use | Wrote past the committed region into the guard page | Arena is full -- reserve a larger region |
| "I reserved 16 MB, am I wasting RAM?" | No -- `PROT_NONE` pages have zero physical backing | Only written pages consume physical memory |
| Does not compile on Windows | `mmap`/`mprotect` are POSIX | Use `VirtualAlloc`/`VirtualProtect` (same concept, different API) |
| Using old `arena_alloc_mmap` API | Function was renamed | Use `arena_init_mmap(&arena, size)` -- it initializes the Arena in one call |

## Exercises

1. **Measure physical usage**: Use `/proc/self/statm` or `getrusage()` to print
   the resident set size before and after `arena_init_mmap`. Verify that
   reserving 256 MB costs nearly zero physical RAM.

2. **Trigger the guard page**: Deliberately write past the end of the arena.
   Run under a debugger (`gdb ./build/lesson_13`) and observe the SIGSEGV at
   the guard page address. Note how the faulting address is exactly at the
   boundary.

3. **Windows port sketch**: Look up `VirtualAlloc` with `MEM_RESERVE` /
   `MEM_COMMIT` and `VirtualProtect`. Write pseudocode for an
   `arena_init_virtualalloc` that does the same reserve/commit/guard pattern.

## Connection to your work

If you have ever written a Node.js server that handled thousands of requests
per second, you have seen GC pauses spike latency at the 99th percentile. An
mmap-backed arena eliminates this entirely -- you reserve address space at
startup, commit pages on demand, and never call malloc or free in the hot path.
This is exactly how professional game engines and high-performance C servers
manage memory.

## What's next

Lesson 14 adds TempMemory checkpoints to the arena -- the ability to save a
position, allocate freely, then roll back. This gives you scoped lifetimes
without individual `free` calls, and it is the foundation for the per-frame
scratch pattern you will use in every game loop.
