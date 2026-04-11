# Lesson 08 -- Buffer Overflow and Uninitialized Reads

> **What you will build:** Four demos that trigger heap buffer overflow,
> stack buffer overflow, uninitialized memory reads, and an off-by-one
> error -- each caught by sanitizers or shown to produce unpredictable
> behavior.

## What you'll learn

- What a heap buffer overflow is and why it corrupts adjacent allocations
- What a stack buffer overflow is and why it enables stack-smashing attacks
- What uninitialized memory reads produce and why branching on them is UB
- Why the off-by-one error (`<=` vs `<`) is the most common overflow cause
- How ASan places red zones around every allocation to catch overflows
- The difference between `malloc` (uninitialized) and `calloc` (zeroed)

## Prerequisites

- A working C compiler (GCC or Clang) on Linux with ASan support
- Familiarity with JavaScript/TypeScript (we use JS analogies throughout)
- Lesson 07 completed (you understand leaks, use-after-free, double-free)

---

## Step 1 -- The guardrails that do not exist in C

In JavaScript, arrays are bounds-checked automatically. You cannot write
past the end of an array and corrupt something else:

```js
const arr = new Int32Array(10);
arr[10] = 42;  // silently ignored (or throws in strict TypedArray mode)
arr[-1] = 42;  // sets a property on the object, does not corrupt memory
```

In C, there are NO bounds checks. The compiler trusts you completely:

```c
int arr[10];
arr[10] = 42;  // writes ONE PAST the end -- corrupts whatever is next
arr[100] = 42; // writes FAR past the end -- corrupts distant memory
```

If you write past the end of a buffer, the write goes through. Whatever
bytes happen to be adjacent -- return addresses on the stack, allocator
metadata on the heap, other program data -- get silently overwritten.

```
JS world:
  [ arr[0] | arr[1] | ... | arr[9] ]
  Accessing arr[10] → undefined (safe, bounded)

C world:
  [ arr[0] | arr[1] | ... | arr[9] | ??? adjacent data ??? ]
  Accessing arr[10] → writes into whatever is next (DANGEROUS)
                       Could be: another variable
                       Could be: the function's return address
                       Could be: allocator bookkeeping
```

Buffer overflows are the root cause of an enormous class of security
exploits. The Morris Worm (1988), Code Red (2001), Heartbleed (2014) --
all buffer overflow variants.

> **Why?** JavaScript arrays are objects with length tracking. Every access
> is checked against the length. C arrays are raw pointers to contiguous
> bytes. There is no length field, no bounds check, no safety net. The CPU
> simply computes `base + index * element_size` and reads/writes that
> address. If the index is out of range, you corrupt whatever lives at
> that address. This is the fundamental difference between a managed and
> unmanaged language.

Create the file `src/lessons/lesson_08.c` and start with the includes:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
```

---

## Step 2 -- Heap buffer overflow

The first demo allocates 16 bytes on the heap but writes 32 bytes into the
buffer. The extra 16 bytes overflow into whatever the allocator placed
adjacent -- typically the next allocation's metadata or ASan's red zone.

Add this function:

```c
static void demo_heap_overflow(void) {
    printf("\n── Demo: Heap Buffer Overflow ─────────────────────\n\n");

    char *buf = malloc(16);
    if (!buf) { fprintf(stderr, "  malloc failed\n"); return; }

    printf("  buf = malloc(16);  // only 16 bytes allocated\n");

    /* BUG: writing 32 bytes into a 16-byte buffer */
    memset(buf, 'A', 32);

    free(buf);
}
```

Here is exactly what happens in memory:

```
After malloc(16) without ASan:
  +----------+---------+----------+
  | metadata | 16 bytes| metadata |  (allocator bookkeeping)
  +----------+---------+----------+
               ^
               buf points here

After memset(buf, 'A', 32):
  +----------+---------+----------+
  | metadata | AAAAAAAA AAAAAAAA  |  16 bytes of 'A' overflow
  +----------+---------+----------+  into the next metadata region
               ^         ^
               valid     CORRUPTED -- allocator metadata destroyed
```

```
After malloc(16) with ASan:
  +----+---------+----+----+
  | RZ | 16 bytes| RZ | RZ |  RZ = Red Zone (poisoned)
  +----+---------+----+----+
        ^         ^
        buf       Writing here triggers ASan abort

ASan report:
  ERROR: heap-buffer-overflow on address 0x...
  WRITE of size 32 at 0x... (16 bytes past allocation)
  allocated at: lesson_08.c:XX
