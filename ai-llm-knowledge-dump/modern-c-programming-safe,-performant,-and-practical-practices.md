# Modern C Programming: Safe, Performant, and Practical Practices

> **Audience:** DreamEcho100 — a full-stack JS/TypeScript developer learning C in the context of
> the Handmade Hero series. Every pillar includes a JS analogy, a Casey Muratori philosophy
> alignment, and a "common beginner mistake" callout.
>
> **Teaching philosophy (Casey Muratori style):** Explain _why_, not just _what_. No cargo-culting.
> Understand the rule before you apply it. Simplicity beats abstraction. Performance is a feature.

---

## How to read this document

Each pillar is structured as:

1. **What** — the rule or practice
2. **Why** — the reasoning from first principles
3. **JS analogy** — bridge from your web-dev background
4. **Casey alignment** — which Handmade Hero philosophy this serves
5. **Code example** — what to do vs. what to avoid
6. **Common mistake** — the error beginners make and exactly why it is wrong

A quick reference summary table appears at the end.

---

## Pillar 1 — Avoid Undefined Behavior (UB)

### What

Never write code whose behavior the C standard leaves undefined. If the standard says "undefined
behavior," the compiler is allowed to delete your code, make demons fly out of your nose, or
silently corrupt memory. All three have been observed in production.

Common UB sources:

- Signed integer overflow (`INT_MAX + 1`)
- Dereferencing a NULL or dangling pointer
- Reading an uninitialized variable
- Out-of-bounds array access
- Shifting by ≥ the type width
- Strict-aliasing violations (casting between incompatible pointer types)

### Why

C gives you maximum control over the machine. The price is that the standard hands the compiler
_license_ to assume UB never happens. A sufficiently smart optimizer will use that assumption to
rearrange and eliminate code in ways that look insane to a human reader. The correct response is
not "my compiler is broken" — it is "I caused UB."

### JS analogy

In JS, `undefined[0]` throws a `TypeError` immediately — the runtime protects you. In C there is
no runtime guardian. Reading past the end of an array does not crash predictably; it reads _some_
memory and keeps going. The first symptom usually appears far from the actual bug.

### Casey alignment

Philosophy 6 — **Debug Ability Over Correctness.** UB produces bugs that are invisible for months
and then manifest as corrupted data in an unrelated function. Eliminating UB sources makes every
bug loud and local — exactly what Casey wants.

### Code example

```c
// ❌ BAD: signed overflow is UB — optimizer may remove the loop entirely
int i = INT_MAX;
i++;            // UB! The compiler *assumes* this never happens.

// ✅ GOOD: use unsigned arithmetic when wrapping is intended
unsigned int u = UINT_MAX;
u++;            // Well-defined: wraps to 0

// ❌ BAD: uninitialized read
int x;
printf("%d\n", x);  // UB — could be 0, could be 666, could corrupt stack

// ✅ GOOD: always initialize
int x = 0;
```

### Common mistake

> "It worked fine in debug mode."

Debug builds use `-O0` (no optimization), so the compiler has no room to exploit UB assumptions.
Release builds use `-O2`/`-O3`, and the optimizer _will_ use those assumptions aggressively. If
something only breaks in release, UB is the first suspect.

**Cure:** Enable `-fsanitize=undefined` and run your tests. It turns UB into runtime crashes with
line numbers. See Pillar 9.

---

## Pillar 2 — Own Your Data: Explicit Ownership and Lifetimes

### What

Every chunk of memory has exactly one owner. The owner is responsible for freeing it. Ownership
must be visible from the code structure — never implied by convention.

