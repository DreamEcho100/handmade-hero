# Memory Management in C -- Complete Course Specification

## Personalized Course Plan

Build a hands-on, build-along course that teaches **every aspect of memory management in C**: from how bytes are laid out in memory, through the bug categories that plague C programs, to arena allocators, pool/slab/buddy allocators, debugging tools, performance optimization, and production-grade patterns -- all in C11, no C++ features, no STL, no garbage collector. The student builds 30 standalone programs and 9 reusable library implementations, culminating in a JSON parser that uses arenas for all allocations.

---

## Source Material

- **Primary inspiration:** Ryan Fleury's talk *"Everyone is doing memory management wrong"* (Back and Forth S2E02) -- the philosophical foundation for arenas as THE default allocation strategy.
- **Supporting references:** Casey Muratori's Handmade Hero (permanent + transient arena model), Ginger Bill's arena allocator articles, Valgrind/ASan documentation, Intel optimization manuals (cache line behavior).
- **Student profile:** `@_ignore/about-me.md` / `@_ignore/about-me-0.md`
- **Instruction manual:** `@ai-llm-knowledge-dump/prompt/course-builder.md`
- **Student background profile:** `@ai-llm-knowledge-dump/llm.txt`
- **C coding standards:** `@ai-llm-knowledge-dump/c-pitfalls-for-web-devs.md`, `@ai-llm-knowledge-dump/modern-c-programming-safe,-performant,-and-practical-practices.md`

---

## Output Directory

`ai-llm-knowledge-dump/generated-courses/c-memory-management/`

All output goes here. Nothing outside this directory.

---

## Context

**Who is this for:** Mazen (DreamEcho100) -- a Cairo-based full-stack web developer (React, Next.js, Node.js, TypeScript) deliberately transitioning into systems programming and game engine development. Currently paused at day 32 of Handmade Hero, building courses in C to develop mechanical intuition before resuming.

**Why this course now:** Memory management is THE foundation skill for everything else. Every other course -- game engines, entity systems, physics, rendering -- eventually requires understanding how memory works in C. Without this course:

- LOATs pool system lessons assume arena knowledge the student doesn't have
- Platform Foundation arenas are magic black boxes
- Handmade Hero's permanent + transient arena model makes no sense
- GEA Chapter 6 (Memory Management) reads as abstract theory
- Every `malloc` call is a potential bug waiting to happen

This course turns memory from a source of fear into a source of power.

**Strategic relationship to other courses:**

- **This course** = "how memory works in C, how to manage it, how to build allocators" (THE FOUNDATION)
- **Platform Foundation Course** = "how to open a window, handle input, render a backbuffer" -- uses arenas from this course for scratch memory
- **LOATs Course** = "how to build entity pool systems with intrusive lists" -- pool allocator concepts (L18) connect directly to LOATs pool system; arenas (L10-16) enable LOATs memory arena lab (Scene 17)
- **Game courses (Snake, Tetris, Asteroids)** = "how to use fixed arrays for game entities" -- this course explains WHY fixed arrays work and what happens when they aren't enough
- **Game Programming Patterns** = "named patterns for systems" -- Object Pool (GPP Ch 19) is L18, Data Locality (GPP Ch 17) is L26-27
- **Game Engine Architecture** = "what engine subsystems exist" -- GEA Ch 6 (Memory Management) is the textbook version of this entire course
- **Handmade Hero (days 33+)** = Casey builds permanent + transient arenas. This course gives Mazen the implementation skills before encountering them in HH.

**Key difference from other courses:** This is NOT a game course. It has no platform layer, no window, no rendering. Every program is pure terminal output. This isolates memory concepts from graphics complexity. The student can focus entirely on how bytes are laid out, allocated, freed, and organized.

---

## Key Design Decisions

1. **All code in C11.** No C++ features. No classes, no constructors/destructors, no templates, no operator overloading, no STL. C11 specifically for `_Alignof`, `_Alignas`, `_Static_assert`, `max_align_t`.

2. **Every lesson is standalone compilable.** `lesson_01.c` through `lesson_30.c` each produce a standalone binary. No lesson depends on compiling another lesson first. Libraries (`arena.h`, `pool.h`, etc.) are shared dependencies, but each lesson `#include`s what it needs and has its own `main()`.

3. **Build system: `./build-dev.sh --lesson=NN`.** Three modes:
   - `--lesson=NN -r` -- debug build with ASan, runs immediately
   - `--lesson=NN --bench -r` -- optimized build (-O2), no sanitizers, benchmarks enabled
   - `--lesson=NN --valgrind -r` -- debug build, runs under Valgrind memcheck
   - `--all` -- builds all 30 lessons, reports any compilation failures

4. **Libraries evolve incrementally.** `arena.h` starts in L10 with basic push/reset, gains alignment in L11, chained blocks in L12, mmap growth in L13, and TempMemory checkpoints in L14. The file in `src/lib/arena.h` is the FINAL version; individual lessons show the evolution. Same pattern for `debug_alloc.h/c` (L09 creates it, L25 extends it).

5. **No platform layer needed.** Pure terminal output via `printf`. This is deliberate -- memory management concepts don't need graphics. Benchmarks use `clock_gettime(CLOCK_MONOTONIC)` via `bench.h`.

6. **9 library implementations.** These are reusable across lessons:

   | Library | Created in | Extended in | Purpose |
   |---------|-----------|-------------|---------|
   | `arena.h` | L10 | L11-L14 | Arena allocator (bump pointer, chained blocks, mmap, checkpoints) |
   | `debug_alloc.h/c` | L09 | L25 | Debug allocator with canaries, fill patterns, leak tracking |
   | `str.h/c` | L16 | -- | Arena-backed string operations |
   | `stretchy_buf.h` | L17 | -- | Dynamic array (stb-style stretchy buffer) |
   | `pool.h/c` | L18 | -- | Fixed-size pool allocator with free list |
   | `freelist.h/c` | L19 | -- | General-purpose free list on arena backing |
   | `stack_alloc.h/c` | L20 | -- | LIFO stack allocator with headers |
   | `slab.h/c` | L21 | -- | Slab allocator (multiple size classes) |
   | `hash_map.h/c` | L29 | -- | Arena-backed hash map |

7. **Common utilities.** Shared across all lessons:

   | File | Purpose |
   |------|---------|
   | `bench.h/c` | Benchmarking harness (BENCH_SUITE, BENCH_CASE macros) |
   | `print_utils.h` | Colored terminal output, hex dump, memory visualization helpers |

8. **C coding standards:** ALL code MUST follow:
   - `@ai-llm-knowledge-dump/c-pitfalls-for-web-devs.md`
   - `@ai-llm-knowledge-dump/modern-c-programming-safe,-performant,-and-practical-practices.md`

---

## Engineering Principle Priority Order

When rules in this prompt conflict with each other, resolve in this order (highest priority first):

1. **Conceptual clarity** -- the student must understand the idea
2. **Correct mental model** -- the student's mental picture must match reality
3. **Demonstrable behavior** -- the student can build it, run it, verify it
4. **Practical engineering usefulness** -- what's taught is actually used in production
5. **Consistency with Ryan Fleury / Casey Muratori philosophy** -- arenas first, simplicity over abstraction
6. **Style guide compliance** -- naming, formatting, terminology
7. **Depth targets** -- approximate word counts

This prevents optimizing for compliance over insight. A lesson that nails concepts 1-4 but is 1,000 words short is DONE. A lesson that hits the depth target but leaves the student confused is NOT done.

