# Lesson 02 -- Stack Allocation Mechanics

> **What you will build:** A program that observes stack frame creation and
> destruction across nested function calls, measures frame sizes, demonstrates
> `alloca` and VLAs for dynamic stack allocation, and (optionally) triggers a
> stack overflow crash.

## What you'll learn

- How stack frames are created on function call and destroyed on return
- The stack grows downward on x86-64 -- deeper calls produce lower addresses
- How local array sizes directly consume stack space (unlike JS)
- `alloca()` and Variable-Length Arrays (VLAs) -- dynamic stack allocation
  with automatic lifetime
- Stack overflow: what it looks like in C vs JavaScript
- `volatile` and `__attribute__((noinline))` as tools for preventing compiler
  optimizations that hide the behavior you want to observe

## Prerequisites

- Lesson 01 (memory layout -- you need to know which segment the stack is)
- A working build system (`./build-dev.sh`)

---

## Step 1 -- Stack frames: the JS model you already know

Before we write any C, let's make sure you understand what a stack frame IS,
because you already know this concept from JavaScript.

Open Chrome DevTools, set a breakpoint inside a deeply nested callback, and
look at the Call Stack panel:

```
Call Stack (Chrome DevTools):
  innerCallback        <-- top of stack (most recent call)
  processItem
  forEach
  handleResponse
  fetchData
  (anonymous)          <-- bottom of stack (program entry)
```

Each entry is a **stack frame**. When `innerCallback` finishes executing, its
frame disappears and control returns to `processItem`. This is exactly what
happens in C -- with one critical difference: in C, you can take the address of
a local variable in each frame and watch the addresses change.

A stack frame contains three things:

1. **Return address** -- where to jump back after the function returns
2. **Saved registers** -- CPU register values the function needs to preserve
3. **Local variables** -- every `int`, `char`, array, etc. declared inside the
   function

```
What the stack looks like with 3 nested calls:

High addresses  (top of memory)
+-----------------------+
|   main() frame        |  return addr + saved regs + locals
|   +-------------------+
|   | depth_1() frame   |  return addr + saved regs + locals
|   | +-----------------+
|   | | depth_2() frame |  return addr + saved regs + locals
|   | | +---------------+
|   | | | depth_3()     |  return addr + saved regs + locals
+---+-+-+---------------+
Low addresses  (stack grows this way --->)
```

Each new call pushes a frame BELOW the previous one. When `depth_3` returns,
its frame is "popped" -- the stack pointer moves back up, and that memory
becomes invalid.

> **Why?** In JavaScript, when you throw an error and look at the stack trace,
> you see function names and line numbers. In C, a stack frame is raw memory
> at a concrete address. You can take the address of a local variable inside
> `depth_3` and it will be LOWER than one in `main`. This is not just trivia --
> when you see a segfault in a game engine, knowing that stack addresses are
> always in the `0x7fff...` range (on x86-64) tells you instantly whether a
> corrupted pointer was supposed to be a stack variable.

---

## Step 2 -- Observing stack growth with noinline functions

Now let's write the code. Start with the includes and the nested functions:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <alloca.h>
#include "common/print_utils.h"
#include "common/bench.h"
```

Now define four functions that call each other in sequence. We need the
`__attribute__((noinline))` annotation on each one:

```c
__attribute__((noinline))
void depth_3(void) {
    volatile int local = 3;
    printf("  depth_3:  &local = %p  (value=%d)\n",
           (void *)&local, local);
}

__attribute__((noinline))
void depth_2(void) {
    volatile int local = 2;
    printf("  depth_2:  &local = %p  (value=%d)\n",
           (void *)&local, local);
    depth_3();
}

