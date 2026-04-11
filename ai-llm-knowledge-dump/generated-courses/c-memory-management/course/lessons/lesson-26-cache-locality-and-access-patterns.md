# Lesson 26 -- Cache Locality and Access Patterns

> **What you'll build:** A benchmark suite that measures row-major vs column-major matrix traversal, sequential vs random array access, and an array-size sweep that reveals L1/L2/L3 cache boundaries on your actual hardware.

## Observable outcome

Row-major traversal is significantly faster than column-major on a 1024x1024 matrix. Sequential access outperforms random access on a 16 MB array by 10-100x. The array-size sweep shows a clear latency staircase as the working set crosses L1, L2, and L3 cache boundaries.

## New concepts

- Cache lines: 64-byte blocks are the unit of transfer between cache and memory
- Spatial locality: accessing nearby addresses is fast because they share a cache line
- Row-major vs column-major stride and its effect on cache utilization
- Hardware prefetcher: detects sequential patterns and pre-loads future cache lines
- The L1/L2/L3/DRAM latency hierarchy

## Files changed

| File | Change type | Summary |
|------|-------------|---------|
| `src/lessons/lesson_26.c` | New | Matrix traversal, sequential/random access, cache sweep benchmarks |
| `src/common/bench.h` | Dependency | Benchmark timing infrastructure |

---

## Background -- why memory access order matters more than algorithm complexity

In web development, we rarely think about memory layout. JavaScript objects are hash maps of properties, and the V8 engine handles the rest. In C, the order in which you touch memory can make a 100x difference in performance -- even with identical algorithmic complexity.

### The cache hierarchy

Modern CPUs do not read individual bytes from DRAM. They fetch **cache lines** -- 64-byte blocks on x86-64. When you read `arr[0]`, the CPU loads bytes `arr[0]` through `arr[63]` into the L1 cache. If your next access is `arr[1]`, it is already in cache (free). If your next access is `arr[4096]`, it is a cache miss (200+ cycle penalty).

```
  +----------+-----------+--------------+--------------+
  | Level    | Size      | Latency      | Bandwidth    |
  +----------+-----------+--------------+--------------+
  | Register | ~1 KB     | 0 cycles     | --           |
  | L1 cache | 32-64 KB  | ~4 cycles    | ~100 GB/s    |
  | L2 cache | 256-512KB | ~12 cycles   | ~50 GB/s     |
  | L3 cache | 4-32 MB   | ~40 cycles   | ~30 GB/s     |
  | DRAM     | GBs       | ~200 cycles  | ~10 GB/s     |
  +----------+-----------+--------------+--------------+
```

At 3 GHz, 4 cycles is ~1.3 nanoseconds. 200 cycles is ~67 nanoseconds. That is a **50x** difference between L1 and DRAM for the same instruction.

### JS analogy: TypedArray vs Object property access

```js
  // TypedArray: contiguous memory, sequential access is fast
  const positions = new Float32Array(1000000);
  for (let i = 0; i < positions.length; i++) {
    positions[i] += 1.0;  // Sequential: cache-friendly
  }

  // Object array: each object may be anywhere in the heap
  const entities = Array.from({length: 1000000}, () => ({x: 0, y: 0}));
  for (let i = 0; i < entities.length; i++) {
    entities[i].x += 1.0;  // Pointer chasing: cache-hostile
  }
```

V8 optimizes this with "hidden classes" and inline caches, but the fundamental problem remains: contiguous data is faster to iterate than scattered data. In C, you have full control over layout.

### Row-major vs column-major

A 2D array in C is stored in **row-major** order:

```
  Logical view:           Memory layout:
  [0][0] [0][1] [0][2]   [0][0] [0][1] [0][2] [1][0] [1][1] [1][2] ...
  [1][0] [1][1] [1][2]    contiguous in memory --->
  [2][0] [2][1] [2][2]

  Row-major traversal (j varies fastest):
    arr[0][0], arr[0][1], arr[0][2], arr[1][0], ...
    Each access is 4 bytes from the last --> one cache line serves 16 ints

  Column-major traversal (i varies fastest):
    arr[0][0], arr[1][0], arr[2][0], arr[0][1], ...
    Each access jumps N*4 bytes --> each access may miss the cache
```

For a 1024x1024 `int` matrix (4 MB), column-major traversal jumps 4096 bytes per access. Each access loads a 64-byte cache line, but only uses 4 bytes of it -- 6.25% utilization. The other 60 bytes are evicted before they are ever read.

---

## Code walkthrough

### Section 1: row-major vs column-major setup

```c
#define MAT_N 1024
int (*mat)[MAT_N] = malloc(MAT_N * MAT_N * sizeof(int));
```

The matrix is allocated as a pointer to arrays of `MAT_N` ints. This syntax `int (*mat)[MAT_N]` creates a proper 2D array in contiguous memory that you can index as `mat[i][j]`. The total size is `1024 * 1024 * 4 = 4 MB` -- too large for L1 (32-64 KB) or L2 (256-512 KB), so access pattern matters enormously.

