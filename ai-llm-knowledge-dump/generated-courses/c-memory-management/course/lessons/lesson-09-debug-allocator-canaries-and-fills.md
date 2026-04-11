# Lesson 09 -- Debug Allocator: Canaries and Fill Patterns

> **What you will build:** A custom debug allocator that wraps `malloc` with
> fill patterns (0xCD for fresh, 0xDD for freed) and canary guards
> (0xDEADBEEF) to catch buffer overflows and use-after-free without relying on
> ASan.

## Observable outcome

Demo 1 shows a normal allocation filled with 0xCD and a clean free. Demo 2
deliberately overflows a buffer and the allocator catches the corrupted trailing
canary on free. Demo 3 shows freed memory filled with 0xDD. Demo 4 prints
allocation statistics including active and total allocations.

## New concepts

- Fill patterns: 0xCD ("clean data") on allocation, 0xDD ("dead data") on free
- Canary values: known sentinel bytes placed before and after each allocation
  to detect overflows
- Allocation tracking: counting total, active, and peak allocations for leak
  detection
- Building your own diagnostic layer on top of `malloc`/`free`

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_09.c` | New | Four demos exercising the debug allocator |
| `src/lib/debug_alloc.h` | Dependency | API: `DALLOC`, `DFREE`, `debug_alloc_report` |
| `src/lib/debug_alloc.c` | Dependency | Implementation: canary placement, fill, stats |

---

## Background

### JS/TS analogy -- when the runtime cannot help you

In JS, V8's GC makes memory bugs impossible at the language level. But what
about C programs that cannot use ASan? Embedded systems, game consoles, or
production builds where ASan's 2-3x memory overhead is unacceptable?

The answer is a **debug allocator** -- a thin wrapper around `malloc`/`free`
that adds lightweight instrumentation. This technique comes from Microsoft's
debug CRT (`_CrtSetDbgFlag`) and similar systems in game engines (Naughty Dog,
id Software).

Think of it like adding `console.log` around every memory operation, but
structured and automatic.

### Fill patterns

The debug allocator fills memory with distinctive byte patterns:

```
0xCD fill on allocation ("Clean Data"):
+--+--+--+--+--+--+--+--+
|CD|CD|CD|CD|CD|CD|CD|CD|
+--+--+--+--+--+--+--+--+

If you see 0xCDCDCDCD (3452816845) as an int, you know
you forgot to initialize it.

0xDD fill on free ("Dead Data"):
+--+--+--+--+--+--+--+--+
|DD|DD|DD|DD|DD|DD|DD|DD|
+--+--+--+--+--+--+--+--+

If you see 0xDDDDDDDD (3722304989) as an int, you know
you are reading freed memory (use-after-free).
```

In JS, V8 has similar internal patterns (the "filler" objects it places in
garbage-collected regions), but you never see them. In C, you choose to add
these patterns yourself.

### Canary values

A **canary** is a known value placed at the boundary of each allocation. The
name comes from the "canary in a coal mine" -- if the canary dies, there is
danger.

```
Memory layout for each debug allocation:

+-------------------+---------------------+-------------------+
| 4-byte leading    | user data (N bytes) | 4-byte trailing   |
| canary: DEADBEEF  |                     | canary: DEADBEEF  |
+-------------------+---------------------+-------------------+
^                    ^                     ^
malloc returns here  DALLOC returns here   checked on DFREE
```

On `DFREE`, the allocator checks both canaries. If either is corrupted,
something wrote out of bounds. The allocator reports the corruption with file
and line information.

Why `0xDEADBEEF` and not zeros? Because a buffer overflow typically writes
ASCII characters, small integers, or NULLs. A distinctive non-zero pattern like
`0xDEADBEEF` is extremely unlikely to appear by accident, making corruption
detection reliable.

### Allocation tracking

The debug allocator maintains counters:
- **Total allocations** -- how many times `DALLOC` was called
- **Total frees** -- how many times `DFREE` was called
- **Active allocations** -- total minus frees (should be 0 at shutdown)
- **Total bytes requested** -- cumulative size requested

If active allocations are non-zero at program exit, you have a leak.

---

## Code walkthrough

### Demo 1: Normal allocation and free

`DALLOC(32)` calls `debug_alloc(32, __FILE__, __LINE__)`. The allocator:
1. Calls `malloc(4 + 32 + 4)` = 40 bytes (canary + data + canary)
2. Writes `0xDEADBEEF` as the leading canary (first 4 bytes)
3. Fills the 32-byte user region with `0xCD`
4. Writes `0xDEADBEEF` as the trailing canary (last 4 bytes)
5. Returns a pointer past the leading canary

```c
uint8_t *buf = DALLOC(32);
// buf now points to 32 bytes, all filled with 0xCD