__attribute__((noinline))
void depth_1(void) {
    volatile int local = 1;
    printf("  depth_1:  &local = %p  (value=%d)\n",
           (void *)&local, local);
    depth_2();
}
```

Two important annotations to understand:

**`__attribute__((noinline))`** tells the compiler "do NOT inline this
function." Without it, the compiler might copy the function body directly into
the caller, collapsing multiple frames into one. That would defeat the purpose
of our experiment -- we want to see separate frames at separate addresses.

**`volatile int local`** tells the compiler "do not optimize away this
variable." Without `volatile`, the compiler might notice that `local` is only
used in the `printf` call and decide to keep it in a CPU register instead of
giving it a stack address. We need it to have a stack address so we can print it.

> **Why?** In JavaScript, V8 decides everything about optimization and you
> never need to fight it. In C with optimizations enabled (`-O2`), the compiler
> is aggressive. It will inline functions, eliminate variables, reorder code,
> and generally make your program faster at the cost of making debugging harder.
> `volatile` and `noinline` are escape hatches that say "I know what I'm doing,
> do not optimize this specific thing."

Now in `main`, call the chain:

```c
int main(int argc, char *argv[]) {
    printf("=== Lesson 02 -- Stack Allocation Mechanics ===\n");

    print_section("Stack Growth Direction (nested calls)");
    printf("  Calling depth_1 -> depth_2 -> depth_3:\n\n");
    volatile int main_local = 0;
    printf("  main:     &local = %p  (value=%d)\n",
           (void *)&main_local, main_local);
    depth_1();
    printf("\n  Observation: addresses DECREASE with deeper calls.\n");
    printf("  The stack grows DOWNWARD on x86-64.\n");
```

### Build and run checkpoint

```bash
./build-dev.sh --lesson=02 -r
```

You should see four addresses, decreasing from `main` to `depth_3`:

```
  main:     &local = 0x7fffffffe3ac  (value=0)
  depth_1:  &local = 0x7fffffffe36c  (value=1)
  depth_2:  &local = 0x7fffffffe32c  (value=2)
  depth_3:  &local = 0x7fffffffe2ec  (value=3)
```

Each address is lower than the previous one. The stack grows DOWNWARD.

```
Before calls:               After depth_1 -> depth_2 -> depth_3:

0x7fff...e3ac [main]        0x7fff...e3ac [main]
                            0x7fff...e36c [depth_1]
                            0x7fff...e32c [depth_2]
                            0x7fff...e2ec [depth_3]  <-- stack pointer
              ^                           ^
              |                           |
         stack pointer            stack grew DOWN
```

---

## Step 3 -- Frame size depends on local variables

In JavaScript, local variable size does not matter. Whether you declare
`const x = 5` or `const arr = new Array(1000000)`, the local variable is just
a reference (8 bytes). The actual data lives on the V8 heap.

In C, local arrays live DIRECTLY ON THE STACK. A `char buf[4096]` inside a
function consumes 4096 bytes of stack space. This is the most important
difference between JS and C stack usage.

Add these two functions:

```c
__attribute__((noinline))
void small_frame(void) {
    volatile char buf[16];
    buf[0] = 'A';
    printf("  small_frame (16 B):  &buf = %p\n", (void *)buf);
}

__attribute__((noinline))
void large_frame(void) {
    volatile char buf[4096];
    buf[0] = 'B';
    printf("  large_frame (4 KB):  &buf = %p\n", (void *)buf);
}
```

And call them from `main`:

```c
    print_section("Frame Size vs Local Arrays");
    small_frame();
    large_frame();
    printf("\n  Larger local arrays push the stack pointer further down.\n");
```

### Build and run checkpoint

```bash
./build-dev.sh --lesson=02 -r
```

You will see that `large_frame`'s buffer address is much lower than
`small_frame`'s -- the 4 KB buffer pushed the stack pointer way down.

```
Before small_frame:     After small_frame:     After large_frame:

  [main]                  [main]                 [main]
  |                       [small_frame]          [small_frame returned]
  v                        16B buf               [large_frame]
  stack pointer           |                       4096B buf
                          v                      |
                          stack pointer          v
                                                 stack pointer (much lower!)
```

> **Why?** This has direct implications for game engines. If you write a
> recursive pathfinding function with `char buffer[8192]` as a local variable,
> each recursion level eats 8 KB of your 8 MB stack limit. You run out after
> about 1000 levels. In JavaScript, `const buffer = new Uint8Array(8192)` puts
> the data on the heap, so recursion depth is only limited by frame overhead
> (~dozens of bytes), not buffer size. In C, prefer heap allocation for large
> buffers inside recursive functions.

---

## Step 4 -- alloca: dynamic stack allocation

`alloca(n)` is like `malloc(n)` but on the stack instead of the heap. It bumps
the stack pointer down by `n` bytes and returns a pointer to that memory. The
critical difference: the memory automatically vanishes when the function
returns. No `free()` needed -- or possible.

Add the `alloca` demo function:

```c
__attribute__((noinline))
void alloca_demo(size_t n) {
    printf("\n  alloca(%zu bytes):\n", n);
    volatile char *buf = (char *)alloca(n);
    buf[0] = 'C';
    buf[n - 1] = 'D';
    printf("    &buf = %p  (first=%c, last=%c)\n",
           (void *)buf, buf[0], buf[n - 1]);
    printf("    NOTE: This memory lives ON THE STACK.\n");
    printf("    It vanishes when this function returns.\n");
}
```

Call it from `main`:

```c
    print_section("alloca() -- Dynamic Stack Allocation");
    alloca_demo(64);
    alloca_demo(1024);
    printf("\n  alloca is fast (just bumps the stack pointer) but risky:\n");
    printf("  - No size limit check\n");
    printf("  - Freed automatically on function return\n");
    printf("  - Not portable (not in C standard)\n");
```

Let's understand what `alloca` does mechanically:

```
Before alloca(64):          After alloca(64):

+-------------------+       +-------------------+
| alloca_demo frame |       | alloca_demo frame |
| local vars        |       | local vars        |
+-------------------+       +-------------------+
| (free stack)      |       | 64 bytes from     |
|                   |       | alloca            |
                            +-------------------+
                            | (free stack)      |
         ^                               ^
         |                               |
    stack pointer                   stack pointer
    (higher)                        (64 bytes lower)
```

The key facts about `alloca`:

- **No `free()` needed or possible.** The memory dies when the function
  returns, because the stack pointer moves back up.
- **Not in the C standard.** It is a POSIX extension. It exists on Linux and
  macOS but may not exist on embedded systems.
- **No overflow protection.** `malloc` returns NULL on failure. `alloca` with
  a huge size silently overflows the stack. No error, just a crash (or worse,
  silent memory corruption).
- **Many codebases ban it.** The Linux kernel coding guidelines, CERT C
  guidelines, and most game engine codebases forbid `alloca`.

> **Why?** In JavaScript, the closest equivalent would be if you could say
> "give me a temporary ArrayBuffer that automatically disappears when this
> function returns." That does not exist in JS because the garbage collector
> handles lifetimes. In C, `alloca` gives you exactly that -- automatic-
> lifetime memory on the stack. But the danger is real: there is no bounds
> check, no error return, no safety net. In production, use arena allocators
> (Lesson 10+) instead.

---

## Step 5 -- Variable-Length Arrays (VLAs)

VLAs are a C99 feature that provide similar functionality to `alloca`, but with
standard C syntax:

```c
__attribute__((noinline))
void vla_demo(int n) {
    printf("\n  VLA with %d elements:\n", n);
    int arr[n];    /* size determined at runtime, allocated on the stack */
    for (int i = 0; i < n; i++) arr[i] = i * i;
    printf("    &arr[0]    = %p\n", (void *)&arr[0]);
    printf("    &arr[%d] = %p\n", n - 1, (void *)&arr[n - 1]);
    printf("    Total size: %zu bytes\n", (size_t)n * sizeof(int));
    printf("    Sum: %d (sanity check)\n", arr[0] + arr[n - 1]);
}
```

Call it from `main`:

```c
    print_section("Variable-Length Arrays (VLA)");
    vla_demo(10);
    vla_demo(100);
    printf("\n  VLAs are in the C99/C11 standard but share alloca's risks.\n");
    printf("  Many codebases (Linux kernel, CERT C) forbid VLAs.\n");
```

The syntax `int arr[n]` where `n` is a runtime variable creates a variable-
length array. Unlike a fixed-size array like `int arr[10]` where the compiler
knows the size at compile time, a VLA's size is computed when the function
runs.

```
VLA with n=10:                   VLA with n=100:

+-------------------+            +-------------------+
| vla_demo frame    |            | vla_demo frame    |
| int arr[10]       |            | int arr[100]      |
| = 40 bytes        |            | = 400 bytes       |
+-------------------+            +-------------------+
```

> **Why?** In TypeScript, `const arr = new Array(n)` allocates on the heap
> with GC tracking. The local variable `arr` is just an 8-byte reference. In
> C, `int arr[n]` puts the ENTIRE array on the stack. If `n` comes from user
> input and is `1000000`, that is 4 MB of stack consumed in one function call.
> VLAs share all of `alloca`'s risks. The Linux kernel removed ALL VLAs in
> 2018 after years of effort. Use heap allocation or arenas for runtime-sized
> buffers.

### Build and run checkpoint

```bash
./build-dev.sh --lesson=02 -r
```

You should see the VLA addresses living on the stack (high `0x7fff...`
addresses), with the 100-element VLA consuming 400 bytes vs 40 bytes for the
10-element VLA.

---

## Step 6 -- Stack overflow: SIGSEGV vs RangeError

The stack on Linux has a default size limit of 8 MB (check with
`ulimit -s`). When your program exceeds this limit, the OS kills it with a
`SIGSEGV` signal -- a segmentation fault. There is no exception to catch, no
error message, no recovery. Your process simply dies.

Compare this to JavaScript:

```js
function overflow(n) { return overflow(n + 1); }
overflow(0);
// RangeError: Maximum call stack size exceeded
// (Catchable! Your program can recover!)
```

In C, the same infinite recursion is a fatal crash:

```c
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
__attribute__((noinline))
int recursive_overflow(int depth) {
    volatile char buf[1024];   /* 1 KB per frame */
    buf[0] = (char)depth;
    if (depth % 1000 == 0) {
        printf("  depth=%d  &buf=%p\n", depth, (void *)buf);
    }
    return recursive_overflow(depth + 1);
}
#pragma clang diagnostic pop
```

Each call allocates a 1 KB local buffer. At ~8000 frames deep (1 KB * 8000 =
8 MB), the stack limit is exhausted and the program crashes.

The `#pragma` lines tell the Clang compiler to suppress the "infinite
recursion" warning. We KNOW it is infinite -- that is the point.

Add the overflow demo to `main` (guarded by a command-line argument so it does
not crash every time):

```c
    int do_overflow = (argc > 1 && strcmp(argv[1], "overflow") == 0);

    print_section("Stack Size Limits");
    printf("  Default stack size on Linux: typically 8 MB (ulimit -s)\n");
    printf("  Each function call pushes: return addr + saved regs + locals\n");
    printf("  A 1 KB local buffer x 8000 recursions = 8 MB -> overflow!\n");

    if (do_overflow) {
        print_section("Stack Overflow Demo (WILL CRASH)");
        printf("  Recursing with 1 KB frames until crash...\n\n");
        recursive_overflow(0);
        /* Never reaches here */
    } else {
        print_section("Stack Overflow Demo (skipped)");
        printf("  Pass \"overflow\" as argument to trigger:\n");
        printf("    ./build/lesson_02 overflow\n");
        printf("  Build with --no-asan to see the raw SIGSEGV.\n");
    }

    printf("\n");
    return 0;
}
```

### Build and run checkpoint

Normal run (safe):
```bash
./build-dev.sh --lesson=02 -r
```

Crash demo (will terminate with SIGSEGV):
```bash
./build-dev.sh --lesson=02 --no-asan -r
# Then run: ./build/lesson_02 overflow
```

You will see the depth counter climbing, the address of `buf` dropping lower
and lower, and then the program dies. No error message from C -- just the
shell reporting "Segmentation fault."

```
Typical output before crash:

  depth=0     &buf=0x7fffffffe110
  depth=1000  &buf=0x7fffffffb110   (lower by ~12 KB per 1000 calls)
  depth=2000  &buf=0x7fffffff8110
  ...
  depth=7000  &buf=0x7fffff...
  Segmentation fault (core dumped)

What happened:

  0x7fff ffff e000  [main frame]
  0x7fff ffff d000  [depth=1 frame]
  0x7fff ffff c000  [depth=2 frame]
         ...           ...
  0x7fff ff80 0000  [depth=~8000]   <-- BOOM: hit the stack guard page
```

> **Why?** In JavaScript, stack overflow throws a catchable `RangeError`. You
> can wrap it in try-catch and recover. In C, stack overflow is `SIGSEGV` -- a
> signal from the operating system that means "you accessed memory you do not
> have permission to access." There is no try-catch (well, there are signal
> handlers, but they are tricky and limited). This is why recursive algorithms
> in C need careful depth limits, or must be converted to iterative form.
> Game engines usually avoid deep recursion entirely.

