# Lesson 25 -- Full Custom Debug Allocator

> **What you'll build:** An extended debug allocator that layers allocation logging, a size histogram, high-water mark tracking, and a detailed leak report on top of the canary-based `debug_alloc` from Lesson 09.

## Observable outcome

The program allocates blocks of varying sizes, frees some, intentionally leaks three, then prints a full diagnostic report: total allocations/frees, high-water mark, a visual ASCII histogram of allocation sizes, and a list of every leaked block with its file:line source location.

## New concepts

- Layered allocator design: wrapping an existing allocator with additional tracking
- High-water mark: the peak memory usage during execution
- Allocation size histograms for profiling allocation patterns
- `__FILE__` / `__LINE__` macros for source-location tracking through wrapper macros
- Leak detection by scanning an allocation log for unfreed entries

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_25.c` | New | Extended debug allocator (`XDAllocState`), workload simulation, leak report |
| `src/lib/debug_alloc.h` | Dependency | Base debug allocator with canary checking and fill patterns |
| `src/lib/debug_alloc.c` | Dependency | Implementation of `debug_alloc`/`debug_free`/`debug_alloc_report` |

---

## Background -- why build your own when Valgrind exists?

Valgrind (Lesson 23) and sanitizers (Lesson 24) are excellent tools, but they have limitations:

1. **Valgrind is 10-20x slow** -- not usable for real-time applications like games
2. **Sanitizers require recompilation** -- and some bugs only appear in release builds
3. **Neither provides allocation profiling** -- they catch *errors*, not *patterns*

A custom debug allocator gives you **always-on profiling** in your debug builds:

```
  JS analogy:

  // In Node.js, you might wrap fetch() to log request sizes:
  function trackedFetch(url) {
    const start = performance.now();
    const result = await fetch(url);
    logRequestSize(url, result.headers.get('content-length'));
    return result;
  }

  // In C, we wrap malloc/free to log allocation sizes:
  #define XDALLOC(size) xdalloc((size), __FILE__, __LINE__)
  #define XDFREE(ptr)   xdfree((ptr),  __FILE__, __LINE__)
```

The `__FILE__` and `__LINE__` macros are expanded by the C preprocessor at the call site. When you write `XDALLOC(64)`, the preprocessor produces `xdalloc(64, "lesson_25.c", 196)`. This gives the debug allocator the source location of every allocation without the programmer having to type it.

### The layered design

```
  +--------------------------------------------+
  |  Your code: XDALLOC(size) / XDFREE(ptr)    |    <-- Lesson 25
  +--------------------------------------------+
  |  XDAllocState: log, histogram, high-water   |    <-- Tracking layer
  +--------------------------------------------+
  |  debug_alloc: canaries, fill patterns       |    <-- Lesson 09
  +--------------------------------------------+
  |  malloc / free                              |    <-- OS allocator
  +--------------------------------------------+
```

Each layer adds one capability. The base `debug_alloc` provides safety (canaries catch overruns, fill patterns make use-after-free visible). The `XDAllocState` layer adds observability (where are my allocations? what sizes? what leaked?). In production, you compile both layers out and go straight to `malloc`.

---

## Code walkthrough

### The XDAllocEntry and XDAllocState structures

```c
typedef struct XDAllocEntry {
  void       *ptr;       /* the returned pointer */
  size_t      size;      /* requested size */
  const char *file;      /* source file (__FILE__) */
  int         line;      /* source line (__LINE__) */
  int         freed;     /* 1 = freed, 0 = still live */
} XDAllocEntry;

