# Lesson 05 -- Virtual Memory and Pages

> **What you will build:** A program that uses `mmap`/`munmap` to allocate
> anonymous memory directly from the OS, demonstrates the reserve-then-commit
> pattern used by production arena allocators, and constructs guard pages that
> catch buffer overflows at the exact instruction that causes them.

## What you'll learn

- Every pointer in C is a virtual address, translated to physical RAM by the
  CPU's MMU
- Pages (4 KB) are the smallest unit of memory the OS manages
- `mmap` with `MAP_ANONYMOUS` for OS-level memory allocation (page-aligned,
  zero-initialized)
- The reserve-vs-commit pattern: `PROT_NONE` reserves address space, `mprotect`
  commits pages
- Guard pages: `PROT_NONE` pages that catch buffer overflows instantly
- When to use `mmap` vs `malloc`

## Prerequisites

- Lesson 01 (memory layout -- you know the five segments)
- Lesson 03 (heap allocation -- you know malloc, and this lesson shows what
  sits underneath it)
- Lesson 04 (alignment -- page alignment is the ultimate alignment)

---

## Step 1 -- Virtual memory: the lie your CPU tells you

Every pointer you have used so far in this course -- every `&variable`, every
`malloc` return value -- is a **virtual address**. It is not a physical RAM
address. It is a number that the CPU's Memory Management Unit (MMU) translates
to a physical address using **page tables**.

In JavaScript, you never think about this. You write `const arr = [1, 2, 3]`
and V8 puts it somewhere in memory. You do not care where, and you could not
find out even if you wanted to.

In C, you CAN print the address (`%p`), but the address you see is still
virtual. The physical RAM address is different, and it can change over time
as the OS moves pages around.

```
What you see (virtual):       What actually happens (physical):

Your program's address space:  Physical RAM:
+------------------+
| Page at 0xA000   |--------> Physical frame at 0x3F000
| Page at 0xB000   |--------> Physical frame at 0x12000
| Page at 0xC000   |--- X --- (no physical backing! PROT_NONE)
| Page at 0xD000   |--------> Physical frame at 0x7A000
+------------------+

The MMU translates virtual -> physical on every memory access.
You never see physical addresses from user-space code.
```

A **page** is the smallest unit of this translation. On x86-64, a page is
4096 bytes (4 KB). Every memory mapping, permission change, and OS-level
allocation operates in page-sized chunks.

Pages marked `PROT_NONE` exist in the virtual address space (they have a page
table entry) but have **no permissions** -- no read, no write, no execute.
Any access triggers a **page fault** that the kernel converts to `SIGSEGV`.

> **Why?** Virtual memory is what makes everything else in this course
> possible. It is why `malloc` can give you a contiguous-looking buffer even
> though the physical RAM might be fragmented. It is why your program cannot
> read another process's memory. It is how the OS gives each process the
> illusion of having the entire address space to itself. And it is how arena
> allocators can reserve gigabytes of address space without using any physical
> RAM.

---

## Step 2 -- Querying the page size

Start the program by querying the system's page size:

```c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/mman.h>
#include "common/print_utils.h"
#include "common/bench.h"

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("=== Lesson 05 -- Virtual Memory and Pages ===\n");

    long page_size = sysconf(_SC_PAGESIZE);
    printf("\n  System page size: %ld bytes (%ld KB)\n",
           page_size, page_size / 1024);
```

`sysconf(_SC_PAGESIZE)` returns the page size at runtime. On virtually all
x86-64 systems this is 4096. All `mmap` operations must work in multiples of
this size.

Why not just hardcode 4096? Because:
- ARM systems may use 16 KB or 64 KB pages
- x86-64 supports "huge pages" of 2 MB or 1 GB
- Portable code should always query