---

## Step 7 -- Use-after-return: the silent danger

There is one more crucial thing to understand about the stack that JavaScript
completely hides from you.

When a function returns, its stack frame is "popped." The stack pointer moves
back up. But the BYTES ARE STILL THERE -- they are not zeroed out or
overwritten (yet). They sit in memory like a ghost, containing the old values,
until the next function call overwrites them with its own frame.

This means you can accidentally return a pointer to a local variable, and it
might "work" for a while:

```c
// DO NOT DO THIS -- undefined behavior!
int *bad_function(void) {
    int local = 42;
    return &local;   // WARNING: address of stack memory returned!
}

// In the caller:
int *ptr = bad_function();
printf("%d\n", *ptr);  // Might print 42... or might print garbage
                        // Depends on whether anything else used the stack since
```

This is called **use-after-return**. The pointer is valid (it points to memory
that exists), but the lifetime of the data has ended. The next function call
will overwrite those bytes.

```
After bad_function returns:

+-------------------+
| caller's frame    |
+-------------------+
| [ghost data: 42]  |  <-- ptr points here, but this memory is "freed"
| (stack pointer    |      Next function call will overwrite it
|  is above here)   |
+-------------------+
```

In JavaScript, this cannot happen. If you create an object inside a function
and return a reference to it, the garbage collector keeps it alive as long as
the reference exists. In C, the stack does not have a garbage collector. When
the frame is gone, the data is gone -- even if you still have a pointer to it.