---

## Emergent Insight Rule

If during a lesson a deeper engineering insight naturally appears (e.g., how arenas relate to operating system page allocation, why Rust's borrow checker exists, how database connection pools mirror L18's pool allocator), the instructor may temporarily deviate from the lesson plan to explore it **provided**:

1. It strengthens the mental model.
2. It relates directly to the current system.
3. The lesson returns to the planned trajectory afterward.

---

## Pedagogical Approach (MANDATORY)

This section defines NON-NEGOTIABLE rules for how content is written. Every lesson file must follow these rules.

**The student learns by BUILDING, not by reading.** Mazen explicitly prefers "hands-on building over passive watching" and wants "Casey Muratori-style: direct, no-gap explanations, engineering reasoning for every choice, teach me to fish." Each lesson is a build-along where understanding solidifies by typing code, compiling, running, and seeing the memory system grow.

### 0. Instructor Voice Lock

The course is written in the voice of a **senior engine programmer doing a live whiteboard session** -- not a textbook, not a generic tutorial. Characteristics:

- **Assumes intelligence, not ignorance.** The student is a professional developer switching domains -- explain new C concepts fully, but don't condescend about things they already know (loops, functions, basic data structures, what an array is).
- **Thinks out loud like a code review.** "We could use malloc here, but then we need to track every allocation for cleanup. What if we just... bump a pointer forward? Let's try it."
- **Talks about trade-offs like production decisions.** "You lose the ability to free individual allocations. In exchange, you eliminate the entire category of memory leaks. For 90% of game code, that's the right trade."
- **Uses casual, direct language.** "This is the bug that ships in production. Don't do this." / "Now we're cooking." / "This is what makes arenas magical."
- **Never drifts into textbook energy.** No "In this section, we will explore the concepts of..." -- instead "Here's the problem. Here's how we fix it. Let's build it."

If the tone starts sounding like a Wikipedia article or a university lecture, it's wrong. Channel Ryan Fleury's directness and Casey Muratori's engineering reasoning: opinionated, practical, always justified.

### 1. ASCII Diagrams for Every Memory Layout and Operation

Not optional. EVERY memory layout, allocation operation, pointer relationship, and data structure gets a diagram:

```
Stack frame layout:
                    HIGH ADDRESS
    ┌─────────────────────────┐
    │ return address          │  ← pushed by CALL
    ├─────────────────────────┤
    │ saved RBP               │  ← function prologue
    ├─────────────────────────┤
    │ int x = 42              │  4 bytes
    ├─────────────────────────┤
    │ char buf[16]            │  16 bytes
    ├─────────────────────────┤
    │ double d = 3.14         │  8 bytes (aligned to 8)
    ├─────────────────────────┤
    │ (padding)               │  4 bytes (alignment gap)
    └─────────────────────────┘
                    LOW ADDRESS (stack grows DOWN)
```

Types of diagrams needed throughout the course:

- **Process memory map** (text, data, BSS, heap, stack segments with addresses)
- **Stack frame layout** (local variables, return address, frame pointer)
- **Heap allocation metadata** (headers, padding, free list pointers)
- **Struct padding and alignment** (byte-level layout with padding gaps marked)
- **Virtual memory** (pages, page tables, reserved vs committed regions)
- **Arena internals** (block header, used region, free region, alloc pointer)
- **Arena operations** (before/after push, before/after reset, chained blocks)
- **Pool allocator** (slots, free list chain, allocated vs free slots)
- **Free list** (embedded next pointers in free blocks)
- **Cache lines** (64-byte lines, AoS vs SoA access patterns with cache hits/misses marked)
- **Before/after for every operation** (malloc/free, arena push/reset, pool alloc/free)

### 2. Mental Model BEFORE Implementation

For every concept: explain WHAT it is, WHY it exists, and what PROBLEM it solves BEFORE showing any code. Build the mental model first.

**WRONG order:**

```c
void *arena_push_size(Arena *a, size_t size, size_t align) {
    size_t offset = align_forward(a->offset, align);
    // ...
```

**RIGHT order:**

```
Every malloc call searches a free list, updates metadata headers,
and potentially locks a mutex. For a game running 60 frames per
second with hundreds of temporary allocations per frame, that
overhead adds up.

What if allocation was just... adding a number?

    ┌──────────────────────────────────────────┐
    │ backing memory (one big block)           │
    │ ████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
    │ ^used       ^offset                  ^end│
    └──────────────────────────────────────────┘

    To allocate 64 bytes: offset += 64. That's it.
    To free everything: offset = 0. That's it.

Here's the code:
```

### 3. Web Dev Bridges (MANDATORY per concept)

Map each concept to web development equivalents the student already knows. Use `> **JS bridge:**` callouts.

| C Memory Concept | Web Dev Equivalent |
|---|---|
| Stack allocation (local variables) | `const x = 42` inside a function -- auto-cleaned when function returns |
| Heap allocation (malloc/free) | `new Object()` -- lives until GC collects it (or you call `free`) |
| Memory leak (forgot to free) | Event listener attached but never removed -- keeps object alive forever |
| Use-after-free | Accessing a DOM node after `removeChild` -- node is detached but reference exists |
| Buffer overflow | Writing past `array.length` -- JS throws, C silently corrupts |
| Arena allocator | React render cycle: allocate temps, use them, GC collects all at once -- but explicit and instant |
| Arena reset | `array.length = 0` -- doesn't deallocate, just resets the "used" marker |
| Pool allocator | Database connection pool -- fixed number of pre-allocated connections, reused via checkout/return |
| Struct padding/alignment | JSON serialization overhead -- extra bytes for structure, not for data |
| Virtual memory pages | Lazy loading in web apps -- reserve the route, only load the component when visited |
| mmap file I/O | `fs.readFileSync` vs memory-mapped -- one copies into a buffer, the other maps the file directly |
| Cache locality (SoA vs AoS) | Column-oriented database vs row-oriented -- same data, different access patterns, 10x performance difference |
| Valgrind / ASan | ESLint + TypeScript strict mode -- catches bugs before they reach production |
| TempMemory checkpoints | `try/finally` or `using` -- mark a scope, clean up when it ends |
| Free list | Thread pool in Node.js -- workers return to a queue when done, reused by next task |

### 4. "Why" Before "How" -- The Problem-Driven Approach

Every concept must answer WHY before showing WHAT. Structure each concept as:

1. **The problem** -- show the broken/slow/unsafe version first
2. **Why it breaks** -- concrete failure scenario (memory leak, segfault, corruption, performance cliff)
3. **The fix** -- the technique that solves it
4. **The trade-off** -- what you give up (flexibility, complexity, per-allocation free)

### 5. Common Misconceptions

Address these explicitly when relevant:

- **"malloc is fine, just don't forget free"** -- In a 100K-line codebase with error paths, conditional branches, and early returns, you WILL forget. Arenas eliminate the problem structurally, not through discipline.
- **"Arenas waste memory"** -- Do the napkin math. A 1 MB arena for a frame that uses 200 KB "wastes" 800 KB. Your machine has 16 GB. That's 0.005%.
- **"You need garbage collection for safety"** -- No. Arenas give you deterministic lifetime management. GC gives you non-deterministic cleanup with pauses. For real-time applications (games, audio), deterministic wins.
- **"Virtual memory is OS magic I don't need to understand"** -- Every arena growth strategy, every mmap call, every guard page depends on understanding virtual vs physical memory.
- **"Alignment doesn't matter on modern hardware"** -- Misaligned atomics are undefined behavior. Misaligned SIMD crashes. Even regular loads can be 2x slower when crossing cache line boundaries.
- **"I'll just use Valgrind to find bugs"** -- Valgrind finds symptoms. Understanding memory layout prevents causes. Use both.
- **"Pool allocators are only for games"** -- Web servers (nginx), databases (PostgreSQL), language runtimes (Go) all use pool/arena allocation patterns.