```

ASan reports the exact byte offset (16 bytes past the allocation boundary),
the stack trace of the write, and the stack trace of the original `malloc`.

> **Why?** In JavaScript, `ArrayBuffer` access is bounds-checked. Writing
> past the end of a `Uint8Array` has no effect. In C, the 16 bytes past
> the end of `buf` belong to whoever is next in the heap -- ASan's red zone
> in a sanitized build, or the allocator's metadata in a normal build.
> Corrupting allocator metadata can turn a simple buffer overflow into a
> remote code execution exploit.

Build and run:

```bash
./build-dev.sh --lesson=08 -r
./build/lesson_08 heap-overflow
```

ASan should abort with `heap-buffer-overflow`.

---

## Step 3 -- Stack buffer overflow

The same pattern, but with a stack array instead of a heap allocation. This
is even more dangerous because the stack contains the function's return
address.

Add this function:

```c
static void demo_stack_overflow(void) {
    printf("\n── Demo: Stack Buffer Overflow ────────────────────\n\n");

    char buf[16];

    /* BUG: writing 32 bytes into a 16-byte stack buffer */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfortify-source"
    memset(buf, 'B', 32);
#pragma clang diagnostic pop

    printf("  buf[0] = 0x%02x\n", (unsigned char)buf[0]);
}
```

The `#pragma` directives silence the compiler's fortify warning so the bug
actually reaches runtime. Without them, the compiler might reject the code
at compile time -- which is actually good, but we want to demonstrate ASan's
runtime detection.

Here is what the stack looks like:

```
Stack frame for demo_stack_overflow:

  High addresses
  +---------------------------+
  | Return address            |  ← WHERE the CPU jumps after this function
  +---------------------------+
  | Saved frame pointer (rbp) |  ← Previous function's stack frame
  +---------------------------+
  | buf[15]                   |
  | buf[14]                   |
  | ...                       |
  | buf[1]                    |
  | buf[0]                    |  ← buf starts here
  +---------------------------+
  Low addresses

  memset(buf, 'B', 32) writes 32 bytes starting at buf[0]:
  - buf[0..15]: 'B' (within bounds -- fine)
  - buf[16..31]: 'B' (OVERFLOW -- stomps saved rbp and return address)

  When the function returns, the CPU jumps to 0x4242424242424242
  instead of the caller. This is the classic stack smashing attack.
```

> **Why?** JavaScript functions do not have return addresses that you can
> overwrite. V8 manages its own call stack internally. In C, the CPU's call
> stack stores return addresses in the same memory space as local variables.
> A buffer overflow can overwrite the return address, making the function
> "return" to an attacker-controlled location. This is the most exploited
> vulnerability class in the history of computing.

Build and run:

```bash
./build/lesson_08 stack-overflow
```

ASan should abort with `stack-buffer-overflow`.

---

## Step 4 -- Uninitialized memory reads

In JavaScript, variables are initialized to `undefined` and `new
Int32Array(5)` is always zeroed. Every value has a predictable starting
state.

In C, local variables and `malloc` memory are NOT initialized:

```c
int x;           // contains whatever bits were on the stack
int *p = malloc(sizeof(int));
// *p contains whatever bits were in that heap region
```

Reading these uninitialized bits is undefined behavior. The compiler is
allowed to assume it never happens, which means it can optimize your code
in ways that break when you do read uninitialized memory.

Add this function:

```c
static void demo_uninitialized(void) {
    printf("\n── Demo: Uninitialized Memory Read ───────────────\n\n");

    int *values = malloc(5 * sizeof(int));
    if (!values) { fprintf(stderr, "  malloc failed\n"); return; }

    /* BUG: reading without writing first */
    for (int i = 0; i < 5; i++) {
        printf("    values[%d] = %d (0x%08x)\n",
               i, values[i], (unsigned)values[i]);
    }

    /* BUG: branching on uninitialized data */
    if (values[0] > 0) {
        printf("    took TRUE branch (unpredictable!)\n");
    } else {
        printf("    took FALSE branch (unpredictable!)\n");
    }

    /* Fix: use calloc for zero-initialized memory */
    int *clean = calloc(5, sizeof(int));
    if (!clean) { free(values); return; }
    printf("\n  Fix: calloc(5, sizeof(int)):\n");
    for (int i = 0; i < 5; i++) {
        printf("    clean[%d] = %d  (zeroed by calloc)\n", i, clean[i]);
    }

    free(values);
    free(clean);
}
```

```
malloc(5 * sizeof(int)) returns uninitialized memory:

  +-----+-----+-----+-----+-----+
  | ??? | ??? | ??? | ??? | ??? |  Could be anything
  +-----+-----+-----+-----+-----+
  Depends on what was previously at this address.
  Fresh process: might be zeros (OS zeroed the page).
  After allocations and frees: old data from previous allocations.

calloc(5, sizeof(int)) returns zeroed memory:

  +-----+-----+-----+-----+-----+
  |  0  |  0  |  0  |  0  |  0  |  Guaranteed zeros
  +-----+-----+-----+-----+-----+
```