> **Why?** Use-after-return is one of the most insidious bugs in C. It can
> "work" in debug builds, pass all your tests, and then crash in production
> when a different function call pattern overwrites the ghost data. Address
> Sanitizer (`-fsanitize=address`) can detect this. Always compile with ASan
> during development.

---

## Build and run

Full build and run:

```bash
./build-dev.sh --lesson=02 -r
```

To trigger the stack overflow crash:

```bash
./build-dev.sh --lesson=02 --no-asan -r
# Then: ./build/lesson_02 overflow
```

---

## Expected output

```
=== Stack Growth Direction (nested calls) ===
  Calling depth_1 -> depth_2 -> depth_3:

  main:     &local = 0x7fffffffe3ac  (value=0)
  depth_1:  &local = 0x7fffffffe36c  (value=1)
  depth_2:  &local = 0x7fffffffe32c  (value=2)
  depth_3:  &local = 0x7fffffffe2ec  (value=3)

  Observation: addresses DECREASE with deeper calls.
  The stack grows DOWNWARD on x86-64.

=== Frame Size vs Local Arrays ===
  small_frame (16 B):  &buf = 0x7fffffffe380
  large_frame (4 KB):  &buf = 0x7fffffffd380

  Larger local arrays push the stack pointer further down.

=== alloca() -- Dynamic Stack Allocation ===
  alloca(64 bytes):
    &buf = 0x7fffffffe330  ...
  alloca(1024 bytes):
    &buf = 0x7fffffffdee0  ...

=== Variable-Length Arrays (VLA) ===
  VLA with 10 elements:
    &arr[0] = 0x7fffffffe370  Total size: 40 bytes
  VLA with 100 elements:
    &arr[0] = 0x7fffffffe1b0  Total size: 400 bytes
```