### 6. Depth Targets (Not Word Counts)

| Type | Depth target | Examples |
|---|---|---|
| Foundation lesson (concepts + first code) | ~5,000-6,000 words | Memory layout, stack, heap, alignment |
| Core technique lesson (new allocator) | ~6,000-8,000 words | Arena basics, arena growth, pool allocator, free list |
| Bug/tool lesson (debugging focused) | ~5,000-6,000 words | Memory bugs, Valgrind, ASan, debug allocator |
| Performance lesson (benchmarks) | ~5,000-6,000 words | Cache locality, SoA vs AoS |
| Capstone lesson (integration) | ~8,000-10,000 words | JSON parser, hash map |

**Prefer depth over length.** These are depth targets, not token quotas. A 6,000-word lesson that nails every concept with precision beats a 9,000-word lesson padded with repetition. The ASCII diagrams, before/after operations, JS bridges, and worked examples naturally produce the required depth -- don't inflate beyond what the concept demands.

> **Note:** The original lessons were 1,083-1,721 words. That was far too thin -- they read as reference material, not tutorials. The targets above represent what is actually needed for a web developer transitioning to systems programming to build genuine understanding.

### 7. Build-Along Methodology

Each lesson is a build-along, NOT a code dump. The student builds incrementally:

1. **Start with an empty file** (or from the library includes) -- "Create `lesson_10.c` with these includes..."
2. **Add code in chunks of 5-20 lines** -- never dump the whole file at once
3. **After EACH chunk:**
   - Explain every line -- what it does, why, what the memory state looks like now
   - ASCII diagram showing memory state after this operation
   - "Build and run" -- compile command + expected output
4. **100% code coverage** -- every line of the final source code must appear in some step
5. **Compile checkpoints** -- at least 3-5 "build and run" moments per lesson

### 8. Lesson Quality Standards (MANDATORY)

Every lesson MUST have ALL of the following. No exceptions:

1. **`## What we're building`** -- one paragraph, what the student will have at the end
2. **`## What you'll learn`** -- bullet list of concepts
3. **`## Prerequisites`** -- which lessons must be completed first
4. **Numbered `## Step N -- [descriptive name]` sections** -- the build-along progression
5. **5-20 lines of code per step** followed by line-by-line explanation
6. **"Build and run" compile checkpoint** after every step that adds code
7. **Complete compilable file at the end** -- student can copy-paste and compile
8. **`> **JS bridge:**` callout for EVERY C concept** with JavaScript/TypeScript analogy
9. **ASCII diagram for every memory operation** (before + after state)
10. **`## Common mistakes` section** with wrong code and explanation
11. **`## Exercises` section** with 2-3 hands-on tasks
12. **`## Connection to your work` section** mapping to LOATs/Platform Foundation/GEA/HH
13. **`## What's next` section** previewing the next lesson

**Minimum depth targets (validated from LOATs experience):**
- Foundation lessons: ~5,000-6,000 words
- Core technique lessons: ~6,000-8,000 words
- Bug/tool lessons: ~5,000-6,000 words
- Performance lessons: ~5,000-6,000 words
- Capstone lessons: ~8,000-10,000 words

The original ~1,300-word lessons lacked sufficient JS bridges, ASCII diagrams, and step-by-step explanations. These targets reflect what is actually needed.

---

## Lesson Progression (30 lessons in 7 groups)

Follow this exact ordering. Each lesson builds on previous ones where indicated by the dependency map.

### Group 1 -- Foundations (Lessons 01-06)

These lessons build the mental model of how memory works in C. No custom allocators yet -- just understanding the terrain.

1. **Lesson 01: C Memory Layout Exploration** -- Map the process address space: text (code), data (initialized globals), BSS (zero-initialized globals), heap (grows up), stack (grows down). Print addresses of variables in each segment. Show that stack addresses decrease while heap addresses increase. ASCII diagram of the full memory map. JS bridge: V8's hidden memory zones vs C's explicit layout.

2. **Lesson 02: Stack Allocation Mechanics** -- How function calls create stack frames: local variables, return addresses, frame pointer chain. Show stack growth direction with address printing. Demonstrate stack overflow with deep recursion. Introduce `alloca()` and why it's dangerous. ASCII diagrams of nested stack frames. JS bridge: JavaScript call stack (max call stack size exceeded) vs C stack overflow (segfault with no error message).

3. **Lesson 03: Heap Allocation -- malloc, calloc, realloc** -- The three heap functions and `free`. Show malloc's hidden metadata header (16-32 bytes per allocation). Demonstrate realloc's copy-on-grow behavior. Show the cost: free list search, metadata updates, potential fragmentation. First benchmark: 1M malloc+free cycles. JS bridge: `new Object()` hides all of this; C makes it explicit.

4. **Lesson 04: Alignment and sizeof** -- Why `sizeof(struct)` isn't always the sum of its fields. Show struct padding with byte-level memory dumps. Demonstrate `_Alignof`, `_Alignas`, `__attribute__((packed))`. Show how field ordering affects struct size. Build a struct padding calculator. ASCII diagrams showing padding bytes between fields. JS bridge: JSON serialization adds overhead for structure -- struct padding is the C equivalent.

5. **Lesson 05: Virtual Memory -- Pages, mmap, Guard Pages** -- The OS gives you virtual addresses, not physical ones. Page size (4 KB), page tables, reserved vs committed memory. `mmap` to allocate pages directly. `mprotect` for guard pages. Show that touching an uncommitted page causes a segfault. This is the foundation for arena growth via virtual memory (L13). JS bridge: lazy-loaded routes in a web app -- the route exists, but the component isn't loaded until visited.

6. **Lesson 06: Memory-Mapped File I/O** -- `mmap` a file instead of `fread`. Compare performance: read entire file into malloc'd buffer vs mmap the file and access it directly. Show that mmap avoids the copy. Benchmark both approaches on a large file. JS bridge: `fs.readFileSync` copies into a Buffer; mmap maps the kernel's page cache directly into your address space.

### Group 2 -- Common Bugs (Lessons 07-09)

These lessons show what goes wrong. The student intentionally creates every major memory bug, learns to recognize the symptoms, and builds a debug allocator to catch them automatically.

7. **Lesson 07: Memory Leaks, Use-After-Free, Double-Free** -- Three programs, each demonstrating one bug. Show ASan output for each. Show Valgrind output for each. Explain why each bug is dangerous (leak = resource exhaustion, use-after-free = data corruption, double-free = heap corruption). JS bridge: leaked event listeners, accessing detached DOM nodes, calling `.close()` twice on a stream.

8. **Lesson 08: Buffer Overflow, Uninitialized Reads, Off-by-One** -- Three more programs. Buffer overflow: write past array bounds, corrupt adjacent stack variable. Uninitialized read: use malloc'd memory without zeroing. Off-by-one: classic `<=` instead of `<` in a loop. Show ASan and UBSan output. JS bridge: JavaScript throws `RangeError` or returns `undefined`; C silently corrupts memory and keeps running.

