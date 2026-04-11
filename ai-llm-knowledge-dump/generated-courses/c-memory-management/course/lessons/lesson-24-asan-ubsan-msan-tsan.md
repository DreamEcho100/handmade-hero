# Lesson 24 -- ASan, UBSan, MSan, TSan: Compiler Sanitizers

> **What you'll build:** Three sub-programs that each trigger a different class of bug -- heap buffer overflow, signed integer overflow, and a data race -- demonstrating what compiler sanitizers catch at runtime with just a flag change.

## Observable outcome

Running `./build/lesson_24 asan` crashes with a colorful ASan report showing "heap-buffer-overflow" with dual stack traces. Running `ubsan` prints a "runtime error: signed integer overflow" diagnostic. Running `race` shows a shared counter losing increments due to unsynchronized threads.

## New concepts

- Four compiler sanitizers: ASan (address), UBSan (undefined behavior), MSan (memory/uninitialized), TSan (thread)
- The `-fsanitize=` compiler flag family
- Why certain sanitizers are mutually exclusive (ASan vs TSan vs MSan)
- Shadow memory: how ASan plants "red zones" around every allocation
- Why signed integer overflow is undefined behavior (not just "wraps around")

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_24.c` | New | Three demos: `demo_asan`, `demo_ubsan`, `demo_race` |
| `src/common/bench.h` | Dependency | Bench harness (linked but not used for benchmarks this lesson) |

---

## Background -- compile-time instrumentation vs Valgrind

In Lesson 23 we used Valgrind, which instruments at runtime (10-20x slowdown). Compiler sanitizers take a different approach: they insert checks **at compile time**, directly into your binary. The result is much faster (typically 2-3x slowdown) but requires recompilation with special flags.

```
  Valgrind approach:                Sanitizer approach:
  +----------+                      +------------------+
  | Binary   | --> Valgrind         | Source code      |
  | (any)    |     (interprets)     |   + -fsanitize=  |
  +----------+                      |   flags          |
                                    +------------------+
                                           |
                                    +--------------+
                                    | Instrumented |
                                    | Binary       | --> CPU (native speed)
                                    +--------------+
```

### The four sanitizers

```
  +-------+------------------------+---------------------------+----------+
  | Name  | Flag                   | Catches                   | Slowdown |
  +-------+------------------------+---------------------------+----------+
  | ASan  | -fsanitize=address     | Buffer overflow, UAF,     | ~2x      |
  |       |                        | double free, leaks        |          |
  +-------+------------------------+---------------------------+----------+
  | UBSan | -fsanitize=undefined   | Signed overflow, null     | ~1.2x    |
  |       |                        | deref, shift errors       |          |
  +-------+------------------------+---------------------------+----------+
  | MSan  | -fsanitize=memory      | Uninitialized reads       | ~3x      |
  |       |                        | (bit-level tracking)      |          |
  +-------+------------------------+---------------------------+----------+
  | TSan  | -fsanitize=thread      | Data races, lock order    | ~5-15x   |
  |       |                        | inversions                |          |
  +-------+------------------------+---------------------------+----------+
```

### The mutual exclusion rule

ASan, MSan, and TSan are **mutually exclusive** -- you can only use one per build. They each instrument memory accesses differently, and combining them produces nonsense. UBSan can be combined with any of them.

```
  OK:   -fsanitize=address,undefined     (ASan + UBSan)
  OK:   -fsanitize=thread,undefined      (TSan + UBSan)
  BAD:  -fsanitize=address,thread        (ASan + TSan conflict!)
  BAD:  -fsanitize=address,memory        (ASan + MSan conflict!)
```

In JS terms, this is like trying to run two different transpilers that both rewrite the same code -- they would fight over the output.

### How ASan works: shadow memory

ASan maps every 8 bytes of your program's memory to 1 byte of "shadow memory." Each shadow byte records whether the corresponding 8 bytes are: fully addressable (0), partially addressable (1-7), or poisoned/freed (negative values). Red zones (poisoned shadow bytes) are planted around every heap allocation, on the stack around local variables, and around globals.

```
  Your allocation:     [-- 40 bytes of data --]
  ASan's view:    [RZ][-- 40 bytes of data --][RZ]
                   ^                            ^
                   red zone (poisoned)          red zone (poisoned)

  Reading arr[10] when arr has 10 elements
  hits the right red zone --> ASan reports "heap-buffer-overflow"
```

---

## Code walkthrough

### demo_asan -- heap buffer overflow

```c
static void demo_asan(void) {
  int *arr = malloc(10 * sizeof(int));    /* 40 bytes */
  for (int i = 0; i < 10; i++) arr[i] = i * 100;

  volatile int oob = arr[10];            /* one past the end! */
  free(arr);
}
```

`arr[10]` reads 4 bytes starting at byte offset 40 -- exactly at the right red zone boundary. ASan's report tells you:

```
  ==PID==ERROR: AddressSanitizer: heap-buffer-overflow
  READ of size 4 at 0x... thread T0
    #0 0x... in demo_asan lesson_24.c:53
  0x... is located 0 bytes to the right of 40-byte region [0x..., 0x...)
  allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in demo_asan lesson_24.c:45