(Your exact addresses will differ due to ASLR.)

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| All nested functions show the same address | Compiler inlined the calls | Add `__attribute__((noinline))` or compile with `-O0` |
| `alloca` buffer survives after function returns | You stored the pointer and used it later -- undefined behavior | Never use an `alloca` pointer outside the function that created it |
| `warning: variable length array` | Compiling with `-Wpedantic` or `-Wvla` | Acknowledge the warning; VLAs are valid C99 but risky |
| Overflow demo does not crash with AddressSanitizer | ASan intercepts the fault before the raw SIGSEGV | Build with `--no-asan` to see the real crash |
| `volatile` variable optimized away anyway | Wrong compiler or optimization level | Use `volatile` AND `-O0`, or check the assembly output with `-S` |

---

## Exercises

1. **Measure exact frame sizes.** Modify `depth_1` and `depth_2` to print
   `&local` in both. Compute the difference -- that is the frame size in
   bytes. Does it match what you would expect for `return address (8 bytes)
   + saved registers + local int (4 bytes) + alignment padding`?

2. **Prove use-after-return.** Write a function that returns `&local_var`.
   Call it, then call another function with a large local buffer, then
   dereference the saved pointer. Observe that the value has changed.
   (Compile with `-O0 -fno-sanitize=address` so ASan does not intercept it.)