9. **Lesson 09: Debug Allocator -- Canaries, Fill Patterns, Overflow Detection** -- Build `debug_alloc.h/c`: a wrapper around malloc that adds canary bytes before and after each allocation, fills freed memory with `0xDD` (to catch use-after-free), fills allocated memory with `0xCC` (to catch uninitialized reads), and tracks all allocations for leak reporting at program exit. This is the student's first custom allocator. JS bridge: `Proxy` objects that intercept property access to detect misuse.

### Group 3 -- Arena Allocators (Lessons 10-16)

The CORE of the course. The student builds a complete arena allocator from scratch, feature by feature. This is the technique that Ryan Fleury says makes "all traditional concerns about memory management in C go away."

10. **Lesson 10: Arena Basics -- Push and Reset** -- Build the minimal arena: a backing buffer, an offset, `arena_push_size()` that bumps the offset forward, `arena_reset()` that sets offset back to zero. Show the per-frame pattern: reset at frame start, push throughout the frame, repeat. Benchmark arena push vs malloc+free. JS bridge: React render cycle -- allocate temps, use them, GC collects. Arena does the same thing but explicitly and instantly.

11. **Lesson 11: Arena Alignment and Type-Safe Macros** -- Add alignment support: `align_forward()` rounds the offset up to the required alignment. Create type-safe macros: `ARENA_PUSH_STRUCT(arena, Type)`, `ARENA_PUSH_ARRAY(arena, count, Type)`. These use `sizeof` and `_Alignof` so the student never manually calculates sizes. Show what happens without alignment (misaligned access, potential crash on ARM). JS bridge: TypeScript generics that enforce type safety at the API level.

12. **Lesson 12: Arena Growth via Chained Blocks** -- Problem: what happens when the arena's backing buffer fills up? Solution: allocate a new block and chain it to the previous one via a linked list. `arena_push_size` checks remaining space; if insufficient, allocates a new block. `arena_reset` rewinds all blocks but keeps them allocated for reuse. Show pointer stability: addresses from earlier blocks remain valid after growth. ASCII diagram of chained block list.

13. **Lesson 13: Arena Growth via Virtual Memory (mmap)** -- Alternative growth strategy: reserve a large virtual address range (e.g., 1 GB) with `mmap(PROT_NONE)`, then commit pages as needed with `mprotect(PROT_READ|PROT_WRITE)`. Advantage over chained blocks: contiguous address space, no linked list traversal. Trade-off: platform-specific (Linux/macOS mmap, Windows VirtualAlloc). This is how production arenas work (Casey's Handmade Hero, Zig's allocator). JS bridge: pre-allocated `ArrayBuffer` that you "unlock" sections of as needed.

14. **Lesson 14: TempMemory Checkpoints -- begin/end** -- Problem: you want to use the arena for temporary work within a function, then undo those allocations while keeping everything before. Solution: `TempMemory` struct that saves the arena's current offset. `temp_begin(arena)` captures the state. `temp_end(temp)` restores it. Show nested checkpoints. Show the danger: using a pointer obtained inside a temp scope after `temp_end` is use-after-free. Add `arena_check` debug assertion. JS bridge: `try/finally` -- mark a scope, guarantee cleanup when it ends.

15. **Lesson 15: Arena Usage Patterns** -- Three real-world patterns, each as a standalone demo: (a) **Per-frame arena** -- game loop resets every frame. (b) **Per-request arena** -- HTTP-style request handler allocates into a request arena, frees all when response is sent. (c) **Per-thread arena** -- each thread gets its own arena, no locks needed. Show why each pattern works and when to choose it. Cross-reference: Casey's permanent + transient arena pair in Handmade Hero.

16. **Lesson 16: Arena-Backed String Handling** -- Build `str.h/c`: a string library that allocates from an arena. `str_push_copy(arena, cstr)`, `str_push_fmt(arena, fmt, ...)`, `str_push_concat(arena, a, b)`. No `strdup`/`strndup` (those use malloc). Show how arena strings avoid the entire category of "who owns this string?" bugs. Build a string interning table. JS bridge: JavaScript strings are immutable and GC'd; arena strings are immutable (once pushed) and freed in bulk.

### Group 4 -- Advanced Allocator Patterns (Lessons 17-22)

Beyond arenas: specialized allocators for specific use cases.

17. **Lesson 17: Stretchy Buffer / Dynamic Array** -- Build `stretchy_buf.h`: an stb-style dynamic array using macros. `buf_push(arr, val)`, `buf_pop(arr)`, `buf_len(arr)`, `buf_cap(arr)`. The array header is stored BEFORE the data pointer (negative offset trick). Show geometric growth (double capacity on overflow). This is the C equivalent of `std::vector` or JavaScript's `Array.push()`. Benchmark against naive realloc-every-time.