typedef struct XDAllocState {
  XDAllocEntry log[XDALLOC_MAX_LOG];   /* fixed-size allocation log */
  size_t       log_count;
  size_t       current_bytes;           /* bytes currently allocated */
  size_t       high_water_bytes;        /* peak bytes ever allocated */
  size_t       current_count;           /* allocations currently live */
  size_t       high_water_count;        /* peak allocation count */
  size_t       total_allocs;
  size_t       total_frees;
  size_t       histogram[HIST_BUCKETS]; /* allocation size distribution */
} XDAllocState;
```

The log is a fixed-size array of 4096 entries. For a debug tool, this is fine. For production, you would use a hash table. The lesson intentionally keeps it simple.

### The xdalloc wrapper

```c
static void *xdalloc(size_t size, const char *file, int line) {
  void *ptr = debug_alloc(size, file, line);  /* base layer: canaries + fill */
  if (!ptr) return NULL;

  /* Log the allocation */
  if (g_xda.log_count < XDALLOC_MAX_LOG) {
    XDAllocEntry *e = &g_xda.log[g_xda.log_count++];
    e->ptr  = ptr;
    e->size = size;
    e->file = file;
    e->line = line;
    e->freed = 0;
  }

  /* Update stats */
  g_xda.total_allocs++;
  g_xda.current_bytes += size;
  g_xda.current_count++;
  if (g_xda.current_bytes > g_xda.high_water_bytes)
    g_xda.high_water_bytes = g_xda.current_bytes;
  g_xda.histogram[hist_bucket(size)]++;

  return ptr;
}
```

Every allocation flows through `debug_alloc` (which adds canaries and fill patterns), then gets logged with source location and size. The high-water mark is updated by comparing `current_bytes` against the previous peak after every allocation.

### The xdfree wrapper

```c
static void xdfree(void *ptr, const char *file, int line) {
  if (!ptr) return;

  /* Find the log entry and mark it freed */
  for (size_t i = 0; i < g_xda.log_count; i++) {
    if (g_xda.log[i].ptr == ptr && !g_xda.log[i].freed) {
      g_xda.log[i].freed = 1;
      g_xda.current_bytes -= g_xda.log[i].size;
      g_xda.current_count--;
      break;
    }
  }

  g_xda.total_frees++;
  debug_free(ptr, file, line);   /* base layer checks canaries on free */
}
```

The linear scan through the log is O(n) -- slow, but acceptable for a debug build. The `debug_free` call checks the canary values before actually freeing, catching any buffer overruns that happened while the memory was live.

### Size histogram

Five buckets cover common allocation patterns:

```c
static const char *hist_labels[HIST_BUCKETS] = {
  "0-32 B", "33-128 B", "129-512 B", "513-4096 B", "4097+ B"
};
```

After the workload, the report prints an ASCII bar chart:

```
  Allocation size histogram:
    0-32 B       [   85] ###########################
    33-128 B     [   47] ###############
    129-512 B    [   34] ###########
    513-4096 B   [   23] #######
    4097+ B      [   11] ###
```

This tells you which size ranges dominate your program. If 80% of allocations are 0-32 bytes, a pool allocator with 32-byte blocks would be a huge win. The histogram guides your allocator design decisions.

### The leak report

```c
static void xdalloc_leak_report(void) {
  size_t leak_count = 0, leak_bytes = 0;
  for (size_t i = 0; i < g_xda.log_count; i++) {
    if (!g_xda.log[i].freed) {
      leak_count++;
      leak_bytes += g_xda.log[i].size;
    }
  }

  if (leak_count == 0) {
    printf("  No leaks detected.\n");
  } else {
    printf("  LEAKED: %zu allocation(s), %zu bytes total:\n", leak_count, leak_bytes);
    for (size_t i = 0; i < g_xda.log_count; i++) {
      if (!g_xda.log[i].freed) {
        printf("    %zu bytes at %p -- allocated at %s:%d\n",
               g_xda.log[i].size, g_xda.log[i].ptr,
               g_xda.log[i].file, g_xda.log[i].line);
      }
    }
  }
}
```

The three intentional leaks appear with exact file:line references:
```
  LEAKED: 3 allocation(s), 1169 bytes total:
    #1: 42 bytes at 0x... -- allocated at lesson_25.c:255
    #2: 128 bytes at 0x... -- allocated at lesson_25.c:256
    #3: 999 bytes at 0x... -- allocated at lesson_25.c:257
```

### Section 2: simulated workload

The demo allocates 200 blocks of varying sizes (8 to 8192 bytes), frees every other one, then frees the rest. This exercises the histogram across all buckets and shows how the high-water mark tracks the peak before any frees happen.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Log fills up and stops tracking | `XDALLOC_MAX_LOG` (4096) exceeded | Increase the constant, or use a dynamic log for production |
| Canary corruption reported on free | A buffer overrun wrote past the allocation | The canary sits immediately after your data; check sizes |
| High-water mark seems low | Freed blocks before allocating new ones | High-water captures the peak; staggered patterns reduce it |
| Leak report shows 0 leaks | The intentional leaks were accidentally freed | The three `leak1/2/3` pointers must not be passed to `XDFREE` |
| `xdfree` is slow with many allocations | Linear scan of the log is O(n) | Use a hash table keyed by pointer for O(1) lookup |

---

## Build and run

```bash
./build-dev.sh --lesson=25 -r              # full demo with leak report
./build-dev.sh --lesson=25 --bench -r      # includes debug alloc vs malloc benchmark
```

The benchmark shows `debug_alloc+free` is 3-5x slower than raw `malloc+free` due to canary writes, fill patterns, and bookkeeping. This confirms why the debug allocator belongs only in debug builds.

---

## Key takeaways

1. A custom debug allocator gives you always-on allocation profiling that Valgrind and sanitizers do not provide: size histograms, high-water marks, and per-allocation source locations.
2. The layered design (your code -> tracking -> canaries -> malloc) mirrors how real game engines and embedded systems build their memory infrastructure. Each layer is compiled out in release.
3. `__FILE__` and `__LINE__` are expanded at the *call site* by the preprocessor. Wrapping them in macros (`XDALLOC`, `XDFREE`) is the only way to capture the caller's location rather than the wrapper's location.
4. The high-water mark answers "what was the peak memory usage?" and the histogram answers "what sizes do I allocate most?" Together, they tell you whether to use a pool (fixed size, high frequency), an arena (batch lifetime), or stick with malloc.
