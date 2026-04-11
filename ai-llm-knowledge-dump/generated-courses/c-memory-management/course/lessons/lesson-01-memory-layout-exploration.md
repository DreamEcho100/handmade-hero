# Lesson 01 -- Memory Layout Exploration

> **What you will build:** A program that prints the addresses of variables from
> every memory segment, revealing the five-segment layout of a C process and
> proving it with `/proc/self/maps`.

## What you'll learn

- The five segments of a Linux process: TEXT, DATA, BSS, HEAP, STACK
- How to print raw pointer addresses with `%p` and `uintptr_t`
- Why the heap grows upward and the stack grows downward
- How to read `/proc/self/maps` to verify the layout from the OS side
- Why C's explicit memory layout matters for game engines and systems programming

## Prerequisites

- A working C compiler (GCC or Clang) on Linux
- Familiarity with JavaScript/TypeScript (we use JS analogies throughout)
- Lesson 00 (build system setup) completed

---

## Step 1 -- What memory looks like to a C program

In JavaScript, you never think about where your variables live in memory. V8
manages everything behind two invisible walls:

```
V8 internals (you never see this)
+-----------------------+
| Call Stack            |  <-- primitive values, function frames
| Heap                  |  <-- objects, closures, arrays
+-----------------------+
```

You cannot ask "where does this object live?" because the garbage collector
moves things around at will. If you write `const obj = { x: 1 }`, you have
zero way of knowing the byte address of that object. It could be at address
1000 right now and at address 50000 after the next GC cycle.

In C, there is no garbage collector. Every byte lives at a concrete address
you can print. And the operating system organizes your program's memory into
five distinct segments, each with a different purpose.

Here is the layout. Memorize this diagram -- it is the foundation of
everything in this course:

```
High addresses (e.g. 0x7fff...)
+----------------------------+
|          STACK             |  local variables, function frames
|            |               |  grows DOWNWARD
|            v               |
|                            |
|      (unmapped gap)        |  huge empty space
|                            |
|            ^               |
|            |               |
|          HEAP              |  malloc/free, grows UPWARD
+----------------------------+
|          BSS               |  uninitialized / zero-init globals
+----------------------------+
|          DATA              |  initialized globals (baked into ELF)
+----------------------------+
|          TEXT              |  machine code (read-only, executable)
+----------------------------+
Low addresses (e.g. 0x5555...)
```

Five segments, from bottom to top:

1. **TEXT** -- Your compiled functions as raw machine code. Read-only and
   executable. Writing to it causes a segfault.
2. **DATA** -- Global variables with non-zero initializers. Their values are
   stored directly in the binary on disk.
3. **BSS** -- Global variables that are uninitialized or set to zero. The OS
   guarantees they start at zero without the binary needing to store the zeros.
4. **HEAP** -- Where `malloc` and `free` operate. Grows upward.
5. **STACK** -- Where local variables and function call frames live. Grows
   downward from the top of user-space memory.

> **Why?** In JavaScript, V8 decides where everything lives and you cannot
> observe it. In C, the segment a variable lands in depends on how you declare
> it -- and you can print the exact byte address of anything. This is not just
> academic. When you are debugging a segfault in a game engine at 3 AM, knowing
> which segment an address belongs to tells you instantly what kind of variable
> you are looking at and what might have gone wrong.

We are not writing any code yet. We are building a mental model. Now let's
create variables that land in each of these five segments.

---

## Step 2 -- Placing variables in each segment

Create a new file or open `src/lessons/lesson_01.c`. Start with these includes
and global declarations:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common/print_utils.h"
#include "common/bench.h"
```

These headers give us `printf`, `malloc`/`free`, `uintptr_t`, and the course's
helper functions for pretty-printing.

Now declare some global variables outside of any function:

```c
/* DATA segment -- has a non-zero initializer */
int global_initialized = 42;
static int static_initialized = 99;

