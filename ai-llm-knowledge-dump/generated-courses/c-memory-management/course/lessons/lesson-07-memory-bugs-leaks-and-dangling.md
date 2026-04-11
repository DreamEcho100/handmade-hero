# Lesson 07 -- Memory Bugs: Leaks and Dangling Pointers

> **What you will build:** A bug showcase that deliberately triggers a memory
> leak, a use-after-free, and a double-free so you can watch AddressSanitizer
> catch each one -- then a corrected version that avoids all three.

## What you'll learn

- What a memory leak is and why C has no garbage collector to save you
- What use-after-free means and why the bug often hides in testing
- What double-free does to the heap allocator's metadata
- How AddressSanitizer (ASan) detects all three bug classes at runtime
- The NULL-after-free discipline that prevents the most common mistakes
- The `goto cleanup` pattern for functions with multiple allocations

## Prerequisites

- A working C compiler (GCC or Clang) on Linux with ASan support
- Familiarity with JavaScript/TypeScript (we use JS analogies throughout)
- Lessons 01-06 completed (you understand malloc, free, and the heap)

---

## Step 1 -- The safety net you are leaving behind

In JavaScript, you literally cannot have these bugs. The garbage collector
makes them structurally impossible:

```js
const obj = { name: "hello" };
// You cannot "free" obj. The GC handles it.
// You cannot "use after free" because free does not exist.
// You cannot "double free" because there is no free to call twice.
```

The GC guarantees that memory is released when no references remain. You
pay for this guarantee with GC pauses, memory overhead, and unpredictable
latency -- but you never corrupt memory.

In C, YOU are the garbage collector. Every `malloc` must be paired with
exactly one `free` on every code path. Get it wrong and you get one of three
bugs:

```
What can go wrong in C (impossible in JS):

  1. LEAK:         malloc → never free
                   Memory grows until the process is killed

  2. USE-AFTER-FREE: malloc → free → read/write through old pointer
                     Reads garbage or corrupts unrelated data

  3. DOUBLE-FREE:  malloc → free → free again
                   Corrupts allocator metadata, enables exploits
```

These three bugs account for the majority of security vulnerabilities in C
programs. They are all consequences of one thing: the programmer controls
both allocation and deallocation, and the compiler cannot verify that every
`malloc` is paired with exactly one `free` at the right time.

> **Why?** In JavaScript, the GC scans for unreachable objects and frees
> them automatically. The trade-off is GC pauses (5-50ms in V8). In C, you
> get zero pauses but zero safety net. Game engines and real-time systems
> choose C's model because they cannot tolerate GC pauses in the middle of
> a frame. The cost is that YOU must get every malloc/free pairing right.

Create the file `src/lessons/lesson_07.c` and start with the includes:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

`stdio.h` gives us `printf`. `stdlib.h` gives us `malloc` and `free`.
`string.h` gives us `memset` and `strcpy`.

---

## Step 2 -- Triggering a memory leak

A memory leak happens when you allocate memory and never free it. The memory
stays reserved for the lifetime of the process, doing nothing useful.

Add this function to your file:

```c
static void demo_leak(void) {
    printf("\n── Demo: Memory Leak ─────────────────────────────\n\n");

    char *buf = malloc(256);
    if (!buf) { fprintf(stderr, "  malloc failed\n"); return; }

    memset(buf, 'A', 256);
    printf("  buf[0] = '%c'  (allocation is alive)\n", buf[0]);

    /* BUG: We never call free(buf). This is the leak. */
    printf("  Exiting without free(buf).\n");
}
```

Here is what happens in memory when this function runs:

```
Before demo_leak():
  HEAP: [ ... free space ... ]

After malloc(256):
  HEAP: [ ... | 256 bytes owned by buf | ... ]
                  ^
                  buf points here

After demo_leak() returns:
  HEAP: [ ... | 256 bytes LEAKED | ... ]
                  ^
                  buf was a local variable -- it is gone
                  Nobody holds this address anymore
                  The 256 bytes can NEVER be freed
```

The local variable `buf` lived on the stack. When the function returned,
`buf` was destroyed. But the 256 bytes on the heap are still marked as
"in use" by the allocator. Nobody holds the address anymore, so nobody
can ever call `free` on it.

> **Why?** In JavaScript, when a variable goes out of scope and no other
> reference exists, the GC marks the object as unreachable and reclaims it.
> In C, going out of scope destroys the pointer but NOT the pointed-to
> memory. The heap allocator has no idea that you lost the only reference.

Build and run:

```bash
./build-dev.sh --lesson=07 -r
./build/lesson_07 leak
```

ASan's leak detector (enabled via `detect_leaks=1` or `-fsanitize=leak`)
reports the allocation site at program exit, including the file name, line
number, and full stack trace. It tells you exactly which `malloc` was never
freed.

---

