# Memory Management in C — Progress Tracker

## Phase 1: Planning Files
- [x] PLAN.md
- [x] PLAN-TRACKER.md
- [x] README.md

## Phase 2: Infrastructure
- [x] `course/build-dev.sh`
- [x] `course/src/common/bench.h`
- [x] `course/src/common/bench.c`
- [x] `course/src/common/print_utils.h`

## Phase 3: Source Code — All reviewed, fixed, building clean

### Library files (reviewed + fixed 2026-03-28)
- [x] `src/lib/arena.h` — Fixed: overflow-safe checks, alignment assert, OOM assert, temp_count reset, arena_push_string, merged arena_free_all+arena_free_mmap→arena_free, renamed arena_alloc_mmap→arena_init_mmap, shared align_forward(), thread safety docs
- [x] `src/lib/debug_alloc.h` / `debug_alloc.c` — Added thread safety docs
- [x] `src/lib/str.h` / `str.c` — Fixed: str_to_cstr out-of-bounds read, str_join overflow check
- [x] `src/lib/hash_map.h` / `hash_map.c` — Fixed: capacity=0 assert, push_key NULL check, added assert.h
- [x] `src/lib/pool.h` / `pool.c` — Fixed: arena allocation NULL check, multiplication overflow check
- [x] `src/lib/freelist.h` / `freelist.c` — Fixed: alignment check in free list search, NULL parameter assert
- [x] `src/lib/stretchy_buf.h` — Fixed: buf_last bounds assert
- [x] `src/lib/slab.h` / `slab.c` — Added thread safety docs
- [x] `src/lib/stack_alloc.h` / `stack_alloc.c` — Added thread safety docs

### Lesson source files (reviewed by agents, 2 fixes applied)
- [x] All 30 lesson .c files build clean (0 warnings)
- [x] 29/30 run clean (L09 is intentional UB for teaching)
- [x] lesson_13.c — Fixed API mismatch (old ArenaMmap removed, now uses arena_init_mmap/arena_free)
- [x] lesson_06.c — Fixed fstat() missing error check

## Phase 4: Lesson Files — Rewritten (expanded from ~100 lines to 250-350 lines each)
- [x] `lessons/lesson-01-memory-layout-exploration.md`
- [x] `lessons/lesson-02-stack-allocation-mechanics.md`
- [x] `lessons/lesson-03-heap-malloc-calloc-realloc.md`
- [x] `lessons/lesson-04-alignment-and-sizeof.md`
- [x] `lessons/lesson-05-virtual-memory-pages.md`
- [x] `lessons/lesson-06-memory-mapped-file-io.md`
- [x] `lessons/lesson-07-memory-bugs-leaks-and-dangling.md`
- [x] `lessons/lesson-08-buffer-overflow-and-uninitialized.md`
- [x] `lessons/lesson-09-debug-allocator-canaries-and-fills.md`
- [x] `lessons/lesson-10-arena-basics-push-and-reset.md`
- [x] `lessons/lesson-11-arena-alignment-and-type-safety.md`
- [x] `lessons/lesson-12-arena-growth-chained-blocks.md`
- [x] `lessons/lesson-13-arena-growth-virtual-memory.md`
- [x] `lessons/lesson-14-arena-temp-memory-checkpoints.md`
- [x] `lessons/lesson-15-arena-patterns-per-frame-per-request.md`
- [x] `lessons/lesson-16-arena-string-handling.md`
- [x] `lessons/lesson-17-stretchy-buffer-dynamic-array.md`
- [x] `lessons/lesson-18-pool-allocator-fixed-size.md`
- [x] `lessons/lesson-19-free-list-on-arena.md`
- [x] `lessons/lesson-20-stack-allocator-lifo.md`
- [x] `lessons/lesson-21-slab-allocator.md`
- [x] `lessons/lesson-22-buddy-allocator-overview.md`
- [x] `lessons/lesson-23-valgrind-memcheck-deep-dive.md`
- [x] `lessons/lesson-24-asan-ubsan-msan-tsan.md`
- [x] `lessons/lesson-25-custom-debug-allocator-full.md`
- [x] `lessons/lesson-26-cache-locality-and-access-patterns.md`
- [x] `lessons/lesson-27-soa-vs-aos.md`
- [x] `lessons/lesson-28-batch-allocation-and-memory-pools.md`
- [x] `lessons/lesson-29-hash-map-with-arena-backing.md`
- [x] `lessons/lesson-30-json-parser-arena-capstone.md`

## Phase 5: Integration Verification
- [x] All 30 lessons compile with `--all` (0 warnings)
- [x] Non-bug lessons are ASan/UBSan clean (29/30 pass)
- [x] L09 intentional UB properly documented and guarded
- [ ] Benchmark lessons produce reasonable numbers with `--bench`
- [ ] Valgrind clean on non-bug lessons