18. **Lesson 18: Pool Allocator -- Fixed-Size Free List** -- Build `pool.h/c`: pre-allocate N fixed-size slots. Free slots are chained via an intrusive free list (the free slot's memory IS the next pointer). `pool_alloc()` pops from free list. `pool_free()` pushes back. O(1) alloc and free, zero fragmentation. Show why this is perfect for entities in a game loop (same-sized objects created and destroyed frequently). JS bridge: database connection pool. Cross-reference: LOATs pool system is a more advanced version of this pattern.

19. **Lesson 19: Free List on Arena -- Mid-Lifetime Deallocation** -- Problem: arenas can't free individual allocations. Sometimes you need to free specific objects before the arena resets (e.g., removing a single entity from a collection). Solution: maintain a free list within the arena's memory. When an object is "freed," add it to the free list. When allocating, check the free list first, then fall back to arena push. Trade-off: only works for fixed-size objects (same limitation as pool allocator).

20. **Lesson 20: Stack Allocator with LIFO Headers** -- Build `stack_alloc.h/c`: like an arena, but each allocation stores a header with its size. `stack_free()` pops the most recent allocation by reading the header and rewinding. LIFO-only (last allocated must be first freed). Show where this is useful: undo systems, recursive algorithms with temporary buffers. Compare with arena TempMemory (which is a simpler version of the same idea).

21. **Lesson 21: Slab Allocator** -- Build `slab.h/c`: multiple pool allocators, one per size class (16, 32, 64, 128, 256 bytes). `slab_alloc(size)` routes to the appropriate pool. This eliminates fragmentation while supporting variable-sized allocations. Show how Linux kernel's SLAB/SLUB allocator uses the same principle. Benchmark against raw malloc for mixed-size workloads.

22. **Lesson 22: Buddy Allocator (Survey)** -- Survey lesson (not a full build). Explain the buddy system: split power-of-two blocks on allocation, coalesce buddies on free. Show the splitting and coalescing with ASCII diagrams. Discuss where buddy allocators are used (OS page allocators, GPU memory managers). This is the most complex allocator in the course -- the goal is understanding, not necessarily building a production version. Provide a minimal working implementation for the student to study.

### Group 5 -- Tools and Debugging (Lessons 23-25)

Professional debugging tools. Reuse the bug programs from L07-L08.

23. **Lesson 23: Valgrind Memcheck Deep Dive** -- Run every bug program from L07-L08 under Valgrind. Learn to read Valgrind output: "Invalid read of size 4," "Conditional jump depends on uninitialised value," "definitely lost: 64 bytes." Show Valgrind's shadow memory technique. Show how to suppress known issues. Show the performance cost (20-50x slowdown). Practical workflow: run under Valgrind before every commit.

24. **Lesson 24: ASan, UBSan, MSan, TSan** -- Compile-time sanitizers as an alternative to Valgrind. ASan (address errors: overflow, use-after-free), UBSan (undefined behavior: signed overflow, null deref), MSan (uninitialized reads), TSan (data races). Show compiler flags for each. Show output format. Compare with Valgrind: faster (2-3x vs 20-50x), but requires recompilation. Practical workflow: always compile with `-fsanitize=address,undefined` during development.

25. **Lesson 25: Full Custom Debug/Tracking Allocator** -- Extend `debug_alloc.h/c` from L09 into a production-grade debug allocator: call-site tracking (`__FILE__`, `__LINE__`), allocation size histogram, peak memory watermark, leak report with allocation origin, double-free detection with freed-list, guard pages via mmap for critical allocations. This is the student's most complex C program so far. JS bridge: Chrome DevTools memory profiler -- same information, but built by hand.

### Group 6 -- Performance and Optimization (Lessons 26-28)

How memory layout affects performance. The CPU cache is the hidden variable that makes or breaks performance.

26. **Lesson 26: Cache Locality and Access Patterns** -- Explain CPU cache hierarchy: L1 (32 KB, ~1 ns), L2 (256 KB, ~4 ns), L3 (8 MB, ~12 ns), main memory (~60 ns). Show that sequential access is 10-100x faster than random access on the same data. Benchmark: iterate an array sequentially vs iterate via random indices. Show cache line size (64 bytes) and how it affects struct layout. ASCII diagrams showing cache lines loading and evicting.

27. **Lesson 27: SoA vs AoS** -- The single most impactful optimization for data-heavy programs. Array of Structs: `Entity entities[1000]` where each Entity has position, velocity, health, sprite, AI state. Struct of Arrays: separate `float positions_x[1000]`, `float positions_y[1000]`, etc. Show that iterating only positions in AoS loads entire Entity structs into cache (wasting bandwidth). SoA loads only position data (cache-efficient). Benchmark both. Show the hybrid approach: hot/cold struct separation. Cross-reference: LOATs Data Layout Lab, GPP Ch 17 (Data Locality), Anton's SoA discussion.

28. **Lesson 28: Batch Allocation and Memory Pools in Hot Paths** -- Combine everything: arena allocation for temporary frame data, pool allocator for entities, SoA layout for hot data, batch processing to maximize cache utilization. Profile a realistic workload: 10,000 entities with position update, collision check, and state machine tick. Show the difference between naive (malloc per entity, AoS, random access) and optimized (pool + SoA + sequential access). This is the "put it all together" performance lesson.

### Group 7 -- Real-World Integration (Lessons 29-30)

Apply everything to build real, useful data structures.

29. **Lesson 29: Arena-Backed Hash Map** -- Build `hash_map.h/c`: open-addressing hash map where all memory comes from an arena. `hash_map_create(arena, capacity)`, `hash_map_set(map, key, value)`, `hash_map_get(map, key)`. Show FNV-1a hash function. Show linear probing with tombstone deletion. Show that the entire hash map is freed by resetting the arena -- no per-entry cleanup needed. Benchmark against a naive linked-list chaining implementation.

30. **Lesson 30: JSON Parser Capstone -- Arena for All Allocations** -- The CAPSTONE. Build a complete JSON parser that uses an arena for every allocation: string storage, parse nodes, arrays, objects. Parse a non-trivial JSON file (a game save file with entities, positions, inventory). Show that the entire parse tree is freed with one `arena_reset()` call -- no recursive cleanup, no reference counting, no GC. Benchmark against a version using malloc for each node. This is the culmination of the course: arenas make complex allocation patterns simple, fast, and correct. Target: ~8,000-10,000 words.

---

## Lesson Dependency Map

```
L01-L06: Independent foundations (can be done in any order, but presented sequentially for narrative)
L07-L08: Independent (bug demos -- need ASan from build system)
L09:     Independent (creates debug_alloc.h/c)
L10:     Creates lib/arena.h (basic push/reset)
L11:     Extends lib/arena.h (alignment macros)
L12:     Extends lib/arena.h (chained block growth)
L13:     Extends lib/arena.h (mmap-based growth) -- depends on L05 (virtual memory)
L14:     Extends lib/arena.h (TempMemory) -- arena.h is COMPLETE after this
L15:     Uses final arena.h (pattern demos)
L16:     Uses final arena.h, creates lib/str.h/c
L17:     Uses final arena.h, creates lib/stretchy_buf.h
L18:     Uses final arena.h, creates lib/pool.h/c
L19:     Uses final arena.h, creates lib/freelist.h/c
L20:     Creates lib/stack_alloc.h/c (independent of arena)
L21:     Creates lib/slab.h/c (uses pool concepts from L18)
L22:     Self-contained buddy allocator
L23-L24: Reuse bug programs from L07-L08
L25:     Extends lib/debug_alloc.h/c from L09
L26-L28: Use bench.h (independent of specific allocator choice, but reference arenas)
L29:     Uses final arena.h, creates lib/hash_map.h/c
L30:     Uses final arena.h, lib/str.h (capstone -- ties everything together)
```

---

## Skill Inventory

| Skill | Introduced | Practiced |
|-------|-----------|-----------|
| Reading memory addresses / pointer arithmetic | L01 | L02, L03, L05, L10 |
| Stack frame understanding | L02 | L14 (TempMemory mirrors stack unwinding) |
| malloc / calloc / realloc / free | L03 | L07, L09, L17, L28 |
| Struct padding and alignment | L04 | L11, L18, L21, L27 |
| mmap / mprotect | L05 | L06, L13, L25 |
| File I/O (mmap vs fread) | L06 | L30 (JSON file loading) |
| ASan / Valgrind interpretation | L07 | L08, L23, L24 |
| Custom allocator design | L09 | L10-L14, L18-L22, L25 |
| Arena push / reset | L10 | L11-L16, L29, L30 |
| Arena alignment | L11 | L12-L16 |
| Arena growth strategies | L12-L13 | L15 (per-frame/request/thread patterns) |
| TempMemory checkpoints | L14 | L15, L30 |
| Arena-backed string handling | L16 | L29 (hash map keys), L30 (JSON strings) |
| Dynamic arrays (stretchy buf) | L17 | L30 (JSON arrays) |
| Pool allocator (fixed-size free list) | L18 | L19, L28 |
| Free list on arena | L19 | L29 (hash map tombstones) |
| Benchmarking | L03 | L06, L10, L17, L18, L26-L29 |
| Cache-aware coding | L26 | L27, L28 |
| SoA layout design | L27 | L28 |
| Complete system integration | L29 | L30 |

---

## Quality Standards

### Code Quality (applies to every file in `course/src/`)

| Rule | Detail |
|------|--------|
| C11 only | No C++ features. Use `_Alignof`, `_Alignas`, `_Static_assert` |
| No undefined behavior | ASan-clean on all non-bug lessons (L07-L08 intentionally trigger UB) |
| Zero-initialization as design principle | `Arena arena = {0}` is a valid empty arena. No constructors. |
| Consistent naming | `lowercase_snake_case` for types and functions. Prefixed namespaces: `arena_push`, `pool_alloc`, `str_push` |
| Section headers in source | `/* ============ Section Name ============ */` style |
| JS bridge comments | `/* JS: Array.push() | C: buf_push() */` first time a pattern appears |
| Compilable in isolation | Every `lesson_NN.c` compiles with `./build-dev.sh --lesson=NN` |

### Lesson Quality

| Rule | Detail |
|------|--------|
| Each lesson standalone compilable | Student can build after every lesson |
| One major concept per lesson | Don't introduce arenas AND chained growth AND TempMemory in one lesson |
| Testable milestone each lesson | Student runs the program and sees something new |
| Web dev bridges present | Every first use of a C concept has a JS/TS counterpart |
| ASCII diagrams for every operation | Allocate, free, grow, shrink -- show memory state before and after |
| No forward references | Lesson N doesn't assume knowledge from Lesson N+2 |
| Problem before solution | Every technique motivated by a concrete failure scenario |
| Complete file at end | Student can copy-paste the final code block and compile |

---

## Cross-References to Existing Courses

| Concept in This Course | Related Course | Connection |
|---|---|---|
| Arena allocator (L10-16) | LOATs | Memory Arena Lab (Scene 17) uses the arena pattern built here |
| Pool allocator (L18) | LOATs | Things pool is a game-specific pool allocator. This course builds the generic version. |
| Free list (L19) | LOATs | LOATs free list for slot reuse (L08) is the same pattern applied to entity management |
| Struct padding (L04) | LOATs | Understanding why `sizeof(Thing)` matters for napkin math in LOATs L02 |
| Cache locality (L26-27) | LOATs | Data Layout Lab (Scene 15) demonstrates AoS vs SoA with entities |
| Per-frame arena (L15) | Platform Foundation | The platform loop's scratch arena for temporary per-frame data |
| mmap / virtual memory (L05, L13) | Platform Foundation | Understanding what happens under the hood when the platform allocates backbuffers |
| Debug allocator (L09, L25) | All courses | Debug allocation tracking can be applied to any project |
| Arena-backed strings (L16) | GPP / GEA | String interning pattern used in parsers, asset systems, config files |
| Object Pool pattern (L18) | GPP Ch 19 | GPP describes the pattern abstractly; this course implements it concretely |
| Data Locality (L26-27) | GPP Ch 17 | GPP describes why layout matters; this course measures and proves it |
| Memory management subsystem | GEA Ch 6 | GEA describes engine memory architecture; this course builds the components |
| Permanent + transient arenas (L15) | Handmade Hero | Casey's two-arena model for game state (permanent) and per-frame scratch (transient) |
| Arena-backed hash map (L29) | Handmade Hero | Casey builds similar structures for string interning and asset lookup |
| Virtual memory arena (L13) | Handmade Hero | Casey's arena grows via VirtualAlloc (Windows) / mmap (Linux) |

---

## Future Lab Ideas

These are NOT part of the 30-lesson course, but could be added as optional advanced modules later, following the LOATs lab pattern (interactive, toggleable modes, terminal visualization):

1. **Memory Visualizer Lab** -- Terminal-based visualization of heap/stack/arena allocations happening in real time. Show a live ASCII representation of the process memory map as allocations and frees occur. Modes: malloc trace, arena trace, pool trace, combined.

2. **Fragmentation Lab** -- Compare allocation strategies under churn (random alloc/free cycles). Show how malloc creates fragmentation holes while arenas have zero fragmentation. Visualize the heap as a bar chart of allocated and free blocks. Modes: malloc-only, arena+pool, best-fit free list, first-fit free list.

3. **Performance Benchmark Lab** -- Side-by-side benchmarks of every allocator built in the course: malloc, arena push, pool alloc, slab alloc, stack alloc, stretchy buf push. Show allocation throughput (ops/sec), memory overhead (bytes per allocation), and latency distribution (min/avg/max/p99). This is the "which allocator should I use?" reference.

4. **Cache Behavior Lab** -- Visualize cache hits and misses for different access patterns. Show a simulated 64-byte cache line being loaded and evicted. Modes: sequential access, random access, stride access, AoS iteration, SoA iteration. Show hit rates and estimated cycle counts for each.

5. **Lifetime Visualizer Lab** -- Show allocation lifetimes as horizontal bars on a timeline. Group by arena/pool/heap. Show that arena allocations form clean rectangular blocks (same start, same end) while malloc allocations are scattered rectangles of different lengths. This visually demonstrates WHY arenas work: lifetime coherence.

---

## Anti-Drift Systems

### 1. Terminology (STRICT)

| Term | Definition | DO NOT use |
|------|-----------|------------|
| Arena | Bump-pointer allocator with bulk reset | "linear allocator" (correct but less common), "region allocator" |
| Push | Allocate from an arena by bumping the offset forward | "alloc" (reserved for malloc-style), "bump" (informal) |
| Reset | Set arena offset back to zero, invalidating all allocations | "clear" (ambiguous), "free" (implies per-allocation) |
| TempMemory | Checkpoint that saves/restores arena state | "scope" (too abstract), "savepoint" (database term) |
| Pool | Fixed-size allocator with free list | "object pool" (GPP term -- use in cross-refs only) |
| Free list | Intrusive linked list of available slots/blocks | "available list" |
| Canary | Known byte pattern placed before/after allocation for overflow detection | "guard bytes" (acceptable alternative) |
| Fill pattern | Known byte value written to freed/uninitialized memory | "poison bytes" (acceptable alternative) |
| Backing memory | The underlying memory block that an allocator manages | "buffer" (too generic) |
| ZII | Zero Is Initialization -- `= {0}` produces a valid state | "default initialization" (C++ term with different meaning) |

### 2. C Style Rules

```c
// lowercase_snake_case for types and functions
typedef struct arena arena;
struct arena { ... };

typedef struct temp_memory temp_memory;
struct temp_memory { arena *arena; size_t saved_offset; };

// Prefixed function names (namespace substitute)
arena_push_size();
arena_reset();
temp_begin();
temp_end();
pool_alloc();
pool_free();
str_push_copy();
str_push_fmt();

// Type-safe macros in UPPER_CASE
ARENA_PUSH_STRUCT(arena, Type);
ARENA_PUSH_ARRAY(arena, count, Type);

// Section headers
/* ============================================================ */
/*                     Arena Allocator                           */
/* ============================================================ */
```

### 3. Quality Benchmarks

3 **golden reference lessons** that define ideal quality. Every review cycle compares new/rewritten lessons against these:

1. **Lesson 10: Arena Basics** -- Tests: ASCII diagrams (arena internals, push operation, reset operation, per-frame pattern), JS bridge quality (React render cycle analogy), benchmark methodology, build-along pacing (concept before code, small chunks, compile checkpoints)
2. **Lesson 07: Memory Bugs** -- Tests: problem-driven approach (each bug demonstrated with working code BEFORE the fix), ASan/Valgrind output shown and explained, JS bridge for each bug category, common mistakes section quality
3. **Lesson 30: JSON Parser Capstone** -- Tests: integration quality (arena + strings + parse tree all working together), complexity management (10,000-word lesson that stays clear), final benchmark showing arena advantage

**Quality dimensions measured per review:**

| Dimension | Ideal (benchmark level) | Warning Signs |
|---|---|---|
| **Mental Model** | ASCII diagram + plain-language explanation BEFORE any code | Jumps straight into implementation |
| **Problem First** | Concrete broken example BEFORE the fix | Solution presented without motivation |
| **Build-Along** | 3-5 incremental code chunks per concept, compile checkpoints | Whole file dumped at once |
| **Web Dev Bridge** | Concrete mapping to JS/TS/React/Node that illuminates | Missing, or forced analogy that confuses |
| **ASCII Diagrams** | Memory state shown before AND after every operation | Text-only explanation of spatial concepts |
| **Failure Modes** | Concrete bugs with "here's what happens" scenarios | Vague "be careful" warnings |
| **Cross-refs** | 1-3 links to existing courses per lesson | >5 (spam) or 0 (isolated) |
| **Depth** | Covers the concept fully with engineering reasoning | "See external resource for details" |

### 4. Rolling Summaries (`summaries/`)

Every group of lessons, produce a summary:

```
summaries/
  group-1-foundations.md        (lessons 01-06)
  group-2-common-bugs.md        (lessons 07-09)
  group-3-arena-allocators.md   (lessons 10-16)
  group-4-advanced-allocators.md (lessons 17-22)
  group-5-tools-debugging.md    (lessons 23-25)
  group-6-performance.md        (lessons 26-28)
  group-7-capstone.md           (lessons 29-30)
```

Each contains:

- Concepts introduced in this group (and cumulative total)
- Library state (which files exist, what API they expose)
- Key code patterns established
- Skills the student now has
- Open questions or forward references to resolve

### 5. Review Protocol

**Every 3 lessons** -- local review:

1. Guideline alignment (terminology, style, naming consistent?)
2. ASCII diagrams present for EVERY memory operation?
3. Problem-before-solution ordering respected?
4. Web dev bridges present and helpful (not forced)?
5. Code compiles clean with `./build-dev.sh --lesson=NN`? ASan-clean?
6. Quality benchmark comparison (is this lesson as good as the golden references?)
7. Complete compilable file at end of lesson?

**After each group** -- full system audit:

1. Concepts in correct dependency order?
2. Any contradictions across lessons?
3. Library APIs consistent? (arena.h from L10 through L14)
4. Terminology consistent across all lessons?
5. Rolling summary up to date?
6. All cross-references valid?

---

## Example Output Structure

```
ai-llm-knowledge-dump/generated-courses/c-memory-management/
├── README.md
├── PLAN.md
├── PLAN-TRACKER.md
├── UPGRADE-PLAN.md
├── UPGRADE-TRACKER.md
├── summaries/
│   ├── group-1-foundations.md
│   ├── group-2-common-bugs.md
│   ├── group-3-arena-allocators.md
│   ├── group-4-advanced-allocators.md
│   ├── group-5-tools-debugging.md
│   ├── group-6-performance.md
│   └── group-7-capstone.md
├── course/
│   ├── build-dev.sh                 (--lesson=NN, --bench, --valgrind, --all, -r)
│   ├── build/                       (compiled binaries: lesson_01 through lesson_30)
│   ├── src/
│   │   ├── common/
│   │   │   ├── bench.h              (benchmarking macros)
│   │   │   ├── bench.c              (benchmarking implementation)
│   │   │   └── print_utils.h        (colored output, hex dump, memory viz)
│   │   ├── lib/
│   │   │   ├── arena.h              (arena allocator -- evolves L10-L14)
│   │   │   ├── debug_alloc.h        (debug allocator -- created L09, extended L25)
│   │   │   ├── debug_alloc.c
│   │   │   ├── str.h                (arena-backed strings -- L16)
│   │   │   ├── str.c
│   │   │   ├── stretchy_buf.h       (dynamic array -- L17)
│   │   │   ├── pool.h               (pool allocator -- L18)
│   │   │   ├── pool.c
│   │   │   ├── freelist.h           (free list on arena -- L19)
│   │   │   ├── freelist.c
│   │   │   ├── stack_alloc.h        (LIFO stack allocator -- L20)
│   │   │   ├── stack_alloc.c
│   │   │   ├── slab.h               (slab allocator -- L21)
│   │   │   ├── slab.c
│   │   │   ├── hash_map.h           (arena-backed hash map -- L29)
│   │   │   └── hash_map.c
│   │   └── lessons/
│   │       ├── lesson_01.c          (memory layout exploration)
│   │       ├── lesson_02.c          (stack allocation mechanics)
│   │       ├── lesson_03.c          (heap: malloc/calloc/realloc)
│   │       ├── lesson_04.c          (alignment and sizeof)
│   │       ├── lesson_05.c          (virtual memory, mmap, guard pages)
│   │       ├── lesson_06.c          (memory-mapped file I/O)
│   │       ├── lesson_07.c          (memory bugs: leaks, UAF, double-free)
│   │       ├── lesson_08.c          (buffer overflow, uninitialized, off-by-one)
│   │       ├── lesson_09.c          (debug allocator: canaries + fills)
│   │       ├── lesson_10.c          (arena basics: push + reset)
│   │       ├── lesson_11.c          (arena alignment + type-safe macros)
│   │       ├── lesson_12.c          (arena growth: chained blocks)
│   │       ├── lesson_13.c          (arena growth: virtual memory / mmap)
│   │       ├── lesson_14.c          (TempMemory checkpoints)
│   │       ├── lesson_15.c          (arena patterns: per-frame/request/thread)
│   │       ├── lesson_16.c          (arena-backed string handling)
│   │       ├── lesson_17.c          (stretchy buffer / dynamic array)
│   │       ├── lesson_18.c          (pool allocator: fixed-size free list)
│   │       ├── lesson_19.c          (free list on arena)
│   │       ├── lesson_20.c          (stack allocator: LIFO headers)
│   │       ├── lesson_21.c          (slab allocator)
│   │       ├── lesson_22.c          (buddy allocator: survey)
│   │       ├── lesson_23.c          (Valgrind memcheck deep dive)
│   │       ├── lesson_24.c          (ASan, UBSan, MSan, TSan)
│   │       ├── lesson_25.c          (full custom debug allocator)
│   │       ├── lesson_26.c          (cache locality + access patterns)
│   │       ├── lesson_27.c          (SoA vs AoS)
│   │       ├── lesson_28.c          (batch allocation + memory pools in hot paths)
│   │       ├── lesson_29.c          (arena-backed hash map)
│   │       └── lesson_30.c          (JSON parser capstone)
│   └── lessons/
│       ├── lesson-01-memory-layout-exploration.md
│       ├── lesson-02-stack-allocation-mechanics.md
│       ├── lesson-03-heap-malloc-calloc-realloc.md
│       ├── lesson-04-alignment-and-sizeof.md
│       ├── lesson-05-virtual-memory-pages.md
│       ├── lesson-06-memory-mapped-file-io.md
│       ├── lesson-07-memory-bugs-leaks-and-dangling.md
│       ├── lesson-08-buffer-overflow-and-uninitialized.md
│       ├── lesson-09-debug-allocator-canaries-and-fills.md
│       ├── lesson-10-arena-basics-push-and-reset.md
│       ├── lesson-11-arena-alignment-and-type-safety.md
│       ├── lesson-12-arena-growth-chained-blocks.md
│       ├── lesson-13-arena-growth-virtual-memory.md
│       ├── lesson-14-arena-temp-memory-checkpoints.md
│       ├── lesson-15-arena-patterns-per-frame-per-request.md
│       ├── lesson-16-arena-string-handling.md
│       ├── lesson-17-stretchy-buffer-dynamic-array.md
│       ├── lesson-18-pool-allocator-fixed-size.md
│       ├── lesson-19-free-list-on-arena.md
│       ├── lesson-20-stack-allocator-lifo.md
│       ├── lesson-21-slab-allocator.md
│       ├── lesson-22-buddy-allocator-overview.md
│       ├── lesson-23-valgrind-memcheck-deep-dive.md
│       ├── lesson-24-asan-ubsan-msan-tsan.md
│       ├── lesson-25-custom-debug-allocator-full.md
│       ├── lesson-26-cache-locality-and-access-patterns.md
│       ├── lesson-27-soa-vs-aos.md
│       ├── lesson-28-batch-allocation-and-memory-pools.md
│       ├── lesson-29-hash-map-with-arena-backing.md
│       └── lesson-30-json-parser-arena-capstone.md
```

---

## Workflow -- follow this order strictly

### Phase 0 -- Analyze (no output yet)

1. Read Ryan Fleury's arena allocator philosophy
2. Read `@ai-llm-knowledge-dump/prompt/course-builder.md`
3. Read `@_ignore/about-me.md` / `@_ignore/about-me-0.md`
4. Read `@ai-llm-knowledge-dump/llm.txt`
5. Read `@ai-llm-knowledge-dump/c-pitfalls-for-web-devs.md`
6. Study existing course source files and lesson files for current quality level

### Phase 1 -- Create/Update Planning Files

Ensure `PLAN.md`, `README.md`, `PLAN-TRACKER.md` are up to date. These already exist -- update them to reflect the upgrade plan if needed.

**Stop here and wait for confirmation before continuing.**

### Phase 2 -- Rewrite All 30 Lessons

Convert every lesson from essay-style to numbered `## Step N` build-along format:

Each lesson rewrite must:
- Restructure into numbered `## Step N -- [descriptive name]` sections
- Add 5-20 lines of code per step with line-by-line explanation
- Add "Build and run" compile checkpoint after each code-adding step
- Add `> **JS bridge:**` callout for every C concept
- Add ASCII diagram for every memory operation (before + after)
- Add complete compilable file at end
- Expand from ~1,300 words to 5,000-8,000 words (10,000 for capstone)
- Add `## Common mistakes` section
- Add `## Exercises` section (2-3 per lesson)
- Add `## Connection to your work` section
- Add `## What's next` section

### Phase 3 -- Verify All Source Files

- All 30 `lesson_NN.c` files compile clean (0 warnings with `-Wall -Wextra -Wpedantic`)
- All non-bug lessons are ASan-clean
- All benchmark-capable lessons produce valid benchmark output with `--bench`
- All non-bug lessons are Valgrind-clean with `--valgrind`

### Phase 4 -- Add Exercises

Add 2-3 guided exercises per lesson:
- Skeleton code provided
- Clear instructions with expected output
- Progressive difficulty (easy/medium/challenge)

### Phase 5 -- Rolling Summaries

Create group summaries in `summaries/` directory after each group of lessons is complete.

### Phase 6 -- Retrospective

Create `COURSE-BUILDER-IMPROVEMENTS.md` documenting what worked, what didn't, and suggestions for future courses.

---

## Engineering Principles to Embed Throughout

These come from Ryan Fleury, Casey Muratori, and the broader systems programming community. Reference them by name in callouts:

- **"When I learned about arenas, all the traditional concerns about memory management in C went away."** (Ryan Fleury) -- Arenas don't just make allocation faster. They eliminate entire categories of bugs.
- **"Arenas are the stack, generalized."** The call stack already does bump allocation (push frame, pop frame). An arena generalizes this to any lifetime you define.
- **"Group by lifetime, not by type."** The fundamental insight: allocations that live and die together should be allocated together. Arenas make this explicit.
- **"Do the napkin math."** Before adding complexity, calculate the actual cost. A 1 MB arena for a frame that uses 200 KB "wastes" 800 KB. Your machine has 16 GB. Move on.
- **"Zero is the default."** If all-zeros is a valid state for every struct, then `memset(0)` is your universal initializer. No constructors needed. This is ZII (Zero Is Initialization).
- **"Two arenas is usually enough."** Casey's Handmade Hero model: one permanent arena (lives forever), one transient/scratch arena (reset every frame). Most programs need nothing more.
- **"The allocator is not the interesting part."** Don't over-engineer allocation. Build a simple arena, use it everywhere, focus your engineering effort on the actual problem your program solves.
- **"Ownership is a graph problem you don't need to solve."** Reference counting, shared_ptr, weak_ptr -- all attempt to solve "who frees this?" Arenas answer: "everyone who was born together dies together." No graph needed.

---

## Common Bugs Reference

| Bug | Symptom | Root Cause | Fix |
|-----|---------|-----------|-----|
| Use-after-reset | Segfault or garbage data after arena reset | Stored a pointer from before reset, used it after | All pointers are invalidated on reset -- re-push if needed |
| Alignment violation | Crash on ARM, slow on x86 | Arena push didn't align the offset | Use `align_forward()` in `arena_push_size` |
| Arena never resets | Memory grows without bound | Forgot `arena_reset()` in the frame loop | Reset at the start of each frame/request |
| TempMemory not ended | Arena grows past intended scope | Called `temp_begin` but forgot `temp_end` | Pair every `temp_begin` with `temp_end` in a pattern |
| Pool free list corruption | Pool runs out of slots despite frees | Freed slot's next pointer not set correctly | Verify free list integrity with debug traversal |
| Double-free in pool | Assertion failure or corruption | Same slot freed twice, added to free list twice | Debug flag: mark freed slots, assert on double-free |
| Canary overwrite undetected | Buffer overflow not caught | Debug allocator canary check only runs on free | Add periodic canary sweep in debug builds |
| sizeof vs countof confusion | Buffer overflow | Used `sizeof(array)` (bytes) where count was needed | Use `sizeof(array)/sizeof(array[0])` or a `COUNTOF` macro |
| Struct padding in serialization | Corrupt data when written to disk | Wrote struct with padding to file, read on different platform | Serialize field by field, or use `__attribute__((packed))` |
| mmap without munmap | Virtual address space exhaustion | Mapped files/regions never unmapped | Track all mmap calls, munmap on cleanup |

---

## Deviation Policy

The `course-builder.md` is a **strong guideline**, not a rigid spec. Deviate when:

- A memory concept requires deeper OS-level explanation than course-builder.md anticipates
- A pattern would confuse a web developer more than help (e.g., excessive use of bitwise operations without motivation)
- A simplification hides something important about how memory actually works

**When you deviate:**

1. Add `/* COURSE NOTE: ... */` in the source file
2. Add a `> **Course Note:**` callout in the relevant lesson
3. Document the deviation in the group's rolling summary

Do **not** deviate just to simplify. Complexity that teaches something important should stay.

---

## Existing State and Upgrade Context

The course already exists with 30 lessons, 30 source files, 9 libraries, and a working build system. The upgrade task is to transform the lessons from reference-style essays (~1,300 words each) into step-by-step build-along tutorials (~5,000-8,000 words each).

**What exists and should be PRESERVED:**
- All 30 source files in `course/src/lessons/` (compile clean, ASan-clean)
- All 9 libraries in `course/src/lib/` (battle-tested, correct)
- Build system `course/build-dev.sh` (debug, bench, valgrind, --all modes)
- Common utilities in `course/src/common/` (bench.h/c, print_utils.h)

**What must be REWRITTEN:**
- All 30 lesson markdown files in `course/lessons/` (essay -> build-along tutorial)

**What must be CREATED:**
- Rolling summaries in `summaries/` directory
- Quality benchmarks documentation

This specification serves as the single source of truth for the rewrite. Every lesson rewrite should be validated against the standards defined here.