```

The phrase "0 bytes to the right" means the access is immediately after the allocation boundary. ASan shows stack traces for BOTH the bad access AND the original `malloc`.

In JS, `arr[10]` on a 10-element array returns `undefined`. In C, it reads whatever is in memory at that address. With ASan, it becomes a hard error.

### demo_ubsan -- signed integer overflow

```c
static void demo_ubsan(void) {
  volatile int x = 2147483647;    /* INT_MAX */
  volatile int y = x + 1;        /* Signed overflow -- undefined behavior! */
}
```

Many web developers assume integer overflow "just wraps around" like in JavaScript's `Int32Array`. In C, that is only true for **unsigned** integers. Signed integer overflow is **undefined behavior** -- the compiler is allowed to assume it never happens.

Why this matters: the compiler can optimize based on the assumption that `x + 1 > x` is always true for signed integers. This can eliminate branches, unroll loops incorrectly, or produce code that behaves completely differently from what you wrote.

UBSan catches it at runtime:
```
  lesson_24.c:88: runtime error: signed integer overflow:
  2147483647 + 1 cannot be represented in type 'int'
```

The `volatile` qualifier prevents the compiler from evaluating `x + 1` at compile time, ensuring UBSan can catch it at runtime.

### demo_race -- data race between threads

```c
static volatile int g_shared_counter = 0;

static void *thread_increment(void *arg) {
  for (int i = 0; i < 100000; i++) {
    g_shared_counter++;    /* DATA RACE: no lock, no atomic */
  }
  return NULL;
}

static void demo_race(void) {
  pthread_t t1, t2;
  pthread_create(&t1, NULL, thread_increment, NULL);
  pthread_create(&t2, NULL, thread_increment, NULL);
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  /* Expected: 200000.  Got: something less. */
}
```

Two threads increment the same counter without synchronization. The `++` operation is not atomic -- it is a read-modify-write sequence, and the threads can interleave, losing updates.

In JS, the event loop makes this impossible (single-threaded). In C with pthreads, you must use mutexes or atomics. TSan catches this:

```
  WARNING: ThreadSanitizer: data race
    Write of size 4 at 0x... by thread T1:
      #0 thread_increment lesson_24.c:125
    Previous write of size 4 at 0x... by thread T2:
      #0 thread_increment lesson_24.c:125
```

TSan conflicts with ASan, so you must build separately:
```bash
clang -fsanitize=thread -g -O1 -o lesson_24_tsan \
  src/lessons/lesson_24.c src/common/bench.c -lpthread
./lesson_24_tsan race
```

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| UBSan prints warning but program continues | UBSan defaults to non-fatal mode | Add `-fno-sanitize-recover=all` to make all errors fatal |
| TSan not triggering | Built with ASan (the default), which conflicts | Build with only `-fsanitize=thread` |
| ASan report missing allocation trace | Binary built without `-g` | Always pass `-g`; sanitizer traces need debug info |
| "signed integer overflow" not detected | Compiler constant-folded the expression at compile time | Use `volatile` to force runtime evaluation |
| Race demo shows exactly 200000 | The race is probabilistic; sometimes both threads get lucky | Run multiple times, or increase iteration count |

---

## Build and run

```bash
# Default build (ASan + UBSan)
./build-dev.sh --lesson=24 -r
./build/lesson_24 asan           # triggers ASan
./build/lesson_24 ubsan          # triggers UBSan

# TSan build (separate, conflicts with ASan)
clang -fsanitize=thread -g -O1 -o lesson_24_tsan \
  src/lessons/lesson_24.c src/common/bench.c -lpthread
./lesson_24_tsan race
```

---

## Key takeaways

1. Compiler sanitizers instrument your code at compile time, catching bugs with only 2-5x slowdown compared to Valgrind's 10-20x. The trade-off is that you must recompile.
2. ASan, MSan, and TSan are mutually exclusive. UBSan can be combined with any of them. A typical CI pipeline runs three separate builds: ASan+UBSan, MSan, and TSan.
3. Signed integer overflow in C is undefined behavior -- not a harmless wrap-around. The compiler is allowed to assume it never happens and optimize accordingly. UBSan catches this.
4. Data races in C are undefined behavior too. Unlike JS's single-threaded event loop, C programs with threads must explicitly synchronize shared state. TSan catches unsynchronized accesses.
5. Think of sanitizers as a linter for memory and threading bugs. In the web world, ESLint catches potential issues before production. Sanitizers do the same for C, but at runtime with real data.
