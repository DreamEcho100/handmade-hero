# Memory Management in C — Course Plan

## Topic Inventory

### Group 1: Foundations
1. C memory layout (text, data, BSS, heap, stack segments)
2. Stack allocation (frames, growth direction, alloca, overflow)
3. Heap allocation (malloc, calloc, realloc, free)
4. Alignment and sizeof (struct padding, packed, _Alignof)
5. Virtual memory (mmap, munmap, pages, reserve vs commit, guard pages)
6. Memory-mapped file I/O (mmap files, comparison with fread)

### Group 2: Common Bugs
7. Memory leaks, use-after-free, double-free
8. Buffer overflow, uninitialized reads, off-by-one
9. Debug allocator (canary bytes, fill patterns, overflow detection)

### Group 3: Arena Allocators (Core)
10. Arena basics (push, reset, fixed-size backing)
11. Arena alignment and type-safe macros
12. Arena growth via chained blocks (pointer stability)
13. Arena growth via virtual memory (mmap reserve + commit)
14. TempMemory checkpoints (begin/end, nested, arena_check)
15. Arena usage patterns (per-frame, per-request, per-thread)
16. Arena-backed string handling

### Group 4: Advanced Allocator Patterns
17. Stretchy buffer / dynamic array
18. Pool allocator (fixed-size, embedded free list)
19. Free list on arena (mid-lifetime deallocation)
20. Stack allocator with LIFO headers
21. Slab allocator
22. Buddy allocator (survey)

### Group 5: Tools & Debugging
23. Valgrind memcheck deep dive
24. ASan, UBSan, MSan, TSan
25. Full custom debug/tracking allocator

### Group 6: Performance & Optimization
26. Cache locality and access patterns
27. SoA vs AoS
28. Batch allocation and memory pools in hot paths

### Group 7: Real-World Integration
29. Arena-backed hash map
30. JSON parser capstone (arena for all allocations)

## Dependency Map

```
L01-L06: Independent (foundations)
L07-L08: Independent (bug demos, need ASan)
L09: Independent (debug_alloc.h/c created here)
L10: Creates lib/arena.h (basic)
L11: Extends lib/arena.h (alignment)
L12: Extends lib/arena.h (chaining)
L13: Extends lib/arena.h (mmap)
L14: Extends lib/arena.h (TempMemory) — arena.h is COMPLETE after this
L15: Uses final arena.h
L16: Uses final arena.h, creates lib/str.h/c
L17: Uses final arena.h, creates lib/stretchy_buf.h
L18: Uses final arena.h, creates lib/pool.h/c
L19: Uses final arena.h, creates lib/freelist.h/c
L20: Creates lib/stack_alloc.h/c (independent of arena)
L21: Creates lib/slab.h/c (independent of arena)
L22: Self-contained buddy allocator
L23-L24: Reuse bug programs from L07-L08
L25: Extends lib/debug_alloc.h/c from L09
L26-L28: Use bench.h, independent
L29: Uses final arena.h, creates lib/hash_map.h/c
L30: Uses final arena.h, lib/str.h (capstone)
```

## Skill Inventory

| Skill | Introduced | Practiced |
|-------|-----------|-----------|
| Reading memory addresses | L01 | L02, L05 |
| malloc/free | L03 | L07, L09, L28 |
| mmap/mprotect | L05 | L06, L13 |
| ASan interpretation | L07 | L08, L24 |
| Arena push/reset | L10 | L11-L16, L29, L30 |
| Arena alignment | L11 | L12-L16 |
| TempMemory checkpoints | L14 | L15, L30 |
| Free list | L18 | L19, L21 |
| Benchmarking | L04 | L06, L10, L16-L18, L26-L29 |
| Cache-aware coding | L26 | L27, L28 |