> **Why?** In JavaScript, `WebAssembly.Memory` uses 64 KB pages (WASM's fixed
> page size). The OS page size is much smaller (4 KB), but WASM abstracts it
> away. In C, you work directly with OS pages, so you need to know the size.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=05 -r
```

You should see `System page size: 4096 bytes (4 KB)`.

---

## Step 3 -- Basic anonymous mmap: allocating pages from the OS

`mmap` with `MAP_ANONYMOUS` allocates virtual memory directly from the OS.
Unlike `malloc` (which goes through a user-space allocator), `mmap` talks
directly to the kernel:

```c
    print_section("mmap -- Anonymous Memory Allocation");
    {
        size_t size = 4 * (size_t)page_size;   /* 4 pages = 16 KB */
        void *mem = mmap(NULL, size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);

        if (mem == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        printf("  mmap(%zu bytes) = %p\n", size, mem);
        printf("  Page-aligned: %p mod %ld = %ld\n",
               mem, page_size,
               (long)((uintptr_t)mem % (uintptr_t)page_size));
```

Let's break down every argument:

- `NULL` -- "I don't care where you put it. Choose an address for me." You
  CAN request a specific address, but that is for advanced use cases.

- `size` -- How many bytes to map. Must be a multiple of the page size (or the
  kernel rounds up). We are asking for 4 pages = 16,384 bytes.

- `PROT_READ | PROT_WRITE` -- Permissions. This memory is readable and
  writable. Other options: `PROT_EXEC` (executable, for JIT compilers) and
  `PROT_NONE` (no access at all -- we will use this for guard pages).

- `MAP_PRIVATE | MAP_ANONYMOUS` -- Two flags combined:
  - `MAP_PRIVATE` means changes are private to this process (copy-on-write).
  - `MAP_ANONYMOUS` means "not backed by a file." The memory starts zeroed.

- `-1, 0` -- File descriptor and offset. Since we are using `MAP_ANONYMOUS`,
  there is no file. `-1` for no fd, `0` for no offset.

Now verify the memory is zero-initialized and use it:

```c
        int all_zero = 1;
        for (size_t i = 0; i < size; i++) {
            if (((char *)mem)[i] != 0) { all_zero = 0; break; }
        }
        printf("  Zero-initialized: %s\n", all_zero ? "YES" : "NO");

        memset(mem, 0xAB, size);
        printf("  Written 0xAB to all %zu bytes.\n", size);

        munmap(mem, size);
        printf("  munmap() -- memory returned to OS.\n");
    }
```

`munmap(mem, size)` releases the pages back to the kernel. This is different
from `free()`: `free` returns memory to the user-space allocator (which may
or may not return it to the OS). `munmap` returns pages DIRECTLY to the
kernel, guaranteed.

```
malloc/free vs mmap/munmap:

malloc(64)                    mmap(NULL, 4096, ...)
   |                             |
   v                             v
Allocator (user-space)        Kernel (OS)
   |                             |
   |--- maybe sbrk/mmap --->    |
   |                             v
   v                          Page tables updated
Free lists updated            Physical page allocated

free(ptr)                     munmap(ptr, 4096)
   |                             |
   v                             v
Allocator (user-space)        Kernel (OS)
   |                             |
   v                             v
Block added to free list      Page tables cleared
(memory NOT returned to OS    Physical page freed
 until allocator decides to)  (memory DEFINITELY returned)
```

> **Why?** In JavaScript, `new ArrayBuffer(16384)` internally uses something
> like `mmap` to get memory from the OS. You just never see it. In C, you
> have the choice: `malloc` for general-purpose allocations where the allocator
> optimizes for speed, or `mmap` for large allocations where you want direct
> OS control (permissions, page alignment, guaranteed zero-initialization).

### Build and run checkpoint

```bash
./build-dev.sh --lesson=05 -r
```

---

## Step 4 -- Reserve vs commit: the arena allocator foundation

This is the most important concept in this lesson for your future work. The
reserve-then-commit pattern is how production arena allocators work. It is
how game engines pre-allocate memory without consuming physical RAM.

The idea:

1. **Reserve** a huge virtual address range (64 MB, 256 MB, even gigabytes)
   with `PROT_NONE`. This costs almost nothing -- no physical RAM is used. You
   are just claiming virtual address space.

2. **Commit** pages on demand with `mprotect(..., PROT_READ | PROT_WRITE)`.
   Only committed pages use physical RAM.

```
Phase 1: RESERVE              Phase 2: COMMIT (as needed)

+---+---+---+---+---+        +---+---+---+---+---+
| N | N | N | N | N |        | RW| RW| N | N | N |
+---+---+---+---+---+        +---+---+---+---+---+
  (all PROT_NONE)               (first 2 committed)
  64 MB of address space        8 KB of physical RAM
  0 bytes of physical RAM       64 MB still reserved

N = PROT_NONE (no access, no physical RAM)
RW = PROT_READ|PROT_WRITE (accessible, backed by physical RAM)
```

Why is this better than `malloc` + `realloc`?

- `realloc` may need to COPY your entire buffer to a new location when it
  grows. For a 100 MB buffer, that is a 100 MB copy.
- With reserve-then-commit, the buffer's BASE ADDRESS NEVER CHANGES. You
  reserved the address range up front. You just commit more pages within
  that range as needed.

```c
    print_section("Reserve vs Commit (Virtual Memory Trick)");
    {
        size_t reserve_size = 64 * 1024 * 1024;   /* 64 MB */
        void *reserved = mmap(NULL, reserve_size,
                              PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1, 0);
        if (reserved == MAP_FAILED) {
            perror("mmap reserve");
            return 1;
        }
        printf("  Reserved %zu MB at %p (no physical memory yet!)\n",
               reserve_size / (1024 * 1024), reserved);
```

We just "allocated" 64 MB but used ZERO physical RAM. The page table entries
exist (marking those virtual addresses as belonging to our process), but they
all point to nothing. Any access would cause a `SIGSEGV`.

Now commit the first page:

```c
        size_t commit_size = (size_t)page_size;
        if (mprotect(reserved, commit_size, PROT_READ | PROT_WRITE) != 0) {
            perror("mprotect commit");
            munmap(reserved, reserve_size);
            return 1;
        }
        printf("  Committed first page (%ld bytes) -- now writable.\n",
               page_size);

        ((int *)reserved)[0] = 42;
        printf("  Wrote 42 to first int: %d\n", ((int *)reserved)[0]);
```

`mprotect(addr, size, prot)` changes the permissions on existing pages.
We changed the first page from `PROT_NONE` (no access) to `PROT_READ |
PROT_WRITE` (full access). The kernel allocates a physical page to back it.

Now commit more pages:

```c
        size_t more = 4 * (size_t)page_size;
        if (mprotect(reserved, commit_size + more, PROT_READ | PROT_WRITE) != 0) {
            perror("mprotect grow");
        }
        printf("  Committed %zu more pages -- total committed: %zu bytes.\n",
               more / (size_t)page_size,
               commit_size + more);

        ((int *)reserved)[page_size / sizeof(int)] = 99;
        printf("  Wrote 99 to second page: %d\n",
               ((int *)reserved)[page_size / sizeof(int)]);

        printf("\n  Summary:\n");
        printf("    Address space reserved: %zu MB\n",
               reserve_size / (1024 * 1024));
        printf("    Physical memory used:   %zu KB\n",
               (commit_size + more) / 1024);

        munmap(reserved, reserve_size);
    }
```

```
After the reserve-then-commit sequence:

Address:  reserved          reserved + 4K       reserved + 20K     reserved + 64MB
          |                 |                   |                  |
          v                 v                   v                  v
          +--------+--------+--------+--------+---------//--------+
          |  RW    |  RW    |  RW    |  RW    |  PROT_NONE ...    |
          | page 0 | page 1 | page 2 | page 3 |  (60+ MB)        |
          | (42)   | (99)   |        |        |  no physical RAM  |
          +--------+--------+--------+--------+---------//--------+
          <---- 20 KB committed ---->
          <------------- 64 MB reserved --------------------------->

Physical RAM used: 20 KB
Virtual address space reserved: 64 MB
Ratio: 0.03% of reserved space actually uses RAM
```

> **Why?** In JavaScript, `WebAssembly.Memory({ initial: 1, maximum: 100 })`
> does something conceptually similar -- it reserves up to 100 pages but only
> allocates 1 initially. `memory.grow(5)` commits 5 more. In C with `mmap`,
> you have full control over this process. This is how the arena allocator in
> Lesson 13 works: reserve a huge range at startup, commit pages as the game
> allocates memory. No copies, no pointer invalidation, no `realloc`.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=05 -r
```

---

## Step 5 -- Guard pages: instant overflow detection

A guard page is a `PROT_NONE` page placed at the boundary of a valid memory
region. Any access to the guard page -- even reading a single byte --
triggers an immediate `SIGSEGV`. No silent corruption, no delayed detection.

```
Layout: [GUARD] [DATA DATA DATA DATA] [GUARD]

+--------+--------+--------+--------+--------+--------+
|PROT_NONE|PROT_RW |PROT_RW |PROT_RW |PROT_RW |PROT_NONE|
| guard  | data   | data   | data   | data   | guard  |
| page   | page 0 | page 1 | page 2 | page 3 | page   |
+--------+--------+--------+--------+--------+--------+
^                                              ^
write here = instant SIGSEGV                   write here = instant SIGSEGV
```

This is what AddressSanitizer (ASan) conceptually does with its "red zones"
around each allocation. Guard pages are the OS-level mechanism -- zero runtime
overhead for in-bounds access (the CPU checks permissions on every memory
access via the page table anyway).

First, set up a signal handler so we can catch the `SIGSEGV` instead of dying:

```c
static jmp_buf jump_buf;
static volatile int got_signal = 0;

static void sigsegv_handler(int sig) {
    (void)sig;
    got_signal = 1;
    longjmp(jump_buf, 1);
}
```

`longjmp` jumps back to the `setjmp` call site, effectively "catching" the
signal. This is the C equivalent of try/catch for signals (though much more
limited and dangerous -- do not use this pattern in production code).

Now build the guard page demo:

```c
    print_section("Guard Pages -- Instant Overflow Detection");
    {
        size_t data_pages = 4;
        size_t data_size = data_pages * (size_t)page_size;
        size_t total_size = data_size + 2 * (size_t)page_size;

        void *region = mmap(NULL, total_size,
                            PROT_NONE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1, 0);
        if (region == MAP_FAILED) { perror("mmap"); return 1; }

        void *data = (char *)region + page_size;
        if (mprotect(data, data_size, PROT_READ | PROT_WRITE) != 0) {
            perror("mprotect");
            munmap(region, total_size);
            return 1;
        }

        printf("  Memory layout:\n");
        printf("    Guard page (PROT_NONE): %p\n", region);
        printf("    Data (%zu pages):        %p -- %p\n",
               data_pages, data, (char *)data + data_size - 1);
        printf("    Guard page (PROT_NONE): %p\n", (char *)data + data_size);
```

The layout:
1. Map the entire region (data + 2 guard pages) as `PROT_NONE`
2. `mprotect` the middle section to `PROT_READ | PROT_WRITE`
3. The first and last pages remain `PROT_NONE` -- the guards

Now write safely to the data, then trigger the guard page:

```c
        memset(data, 0x42, data_size);
        printf("\n  Wrote 0x42 to all %zu data bytes -- OK.\n", data_size);

        printf("\n  Attempting to write 1 byte past the data region...\n");

        struct sigaction sa, old_sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigsegv_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, &old_sa);
        sigaction(SIGBUS, &sa, NULL);

        got_signal = 0;
        if (setjmp(jump_buf) == 0) {
            volatile char *guard = (char *)data + data_size;
            *guard = 0xFF;   /* ONE byte past the data -> hits PROT_NONE guard */
            printf("  ERROR: Should not reach here!\n");
        } else {
            printf("  CAUGHT SIGSEGV! Guard page detected the overflow.\n");
            printf("  Without guard pages, this write would silently corrupt memory.\n");
        }

        sigaction(SIGSEGV, &old_sa, NULL);
        munmap(region, total_size);
    }
```

```
What happened:

data starts at:    region + page_size
data ends at:      region + page_size + data_size - 1
guard page starts: region + page_size + data_size

We wrote to:       region + page_size + data_size  <-- FIRST BYTE of guard page

            data region (RW)          guard page (PROT_NONE)
...  0x42  0x42  0x42  0x42  |  0xFF  xxxxxxxxxx
                              ^
                         page boundary
                         This write hits PROT_NONE
                         -> page fault
                         -> kernel sends SIGSEGV
                         -> our handler catches it
```

> **Why?** In JavaScript, writing past the end of a `TypedArray` is silently
> ignored (in non-strict mode) or throws a `RangeError`. In C, writing past
> the end of a buffer is **undefined behavior** -- usually silent corruption
> that manifests hours later as a crash in completely unrelated code. Guard
> pages catch the overflow at the EXACT instruction that causes it.
> Production allocators use guard pages in debug builds. Electric Fence and
> DUMA are classic tools that place every allocation adjacent to a guard page.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=05 -r
```

You should see the "CAUGHT SIGSEGV!" message, proving the guard page worked.

---

## Step 6 -- mmap vs malloc: when to use each

Add a comparison printout:

```c
    print_section("mmap vs malloc -- When to Use Each");
    printf("  malloc:\n");
    printf("    - General purpose, any size\n");
    printf("    - Fast for small allocations (uses free lists internally)\n");
    printf("    - No page alignment guarantee\n");
    printf("    - Cannot control read/write/exec permissions\n\n");
    printf("  mmap:\n");
    printf("    - Page-granularity only (4 KB minimum)\n");
    printf("    - Good for large allocations (> 128 KB)\n");
    printf("    - Page-aligned, zero-initialized\n");
    printf("    - Can set permissions (PROT_NONE, guard pages)\n");
    printf("    - Can map files into memory (Lesson 06)\n");
    printf("    - Used internally by malloc for large allocations!\n\n");
    printf("  Arena allocators (Lesson 13) use mmap for backing memory,\n");
    printf("  then hand out small pieces via pointer bumping.\n");

    printf("\n");
    return 0;
}
```

Here is the decision matrix:

```
Need                           Use
------------------------------ ---------------------------
Small allocation (< 128 KB)   malloc
Large allocation (> 128 KB)    mmap (malloc uses it internally anyway)
Page-aligned memory            mmap
Permission control             mmap + mprotect
Guard pages                    mmap with PROT_NONE
Reserve-then-commit            mmap with PROT_NONE + mprotect
Map a file into memory         mmap with fd (Lesson 06)
Arena allocator backing        mmap
Frequent alloc/free            malloc (free lists are optimized for this)
```

Fun fact: when you call `malloc(256 * 1024)` (256 KB), glibc's allocator
internally calls `mmap` instead of `sbrk`. So for large allocations, you are
using `mmap` anyway -- `malloc` just adds its header overhead and free-list
tracking on top.

---

## Build and run

```bash
./build-dev.sh --lesson=05 -r
```

---

## Expected output

```
  System page size: 4096 bytes (4 KB)

