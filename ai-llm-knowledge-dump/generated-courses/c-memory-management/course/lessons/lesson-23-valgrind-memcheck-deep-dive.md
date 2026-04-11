# Lesson 23 -- Valgrind Memcheck Deep Dive

> **What you'll build:** A collection of intentionally buggy programs that each trigger a different class of memory error, then learn to read Valgrind's diagnostic output to locate and understand each bug.

## Observable outcome

When run normally, the program prints descriptions of each bug type. When run under Valgrind with `--leak-check=full --track-origins=yes`, each sub-program produces characteristic error messages -- "definitely lost", "Invalid read", "Conditional jump depends on uninitialised value" -- with stack traces pointing to the exact source lines.

## New concepts

- Six categories of memory errors Valgrind detects: leak, use-after-free, uninitialized read, overlapping memcpy, heap buffer overflow, double free
- Reading Valgrind stack traces: PID prefix, "Invalid read/write of size N", "Address is N bytes inside/after a block"
- Key Valgrind flags: `--leak-check=full`, `--track-origins=yes`, `--show-reachable=yes`
- Why Valgrind and ASan conflict (both replace malloc/free)

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_23.c` | New | Six `demo_*` functions, each triggering one error class |
| `src/common/print_utils.h` | Dependency | Section printing helpers |

---

## Background -- Valgrind for web developers

If you have used the Chrome DevTools Memory tab to find leaked DOM nodes or detached event listeners, Valgrind is the C equivalent -- except it works at the byte level. Every single byte of memory is tracked: is it allocated? freed? initialized? Valgrind knows.

### How Valgrind works

Valgrind does NOT instrument your binary at compile time. Instead, it runs your program on a **synthetic CPU** -- a software interpreter that executes every instruction while maintaining shadow metadata for every byte of memory. This is why:

- It imposes a 10-20x slowdown (every memory access is checked)
- You do NOT need to recompile (any binary works)
- It can catch bugs that compile-time tools miss (like uninitialized reads)

```
  Normal execution:             Valgrind execution:
  +----------+                  +-------------------+
  | Your     |                  | Valgrind Runtime  |
  | Program  | --> CPU          |  +-----------+    |
  +----------+                  |  | Your      |    |
                                |  | Program   | --> Synthetic CPU
                                |  +-----------+    |  + Shadow Memory
                                +-------------------+
```

### The critical ASan conflict

Valgrind and ASan both replace `malloc`/`free` with their own instrumented versions. Running both simultaneously produces false positives or crashes. The rule:

```
  ASan = compile-time instrumentation (fast, 2x slowdown)
  Valgrind = runtime instrumentation (thorough, 20x slowdown)
  NEVER use both at the same time.
```

The build script provides `--no-asan` for Valgrind runs.

### Valgrind's leak categories

```
  +-------------------+------------------------------------------------+
  | Category          | Meaning                                        |
  +-------------------+------------------------------------------------+
  | Definitely lost   | No pointer to the block exists anywhere         |
  | Indirectly lost   | Reachable only through a definitely-lost block |
  | Possibly lost     | An interior pointer exists (maybe intentional) |
  | Still reachable   | A pointer exists at exit but was never freed   |
  +-------------------+------------------------------------------------+
```

In JS terms, "definitely lost" is like a closure that captured an object but the closure itself is unreachable -- except JS would GC it, and C does not.

---

## Code walkthrough

### Program structure

The lesson uses a command-line dispatch pattern. Each bug type is a separate function:

```c
int main(int argc, char *argv[]) {
  const char *test = argv[1];
  if (strcmp(test, "leak") == 0)              demo_leak();
  else if (strcmp(test, "uaf") == 0)          demo_uaf();
  else if (strcmp(test, "uninit") == 0)       demo_uninit();
  else if (strcmp(test, "overlap") == 0)      demo_overlap();
  else if (strcmp(test, "invalid-write") == 0) demo_invalid_write();
  else if (strcmp(test, "double-free") == 0)  demo_double_free();
}
```

This lets you run each demo individually under Valgrind and study one error class at a time.

### demo_leak -- memory leak

```c
static void demo_leak(void) {
  char *buf = malloc(1024);
  memset(buf, 'A', 1024);
  /* Intentionally no free(buf) -- this is the leak */
}
```

Valgrind reports:
```
  ==PID== 1,024 bytes in 1 blocks are definitely lost
  ==PID==    at 0x...: malloc
  ==PID==    by 0x...: demo_leak (lesson_23.c:37)
