# Lesson 03 -- Heap: malloc, calloc, realloc, free

> **What you will build:** A program that exercises all four core heap
> functions, compares their zeroing behavior with raw byte dumps, demonstrates
> the safe `realloc` pattern, and benchmarks `malloc+free` vs `calloc+free`.

## What you'll learn

- `malloc` returns uninitialized (garbage) memory -- unlike JS `new ArrayBuffer`
- `calloc` returns zeroed memory with built-in overflow protection
- `realloc` may move your data to a new address -- the safe pattern to avoid leaks
- `free` and the double-free problem
- Memory ownership: who is responsible for freeing what
- The hidden allocator metadata behind every allocation
- malloc's default alignment guarantee (16 bytes on 64-bit)

## Prerequisites

- Lesson 01 (memory layout -- you know where the heap segment lives)
- Lesson 02 (stack allocation -- you understand stack vs heap trade-offs)

---

## Step 1 -- How heap allocation differs from everything you know in JS

In JavaScript, when you write `const obj = { x: 1, y: 2 }`, three things
happen invisibly:

1. V8 allocates space on its internal heap
2. V8 writes the object data
3. V8 tracks the object for garbage collection

You never call "free." You never think about when the memory is released. The
garbage collector watches for objects that have no references and reclaims
their memory automatically.

In C, heap allocation is completely manual:

```
JS world:                          C world:
const buf = new ArrayBuffer(64);   void *buf = malloc(64);
// ... use buf ...                 // ... use buf ...
// GC frees it eventually          free(buf);  // YOU must free it
```

Forgetting `free` in C means the memory is **leaked** -- it stays allocated
until the process exits. There is no garbage collector coming to save you.
Calling `free` twice on the same pointer is a **double-free** -- it corrupts
the allocator's internal data structures and can cause crashes, security
vulnerabilities, or silent data corruption.

This is not just a theoretical concern. Memory leaks in a game engine that
allocates and frees textures, mesh data, and audio buffers 60 times per second
will eventually consume all available RAM and crash.

> **Why?** JavaScript's garbage collector is a convenience that costs
> performance (GC pauses) and predictability (you cannot control when memory
> is freed). C's manual management gives you full control at the cost of full
> responsibility. Game engines choose C/C++ specifically because they need that
> control -- a GC pause during a frame render causes visible stuttering.

---

## Step 2 -- How the heap allocator works under the hood

Before we use `malloc`, let's understand what it actually does. The heap is
managed by a **user-space allocator** (glibc's ptmalloc2, musl's mallocng,
jemalloc, etc.) that sits between your code and the OS:

```
Your code: malloc(64)
       |
       v
+------------------+
|  Allocator       |  Checks its free lists.
|  (ptmalloc2)     |  Found a suitable free block? Return it.
+------------------+  No free block? Ask the OS for more pages.
       |                                  |
       |                                  v
       |                          OS: mmap() or sbrk()
       |                          (gives whole pages, e.g. 4096 bytes)
       v
  +------+--------------------+
  | hdr  |  64 bytes for you  |  <-- pointer returned to you
  +------+--------------------+
  ^
  |
  Hidden metadata (size, flags, free-list pointers)
  Typically 16-32 bytes
```

Key insight: every `malloc`'d block has a **hidden header** that the allocator
uses to track the block. This header typically contains:

- The size of the allocation
- Status flags (allocated vs free)
- Pointers for linking free blocks together

This is why `malloc(1)` actually uses about 32 bytes: 16 for the header, 16
for the minimum chunk alignment. You do not see this overhead in JavaScript
either -- V8 has similar overhead per object, but you never observe it.

> **Why?** Understanding the hidden header explains several behaviors that
> confuse beginners: (1) `free(ptr)` does not take a size argument because the
> header already stores it, (2) writing before the start of your allocation
> corrupts the header and crashes on the next `free`, and (3) `malloc(1)` and
> `malloc(16)` often use the same amount of memory because of minimum chunk
> sizes.

```
What memory looks like after malloc(64):

       Header (hidden)     Your 64 bytes
       +--------+         +----------------------------------+
       | size=64|         | [uninitialized garbage bytes]    |
       | flags  |         |                                  |
       | prev/  |         |                                  |
       | next   |         |                                  |
       +--------+         +----------------------------------+
                ^
                |
         ptr returned by malloc points HERE
         (you never see the header)
```

---

## Step 3 -- malloc: reading uninitialized garbage

Now let's write code. Start with the helper function for printing raw bytes,
then `main`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "common/print_utils.h"
#include "common/bench.h"

