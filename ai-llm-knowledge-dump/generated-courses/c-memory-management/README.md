# Memory Management in C — A Complete Course

A comprehensive, hands-on course covering every aspect of memory management in C: from foundations through arena allocators to production-grade patterns.

## Source Inspiration

Based on Ryan Fleury's talk *"Everyone is doing memory management wrong"* (Back and Forth S2E02), expanded into a full 30-lesson course covering all memory management topics in C.

## Prerequisites

- Basic C programming (functions, pointers, structs, arrays)
- A Linux/macOS system with `clang` (or `gcc`)
- Optional: `valgrind` for memory checking lessons

## Course Structure

| Group | Lessons | Topic |
|-------|---------|-------|
| 1 | 01-06 | **Foundations** — Memory layout, stack, heap, alignment, virtual memory |
| 2 | 07-09 | **Common Bugs** — Leaks, dangling pointers, overflows, debug allocators |
| 3 | 10-16 | **Arena Allocators** — The core: push/reset, growth, checkpoints, strings |
| 4 | 17-22 | **Advanced Allocators** — Pools, free lists, slabs, buddy allocator |
| 5 | 23-25 | **Tools & Debugging** — Valgrind, sanitizers, custom tracking allocator |
| 6 | 26-28 | **Performance** — Cache locality, SoA vs AoS, batch allocation |
| 7 | 29-30 | **Real-World** — Arena-backed hash map, JSON parser capstone |

## Building and Running

```bash
# Build a single lesson (debug mode with ASan)
./course/build-dev.sh --lesson=01 -r

# Build with benchmarks (optimized, no sanitizers)
./course/build-dev.sh --lesson=10 --bench -r

# Run under valgrind
./course/build-dev.sh --lesson=10 --valgrind -r

# Build all lessons
./course/build-dev.sh --all
```

Each lesson produces a standalone binary at `./course/build/lesson_NN`.

## Key Concepts Covered

- **Arena allocators**: O(1) allocation, bulk free, zero fragmentation
- **Virtual memory**: mmap, page tables, reserve vs commit, guard pages
- **Memory bugs**: leak detection, use-after-free, double-free, buffer overflow
- **Pool/slab/buddy allocators**: fixed-size allocation, free lists, coalescing
- **Performance**: cache locality, struct layout, SoA vs AoS
- **Debugging tools**: Valgrind, AddressSanitizer, UBSan, custom debug allocators
- **Comparative benchmarks**: every key concept includes correct vs naive implementations measured in hot loops