## Step 3 -- Triggering use-after-free

Use-after-free happens when you free memory and then read or write through
the old pointer. The pointer still holds the same numeric address, but that
memory now belongs to the allocator -- or worse, to a completely different
allocation.

Add this function:

```c
static void demo_use_after_free(void) {
    printf("\n── Demo: Use-After-Free ──────────────────────────\n\n");

    char *msg = malloc(128);
    if (!msg) { fprintf(stderr, "  malloc failed\n"); return; }

    strcpy(msg, "Hello from the heap!");
    printf("  Before free: msg = \"%s\"\n", msg);

    free(msg);
    printf("  free(msg) called.\n");

    /* BUG: reading freed memory */
    printf("  [BUG] Reading after free: msg[0] = 0x%02x\n",
           (unsigned char)msg[0]);
}
```

Here is what happens step by step:

```
After malloc(128) + strcpy:
  HEAP: [ ... | "Hello from the heap!\0" ... (128 bytes) | ... ]
                  ^
                  msg points here, data is valid

After free(msg):
  HEAP: [ ... | ??? freed region ??? (128 bytes) | ... ]
                  ^
                  msg STILL points here
                  but the memory is now owned by the allocator
                  another malloc() could reuse these exact bytes

After reading msg[0]:
  We read whatever byte happens to be at that address.
  Could be: the old 'H' (not overwritten yet)
  Could be: allocator metadata (if allocator reused it)
  Could be: data from a DIFFERENT allocation
  This is undefined behavior.
```

The treacherous part: without ASan, this often "works" in testing. The freed
memory has not been overwritten yet, so you read the old data. The bug hides
until production when another allocation reuses those bytes and suddenly your
"valid" string becomes garbage from an unrelated part of the program.

> **Why?** In JavaScript, `delete obj.name` removes a property but the
> object itself stays alive as long as any reference exists. There is no
> way to free the backing memory while a reference still points to it. In
> C, `free(msg)` releases the memory immediately. The pointer `msg` becomes
> a "dangling pointer" -- it holds an address that is no longer yours.

Build and run:

```bash
./build/lesson_07 uaf
```

ASan quarantines freed memory (marks it as "poisoned"). Any access triggers
an immediate abort with a stack trace showing both the access site AND the
original free site.

---

## Step 4 -- Triggering double-free

Double-free happens when you call `free()` twice on the same pointer. The
second call corrupts the allocator's internal data structures.

Add this function:

```c
static void demo_double_free(void) {
    printf("\n── Demo: Double-Free ─────────────────────────────\n\n");

    char *data = malloc(64);
    if (!data) { fprintf(stderr, "  malloc failed\n"); return; }

    memset(data, 'X', 64);
    printf("  First free(data)... OK.\n");
    free(data);

    printf("  [BUG] Second free(data)...\n");
    free(data);   /* BUG: double free */
}
```

Here is what the allocator sees:

```
After malloc(64):
  Allocator free list: [ block_A ] → [ block_B ] → ...
  data points to a 64-byte region NOT in the free list

After first free(data):
  Allocator free list: [ data's block ] → [ block_A ] → [ block_B ] → ...
  data's 64 bytes are back in the free list

After second free(data):
  Allocator free list: [ data's block ] → [ data's block ] → [ block_A ] → ...
                         ^                  ^
                         SAME block listed TWICE
                         The free list is now circular/corrupt
                         Next two malloc() calls return the SAME address
                         Two different parts of the program write to the same memory
```

Without ASan, the consequences range from nothing (if you are lucky) to
corrupting other allocations or enabling remote code execution (if an
attacker crafts the right input). This is one of the most dangerous bugs
in C because it silently poisons the allocator's state, and the crash
happens much later in a completely unrelated part of the program.

> **Why?** In JavaScript, there is no `free` function. Object lifetime is
> managed entirely by the GC's reachability analysis. In C, you can call
> `free` on anything -- the allocator trusts you to pass a valid, currently-
> allocated pointer. Passing the same pointer twice violates that trust and
> corrupts the allocator's bookkeeping.

Build and run:

```bash
./build/lesson_07 double
```

ASan detects that the pointer was already returned to the allocator and
aborts immediately before any corruption can spread.

---

## Step 5 -- How ASan catches all three

AddressSanitizer instruments every memory access at compile time. It is a
compiler flag, not a separate tool. When you compile with
`-fsanitize=address`, the compiler inserts checks around every read and
write.