static void print_bytes(const char *label, const void *ptr, size_t n) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    printf("  %s: ", label);
    for (size_t i = 0; i < n && i < 32; i++) {
        printf("%02x ", bytes[i]);
    }
    if (n > 32) printf("...");
    printf("\n");
}
```

The `print_bytes` helper casts the pointer to `uint8_t *` (unsigned byte
pointer) and prints each byte in hexadecimal. `%02x` means "print as hex,
zero-padded to 2 digits."

Now the first `malloc` demo:

```c
int main(void) {
    printf("=== Lesson 03 -- Heap: malloc, calloc, realloc, free ===\n");

    print_section("malloc -- Uninitialized Memory");
    {
        int *arr = (int *)malloc(5 * sizeof(int));
        if (!arr) { perror("malloc"); return 1; }

        printf("  malloc(5 * sizeof(int)) = %p\n", (void *)arr);
        printf("  Contents (BEFORE writing):\n");
        for (int i = 0; i < 5; i++) {
            printf("    arr[%d] = %d  (garbage!)\n", i, arr[i]);
        }

        for (int i = 0; i < 5; i++) arr[i] = (i + 1) * 10;
        printf("  Contents (AFTER writing):\n");
        for (int i = 0; i < 5; i++) {
            printf("    arr[%d] = %d\n", i, arr[i]);
        }
        free(arr);
        printf("  free(arr) -- memory returned to the heap.\n");
    }
```

Let's walk through every line:

- `(int *)malloc(5 * sizeof(int))` -- Allocate space for 5 ints. `sizeof(int)`
  is 4 bytes on most platforms, so this requests 20 bytes. The `(int *)` cast
  tells the compiler we are treating the returned `void *` as an `int *`.

- `if (!arr)` -- `malloc` returns NULL on failure (out of memory). ALWAYS
  check. In JavaScript, `new Array()` throws an exception on failure. In C,
  `malloc` silently returns NULL, and dereferencing NULL is a segfault.

- The "BEFORE writing" loop prints whatever garbage bytes are at the address.
  On a fresh `malloc`, these might be zeros (if the OS gave fresh pages) or
  leftover data from a previous allocation (if the allocator reused a free
  block). You cannot rely on either behavior.

- `free(arr)` returns the memory to the allocator. After this call, `arr` is
  a **dangling pointer** -- it still holds the old address, but that address
  is no longer valid. Dereferencing it is undefined behavior.

> **Why?** In JavaScript, `new ArrayBuffer(20)` is ALWAYS zeroed. The spec
> guarantees it. In C, `malloc` returns whatever bits were previously at that
> memory address. Reading uninitialized memory is technically undefined
> behavior -- the compiler is allowed to assume you never do this and may
> optimize in surprising ways. This is a major source of bugs. If you need
> zeros, use `calloc` (next step).

### Build and run checkpoint

```bash
./build-dev.sh --lesson=03 -r
```

You should see garbage values in the "BEFORE" section and clean values in the
"AFTER" section.

---

## Step 4 -- calloc: guaranteed zeros and overflow protection

`calloc` is `malloc` with two improvements: it zeros the memory and checks
for integer overflow.

```c
    print_section("calloc -- Zero-Initialized Memory");
    {
        int *arr = (int *)calloc(5, sizeof(int));
        if (!arr) { perror("calloc"); return 1; }

        printf("  calloc(5, sizeof(int)) = %p\n", (void *)arr);
        printf("  Contents (guaranteed zero):\n");
        for (int i = 0; i < 5; i++) {
            printf("    arr[%d] = %d\n", i, arr[i]);
        }
        print_bytes("Raw bytes", arr, 20);
        free(arr);
    }
```

`calloc(5, sizeof(int))` does three things:

1. Computes `5 * sizeof(int)` = 20 bytes
2. Checks that `5 * 4` does not overflow (it does not)
3. Allocates 20 bytes and fills them with zeros

The overflow protection is critical. Consider this danger:

```c
size_t count = SIZE_MAX;  /* e.g. 18446744073709551615 on 64-bit */
void *p = malloc(count * 2);
/* count * 2 wraps around to a small number!
   malloc succeeds with a tiny buffer.
   You think you have SIZE_MAX*2 bytes. Disaster. */

void *q = calloc(count, 2);
/* calloc detects the overflow and returns NULL. Safe. */
```

This prevents an entire class of security bugs where user-controlled sizes
cause wrap-around allocations. If the count comes from a network packet or
file header, an attacker could craft a value that overflows the multiplication,
causing a tiny allocation that your code then writes past.

Now let's compare them side-by-side:

```c
    print_section("malloc vs calloc -- Content Comparison");
    {
        void *m = malloc(64);
        void *c = calloc(1, 64);

        printf("  malloc(64) raw bytes:\n");
        print_bytes("  malloc", m, 64);
        printf("  calloc(1, 64) raw bytes:\n");
        print_bytes("  calloc", c, 64);
        printf("\n  calloc is guaranteed zero; malloc is undefined.\n");

        free(m);
        free(c);
    }
```

### Build and run checkpoint

```bash
./build-dev.sh --lesson=03 -r
```

The calloc bytes will all be `00`. The malloc bytes will be... whatever.
Possibly zeros (if the OS gave fresh pages), possibly garbage (if reusing
freed memory).

> **Why?** `calloc` is like JavaScript's `new ArrayBuffer(n)` -- zeroed and
> safe to read immediately. `malloc` is like getting a used whiteboard that
> might still have someone else's notes on it. In game engines, use `calloc`
> when you need a clean slate (initialization arrays, cleared textures). Use
> `malloc` when you are about to overwrite every byte anyway (loading data from
> disk, receiving network packets) -- skipping the zeroing is faster.

---

## Step 5 -- realloc: the pointer might move

`realloc` resizes an existing allocation. This is conceptually similar to
JavaScript's array growing when you push elements, but with a critical
difference: the pointer may change.

```c
    print_section("realloc -- Resize an Allocation");
    {
        int *arr = (int *)malloc(3 * sizeof(int));
        arr[0] = 100; arr[1] = 200; arr[2] = 300;
        void *old_ptr = arr;

        printf("  Initial: %p  [%d, %d, %d]\n", (void *)arr,
               arr[0], arr[1], arr[2]);

        arr = (int *)realloc(arr, 6 * sizeof(int));
        printf("  After realloc to 6: %p  [%d, %d, %d, ?, ?, ?]\n",
               (void *)arr, arr[0], arr[1], arr[2]);

        if ((void *)arr == old_ptr) {
            printf("  Pointer UNCHANGED (grew in place).\n");
        } else {
            printf("  Pointer CHANGED (data was copied to new location).\n");
        }

        arr[3] = 400; arr[4] = 500; arr[5] = 600;

        old_ptr = arr;
        arr = (int *)realloc(arr, 2 * sizeof(int));
        printf("  After realloc to 2: %p  [%d, %d]\n",
               (void *)arr, arr[0], arr[1]);

        free(arr);
    }
```

What `realloc(ptr, new_size)` does:

1. If the allocator has room to grow the block in place (the bytes after your
   allocation are free), it extends the allocation and returns the SAME
   pointer.
2. If it cannot grow in place, it allocates a NEW block of `new_size` bytes,
   copies your old data, frees the old block, and returns the NEW pointer.
3. If `new_size` is zero, behavior is implementation-defined (may free, may
   return a non-NULL pointer that must not be dereferenced).
4. If `new_size` allocation fails, it returns NULL and the OLD pointer remains
   valid and unchanged.

```
Case 1: realloc grows in place
Before:  [...header...][100][200][300][free space..........]
After:   [...header...][100][200][300][  ?][  ?][  ?]
         Same pointer!

Case 2: realloc must move
Before:  [...header...][100][200][300][NEXT_BLOCK_HEADER...]
After:   Old block freed. New block somewhere else:
         [...header...][100][200][300][  ?][  ?][  ?]
         DIFFERENT pointer!
```

> **Why?** In JavaScript, `arr.push(item)` might internally reallocate the
> backing store, but the array reference (`arr`) never changes. You do not need
> to worry about it. In C, `realloc` CAN change the pointer, and if anyone
> else has a copy of the old pointer, their copy is now a **dangling pointer**.
> This is why dynamic arrays in C are tricky -- every `realloc` can invalidate
> every pointer into the array. Game engines often solve this by pre-allocating
> large buffers or using arena allocators.

---

## Step 6 -- The safe realloc pattern

The most common `realloc` bug:

```c
/* WRONG -- leaks memory on failure! */
ptr = realloc(ptr, new_size);
/* If realloc returns NULL:
   - ptr is now NULL
   - The old allocation is still alive but unreachable
   - Memory leak! */
```

The safe pattern:

```c
/* CORRECT -- preserves the old pointer on failure */
void *tmp = realloc(ptr, new_size);
if (!tmp) {
    /* realloc failed. ptr is still valid.
       Handle the error (free ptr, return error, etc.) */
    free(ptr);
    return;
}
ptr = tmp;
```

Add this demo to your `main`:

```c
    print_section("Safe realloc Pattern");
    {
        printf("  WRONG (leaks on failure):\n");
        printf("    ptr = realloc(ptr, new_size);  // if NULL, old ptr lost!\n\n");
        printf("  CORRECT:\n");
        printf("    void *tmp = realloc(ptr, new_size);\n");
        printf("    if (!tmp) { /* handle error, ptr still valid */ }\n");
        printf("    ptr = tmp;\n");

        int *data = (int *)malloc(10 * sizeof(int));
        for (int i = 0; i < 10; i++) data[i] = i;

        void *tmp = realloc(data, 20 * sizeof(int));
        if (tmp) {
            data = (int *)tmp;
            printf("\n  Realloc succeeded: data=%p, data[9]=%d\n",
                   (void *)data, data[9]);
        }
        free(data);
    }
```

This pattern will appear dozens of times in C code. Burn it into muscle
memory: always use a temporary variable for `realloc`.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=03 -r
```

---

## Step 7 -- Alignment guarantee and allocation overhead

`malloc` returns pointers aligned to at least 16 bytes on 64-bit systems. This
guarantees any standard C type (`double` needs 8-byte alignment, `long double`
may need 16) can be used with the returned pointer.

```c
    print_section("Allocation Size and Alignment");
    {
        void *p1 = malloc(1);
        void *p2 = malloc(7);
        void *p3 = malloc(16);
        void *p4 = malloc(100);

        printf("  malloc(1)   = %p  (mod 16 = %zu)\n",
               p1, (uintptr_t)p1 % 16);
        printf("  malloc(7)   = %p  (mod 16 = %zu)\n",
               p2, (uintptr_t)p2 % 16);
        printf("  malloc(16)  = %p  (mod 16 = %zu)\n",
               p3, (uintptr_t)p3 % 16);
        printf("  malloc(100) = %p  (mod 16 = %zu)\n",
               p4, (uintptr_t)p4 % 16);
        printf("\n  malloc guarantees alignment suitable for any basic type\n");
        printf("  (typically 16-byte aligned on 64-bit systems).\n");

        free(p1); free(p2); free(p3); free(p4);
    }
```

The `(uintptr_t)p1 % 16` trick casts the pointer to an integer and checks if
it is divisible by 16. All four results should be 0.

This means you can safely do:

```c
void *buf = malloc(100);
double *d = (double *)buf;   /* Safe -- 16-byte aligned satisfies 8-byte requirement */
int *i = (int *)buf;         /* Safe -- 16-byte aligned satisfies 4-byte requirement */
```

If you need alignment beyond 16 bytes (e.g., 64-byte cache-line alignment for
SIMD or multithreaded code), use `aligned_alloc(64, size)` or
`posix_memalign(&ptr, 64, size)`. Regular `malloc` cannot help you.

> **Why?** In JavaScript, TypedArray alignment is handled automatically. When
> you create a `Float64Array`, V8 ensures the underlying buffer is 8-byte
> aligned. In C, `malloc`'s 16-byte guarantee covers all standard types. But
> game engines frequently need 64-byte cache-line alignment for SIMD vectors
> (SSE/AVX) and to prevent false sharing between threads. That requires
> `aligned_alloc`, which we will use in the arena allocator lessons.

---

## Step 8 -- Benchmarking malloc vs calloc

The benchmark section (enabled with `--bench`) times `malloc+free` vs
`calloc+free` cycles:

```c
#ifdef BENCH_MODE
    {
        BenchSuite suite;
        long N = 5000000;

        BENCH_SUITE(suite, "malloc+free vs calloc+free (64 bytes)") {
            BENCH_CASE(suite, "malloc+free", i, N) {
                void *p = malloc(64);
                *(volatile char *)p = 0;
                free(p);
            }
            BENCH_CASE_END(suite, N);

            BENCH_CASE(suite, "calloc+free", i, N) {
                void *p = calloc(1, 64);
                *(volatile char *)p = 0;
                free(p);
            }
            BENCH_CASE_END(suite, N);
        }
    }
#else
    bench_skip_notice("lesson_03");
#endif

    printf("\n");
    return 0;
}
```

The `*(volatile char *)p = 0` line prevents the compiler from optimizing away
the allocation entirely. Without it, the compiler might realize you never use
the memory and eliminate the `malloc`/`free` pair.

On most systems, `calloc` is comparable to `malloc` for small allocations.
For large allocations, `calloc` can actually be FASTER because the OS gives
it pre-zeroed pages (via `mmap`), while `malloc` + `memset` does an extra
pass over the memory.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=03 --bench -r
```

---

## Build and run

Normal run:

```bash
./build-dev.sh --lesson=03 -r
```

With benchmarks:

```bash
./build-dev.sh --lesson=03 --bench -r
```

---

## Expected output

```
=== malloc -- Uninitialized Memory ===
  malloc(5 * sizeof(int)) = 0x5555555592a0
  Contents (BEFORE writing):
    arr[0] = 0  (garbage!)
    arr[1] = 0  (garbage!)
    ...
  Contents (AFTER writing):
    arr[0] = 10
    arr[1] = 20
    ...
  free(arr) -- memory returned to the heap.

=== calloc -- Zero-Initialized Memory ===
  calloc(5, sizeof(int)) = 0x5555555592d0
  Contents (guaranteed zero):
    arr[0] = 0
    arr[1] = 0
    ...
  Raw bytes: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

=== realloc -- Resize an Allocation ===
  Initial: 0x5555555592a0  [100, 200, 300]
  After realloc to 6: 0x5555555592a0  [100, 200, 300, ?, ?, ?]
  Pointer UNCHANGED (grew in place).
```

(Your exact addresses will differ.)

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Reading malloc'd memory shows all zeros | Fresh pages from the OS happen to be zeroed, but this is NOT guaranteed | Never rely on it -- use `calloc` if you need zeros |
| Memory leak after realloc failure | Used `ptr = realloc(ptr, size)` -- if NULL, old ptr is lost | Always use the `tmp` pattern |
| `calloc(SIZE_MAX, 2)` returns NULL | `calloc` detected that `count * size` overflows | This is correct behavior -- `malloc(SIZE_MAX * 2)` would silently wrap |
| Double free crash | Called `free(arr)` twice | Set pointer to NULL after free: `free(arr); arr = NULL;` |
| Use-after-free: data looks correct | The allocator has not reused the block yet -- pure luck | Always NULL-out freed pointers; compile with ASan to catch this |

---

## Exercises

1. **Visualize the hidden header.** Allocate two adjacent blocks:
   `void *a = malloc(32); void *b = malloc(32);`. Print both addresses. The
   difference should be more than 32 bytes -- the extra bytes are the
   allocator's hidden header. How many header bytes does your allocator use?

2. **Trigger a double-free.** Allocate a block, free it, then free it again.
   Compile WITHOUT AddressSanitizer (`--no-asan`). What happens? (On modern
   glibc, you will see `free(): double free detected` followed by
   `abort()`.)

3. **Prove realloc copies data.** Allocate 10 ints, fill them, print the
   pointer. Then `realloc` to 10000 ints (forcing a move). Print the new
   pointer. Verify the first 10 ints are preserved at the new address.

4. **calloc vs malloc+memset race.** In bench mode, compare `calloc(1, 1MB)`
   vs `malloc(1MB) + memset(0)`. For large allocations, `calloc` should win
   because it can skip the memset when the OS provides zero pages.

5. **Build a tiny dynamic array.** Write a struct with `int *data`,
   `size_t length`, `size_t capacity`. Implement `push(array, value)` that
   calls `realloc` (with the safe pattern) when `length == capacity`. This
   is the C equivalent of `Array.push()` in JavaScript.

---

## Connection to your work

In game engines:

- **malloc/free for one-time allocations.** Loading a level? `malloc` a buffer,
  read the file, process it. `free` when done. Simple ownership.

- **calloc for zero-initialized structures.** Entity arrays, particle systems,
  component tables -- anything that should start at zero.

- **realloc for growing buffers.** Dynamic arrays of entities, command buffers,
  string builders. Always use the safe `tmp` pattern.

- **The hidden header overhead matters.** If you have 100,000 tiny allocations
  (32 bytes each), the headers add 1.6 MB of overhead. Arena allocators
  (Lesson 10+) eliminate this overhead by allocating in bulk from the OS and
  parceling out chunks with no per-allocation header.

---

## What's next

In Lesson 04, we dive into **alignment and sizeof** -- the rules that govern
how the compiler arranges fields inside structs, why a struct with `{char, int,
char, double}` wastes 10 bytes out of 24, and how to fix it. This directly
affects memory consumption when you have millions of entities.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_03.c`. It contains all
demos plus the benchmark, along with the `print_utils.h` and `bench.h`
dependencies.