/* BSS segment -- no initializer, or explicitly zero */
int global_uninitialized;
static int static_uninitialized;
static int global_zero = 0;
```

And a function whose address we will print:

```c
void text_segment_function(void) {
    /* This function exists so we can print the address of code. */
}
```

Let's walk through each declaration:

- `int global_initialized = 42;` -- This has a non-zero initializer (`42`), so
  the compiler puts it in the **DATA** segment. The value `42` is literally
  stored in the compiled binary file on disk. When the OS loads your program, it
  maps this section of the binary into memory, and the variable starts life
  already containing `42`.

- `static int static_initialized = 99;` -- Same deal. The `static` keyword
  limits visibility to this file, but it still lives in DATA because it has a
  non-zero initializer.

- `int global_uninitialized;` -- No initializer. This goes in the **BSS**
  segment. The OS guarantees it starts at zero, but the binary does not need to
  store that zero. This is a space optimization.

- `static int global_zero = 0;` -- Even though you wrote `= 0`, the compiler
  is smart enough to know this is the same as "starts at zero." It goes in BSS,
  not DATA.

- `text_segment_function` -- The compiled machine code for this function lives
  in the **TEXT** segment. We can take its address and print it.

> **Why?** In JavaScript, `const MAGIC = 42` at module scope goes on the V8
> heap. There is no distinction between "initialized" and "uninitialized"
> globals -- the engine handles it all. In C, this distinction is meaningful.
> A 10 MB array of zeros declared globally takes zero bytes in BSS but 10 MB in
> DATA if you accidentally initialize even one element to a non-zero value.
> Game engines with large lookup tables care about this.

```
Where each variable lands:

Declaration                    | Segment | Why
-------------------------------|---------|------------------------------------------
int global_initialized = 42    | DATA    | Non-zero initializer baked into binary
static int static_initialized  | DATA    | Same -- static just limits visibility
int global_uninitialized       | BSS     | No initializer -- OS zeros it for free
static int global_zero = 0     | BSS     | Zero initializer counts as "no init"
text_segment_function          | TEXT    | Compiled machine code
```

---

## Step 3 -- Printing addresses with %p

Now let's write `main()` and print the address of every variable we declared.
Add this to your file:

```c
int main(void) {
    printf("=== Lesson 01 -- C Memory Layout Exploration ===\n");

    /* STACK segment -- local variables */
    int stack_var_a = 10;
    int stack_var_b = 20;
    char stack_array[64];
    stack_array[0] = 'A';  /* touch it so compiler doesn't optimize away */

    /* HEAP segment -- dynamic allocations */
    void *heap_ptr_a = malloc(64);
    void *heap_ptr_b = malloc(1024);
    void *heap_ptr_c = malloc(64 * 1024);
```

Here is what we just created:

- `stack_var_a`, `stack_var_b`, `stack_array` -- These are local variables
  inside `main()`. They live on the **STACK**.
- `heap_ptr_a`, `heap_ptr_b`, `heap_ptr_c` -- These are pointers returned by
  `malloc()`. The pointers themselves are on the stack (they are local
  variables), but the memory they point to is on the **HEAP**.

Now print the TEXT segment addresses:

```c
    print_section("TEXT segment (code)");
    print_ptr("main()",                  (void *)(uintptr_t)main);
    print_ptr("text_segment_function()", (void *)(uintptr_t)text_segment_function);
    print_ptr("printf()",               (void *)(uintptr_t)printf);
```

Let's break down `(void *)(uintptr_t)main`:

1. `main` without parentheses is a **function pointer** -- a pointer to the
   first machine instruction of the `main` function. In JavaScript, a function
   name without `()` gives you a reference you can pass around. In C, it gives
   you the raw memory address of the compiled code.

2. We cast through `uintptr_t` (an integer type guaranteed to hold a pointer)
   and then to `void *` because the `%p` format specifier expects `void *`.
   Function pointers and data pointers are technically different types in C.

> **Why?** In JavaScript, there is no way to ask "what memory address does this
> function live at?" Functions are objects on the heap, and their address is
> hidden from you. In C, a function name IS a pointer to its first instruction.
> You can print it, store it, and even call functions through pointers -- which
> is how vtables, callbacks, and plugin systems work in game engines.

Now print DATA and BSS:

```c
    print_section("DATA segment (initialized globals)");
    print_ptr("global_initialized",  &global_initialized);
    print_ptr("static_initialized",  &static_initialized);

    print_section("BSS segment (uninitialized/zero globals)");
    print_ptr("global_uninitialized", &global_uninitialized);
    print_ptr("static_uninitialized", &static_uninitialized);
    print_ptr("global_zero",          &global_zero);
    printf("  (BSS values are zero: global_uninitialized=%d, global_zero=%d)\n",
           global_uninitialized, global_zero);
```

The `&` operator takes the address of a variable. `&global_initialized` gives
you a pointer to where `global_initialized` lives in memory. The `print_ptr`
helper formats it with `%p`.

Notice we also print the VALUES of the BSS variables to prove they are zero.
The OS guarantees this. You did not initialize them, and they are zero anyway.

Now print HEAP and STACK:

```c
    print_section("HEAP segment (malloc)");
    print_ptr("heap_ptr_a (64 B)",     heap_ptr_a);
    print_ptr("heap_ptr_b (1 KB)",     heap_ptr_b);
    print_ptr("heap_ptr_c (64 KB)",    heap_ptr_c);

    print_section("STACK segment (local variables)");
    print_ptr("stack_var_a",   &stack_var_a);
    print_ptr("stack_var_b",   &stack_var_b);
    print_ptr("stack_array",   stack_array);
```

For heap pointers, we print the pointer VALUE (where `malloc` put the memory).
For stack variables, we print the ADDRESS of the variable itself (`&stack_var_a`).

> **Why?** The `&` operator is the single most important operator for
> understanding C memory. In JavaScript, there is no equivalent -- you cannot
> take the address of `let x = 5`. In C, `&x` gives you the exact byte
> position in the process address space. Master this and you understand
> pointers. Pointers are just addresses.

---

## Step 4 -- The address ordering summary

Add this block to see all five segments in order:

```c
    print_section("Address Ordering Summary");
    printf("  Typical Linux x86-64 layout (low to high):\n\n");
    printf("    TEXT   (code)              ~ %p\n", (void *)(uintptr_t)main);
    printf("    DATA   (init globals)      ~ %p\n", (void *)&global_initialized);
    printf("    BSS    (zero globals)      ~ %p\n", (void *)&global_uninitialized);
    printf("    HEAP   (malloc, grows up)  ~ %p\n", heap_ptr_a);
    printf("    ...    (unmapped gap)\n");
    printf("    STACK  (locals, grows dn)  ~ %p\n", (void *)&stack_var_a);
```

When you run this, you will see something like:

```
    TEXT   (code)              ~ 0x5555555551a9
    DATA   (init globals)      ~ 0x555555558010
    BSS    (zero globals)      ~ 0x55555555801c
    HEAP   (malloc, grows up)  ~ 0x5555555592a0
    ...    (unmapped gap)
    STACK  (locals, grows dn)  ~ 0x7fffffffe3ac
```

Look at those addresses carefully:

```
Segment    Address prefix    Position
-------    ---------------   --------
TEXT       0x5555 5555 1...  lowest
DATA       0x5555 5558 0...  slightly higher
BSS        0x5555 5558 0...  just above DATA
HEAP       0x5555 5559 2...  above BSS
STACK      0x7fff ffff e...  WAY higher
```

The TEXT, DATA, BSS, and HEAP addresses all start with `0x5555...`. They are
relatively close together. But the STACK address starts with `0x7fff...` --
that is a massive jump. The OS deliberately leaves a huge unmapped gap between
the heap and the stack so they can grow toward each other without colliding.

> **Why?** This gap is not wasted physical RAM. It is unmapped virtual address
> space -- just numbers that do not correspond to any physical memory. The OS
> only allocates physical pages when you actually write to an address. So a
> "gap" of terabytes costs nothing. This is the key insight of virtual memory,
> which we explore fully in Lesson 05.

```
Address space visualization (not to scale):

0x0000...          0x5555...                          0x7fff...
|                  |                                  |
v                  v                                  v
[  unused  ] [ TEXT DATA BSS HEAP ----> .... <---- STACK ]
                                   ^               ^
                                   |               |
                              grows up        grows down
```

---

## Step 5 -- The heap grows up, the stack grows down

Now let's prove the growth directions. Add the stack direction test:

```c
    print_section("Stack Growth Direction");
    volatile int outer_var = 0;
    printf("  &outer_var (main):    %p\n", (void *)&outer_var);
    printf("  &stack_var_a:         %p\n", (void *)&stack_var_a);
    printf("  &stack_var_b:         %p\n", (void *)&stack_var_b);

    ptrdiff_t stack_diff = (uint8_t *)&stack_var_a - (uint8_t *)&stack_var_b;
    printf("  Difference (a - b):   %td bytes\n", stack_diff);
    printf("  (Variables declared earlier may have higher addresses,\n");
    printf("   but this is compiler-dependent. The key insight is that\n");
    printf("   deeper function calls use LOWER addresses.)\n");
```

The `ptrdiff_t` type is specifically designed to hold the difference between
two pointers. We cast to `uint8_t *` so the difference is measured in bytes
(not in `int`-sized units). The `%td` format specifier prints a `ptrdiff_t`.

Important: within a single function, the compiler can reorder local variables
however it wants. You might see `stack_var_a` at a higher address than
`stack_var_b`, or vice versa. The ordering within one frame is
compiler-dependent. What IS guaranteed (on x86-64) is that when you call
another function, that function's frame will be at LOWER addresses. We prove
this in Lesson 02.

Now add the heap direction test:

```c
    print_section("Heap Growth Direction");
    printf("  heap_ptr_a: %p\n", heap_ptr_a);
    printf("  heap_ptr_b: %p\n", heap_ptr_b);
    printf("  heap_ptr_c: %p\n", heap_ptr_c);
    ptrdiff_t heap_diff = (uint8_t *)heap_ptr_b - (uint8_t *)heap_ptr_a;
    printf("  Difference (b - a):   %td bytes\n", heap_diff);
    printf("  (Heap generally grows upward, but malloc may use different\n");
    printf("   arenas/bins, so addresses aren't always strictly increasing.)\n");
```

You should see `heap_ptr_b` at a higher address than `heap_ptr_a` (positive
difference), confirming upward growth. However, `heap_ptr_c` (64 KB) might
jump to a completely different region because glibc uses `mmap` for large
allocations (typically > 128 KB). This is normal and expected.

> **Why?** In JavaScript, `const a = []; const b = []` -- you have no idea
> where `a` and `b` live relative to each other. V8 might put them next to
> each other, or on opposite sides of the heap. In C, `malloc` calls generally
> return ascending addresses (heap grows up), and nested function calls produce
> descending stack addresses (stack grows down). Understanding this lets you
> predict cache behavior and debug memory corruption.

```
Before our allocations:           After our allocations:

   HEAP start                        HEAP start
   +--------+                        +--------+
   |        |                        | 64B    |  <- heap_ptr_a
   +--------+                        +--------+
                                     | 1KB    |  <- heap_ptr_b
                                     +--------+
                                     | 64KB   |  <- heap_ptr_c (maybe mmap'd elsewhere)
                                     +--------+
                                          ^
                                     heap grows UP
```

---

## Step 6 -- Reading /proc/self/maps

This is the most powerful debugging technique in this lesson. Linux exposes a
pseudo-file at `/proc/self/maps` that lists every mapped region of the current
process. Add this code:

```c
    print_section("Process Memory Map (Linux: /proc/self/maps)");
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[256];
        int count = 0;
        printf("  (Showing first 20 lines)\n\n");
        while (fgets(line, sizeof(line), maps) && count < 20) {
            printf("  %s", line);
            count++;
        }
        if (!feof(maps)) printf("  ... (truncated)\n");
        fclose(maps);
    } else {
        printf("  /proc/self/maps not available (not on Linux?).\n");
        printf("  On macOS, try: vmmap <pid>\n");
    }
```

Let's break this down:

- `/proc/self/maps` is not a real file on disk. It is a virtual file that the
  Linux kernel generates on the fly. `self` refers to the currently running
  process (the one reading the file).

- Each line looks like:
  ```
  55555555a000-55555557b000 rw-p 00000000 00:00 0   [heap]
  7ffff7d90000-7ffff7f22000 r-xp 00000000 08:01 1234 /lib/x86_64-linux-gnu/libc.so.6
  ```

- The first column is the address range. The second column is permissions:
  `r` = read, `w` = write, `x` = execute, `p` = private (copy-on-write).

- `r-xp` = read + execute + private = TEXT segment (code you can run but not
  modify)
- `rw-p` = read + write + private = DATA, BSS, HEAP, STACK segments
- The label at the end (like `[heap]` or `[stack]`) confirms which segment you
  are looking at.

> **Why?** In JavaScript, you never see this because V8 manages it all
> internally. The closest thing is Chrome DevTools' "Memory" tab, but that
> shows V8's view of objects, not the OS's view of pages. In C and systems
> programming, reading `/proc/self/maps` is a standard debugging technique.
> When you get a segfault at address `0x7fff...something`, you check the maps
> file to see if that address falls in a valid region.

Finally, clean up and return:

```c
    free(heap_ptr_a);
    free(heap_ptr_b);
    free(heap_ptr_c);

    printf("\n");
    return 0;
}
```

Every `malloc` needs a matching `free`. We will drill this into your brain
throughout the course.

---

## Step 7 -- Build and run

Compile and run the complete program:

```bash
./build-dev.sh --lesson=01 -r
```

You should see output organized into labeled sections. Here is what to look
for:

1. **TEXT addresses** start with `0x5555 5555 1...` -- these are your compiled
   functions.

2. **DATA and BSS** are close together, just above TEXT.

3. **HEAP** is above BSS. The three malloc addresses should generally increase.

4. **STACK** has addresses starting with `0x7fff...` -- way above everything
   else.

5. The `/proc/self/maps` output shows the same address ranges with permissions,
   confirming the layout.

If you are on macOS instead of Linux, the `/proc/self/maps` section will say
"not available." Use `vmmap <pid>` on macOS, or better yet, use a Linux VM or
WSL. This course targets Linux.

---

## Expected output

Your exact addresses will differ (ASLR randomizes them on each run), but the
relative ordering should look like this:

```
TEXT   (code)              ~ 0x5555555551a9
DATA   (init globals)      ~ 0x555555558010
BSS    (zero globals)      ~ 0x55555555801c
HEAP   (malloc, grows up)  ~ 0x5555555592a0
...    (unmapped gap)
STACK  (locals, grows dn)  ~ 0x7fffffffe3ac
```

The key observation: TEXT < DATA < BSS < HEAP < ... huge gap ... < STACK.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `warning: format specifies type 'void *' but the argument has type 'int *'` | Passing a typed pointer to `%p` without casting | Always cast: `(void *)&var` |
| BSS variable has a non-zero value | You gave it an initializer like `= 1` | Remove the initializer or set it to `= 0` |
| `/proc/self/maps` prints "not available" | Running on macOS or a container that hides procfs | Use `vmmap <pid>` on macOS instead |
| Heap addresses are not strictly increasing | `malloc` uses multiple arenas/bins; small and large allocs may come from different regions | This is expected -- the general trend is upward, but the allocator is free to reuse gaps |
| Addresses change between runs | ASLR (Address Space Layout Randomization) | This is a security feature. The relative ordering stays the same. To disable for debugging: `setarch $(uname -m) -R ./your_program` |

---

## Exercises

1. **Add a static local variable.** Inside `main`, add `static int
   static_local = 77;` and print its address. Which segment does it land in?
   (Hint: `static` locals live for the entire program, not just the function
   call. They go in DATA or BSS, not the stack.)

2. **Make a large BSS array.** Add `static char big_array[10 * 1024 * 1024];`
   (10 MB, zero-initialized). Compile the program and check the binary size
   with `ls -la build/lesson_01`. Now change it to `= {1}` (one non-zero byte)
   and recompile. Watch the binary size explode. This is the DATA vs BSS
   difference in action.

3. **Disable ASLR and compare.** Run the program twice normally -- addresses
   differ each time. Then run with
   `setarch $(uname -m) -R ./build/lesson_01` twice -- addresses should be
   identical. ASLR randomizes the base of each segment for security, but the
   relative layout stays the same.

4. **Parse /proc/self/maps.** Instead of printing the raw lines, write a
   version that searches for `[heap]` and `[stack]` labels and prints just
   those address ranges. Compare them to the addresses you printed manually.

---

## Connection to your work

In a game engine, you will:

- Put **lookup tables** (sine tables, color ramps) in DATA or BSS for instant
  access at startup with no allocation overhead.
- Use the **HEAP** for dynamically sized data like entity lists, mesh buffers,
  and texture uploads.
- Keep the **STACK** lean -- large local arrays eat into the 8 MB stack limit.
  A recursive pathfinding algorithm with big local buffers can overflow the
  stack.
- Use `/proc/self/maps` to debug memory corruption -- when you get a segfault
  at a specific address, maps tells you which segment it falls in and what
  permissions it has.

The five-segment mental model is the foundation. Every lesson that follows
builds on it: Lesson 02 dives deep into the stack, Lesson 03 into the heap,
and Lesson 05 reveals the virtual memory system underneath it all.

---

## What's next

In Lesson 02, we go inside the **stack** segment. You will watch stack frames
being created and destroyed as functions call each other, measure frame sizes,
try `alloca` and VLAs for dynamic stack allocation, and intentionally crash
the program with a stack overflow.

---

## Complete source reference

The full compilable source is at `src/lessons/lesson_01.c`. It contains
everything covered in this lesson, including the `print_utils.h` and `bench.h`
dependencies.