```
How ASan works (simplified):

  Without ASan:
  +----------+----------+----------+
  | alloc A  | alloc B  | alloc C  |
  +----------+----------+----------+
  Corrupting A's neighbors is silent.

  With ASan:
  +----+--------+----+----+--------+----+----+--------+----+
  | RZ | alloc A| RZ | RZ | alloc B| RZ | RZ | alloc C| RZ |
  +----+--------+----+----+--------+----+----+--------+----+
    ^               ^
    Red Zones (poisoned bytes)
    Any read/write here → immediate abort + stack trace

  On free:
  +----+--------+----+
  | RZ | QQQQQQ | RZ |  Q = quarantined (poisoned)
  +----+--------+----+
  Reading freed memory → abort (use-after-free detected)
  Freeing again → abort (double-free detected)

  At exit:
  Any allocation not freed → reported as leak
```

ASan adds memory overhead (typically 2-3x) and runtime overhead (about 2x).
This is fine for development and testing. You would not ship a game with
ASan enabled, but you should develop with it enabled at all times.

> **Why?** In JavaScript, V8 has built-in GC safety. In C, ASan is the
> closest equivalent -- it is a compile-time flag that adds runtime safety
> checks. Think of it as `"use strict"` for memory: it makes bugs crash
> loudly instead of hiding silently. Always develop with
> `-fsanitize=address`.

---

## Step 6 -- The fixed version: NULL-after-free

Now write the corrected version that avoids all three bugs. The discipline
is simple and mechanical:

1. Every `malloc` must have a matching `free` on every code path.
2. After `free(ptr)`, immediately set `ptr = NULL`.
3. `free(NULL)` is a guaranteed no-op per the C standard.

Add this function:

```c
static void demo_fixed(void) {
    printf("\n── Demo: Fixed Versions ──────────────────────────\n\n");

    /* Fix 1: Always free what you malloc */
    char *buf = malloc(256);
    if (!buf) { fprintf(stderr, "  malloc failed\n"); return; }
    memset(buf, 'A', 256);
    printf("  buf[0] = '%c'\n", buf[0]);
    free(buf);
    buf = NULL;   /* NULL-after-free */
    printf("  free(buf); buf = NULL;  -- no leak.\n\n");

    /* Fix 2: Copy what you need BEFORE freeing */
    char *msg = malloc(128);
    if (!msg) { fprintf(stderr, "  malloc failed\n"); return; }
    strcpy(msg, "Hello from the heap!");
    char saved_char = msg[0];    /* Save before free */
    free(msg);
    msg = NULL;
    printf("  saved_char = '%c' (copied before free)\n\n", saved_char);

    /* Fix 3: NULL prevents double-free */
    char *data = malloc(64);
    if (!data) { fprintf(stderr, "  malloc failed\n"); return; }
    memset(data, 'X', 64);
    free(data);
    data = NULL;
    free(data);   /* free(NULL) is a no-op -- safe! */
    printf("  free(data) twice -- second is free(NULL), safe.\n");

    printf("\n  All three bugs avoided. ASan is happy.\n");
}
```

Here is what NULL-after-free gives you:

```
Without NULL-after-free:
  free(data);
  // data still holds 0x55555570  (dangling pointer)
  // Reading *data → use-after-free
  // free(data) again → double-free

With NULL-after-free:
  free(data);
  data = NULL;
  // data is now 0x0 (NULL)
  // Reading *data → segfault (immediate, obvious crash)
  // free(data) again → free(NULL) → no-op (safe)
```

> **Why?** In JavaScript, setting `obj = null` tells the GC "I am done
> with this." In C, setting `ptr = NULL` after free does not free the
> memory (that already happened), but it prevents the two most common
> follow-up bugs: accidentally reading through a stale pointer, and
> accidentally freeing the same pointer twice. It is the closest thing
> C has to JS's automatic memory safety.

Build and run:

```bash
./build/lesson_07 fixed
```

ASan should report zero errors. Clean exit.

---

## Step 7 -- The goto cleanup pattern

Real functions often allocate multiple resources. If any allocation fails
partway through, you need to free everything that was already allocated.
The standard C pattern uses `goto`:

```c
int process_data(void) {
    char *buf_a = NULL;
    char *buf_b = NULL;
    int result = -1;   /* assume failure */

    buf_a = malloc(256);
    if (!buf_a) goto cleanup;

    buf_b = malloc(512);
    if (!buf_b) goto cleanup;

    /* ... use buf_a and buf_b ... */
    result = 0;   /* success */

cleanup:
    free(buf_b);  /* free(NULL) is safe if buf_b was never allocated */
    free(buf_a);
    return result;
}
```

```
Control flow with goto cleanup:

  buf_a = malloc(256) ─── success ───→ buf_b = malloc(512) ─── success ───→ use both → result=0
       │                                     │
       │ fail                                │ fail
       ↓                                     ↓
    cleanup:                              cleanup:
    free(buf_b) ← buf_b is NULL, no-op   free(buf_b) ← buf_b is NULL, no-op
    free(buf_a) ← buf_a is NULL, no-op   free(buf_a) ← frees the 256 bytes
    return -1                             return -1
```