```

The stack trace points to the exact `malloc` call. In JS, this would be caught by the heap snapshot diff tool. In C, you need Valgrind or a debug allocator (Lesson 25).

### demo_uaf -- use-after-free

```c
static void demo_uaf(void) {
  char *buf = malloc(64);
  memset(buf, 'X', 64);
  free(buf);
  volatile char val = buf[0];    /* Reading freed memory! */
}
```

Valgrind reports BOTH the invalid read AND the original free site AND the allocation site -- three stack traces in one report. This is Valgrind's strength: it gives you the full history.

```
  ==PID== Invalid read of size 1
  ==PID==    at 0x...: demo_uaf (lesson_23.c:71)
  ==PID==  Address 0x... is 0 bytes inside a block of size 64 free'd
  ==PID==    at 0x...: free
  ==PID==    by 0x...: demo_uaf (lesson_23.c:67)
  ==PID==  Block was alloc'd at
  ==PID==    at 0x...: malloc
  ==PID==    by 0x...: demo_uaf (lesson_23.c:61)
```

### demo_uninit -- uninitialized read

```c
static void demo_uninit(void) {
  int *arr = malloc(32 * sizeof(int));    /* malloc does NOT zero memory */
  if (arr[7] > 0) {                       /* branching on garbage! */
    printf("positive: %d\n", arr[7]);
  }
  free(arr);
}
```

The key insight: `malloc` allocates raw memory. Unlike `calloc` or JS's `new Array()`, the bytes contain whatever was there before. Branching on uninitialized data is undefined behavior.

With `--track-origins=yes`, Valgrind traces the uninitialized value back to its allocation:
```
  ==PID== Conditional jump depends on uninitialised value(s)
  ==PID==  Uninitialised value was created by a heap allocation
  ==PID==    at 0x...: malloc
```

### demo_overlap -- overlapping memcpy

```c
static void demo_overlap(void) {
  char buf[64];
  memset(buf, 'A', sizeof(buf));
  memcpy(buf + 5, buf, 20);    /* src and dst overlap by 15 bytes! */
}
```

The C standard says `memcpy` behavior is undefined when source and destination overlap. Use `memmove` instead -- it handles overlaps correctly. Valgrind catches this even though it often "works" in practice.

### demo_invalid_write -- heap buffer overflow

```c
static void demo_invalid_write(void) {
  char *buf = malloc(16);
  for (int i = 0; i < 20; i++) {
    buf[i] = (char)('A' + i);    /* writes 4 bytes past the end! */
  }
  free(buf);
}
```

Valgrind's signature phrase "0 bytes after a block of size 16" tells you this is a buffer overflow (writing past the end), not an underflow (writing before the start).

### demo_double_free

```c
static void demo_double_free(void) {
  char *buf = malloc(32);
  free(buf);
  free(buf);     /* Double free! */
}
```

Valgrind shows both free sites and the original allocation. In JS, you cannot double-free because the GC handles it. In C, double-free corrupts the heap's internal bookkeeping and can lead to arbitrary code execution -- this is a security vulnerability.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Valgrind says "ASan runtime does not support..." | Built with `-fsanitize=address` | Rebuild with `--no-asan` |
| No errors reported for `uninit` | Compiler optimized away the branch | Use `-O0` or mark the variable `volatile` |
| `demo_double_free` crashes before Valgrind can report | glibc's built-in checks abort first | Build with `--no-asan` and run under Valgrind |
| "Conditional jump" not showing origin | Missing `--track-origins=yes` | Add the flag; it costs ~2x more but traces the uninitialized value |

---

## Build and run

```bash
# Build without sanitizers (required for Valgrind)
./build-dev.sh --lesson=23 --no-asan

# Run each demo under Valgrind
valgrind --leak-check=full --track-origins=yes ./build/lesson_23 leak
valgrind --leak-check=full --track-origins=yes ./build/lesson_23 uaf
valgrind --leak-check=full --track-origins=yes ./build/lesson_23 uninit
valgrind --leak-check=full --track-origins=yes ./build/lesson_23 overlap
valgrind --leak-check=full --track-origins=yes ./build/lesson_23 invalid-write
valgrind --leak-check=full --track-origins=yes ./build/lesson_23 double-free
```

---

## Key takeaways

1. Valgrind runs your program on a synthetic CPU and tracks every byte of memory. It catches six major error classes: leaks, use-after-free, uninitialized reads, overlapping copies, buffer overflows, and double frees.
2. Valgrind and ASan conflict because both replace `malloc`/`free`. Never use both at the same time. ASan is faster (2x slowdown); Valgrind is more thorough (20x slowdown, but catches uninit reads that ASan misses).
3. The `--track-origins=yes` flag is essential for uninitialized-read bugs. Without it, Valgrind tells you *where* the bad branch is but not *where* the uninitialized memory came from.
4. In JS, the garbage collector and runtime prevent most of these errors by design. In C, you are the runtime -- Valgrind is the closest thing to having a GC watching over your shoulder, telling you what you got wrong.