hex_dump(buf, 32);
// Output: cd cd cd cd cd cd cd cd cd cd cd cd ...

strcpy((char *)buf, "Hello, debug allocator!");
DFREE(buf);
// Canaries checked -- both intact -- memory filled with 0xDD -- freed
```

On `DFREE`, the allocator:
1. Steps back 4 bytes to find the leading canary
2. Checks both canaries for `0xDEADBEEF`
3. Fills the user region with `0xDD` (dead data)
4. Calls `free` on the original malloc pointer
5. Increments the free counter

### Demo 2: Overflow detection

Writing 20 bytes into a 16-byte `DALLOC` overwrites the trailing canary:

```c
uint8_t *buf = DALLOC(16);
memset(buf, 'X', 20);  // 4 bytes stomp on trailing canary

DFREE(buf);  // DETECTS: trailing canary corrupted!
```

What happens inside:
- Bytes 0-15: filled with 'X' (0x58) -- within bounds
- Bytes 16-19: filled with 'X' -- OVERWRITES the trailing canary
- On DFREE, the allocator reads bytes 16-19 expecting `0xDEADBEEF`
- Instead it finds `0x58585858` -- corruption detected

The allocator prints an error with the file name and line number of the original
`DALLOC` call, telling you exactly which allocation was overflowed.

### Demo 3: Freed memory pattern

After `DFREE`, every byte in the user region is set to `0xDD`:

```c
uint8_t *buf = DALLOC(32);
strcpy((char *)buf, "This will be overwritten");

uint8_t *peek = buf;  // save address for peeking (UB in real code!)
DFREE(buf);

// peek now shows: dd dd dd dd dd dd dd dd dd dd ...
```

If you ever see `0xDDDDDDDD` when debugging, you immediately know you are
reading freed memory. Without the fill pattern, freed memory contains the old
data until it is reused by another allocation -- making use-after-free bugs
much harder to detect.

### Demo 4: Allocation statistics

The final demo creates several allocations, frees some, and prints a report:

```c
void *a = DALLOC(64);
void *b = DALLOC(128);
void *c = DALLOC(256);
void *d = DALLOC(512);

DFREE(a);
DFREE(b);
DFREE(c);
// d is deliberately NOT freed

debug_alloc_report();
// Output: 4 total allocs, 3 frees, 1 ACTIVE (leak!)
```

The report immediately tells you how many allocations are still live. In a real
project, you would call `debug_alloc_report()` at program shutdown and
investigate any active allocations.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Canary corruption on every free | Writing exactly to the allocation boundary (off by a few bytes) | Check that your writes stay strictly within the requested size |
| 0xCD values appearing in production | Debug allocator left enabled in release build | Guard debug allocator behind `#ifdef DEBUG` or build flag |
| Stats show more frees than allocs | Freeing a pointer not obtained from `DALLOC` | Only pass `DALLOC`-returned pointers to `DFREE` |

---

## Build and run

```bash
./build-dev.sh --lesson=09 -r
```

---

## Key takeaways

1. A debug allocator wraps `malloc`/`free` with fill patterns and canary values,
   providing overflow and use-after-free detection without ASan's overhead.

2. Fill patterns make bugs visible: `0xCDCDCDCD` means uninitialized data;
   `0xDDDDDDDD` means freed (dead) data. When you see these in a debugger, you
   instantly know the bug category.

3. Canary values (`0xDEADBEEF`) bracket each allocation. On free, the allocator
   checks them -- any corruption means a buffer overflow occurred.

4. Allocation tracking (total, active, peak) provides leak detection at program
   shutdown without needing external tools.

5. This technique scales from hobby projects to AAA game engines. Microsoft's
   debug CRT, Valve's Source engine, and Naughty Dog's allocator all use
   variants of this pattern. In later lessons, you will build more sophisticated
   allocators on top of these debugging foundations.