### Benchmark: row-major vs column-major

```c
/* Row-major: i is outer, j is inner */
BENCH_CASE(suite1, "row-major", iter, ops) {
  int i = (int)(iter / MAT_N);    /* row changes slowly */
  int j = (int)(iter % MAT_N);    /* column changes fast */
  mat[i][j] += 1;
}

/* Column-major: j is outer, i is inner */
BENCH_CASE(suite1, "column-major", iter, ops) {
  int j = (int)(iter / MAT_N);    /* column changes slowly */
  int i = (int)(iter % MAT_N);    /* row changes fast */
  mat[i][j] += 1;
}
```

Both benchmarks do the same work (increment each element once). The only difference is the order of access. On typical hardware, row-major is 3-10x faster for this matrix size.

### Section 2: sequential vs random access

```c
#define ARR_SIZE (4 * 1024 * 1024)    /* 4M ints = 16 MB */
int *arr = malloc(ARR_SIZE * sizeof(int));

/* Pre-computed random indices (deterministic via LCG) */
uint32_t *indices = malloc(ARR_SIZE * sizeof(uint32_t));
for (int i = 0; i < ARR_SIZE; i++)
  indices[i] = lcg_next() % ARR_SIZE;
```

Sequential access (`arr[iter]`) lets the hardware prefetcher work. It detects the linear pattern and pre-loads future cache lines before you ask for them. Random access (`arr[indices[iter]]`) defeats the prefetcher -- every access is unpredictable and may go all the way to DRAM.

For a 16 MB array (exceeds most L3 caches), random access can be 10-100x slower than sequential.

### Section 3: array-size sweep

The most revealing benchmark. It allocates arrays from 4 KB to 16 MB and times a stride-16 walk through each:

```c
for (int s = 0; s < nsweep; s++) {
  size_t sz = sweep_sizes[s];
  size_t count = sz / sizeof(int);

  /* Stride-16 walk: touches one element per cache line */
  for (long p = 0; p < passes; p++) {
    for (size_t i = 0; i < count; i += 16) {
      buf[i] += 1;
    }
  }
}
```

The stride of 16 integers (`16 * 4 = 64 bytes = one cache line`) means each access touches a different cache line. This isolates **latency** from bandwidth. You should see clear jumps:

```
  Array Size    Stride Walk    Notes
  ----------    -----------    -----
  4 KB          1.20 ns        <= L1 (fast)
  32 KB         1.25 ns        <= L1 (fast)
  64 KB         2.80 ns        L2 range       <-- jump!
  256 KB        3.10 ns        L2 range
  512 KB        5.50 ns        L3 range       <-- jump!
  4 MB          6.20 ns        L3 range
  16 MB         12.40 ns       > L3 (DRAM)    <-- jump!
```

The latency jumps correspond to the working set exceeding each cache level. Your exact numbers will depend on your CPU's cache sizes.

---

## Common mistakes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Row-major and column-major show similar times | Matrix fits in L1 (too small) | Use `MAT_N >= 1024` so the matrix exceeds L1 |
| Sweep shows no clear latency jumps | CPU has unusually large caches, or compiler optimized away accesses | Check your CPU's cache sizes with `lscpu`; use `volatile` on the accumulator |
| Random access only 2-3x slower, not 10-100x | Array fits in L3 | Use a larger array (16+ MB) to spill to DRAM |
| Results vary wildly between runs | Other processes competing for cache | Run on a quiet system; take the minimum of several iterations |
| Column-major matches row-major at `-O3` | Compiler reordered the loop | Check the generated assembly; some compilers are smart enough to fix this |

---

## Build and run

```bash
./build-dev.sh --lesson=26 -r              # explanations only (no benchmarks)
./build-dev.sh --lesson=26 --bench -r      # run all benchmarks
```

---

## Key takeaways

1. Memory access order can make a 100x difference in performance with identical algorithmic complexity. Cache lines (64 bytes on x86-64) are the fundamental unit -- touching one byte loads 64.
2. Row-major traversal of 2D arrays in C is cache-friendly because C stores arrays in row-major order. Always make the innermost loop index vary the fastest-changing dimension.
3. The hardware prefetcher detects sequential stride patterns and pre-loads future cache lines. Random access defeats it, forcing every access through the full cache hierarchy to DRAM.
4. The array-size sweep is a practical way to measure your CPU's cache boundaries. The latency jumps tell you exactly where L1 ends and L2 begins, where L2 ends and L3 begins, and where L3 spills to DRAM.
5. In JS, `TypedArray` gives you contiguous memory and sequential access patterns. In C, you get this by default with arrays -- but you can also accidentally destroy it with pointer-chasing data structures (linked lists, trees with heap-allocated nodes).