The values you see from `malloc` are unpredictable. In a fresh process you
might see zeros because the OS zeroed the page. But after some allocations
and frees, you will see old data from previous allocations. The branch on
`values[0] > 0` can go either direction depending on what was previously
in that memory.

> **Why?** In JavaScript, `new Int32Array(5)` is always zeroed. `let x;`
> gives `undefined` (a predictable value). In C, you choose between
> `malloc` (fast, uninitialized) and `calloc` (slightly slower, zeroed).
> The performance difference matters when you are allocating millions of
> small buffers per frame. But the safety difference matters more: reading
> uninitialized memory is undefined behavior that can cause the compiler
> to generate code that does literally anything.

Build and run:

```bash
./build/lesson_08 uninitialized
```

The output shows garbage values and a branch that goes unpredictably. If
you build with `-fsanitize=memory` (MSan), it catches the uninitialized
read at the exact instruction.

---

## Step 5 -- The off-by-one error

The most common cause of buffer overflows is not a wild memset -- it is a
loop bound that uses `<=` instead of `<`. This is the off-by-one error.

Add this function:

```c
static void demo_off_by_one(void) {
    printf("\n── Demo: Off-By-One Error ─────────────────────────\n\n");

    int *arr = malloc(10 * sizeof(int));
    if (!arr) { fprintf(stderr, "  malloc failed\n"); return; }

    /* BUG: loop condition uses <= instead of < */
    for (int i = 0; i <= 10; i++) {
        arr[i] = i;   /* When i == 10, writes past the end */
    }

    free(arr);
}
```

```
Array of 10 ints:

  Valid indices: 0  1  2  3  4  5  6  7  8  9
  arr[10] →     ↑ ONE PAST THE END -- out of bounds

  for (int i = 0; i <= 10; ...)
                       ^^
                       Should be i < 10
                       <= means the loop runs 11 times for a 10-element array
```

An array of 10 elements has indices 0 through 9. Index 10 is one past the
end. This single-element overrun is just as dangerous as a larger overflow
-- it corrupts the first bytes of whatever follows the allocation.

The fix is trivial but easy to forget:

```c
for (int i = 0; i < 10; i++) {  /* < not <= */
    arr[i] = i;
}
```

> **Why?** In JavaScript, `Array.from({length: 10}, (_, i) => i)`
> eliminates manual indexing entirely. `for...of` loops never go out of
> bounds. In C, you write the loop bounds yourself, and getting them wrong
> by one is the most frequent source of buffer overflows. Always use `<`
> with array lengths, never `<=`. If you find yourself writing `<= size`,
> stop and check whether you mean `< size`.

Build and run:

```bash
./build/lesson_08 off-by-one
```

ASan catches the single-element overrun as `heap-buffer-overflow`.

---

## Step 6 -- How ASan red zones work

ASan places "red zones" of poisoned bytes around every allocation. These
red zones are the mechanism that makes all four demos detectable:

```
Without ASan — allocations are packed together:

  +----------+----------+----------+
  | alloc A  | alloc B  | alloc C  |
  +----------+----------+----------+
  Writing past A silently corrupts B. No detection possible.

With ASan — red zones surround every allocation:

  +----+--------+----+----+--------+----+----+--------+----+
  | RZ | alloc A| RZ | RZ | alloc B| RZ | RZ | alloc C| RZ |
  +----+--------+----+----+--------+----+----+--------+----+
    ^-- poisoned       ^-- poisoned       ^-- poisoned
    Writing past A hits red zone → immediate abort with stack trace!
```

For each byte in the process's address space, ASan maintains a "shadow
byte" that says whether the real byte is valid to access. Before every load
and store, the compiler inserts a check of the shadow byte. If the byte is
poisoned (red zone, freed memory, stack padding), ASan aborts immediately.

The red zones add memory overhead (typically 2-3x) but catch overflows at
the exact instruction that causes them, with both the access stack trace
and the allocation stack trace.

> **Why?** This is the same principle as JavaScript's bounds checking, but
> implemented at the compiler level rather than the language level. JS
> checks `index < array.length` on every access. ASan checks
> `shadow[addr] == 0` on every access. Both catch out-of-bounds writes.
> The difference is that JS checks are always on, while ASan checks are
> optional and add overhead. In production C code, you rely on getting the
> bounds right. During development, ASan verifies that you did.

---

## Step 7 -- Wire up main and run all demos

Add the dispatch logic:

```c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("  Usage: ./build/lesson_08 <demo>\n");
        printf("  Demos: heap-overflow  stack-overflow  ");
        printf("uninitialized  off-by-one\n");
        return 0;
    }

    const char *demo = argv[1];

    if      (strcmp(demo, "heap-overflow")  == 0)  demo_heap_overflow();
    else if (strcmp(demo, "stack-overflow") == 0)  demo_stack_overflow();
    else if (strcmp(demo, "uninitialized")  == 0)  demo_uninitialized();
    else if (strcmp(demo, "off-by-one")     == 0)  demo_off_by_one();
    else fprintf(stderr, "  Unknown demo: '%s'\n", demo);

    return 0;
}
```

Build and run all four:

```bash
./build-dev.sh --lesson=08 -r
./build/lesson_08 heap-overflow     # ASan: heap-buffer-overflow
./build/lesson_08 stack-overflow    # ASan: stack-buffer-overflow
./build/lesson_08 uninitialized     # garbage values, random branch
./build/lesson_08 off-by-one        # ASan: heap-buffer-overflow
```

Each demo except `uninitialized` should trigger an ASan abort. The
`uninitialized` demo prints garbage values and takes an unpredictable
branch -- to catch it at runtime, you would need MSan
(`-fsanitize=memory`) instead of ASan.

---

## Build and run

```bash
./build-dev.sh --lesson=08 -r
./build/lesson_08 heap-overflow     # ASan: heap-buffer-overflow
./build/lesson_08 stack-overflow    # ASan: stack-buffer-overflow
./build/lesson_08 uninitialized     # garbage values, random branch
./build/lesson_08 off-by-one        # ASan: heap-buffer-overflow
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Stack overflow demo not caught | Compiler optimized away the buffer or inlined `memset` | Build with `-O0` and ASan enabled |
| Uninitialized values are always 0 | Debug allocator or OS zeroed the page | Run in release mode or allocate/free several times first to get stale data |
| Off-by-one not caught without ASan | The overwrite lands in padding between heap allocations | Always develop with `-fsanitize=address` to catch these reliably |
| `memset` warning on stack demo | Compiler's fortify-source detects the overflow at compile time | Use the `#pragma` to suppress the warning, or build with `-U_FORTIFY_SOURCE` |

---

## Exercises

1. **Heap underflow.** Allocate 16 bytes, then write to `buf[-1]`. Does
   ASan catch it? What does the report say? (Hint: "heap-buffer-overflow"
   on the left side of the allocation.)

2. **String overflow.** Allocate `char *buf = malloc(8)` and then call
   `strcpy(buf, "This string is way too long")`. Run with ASan. The
   overflow happens at the NUL terminator boundary -- how many bytes past
   the end does ASan report?

3. **Calloc vs memset.** Benchmark `calloc(1000, sizeof(int))` vs
   `malloc(1000 * sizeof(int))` followed by `memset(p, 0, ...)` in a
   loop of 1 million iterations. Is there a measurable difference? (Hint:
   `calloc` can sometimes skip the zeroing if the OS already zeroed the
   page.)

4. **Find the off-by-one.** Write a function that takes a string and copies
   it into a `malloc`'d buffer of `strlen(str)` bytes (not `strlen(str)+1`).
   What happens? The NUL terminator is the off-by-one.

---

## Connection to your work

In a game engine, buffer overflows and uninitialized reads cause the
most baffling bugs:

- A **heap overflow** in the texture loader corrupts the next allocation,
  which happens to be an entity's position data. The entity teleports to
  coordinates derived from corrupted texture bytes. The texture looks fine.
  The entity's position makes no sense. You spend hours looking at the
  wrong system.

- A **stack overflow** in a recursive pathfinding algorithm overwrites the
  return address. The game crashes in `glSwapBuffers` or `XNextEvent` --
  nowhere near the actual bug.

- An **uninitialized read** in the particle system makes particles spawn
  at random positions on the first frame, then look correct after that
  (because the memory gets properly written on subsequent frames). QA
  reports "particles flash on first frame" and you cannot reproduce it
  because your test launches into a different scene.

The debug allocator you build in Lesson 09 adds canary values around every
allocation to catch overflows without ASan's overhead. The arena allocator
from Lessons 10-12 sidesteps many of these bugs by eliminating individual
frees entirely.

---

## What's next

In Lesson 09, you build a debug allocator that wraps `malloc` with canary
values and fill patterns. It catches buffer overflows and use-after-free
without ASan's 2-3x memory overhead -- lightweight enough to leave enabled
in development builds.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_08.c`. It contains
everything covered in this lesson, including all four demo functions and
the argv dispatch logic.