=== mmap -- Anonymous Memory Allocation ===
  mmap(16384 bytes) = 0x7ffff7fba000
  Page-aligned: 0x7ffff7fba000 mod 4096 = 0
  Zero-initialized: YES
  Written 0xAB to all 16384 bytes.
  munmap() -- memory returned to OS.

=== Reserve vs Commit (Virtual Memory Trick) ===
  Reserved 64 MB at 0x7ffff3fba000 (no physical memory yet!)
  Committed first page (4096 bytes) -- now writable.
  Wrote 42 to first int: 42
  ...
  Summary:
    Address space reserved: 64 MB
    Physical memory used:   20 KB

=== Guard Pages -- Instant Overflow Detection ===
  ...
  Wrote 0x42 to all 16384 data bytes -- OK.
  Attempting to write 1 byte past the data region...
  CAUGHT SIGSEGV! Guard page detected the overflow.
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `mmap` returns `MAP_FAILED` | Requested too much memory, or bad flags | Check `errno` with `perror`; try a smaller size |
| `mprotect` returns `-1` | Address or size not page-aligned | Ensure both are multiples of `sysconf(_SC_PAGESIZE)` |
| Guard page demo does not catch the fault | Signal handler not installed, or ASan intercepted it | Build with `--no-asan` for raw signal behavior |
| Writing to reserved (PROT_NONE) memory does not crash | Some OS configs overcommit; or you are accessing committed pages | Double-check your pointer math and mprotect ranges |
| `munmap` does not free memory (RSS stays high) | Mapped more than you think, or the mapping address was wrong | Always pass the exact same `ptr` and `size` to `munmap` that `mmap` returned |