Initialize every pointer to `NULL` at the top of the function. The cleanup
label frees them all in reverse order. If an allocation failed, that pointer
is still `NULL`, and `free(NULL)` is a safe no-op. This pattern ensures
that every code path -- success or failure -- frees everything correctly.

> **Why?** In JavaScript, you use `try/finally` to ensure cleanup happens
> regardless of errors. In C, `goto cleanup` is the idiomatic equivalent.
> It looks scary if you learned "goto is evil" in school, but this is the
> one universally accepted use of goto in C. The Linux kernel uses this
> pattern extensively.

---

## Step 8 -- Wire up main and run all demos

Add the dispatch logic so you can run each demo independently:

```c
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("  Usage: ./build/lesson_07 <demo>\n");
        printf("  Demos: leak  uaf  double  fixed\n");
        return 0;
    }

    const char *demo = argv[1];

    if      (strcmp(demo, "leak")   == 0)  demo_leak();
    else if (strcmp(demo, "uaf")    == 0)  demo_use_after_free();
    else if (strcmp(demo, "double") == 0)  demo_double_free();
    else if (strcmp(demo, "fixed")  == 0)  demo_fixed();
    else fprintf(stderr, "  Unknown demo: '%s'\n", demo);

    return 0;
}
```

Build and run each demo in sequence:

```bash
./build-dev.sh --lesson=07 -r
./build/lesson_07 leak       # ASan reports leak at exit
./build/lesson_07 uaf        # ASan catches use-after-free
./build/lesson_07 double     # ASan catches double-free
./build/lesson_07 fixed      # Clean exit, no errors
```

Each buggy demo should produce an ASan diagnostic with the exact file, line
number, and stack trace of the bug. The `fixed` demo should exit cleanly
with no ASan output.

---

## Build and run

```bash
./build-dev.sh --lesson=07 -r
./build/lesson_07 leak       # ASan: leak detected at exit
./build/lesson_07 uaf        # ASan: heap-use-after-free
./build/lesson_07 double     # ASan: double-free
./build/lesson_07 fixed      # Clean — no errors
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| No ASan output for `leak` | `detect_leaks` not enabled | Set `ASAN_OPTIONS=detect_leaks=1` or build with `-fsanitize=leak` |
| `uaf` demo prints the old string without crashing | Built without `-fsanitize=address` | Ensure ASan is enabled in the build script -- without it, the bug is silent |
| `fixed` demo still reports a leak | Forgot to free on an early-return error path | Check ALL return paths between malloc and free |
| "But my program runs fine without ASan" | The bugs are real but happen to not crash on your test inputs | Memory bugs are latent. They corrupt silently and crash later in unrelated code. Always use ASan. |

---

## Exercises

1. **Add a fourth bug: dangling pointer across functions.** Write a function
   `char *make_message(void)` that mallocs a buffer, writes a string into
   it, frees it, then returns the pointer. Call it from main and try to
   print the result. Run with ASan enabled. What does the report say?

2. **Leak on error path.** Write a function that allocates two buffers. If
   the second allocation fails (simulate with `if (1)`), return early. Run
   with ASan leak detection. Fix it with the `goto cleanup` pattern.

3. **Count your leaks.** Remove the `free` call from the fixed demo's first
   block and run with `ASAN_OPTIONS=detect_leaks=1`. How many bytes does
   ASan report leaked? Does the byte count match your `malloc(256)` call?

4. **Aliased pointers.** Create two pointers to the same malloc'd region:
   `char *a = malloc(64); char *b = a;`. Free via `a`, then set `a = NULL`.
   Now read through `b`. What happens? This is why NULL-after-free does
   not catch ALL use-after-free bugs -- only the ones through the same
   pointer variable.

---

## Connection to your work

In a game engine, memory bugs are the number one cause of mysterious crashes:

- **Leaks** cause the game to consume more and more RAM each frame until
  the OS kills it or the frame rate tanks from swapping.
- **Use-after-free** causes entities to suddenly have garbage health values,
  positions to teleport to random coordinates, or textures to display as
  noise -- but only on certain frames, making the bug nearly impossible to
  reproduce.
- **Double-free** corrupts the allocator, causing a crash in a completely
  unrelated part of the code -- the audio system crashes because the physics
  system's double-free poisoned the heap metadata.

The arena allocator you will build in Lessons 10-12 eliminates all three
bug classes by design: you push allocations and reset the whole arena at
once. No individual frees means no leaks, no use-after-free, no
double-free. But first, you need to understand these bugs viscerally --
which is why this lesson exists.

---

## What's next

In Lesson 08, we tackle buffer overflows and uninitialized reads -- writing
past the end of arrays and reading memory that was never initialized. These
are the bugs that cause security exploits and data corruption, and ASan
catches them too.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_07.c`. It contains
everything covered in this lesson, including all four demo functions and
the argv dispatch logic.