Think in _lifetime waves_ (Casey's term):

```
Wave 1 — Process lifetime: allocated once at startup, freed by OS on exit
Wave 2 — State lifetime:   allocated when a state begins, freed when it ends
Wave 3 — Frame lifetime:   allocated at frame start, reset (not freed) at frame end
```

### Why

Memory bugs — dangling pointers, double frees, leaks — almost always trace back to unclear
ownership. When two functions both think they own a pointer, one of them will free it and the
other will use the freed memory. When nobody thinks they own it, it leaks.

### JS analogy

JS has a garbage collector that tracks all references and frees memory when the last one drops.
In C, _you_ are the garbage collector. You must track every allocation yourself. Think of it as
if JS gave you `WeakRef` but removed the GC — you must null-out the ref and free the backing
store manually.

```
JS:  let buf = new ArrayBuffer(1024);
     // GC frees it when buf goes out of scope

C:   void* buf = malloc(1024);
     // You MUST call free(buf) when you're done — no GC
```

### Casey alignment

Philosophy 1 — **Resource Lifetimes in Waves.** Casey's key insight: stop thinking about
individual acquire/release pairs and start thinking about _groups_ with shared lifetimes. If all
the entities in a level share one arena, you free the arena when the level ends — one `free()`, O(1),
instead of thousands of individual `free(entity)` calls.

### Code example

```c
// ✅ GOOD: arena pattern — frame allocator
typedef struct {
    uint8_t *base;
    size_t   used;
    size_t   capacity;
} Arena;

void* arena_push(Arena *a, size_t size) {
    assert(a->used + size <= a->capacity);
    void *ptr = a->base + a->used;
    a->used += size;
    return ptr;
}

void arena_reset(Arena *a) {
    a->used = 0;  // 🔴 HOT PATH: O(1) "free" of everything
}

// Frame lifetime pattern:
Arena frame_arena;
// ...
while (running) {
    arena_reset(&frame_arena);     // throw away last frame's scratch data
    update_game(&game, &frame_arena);
    render_game(&game);
}

// ❌ BAD: malloc/free per entity (Wave 3 inside hot path)
for (int i = 0; i < particle_count; i++) {
    Particle *p = malloc(sizeof(Particle));  // 🔴 HOT PATH: heap allocation!
    // ...
    free(p);  // What if we forget? Leak.
}
```

### Common mistake

Returning a pointer to a local variable:

```c
// ❌ DANGLING POINTER — stack memory is gone after return
int* make_number(void) {
    int x = 42;
    return &x;  // UB: x's lifetime ends here
}

// ✅ GOOD options:
// 1) Return by value (prefer this):
int make_number(void) { return 42; }
// 2) Caller provides storage:
void make_number(int *out) { *out = 42; }
// 3) Static (careful — not thread-safe, not re-entrant):
int* make_number(void) { static int x = 42; return &x; }
```

---

## Pillar 3 — Prefer Stack Allocation

### What

Allocate on the stack (local variables, VLAs with known small sizes, fixed arrays) instead of
the heap (`malloc`) whenever the lifetime fits the enclosing scope and the size is bounded.

### Why

Stack allocation is:

- **O(1) and effectively free** — just decrement the stack pointer
- **Cache-friendly** — the stack is almost always hot in L1 cache
- **Automatically freed** — when the function returns, the stack frame is gone
- **Cannot leak** — there is no `malloc` to forget to `free`

### JS analogy

In JS every object goes to the heap. V8 is very good at short-lived objects (generational GC),
but you still pay for GC pauses. In C, a local `int buf[256]` costs you literally one instruction
(move the stack pointer). There is no GC, no collection pause, no allocator overhead.

### Casey alignment

Philosophy 7 — **No Premature Optimization, But No Stupid Code Either.** Using the heap when
the stack suffices is "stupid code" — you pay allocator overhead for no reason. Keep scratch
buffers on the stack; graduate to arenas or static arrays for larger, longer-lived data.

### Code example

```c
// ✅ GOOD: small fixed scratch buffer on stack — zero overhead
void format_score(char *out, int score) {
    char buf[32];           // Stack: free, cache-hot, auto-released
    snprintf(buf, sizeof(buf), "SCORE: %d", score);
    // ... use buf ...
}

// ❌ BAD: heap allocation for a 32-byte string that lives one frame
void format_score_bad(char *out, int score) {
    char *buf = malloc(32); // 🔴 Allocator overhead + possible NULL + must free
    snprintf(buf, 32, "SCORE: %d", score);
    // ... use buf ...
    free(buf);              // Easy to forget
}
```

### Common mistake

Stack overflow from a large local array:

```c
// ❌ STACK OVERFLOW RISK: 4 MB on the stack — default Linux stack = 8 MB
void load_level(void) {
    uint8_t level_data[4 * 1024 * 1024];  // Blows up silently!
}

// ✅ GOOD: large data lives in static storage or on the heap
static uint8_t level_data[4 * 1024 * 1024];  // Allocated at program start
```

Rule of thumb: anything over ~64 KB should not be a local variable.

---

## Pillar 4 — `const`-Correctness

### What

Mark every pointer parameter `const T *` when the function does not modify what the pointer
points to. Mark every pointer-to-pointer `T * const p` when the pointer itself won't be
reassigned. Be explicit.

### Why

`const` is a promise to the compiler and to every reader: "I won't modify this." The compiler
can use that promise to:

- Catch accidental writes at compile time
- Prove aliasing doesn't apply, enabling better optimization
- Allow the parameter to be the result of a `const` expression

### JS analogy

Think of `const` in JS: `const arr = [1,2,3]` — you can't reassign `arr`, but you can
mutate `arr[0]`. In C, `const int *p` means "I won't change what p points to" (analogous to
freezing the array contents), while `int * const p` means "I won't reassign the pointer itself"
(analogous to `const arr` in JS — can't rebind, but can mutate through).

### Code example

```c
// ✅ GOOD: read-only access makes the contract visible
int sum_pixels(const uint32_t *pixels, size_t count) {
    int total = 0;
    for (size_t i = 0; i < count; i++) {
        total += (int)(pixels[i] & 0xFF);
    }
    return total;
}

// ❌ BAD: read-only function silently accepts a non-const pointer —
//         the reader can't tell if pixels are modified
int sum_pixels_bad(uint32_t *pixels, size_t count) { ... }
```

### Common mistake

Dropping `const` with a C-cast to "fix" a warning:

```c
// ❌ BAD: casting away const is a smell — you're lying to the compiler
void shady(const char *s) {
    char *mutable = (char *)s;  // Now we can "accidentally" change s
    mutable[0] = 'X';           // UB if s points to a string literal!
}
```

If you need to modify the string, the parameter should not be `const` — fix the signature, not
the warning.

---

## Pillar 5 — Use `size_t` for Sizes and Array Indices

### What

Use `size_t` (from `<stddef.h>`) for all sizes, counts, and array indices. Use `ptrdiff_t` for
signed differences between pointers.

### Why

`size_t` is the unsigned integer type that is guaranteed to hold the largest possible object size
on the target platform. On a 64-bit system it is a 64-bit unsigned integer. On a 32-bit system
it is 32-bit. Using `int` or `unsigned int` for a size can silently truncate on 64-bit
platforms when the size exceeds 2 GB — and game assets do reach that range.

Standard library functions (`memcpy`, `malloc`, `strlen`, `fread`) all speak `size_t`. Mixing
`int` with their return/parameter types produces signed-unsigned comparison warnings and
potential truncation bugs.

### JS analogy

In JS, array indices are just numbers. The language handles the width. In C you are choosing the
width yourself and you must choose a type that is at least as wide as the address space.

### Code example

```c
// ✅ GOOD
size_t width  = 800;
size_t height = 600;
size_t pixel_count = width * height;  // No overflow on 64-bit

uint32_t *pixels = malloc(pixel_count * sizeof(uint32_t));
for (size_t i = 0; i < pixel_count; i++) {
    pixels[i] = 0xFF000000;
}

// ❌ BAD: int overflows at 2^31 pixels (~2 billion); signed/unsigned mismatch
int n = width * height;
for (int i = 0; i < n; i++) { ... }  // Compiler warning; wrong on large images
```

### Common mistake

Comparing `size_t` to -1 or any negative value:

```c
size_t find_index(...);  // Returns SIZE_MAX to signal "not found"

// ❌ BUG: (size_t)(-1) == SIZE_MAX, NOT -1 — this comparison is always false
if (find_index(x) == -1) { ... }

// ✅ CORRECT:
if (find_index(x) == SIZE_MAX) { ... }
```

---

## Pillar 6 — Centralize and Control Allocation

### What

Route all dynamic allocation through a small number of well-known allocation primitives that you
control. Avoid scattered `malloc`/`free` calls throughout the codebase.

Patterns in order of preference:

1. **Fixed-size static arrays** — allocated at compile time, zero runtime cost
2. **Stack arrays** — for small, scope-limited scratch data
3. **Arena allocators** — one large `malloc` up front; bump-pointer inside
4. **Free-list allocators** — for fixed-size objects that need reuse
5. **Raw `malloc`/`free`** — last resort, documented at the call site with its lifetime wave

### Why

Scattered `malloc` calls make lifetimes implicit, make profiling hard, make debugging harder,
and prevent batch operations. A centralized arena means you can dump the allocator's state,
track high-water marks, detect overflows, and reset everything with one call.

### Casey alignment

Philosophy 1 — **Resource Lifetimes in Waves** and
Philosophy 8 — **Ownership and Control.** Casey allocates one giant block at startup, subdivides
it manually into arenas for each system, and never calls `malloc` in the game loop. Every
allocation is intentional and visible.

### Code example

```c
// ✅ GOOD: all game memory from a single large allocation
#define GAME_MEMORY_SIZE (256 * 1024 * 1024)  // 256 MB

typedef struct {
    Arena permanent;   // Wave 1 — whole process
    Arena level;       // Wave 2 — current level
    Arena frame;       // Wave 3 — per-frame scratch
} GameMemory;

GameMemory *game_memory_init(void) {
    // One malloc for the whole game
    uint8_t *backing = malloc(GAME_MEMORY_SIZE);
    assert(backing && "Out of memory at startup");
    // ... partition backing into sub-arenas ...
}

// ❌ BAD: malloc in the game update function (hot path)
void update_game(GameState *gs) {
    // 🔴 HOT PATH: heap allocation 60×/second
    Particle *p = malloc(sizeof(Particle));
    // Where is free? Who owns p? Nobody knows.
}
```

### Common mistake

Checking `malloc == NULL` every time but ignoring `assert`:

```c
// ❌ WEAK: silently returns a broken struct on OOM
Texture load_texture(const char *path) {
    Texture t = {0};
    t.data = malloc(size);
    if (!t.data) return t;  // Caller won't check the empty struct
    // ...
    return t;
}

// ✅ CASEY STYLE: fail fast and loud at the boundary
Texture load_texture(const char *path) {
    void *data = malloc(size);
    if (!data) {
        fprintf(stderr, "FATAL: out of memory loading '%s'\n", path);
        exit(1);
    }
    // ...
}
```

---

## Pillar 7 — Data-Oriented Design (DOD)

### What

Organize your data structures around how the _CPU_ reads them, not around how the _problem
domain_ models them. Prefer arrays of scalars over arrays of structs when you process all
instances each frame.

### Why

Modern CPUs are memory-bound: L1 cache is 64 bytes per cache line. If you iterate over an
array of fat structs and only touch one field per iteration, you evict useful data on every
cache miss, wasting ~200 cycles per miss. Processing a tightly packed array of just the field
you need keeps the cache hot.

```
Object-oriented layout (bad for sequential updates):
┌──────────────────────────────────────────────────┐
│ Enemy[0]: pos(12), vel(12), tex_id(4), ai(256)  │ ← 284 bytes
├──────────────────────────────────────────────────┤
│ Enemy[1]: pos(12), vel(12), tex_id(4), ai(256)  │
└──────────────────────────────────────────────────┘
Updating positions reads 284 B per enemy but only uses 24 B → 91% waste

Data-oriented layout (cache-friendly):
┌────────────────────┐  ┌────────────────────┐  ┌───────────┐
│ positions[0..N]    │  │ velocities[0..N]   │  │ tex_ids[] │
│ Vec3 × N = 12N B  │  │ Vec3 × N = 12N B  │  │  ...      │
└────────────────────┘  └────────────────────┘  └───────────┘
Updating positions reads 12 B per enemy, 100% useful data
```

**SoA layout is also the structural precondition for compiler auto-vectorization.** A contiguous `float x[N]` array can be processed with SIMD instructions — 4 or 8 values per cycle using SSE2/AVX. An interleaved AoS `Enemy` struct array cannot: the padding bytes between each `x` field break the contiguous run the SIMD unit requires. Cache efficiency and auto-vectorizability share the same root cause — both depend on the hot data being physically adjacent in memory. Getting the layout right once buys both benefits for free.

### JS analogy

JS objects are hash maps internally — each property lookup is an indirection. A JS `Float32Array`
holding a million positions is fundamentally different: typed, contiguous, no dispatch, and
something V8 can SIMD-vectorize. DOD in C is you doing that transformation deliberately.

### Casey alignment

Philosophy 2 — **Performance is a Feature** and
Philosophy 7 — **No Premature Optimization, But No Stupid Code Either.** Data layout is an
architectural decision, not a micro-optimization. Getting it right from the start costs nothing.
Getting it wrong forces a painful rewrite when perf becomes a problem.

### Code example

```c
// ❌ BAD: array of structs (AoS) — cache-unfriendly update
typedef struct {
    float x, y;        // 8 bytes
    float vx, vy;      // 8 bytes
    int   sprite_id;   // 4 bytes
    char  name[64];    // 64 bytes — rarely used during update!
} Enemy;
Enemy enemies[1000];   // Update loop drags 84 KB through cache

// ✅ GOOD: struct of arrays (SoA) — cache-friendly update
#define MAX_ENEMIES 1000
typedef struct {
    float x[MAX_ENEMIES], y[MAX_ENEMIES];        // 8 KB — fits in L1!
    float vx[MAX_ENEMIES], vy[MAX_ENEMIES];      // 8 KB
    int   sprite_id[MAX_ENEMIES];                // 4 KB
    char  name[MAX_ENEMIES][64];                 // Only touched when rendering name
    int   count;
} EnemyPool;

// Update all positions: touches 8 KB of contiguous floats ✅
for (int i = 0; i < pool.count; i++) {
    pool.x[i] += pool.vx[i];
    pool.y[i] += pool.vy[i];
}
```

### Common mistake

Applying SoA everywhere dogmatically. Use AoS when:

- You almost always need all fields together (e.g., a `Vertex` with pos + uv + normal)
- The number of instances is small (< ~64) — cache effects are negligible

Profile first. Layout is not free to change after the fact, but neither is premature SoA
conversion.

---

## Pillar 8 — Explicit Error Handling

### What

Return errors explicitly via return values or out-parameters. Never silently ignore failure.
Fail fast and loud (print the error + the context + `exit(1)`) at unrecoverable points. Use
`errno` + `strerror` when wrapping OS calls.

### Why

C has no exceptions. Ignoring a return code is not "optimistic" — it is programming by wishful
thinking. `fopen` returns NULL on failure. `malloc` returns NULL on OOM. A missed check means
your program continues with a NULL pointer and crashes far from the real problem, leaving no
useful error information.

### Casey alignment

Philosophy 6 — **Debug Ability.** A program that exits with
`"FATAL: fopen('config.dat') failed: No such file or directory"` is infinitely easier to debug
than one that segfaults 100 lines later inside a rendering function.

### Code example

```c
// ✅ GOOD: explicit error, useful message, contextual info
FILE *f = fopen(path, "rb");
if (!f) {
    fprintf(stderr, "FATAL: cannot open '%s': %s\n", path, strerror(errno));
    exit(1);
}

// ✅ GOOD: error return pattern for library code
typedef enum { ERR_OK = 0, ERR_IO, ERR_OOM, ERR_BAD_MAGIC } ErrorCode;

ErrorCode load_asset(const char *path, Asset *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return ERR_IO;
    // ...
    return ERR_OK;
}

// ❌ BAD: silent swallow — now you have a NULL and no idea why
FILE *f = fopen(path, "rb");
// (forgot to check)
fread(buf, 1, 64, f);  // Segfault here, no diagnostic
```

### Common mistake

Checking `malloc` but not `fopen`, or checking `fopen` but not `fread`:

```c
// fread returns the number of elements read — check it!
size_t read = fread(buf, sizeof(Header), 1, f);
if (read != 1) {
    fprintf(stderr, "Truncated file: expected header, got %zu elements\n", read);
    exit(1);
}
```

---

## Pillar 9 — Sanitizers and Static Analysis

### What

Build with sanitizers during development:

| Sanitizer                          | Flag                   | Catches                                        |
| ---------------------------------- | ---------------------- | ---------------------------------------------- |
| AddressSanitizer (ASan)            | `-fsanitize=address`   | Buffer overflows, use-after-free, double-free  |
| UndefinedBehaviorSanitizer (UBSan) | `-fsanitize=undefined` | Signed overflow, null deref, misaligned access |
| MemorySanitizer (MSan)             | `-fsanitize=memory`    | Reads from uninitialized memory                |

Run `clang-tidy` or `cppcheck` for static analysis. Use `valgrind --tool=memcheck` on Linux for
leak detection when ASan is unavailable.

Also use strict compiler warnings: `-Wall -Wextra -Wconversion -Wshadow`.

### Why

These tools transform invisible, randomly-manifesting bugs into loud crashes with exact file and
line numbers. The overhead (2–10× slowdown) is acceptable in debug builds; sanitizers are
disabled in release.

### Casey alignment

Philosophy 6 — **Debug Ability.** The sanitizers are the best "crash-and-tell-me-where" tool
you have. Casey wants bugs to be loud and local — sanitizers operationalize that requirement.

### Code example (build flags in `build-dev.sh`)

```bash
# Debug build — all sanitizers on
clang -g -O0 \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -Wall -Wextra -Wconversion -Wshadow \
    -o game_debug game.c

# Release build — sanitizers off, optimize
clang -O2 -DNDEBUG -o game game.c
```

### Common mistake

Disabling sanitizers because "it runs slower." A game that crashes on a user's machine due to
a use-after-free costs more than every minute of slower debug iteration. Keep them on for all
debug and test runs.

---

## Pillar 10 — `static inline` Over Function-Like Macros

### What

Replace function-like `#define` macros with `static inline` functions wherever possible.
Reserve macros for genuine compile-time constants, conditional compilation (`#ifdef`), and
token-pasting that `inline` cannot do.

### Why

Macros are textual substitution — the preprocessor copies the text verbatim before the
compiler sees it. This means:

- No type checking — `MAX(1.0, 2)` silently promotes
- Double evaluation — `MAX(i++, j++)` increments twice
- No debugger visibility — you can't step into a macro
- Confusing error messages — errors point to the expansion site

`static inline` gives you the same zero-overhead inlining with full type safety, single
evaluation, debugger visibility, and readable error messages.

### JS analogy

A macro is like an `eval()` string — textual substitution with no type enforcement. A
`static inline` is like a regular function — the compiler verifies it.

### Code example

```c
// ❌ BAD: macro with double-evaluation bug
#define MAX(a, b) ((a) > (b) ? (a) : (b))
int result = MAX(i++, j++);  // BUG: i or j incremented twice!

// ✅ GOOD: typed, single-evaluation, debuggable
static inline int max_int(int a, int b) { return a > b ? a : b; }
static inline float max_f32(float a, float b) { return a > b ? a : b; }

// ✅ GOOD: macros are fine for compile-time constants
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600
#define TILE_SIZE      32

// ✅ GOOD: macros for conditional compilation
#ifdef DEBUG
    #define ASSERT(cond) do { if (!(cond)) { fprintf(stderr, "ASSERT FAILED: " #cond " at %s:%d\n", __FILE__, __LINE__); __builtin_trap(); } } while(0)
#else
    #define ASSERT(cond) ((void)(cond))
#endif
```

### Common mistake

Using `static inline` for large functions you expect the compiler to actually inline. `inline`
is a _hint_. If the function is large, the compiler will ignore it. Use `__attribute__((always_inline))`
sparingly and only after profiling proves it matters.

---

## Pillar 11 — Fixed-Width Integer Types (`<stdint.h>`)

### What

Use `int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`
from `<stdint.h>` whenever the bit width matters (pixel data, file formats, wire protocols,
hardware registers). Use `size_t` for sizes and counts. Use plain `int` for loop indices and
flags where width is irrelevant.

### Why

C's `int`, `long`, and `short` have _implementation-defined_ widths. `int` is at least 16 bits;
on most platforms it is 32 bits, but MSVC x64 also makes `long` 32 bits while GCC x64 makes it
64 bits. Code that assumes `long == 64 bit` breaks silently when compiled on a different platform.

Pixel data, file headers, and network packets have exact layout requirements. Use exact-width
types there and the code is correct on every platform by construction.

### JS analogy

JS has `TypedArray`: `Uint8Array`, `Int32Array`, `Float64Array` — exactly the same idea. When
you create a `Uint8Array` you know every element is 8 bits unsigned. `stdint.h` gives you the
same guarantee in C.

### Code example

```c
#include <stdint.h>

// ✅ GOOD: pixel format is exactly 32 bits everywhere
typedef uint32_t Pixel;  // ARGB: A[31:24] R[23:16] G[15:8] B[7:0]

Pixel make_pixel(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)(0xFF000000) |
           ((uint32_t)r << 16)   |
           ((uint32_t)g <<  8)   |
           (uint32_t)b;
}

// ✅ GOOD: file header with exact layout
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;    // 0x4D42 "BM"
    uint32_t size;
    uint16_t reserved1, reserved2;
    uint32_t offset;
} BmpHeader;
#pragma pack(pop)

// ❌ BAD: 'long' is 32 bits on Windows, 64 bits on Linux x64
typedef unsigned long Pixel;  // Width is platform-dependent!
```

### Common mistake

Mixing `int` and `uint32_t` in arithmetic:

```c
int x = some_function();   // Could be negative
uint32_t mask = x & 0xFF;  // Signed/unsigned mix — implicit conversion warning
                            // If x is negative, the result is surprising
```

---

## Pillar 12 — Clean Header Modules

### What

A header (`.h`) is a _public interface declaration file_, not an implementation file. It must:

- Declare types, structs, enums, and `extern` variables that other TUs need
- Declare function prototypes for exported functions
- Never define (implement) a function unless it is `static inline`
- Never define a variable
- Never `#include` headers it does not directly use
- Have include guards or `#pragma once`

### Why

Each `.h` included in N source files is compiled N times. A definition in a header creates N
copies and a linker error ("multiple definition of `foo`"). An unnecessary `#include` in a
widely-used header pollutes every TU's compilation and increases build times.

### JS analogy

A header is like a TypeScript `.d.ts` declaration file — it describes the _shape_ of the API,
not the implementation. The implementation lives in `.c` files (analogous to `.js` or `.ts`
files). You wouldn't put function bodies in a `.d.ts`.

### Code example

```c
// ✅ GOOD: game/main.h — pure declarations
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int     x, y;
    int     width, height;
    uint8_t tile_id;
} Tile;

void game_update(GameState *gs, float dt);
void game_render(const GameState *gs, uint32_t *pixels, size_t pitch);

// ❌ BAD in a header: definition creates multiple-definition linker error
// int g_score = 0;            // WRONG — put in game.c as: int g_score = 0;
                               //         declare in header as: extern int g_score;

// ❌ BAD in a header: non-inline function definition
// void game_update(...) { ... }  // WRONG — put this in game.c
```

### Common mistake

Circular includes. If `game.h` includes `utils.h` and `utils.h` includes `game.h`, you break
the build. Fix: forward-declare the type you need instead of including the full header.

```c
// utils.h — only needs to know GameState exists, not its contents
struct GameState;                          // Forward declaration
void utils_log_state(struct GameState *s); // OK

// ❌ BAD utils.h:
// #include "game.h"   // Circular!
```

---

## Pillar 13 — Strict Build Flags

### What

Enable aggressive compiler warnings and treat them as errors. Never ship code that produces
warnings. Maintain separate debug and release build flags.

Recommended flag set for `clang` on Linux:

```bash
# Errors
-Werror

# Core warnings
-Wall -Wextra

# Extra precision
-Wconversion           # Implicit narrowing conversions
-Wshadow               # Local variable hides outer scope variable
-Wdouble-promotion     # float silently promoted to double
-Wundef                # Undefined macro used in #if
-Wnull-dereference      # Possible null dereference
-Wstrict-prototypes    # Missing parameter types in function declaration

# Debug only
-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer

# Release only
-O2 -DNDEBUG
```

### Why

Warnings are the compiler telling you "this is probably not what you meant." `-Wconversion`
catches the `int` → `uint8_t` truncation bugs that produce wrong pixel colors. `-Wshadow`
catches the variable naming collisions that produce wrong values. `-Werror` makes them build
failures so they cannot be ignored.

### Casey alignment

Philosophy 9 — **Tools Matter.** A one-second build that fails loudly on every warning is a
better tool than a ten-second build that silently ships subtle bugs.

### Common mistake

Suppressing warnings with casts instead of fixing the underlying problem:

```c
uint8_t r = (uint8_t)some_int;  // Cast silences -Wconversion warning
                                 // But IF some_int > 255, r is silently truncated

// ✅ BETTER: clamp explicitly — document the intent
uint8_t r = (uint8_t)(some_int > 255 ? 255 : some_int < 0 ? 0 : some_int);
```

---

## Pillar 14 — Write Deterministic, Reproducible Code

### What

Given the same input, your program must always produce the same output. Eliminate hidden
non-determinism: uninitialized reads, HashMap iteration order, PRNG without seed, races
between threads (if any).

For games: see it as a precondition for input recording/playback (Casey implements this in
Handmade Hero): record all inputs → re-feed them → game replays identically.

### Why

Non-determinism makes bugs impossible to reproduce reliably and makes testing impossible to
automate. If the game crashes only on Tuesdays because `rand()` is seeded with `time(NULL)`,
you cannot write a regression test for it.

### Casey alignment

Philosophy 10 — **Transparency and Inspectability.** Input playback is Casey's live-coding
debug tool. Determinism is its prerequisite.

### Code example

```c
// ✅ GOOD: seed the PRNG from a fixed or recorded seed
uint32_t rng_seed = 42;  // Or: record the player's chosen seed

static uint32_t rng_next(uint32_t *state) {
    // xorshift32 — fast, deterministic, reproducible
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (*state = x);
}

// ❌ BAD: time-seeded — different output every run
srand((unsigned)time(NULL));
int val = rand();  // Can't record/replay, can't unit test
```

### Common mistake

Relying on pointer ordering. Pointers from `malloc` are not at fixed addresses between runs
(ASLR), so comparing two unrelated pointers as a sort key produces different orders in different
runs. Only compare pointers within the same array.

---

## Pillar 15 — Minimal, Honest APIs

### What

Design function interfaces to accept exactly the data they need and no more. Do not pass a
large struct when the function only needs two fields. Do not return a struct when you only need
to signal success/failure. Make every parameter visible at the call site.

### Why

Minimal APIs are composable, testable, and readable. A function that takes a `DrawContext*` (50
fields) is harder to test, harder to reuse, and harder to reason about than one that takes
`(uint32_t *pixels, size_t pitch, int x, int y, uint32_t color)`.

### Casey alignment

Philosophy 3 — **Simplicity Over Abstraction** and
Philosophy 5 — **Compression-Oriented Programming.** If the entire function fits in your head,
you can reason about it, debug it, and optimize it.

### Code example

```c
// ✅ GOOD: takes only what it needs — can be tested in isolation
void draw_rect(uint32_t *pixels, int pitch,
               int x, int y, int w, int h,
               uint32_t color);

// ❌ BAD: takes the whole world — you must construct a full GameContext to test it
void draw_rect(GameContext *ctx, Rect r, StyleSheet *style);
```

### Common mistake

"God function" parameters — passing a global `GameState` to a pure math utility:

```c
// ❌ draw_rect has no reason to know about tiles, enemies, or audio state
void draw_rect(GameState *gs, int x, int y, int w, int h);

// The rule:
// utils/ functions MUST NOT take any game-specific type as parameter
// (See utils/ parameter test in course-builder.md)
```

---

## Pillar 16 — Document Invariants and Preconditions

### What

Write a comment above every function that documents:

- What it _assumes_ about its inputs (preconditions)
- What it _guarantees_ about its output (postconditions)
- Any invariants the caller must maintain

Use `assert()` to machine-check preconditions at runtime in debug builds.

### Why

Undocumented preconditions are time bombs. If `draw_sprite` assumes `sprite_id < MAX_SPRITES`
but nobody writes that down, future code will call it with an unchecked index, causing an
out-of-bounds write that corrupts unrelated memory a hundred frames later.

### Casey alignment

Philosophy 6 — **Debug Ability.** An `assert` that fires immediately at the function entry
with a message like `assert(id < MAX && "sprite_id out of range")` is infinitely easier to
debug than silent memory corruption.

### Code example

```c
// ✅ GOOD: precondition documented + asserted
// Precondition:  pixels != NULL, pitch >= width, 0 <= x < pitch, 0 <= y < height
// Postcondition: The pixel at (x, y) contains ARGB color.
void draw_pixel(uint32_t *pixels, int pitch, int x, int y, uint32_t color) {
    assert(pixels != NULL);
    assert(x >= 0 && y >= 0);
    assert(x < pitch);
    pixels[y * pitch + x] = color;
}
```

### Common mistake

Asserting the precondition but not including the diagnostic message:

```c
assert(id < MAX_SPRITES);     // Fires with "Assertion failed" — which id? Where?

assert(id < MAX_SPRITES && "sprite_id out of range — check caller");  // ✅
```

In C, the `&&` with a string literal is always truthy as far as the assert condition goes; the
string only appears in the failure message to give context.

---

## Pillar 17 — Aggressive Assertions

### What

Use `assert()` freely in debug builds to enforce every invariant, precondition, and postcondition
you can think of. Compile it to a no-op in release with `-DNDEBUG`. For truly fatal conditions
that must be checked in release, use an `ASSERT_FATAL` macro that calls `exit(1)`.

### Why

An assertion is documentation that executes. It says "I believe this is always true here." If
it fires, you immediately know the belief was wrong — at exactly the line where the assumption
was made, not hundreds of lines later in the symptom.

### Code example

```c
// In a shared header (e.g., base.h):

// Debug assert — compiled out in release builds
#ifdef NDEBUG
    #define ASSERT(cond)       ((void)0)
    #define ASSERT_MSG(c, msg) ((void)0)
#else
    #define ASSERT(cond) \
        do { if (!(cond)) { \
            fprintf(stderr, "ASSERT: " #cond "\n  at %s:%d\n", __FILE__, __LINE__); \
            __builtin_trap(); \
        }} while (0)
    #define ASSERT_MSG(cond, msg) \
        do { if (!(cond)) { \
            fprintf(stderr, "ASSERT: " msg "\n  (" #cond ")\n  at %s:%d\n", \
                    __FILE__, __LINE__); \
            __builtin_trap(); \
        }} while (0)
#endif

// Release-present fatal assert — use for unrecoverable external errors
#define ASSERT_FATAL(cond, msg) \
    do { if (!(cond)) { \
        fprintf(stderr, "FATAL: " msg "\n"); \
        exit(1); \
    }} while(0)
```

### Common mistake

Using `assert` for user input or external data (file contents, network input):

```c
// ❌ BAD: user can craft a file with a bad magic number — assert is disabled in release
assert(header.magic == EXPECTED_MAGIC);

// ✅ GOOD: external data failures must be handled in release too
ASSERT_FATAL(header.magic == EXPECTED_MAGIC, "corrupt save file");
```

`assert` is for _internal_ logic invariants. External data must always be validated with
proper error handling, even in release.

---

## Pillar 18 — Minimize and Contain Global State

### What

Avoid mutable global variables. If you need shared state, pass it explicitly as a parameter or
collect it into a single well-named context struct that is passed down the call tree.

### Why

Mutable globals are the source of action-at-a-distance bugs: `draw_rect` silently reads
`g_current_color`; someone changes `g_current_color` in an unrelated function; `draw_rect` now
produces the wrong color. The root cause is invisible at the call site.

### JS analogy

Mutable global state in C is like mutating `window.someGlobal` from multiple unrelated modules —
it works until it doesn't, and when it breaks it's extremely hard to trace.

### Code example

```c
// ❌ BAD: hidden state coupling draw and whatever sets this global
uint32_t g_current_color;  // Who last touched this?

void draw_rect(...) { ... g_current_color ... }

// ✅ GOOD: explicit — color is visible at the call site
void draw_rect(uint32_t *pixels, int pitch, int x, int y,
               int w, int h, uint32_t color);

// ✅ GOOD: if you must share state, make it explicit
typedef struct { uint32_t draw_color; ... } DrawContext;
void draw_rect(DrawContext *dc, int x, int y, int w, int h);
```

### Common mistake

Static local variables used as hidden state between calls:

```c
// ❌ BUG: non-re-entrant, hard to test, order-dependent
int get_next_id(void) {
    static int counter = 0;
    return counter++;
}
```

This is fine for monotone IDs within a well-understood scope. It becomes a problem when the
"reset" behavior or thread safety is not obvious to the next reader.

---

## Pillar 19 — Simplicity as a Design Principle

### What

When two implementations solve the same problem, prefer the one a reader can fully understand
in 30 seconds. Complexity is not sophistication — it is liability. Every line you write is a
line you must debug, maintain, and explain.

### Why

Casey's mantra: "The best code is the code you don't write. The second best is code so simple
you can hold it all in your head at once." A 50-line game loop you understand beats a 5-line
macro-generated loop you have to reverse-engineer every time you read it.

### Casey alignment

Philosophy 3 — **Simplicity Over Abstraction** and
Philosophy 5 — **Compression-Oriented Programming.**

### Practical rules

1. **Write the simple thing first.** Add abstraction only when you have two _concrete_ duplicates
   and a clear _mechanical_ way to unify them.

2. **Prefer flat over nested.** Three levels of indentation is a smell. If you need four, the
   function is doing too much.

3. **Prefer concrete over generic.** A `render_tile(Tile t, ...)` is clearer than a
   `render_entity(void *entity, EntityType type, ...)`.

4. **Delete dead code.** Commented-out code, `#if 0` blocks, and "maybe I'll need this later"
   helpers are noise. Git gives you history — delete confidently.

5. **Short functions.** A function that fits on one screen (< ~50 lines) can be fully understood
   in one read. If you need to scroll, consider splitting at a natural seam.

### Code example

```c
// ❌ BAD: over-engineered "flexible" clear
typedef enum { CLEAR_COLOR, CLEAR_CHECKERBOARD, CLEAR_GRADIENT } ClearMode;
void clear_screen(uint32_t *pixels, size_t count, ClearMode mode, ...);

// You only ever call it with CLEAR_COLOR. The other modes are dead code.

// ✅ GOOD: simple, direct, one job
void clear_screen(uint32_t *pixels, size_t count, uint32_t color) {
    for (size_t i = 0; i < count; i++) pixels[i] = color;
}
```

### Common mistake

The "I'll need this later" abstraction. Resist the urge to make things _configurable_ when you
have exactly one configuration. YAGNI — You Aren't Gonna Need It — applies to C as much as to
JavaScript.

---

## Pillar 20 — Measure Before You Optimize

### What

Never change code for performance reasons without a profiler measurement confirming that the
changed code is actually on the hot path and is actually the bottleneck.

Tools: `perf stat` / `perf record` + `perf report` on Linux; `gprof`; Casey's preferred
approach: manually instrument with `QueryPerformanceCounter` / `clock_gettime` and print frame
timings.

### Why

Intuitions about what is slow are wrong more often than right. The bottleneck is almost always
in a place you did not expect. Optimizing the wrong function wastes time and adds complexity
with zero user-visible benefit.

### Casey alignment

Philosophy 2 — **Performance is a Feature** and
Philosophy 7 — **No Premature Optimization.** Casey's mantra: "I don't care what you _think_
is fast. I care what your _profiler_ says is fast." Design with performance in mind from the
start (data layout, arena allocation, batching) but do not micro-optimize random functions.

### Code example

```c
// ✅ GOOD: simple timing instrumentation — Casey's approach
#include <time.h>

static double get_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// In game loop:
double t0 = get_seconds();
update_game(&game, dt);
double t1 = get_seconds();

double render_t0 = get_seconds();
render_game(&game, pixels);
double render_t1 = get_seconds();

// Print every second so you can see trends
if (frame_count % 60 == 0) {
    printf("update: %.2f ms  render: %.2f ms\n",
           (t1 - t0) * 1000.0,
           (render_t1 - render_t0) * 1000.0);
}
```

### Common mistake

Micro-optimizing the renderer before profiling reveals it is the bottleneck:

```c
// ❌ Spent 3 hours hand-unrolling this loop
// Profiler: renderer = 2 ms/frame, game logic = 14 ms/frame → wrong bottleneck!

// ✅ Profile first:
// perf record -g ./game
// perf report
// THEN decide where to spend effort
```

---

## Quick Reference Table

| Pillar | Rule                                | Casey Philosophy            | Key flag / tool            |
| ------ | ----------------------------------- | --------------------------- | -------------------------- |
| 1      | Avoid UB                            | #6 Debug Ability            | `-fsanitize=undefined`     |
| 2      | Explicit ownership / lifetime waves | #1 Resource Lifetimes       | Arena pattern              |
| 3      | Prefer stack allocation             | #7 No Stupid Code           | Local arrays < 64 KB       |
| 4      | `const`-correctness                 | #3 Simplicity               | Compiler enforced          |
| 5      | `size_t` for sizes                  | #7 No Stupid Code           | `<stddef.h>`               |
| 6      | Centralized allocation              | #1 Lifetimes / #8 Ownership | Arena allocator            |
| 7      | Data-oriented design (SoA)          | #2 Performance              | CPU cache profiler         |
| 8      | Explicit error handling             | #6 Debug Ability            | `errno` / `strerror`       |
| 9      | Sanitizers + strict warnings        | #9 Tools Matter             | ASan / UBSan               |
| 10     | `static inline` over macros         | #3 Simplicity               | Compiler inlining          |
| 11     | Fixed-width types (`stdint.h`)      | #7 No Stupid Code           | `<stdint.h>`               |
| 12     | Clean headers (declarations only)   | #5 Compression              | `#pragma once`             |
| 13     | Strict build flags                  | #9 Tools Matter             | `-Wall -Wextra -Werror`    |
| 14     | Deterministic code                  | #10 Inspectability          | Fixed RNG seed             |
| 15     | Minimal honest APIs                 | #3 / #5 Simplicity          | Parameter audit            |
| 16     | Document invariants                 | #6 Debug Ability            | `assert` + comments        |
| 17     | Aggressive assertions               | #6 Debug Ability            | `assert` / `ASSERT` macro  |
| 18     | Minimize global state               | #5 Compression              | Explicit context structs   |
| 19     | Simplicity as design principle      | #3 / #5                     | Code review: fits in head? |
| 20     | Measure before optimizing           | #2 / #7 Performance         | `perf`, `clock_gettime`    |

---

## Interaction with the Three-Layer Architecture

These pillars apply to all three layers, but the application differs:

| Pillar           | `utils/` (Reusable systems)                          | `game/` (Game logic)                         | `platforms/` (OS layer)                       |
| ---------------- | ---------------------------------------------------- | -------------------------------------------- | --------------------------------------------- |
| 2 (Ownership)    | Return by value or caller-provides-storage pattern   | Arena per game state wave                    | Platform allocates backbuffer once (Wave 1/2) |
| 6 (Alloc)        | No internal `malloc` — accept pre-allocated buffers  | Game arena; no per-frame `malloc`            | One-time `malloc` for backbuffer, window      |
| 7 (DOD)          | Generic pixel/math operations can be SoA-friendly    | Decide AoS vs SoA based on update patterns   | Irrelevant (thin layer)                       |
| 8 (Errors)       | Return error codes; no `exit()` in library code      | `exit()` on fatal game data corruption       | `exit()` on fatal OS/GPU failure              |
| 15 (Minimal API) | Must not take any game-specific type as parameter    | may take `GameState*`                        | may take platform-internal state              |
| 18 (Globals)     | Zero globals — all state threaded through parameters | Single `GameState` struct; no module globals | May have one `PlatformState` singleton        |

---

_This document is a living reference. Update it when you discover a new pattern or a new failure
mode. Always cross-reference with `course-builder.md` for how these rules map onto specific
lesson structures._