---

## Exercises

1. **Measure page fault cost.** Map 1000 pages with `PROT_NONE`, then
   commit them one at a time and write a byte to each. Time how long each
   `mprotect` + first write takes. This measures the cost of a page fault.

2. **Reservation limits.** How much virtual address space can you reserve
   with `PROT_NONE`? Try 1 GB, 10 GB, 100 GB, 1 TB. On a 64-bit system,
   you should be able to reserve terabytes because no physical RAM is used.

3. **Build a simple bump allocator.** Reserve 16 MB with `mmap`, commit the
   first page, and write a `bump_alloc(size)` function that returns a pointer
   and advances an offset. When the offset crosses a page boundary, commit the
   next page with `mprotect`. This is the beginning of the arena allocator
   from Lesson 13.

4. **Electric Fence style.** Write a function `guarded_alloc(size)` that
   allocates one page of data followed by one guard page. Return a pointer
   offset so that the end of the allocation is exactly at the guard page
   boundary. Any off-by-one write will immediately SIGSEGV.

5. **Compare mmap zeroing vs calloc.** Allocate 1 MB with `mmap` (guaranteed
   zero) and 1 MB with `calloc` (also guaranteed zero). Time the allocation
   itself. Then time the first access to every page. `mmap` does lazy zeroing
   (on first access), while `calloc` may pre-zero or use zero pages.