3. **Find the recursion limit.** The overflow demo uses 1 KB buffers. Change
   it to 1 byte buffers. How deep can you recurse before crashing? The
   difference tells you how much overhead each frame has beyond the local
   variables.

4. **Compare alloca performance.** Write a tight loop that calls `alloca(64)`
   1 million times inside separate function calls. Compare with
   `malloc(64); free(64)` 1 million times. `alloca` should be dramatically
   faster because it is just a stack pointer decrement.

---

## Connection to your work

In game engines:

- **Function calls are not free.** Each call pushes a frame. In a hot loop
  that runs every frame (60x per second), deeply nested function calls add
  measurable overhead. This is why game engines often use flat, iterative
  code instead of deeply recursive abstractions.

- **Stack size limits matter.** Recursive algorithms (pathfinding, scene graph
  traversal, BSP tree walking) must either bound their depth or use an
  explicit stack on the heap. A recursive flood-fill on a 1024x1024 grid can
  need 1 million levels of recursion -- immediate stack overflow.

- **alloca has a place -- barely.** Some engines use `alloca` for small,
  known-size temporary buffers in hot paths where the malloc overhead is
  unacceptable. But arena allocators (Lesson 10+) are almost always a better
  choice.

---

## What's next

In Lesson 03, we leave the stack and dive into the **heap** -- the other half
of runtime memory. You will learn `malloc`, `calloc`, `realloc`, and `free`,
understand the hidden metadata behind every allocation, and see the safe
`realloc` pattern that prevents memory leaks.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_02.c`. It contains
all the functions, the overflow demo (gated behind a command-line argument),
and the `print_utils.h` and `bench.h` dependencies.