---

## Connection to your work

In game engines:

- **Arena allocators use reserve-then-commit.** You reserve a huge range at
  startup (say 256 MB per arena), then commit pages as allocations grow.
  No realloc, no copies, no pointer invalidation. This is the dominant
  memory management pattern in modern game engines.

- **Guard pages catch buffer overflows in debug builds.** When you are
  developing, you want to catch every off-by-one error immediately. Placing
  guard pages around critical allocations (entity arrays, command buffers)
  turns silent corruption into instant crashes with backtraces.

- **mmap for large resource loading.** A 500 MB game level file can be
  `mmap`'d rather than `fread`'d. Only the pages you access are loaded from
  disk. Streaming open-world games use this technique to load level data
  lazily as the player moves through the world.

- **Permission control for security.** After loading and decrypting a game
  binary, you can `mprotect` the code pages to `PROT_READ | PROT_EXEC` and
  the data pages to `PROT_READ` only. This makes exploitation harder.

---

## What's next

In Lesson 06, we use `mmap`'s other superpower: **memory-mapped file I/O**.
Instead of reading a file into a buffer with `fread`, you map the file
directly into your address space and access it via pointer. The file becomes
an array. This is how game engines load large asset files with zero-copy
efficiency.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_05.c`. It contains all
demos including the signal handler, reserve-then-commit, and guard page
demonstration.
