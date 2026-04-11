# C Memory Management Course — Upgrade Tracker

## Overview

Upgrading from reference-style (essay) to LOATs-quality interactive tutorial course.

**Current:** 30 lessons, ~41K words, 0 Steps, essay-style
**Target:** 30 lessons, ~184K words, 6-10 Steps each, full build-along tutorials

---

## Phase 1: Prompt Files

| Task | Status | Notes |
|---|---|---|
| Create `prompt/build-c-memory-management-course/` directory | [x] | Done |
| Write `_base.md` (main course spec) | [x] | 876 lines, 8,768 words |
| Write lab spec files (if labs added) | [ ] | After Phase 2 |

## Phase 2: Lesson Rewrites (30 lessons)

### Group 1 — Foundations (Lessons 01-06)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 01 — Memory Layout Exploration | 1,406→3,391 | ~6,000 | 0→7 | [x] Rewritten |
| 02 — Stack Allocation Mechanics | 1,446→3,567 | ~6,000 | 0→7 | [x] Rewritten |
| 03 — Heap malloc/calloc/realloc | 1,305→3,347 | ~6,000 | 0→8 | [x] Rewritten |
| 04 — Alignment and sizeof | 1,328→3,477 | ~6,000 | 0→8 | [x] Rewritten |
| 05 — Virtual Memory Pages | 1,409→3,545 | ~6,000 | 0→6 | [x] Rewritten |
| 06 — Memory-Mapped File I/O | 1,380→3,700 | ~6,000 | 0→8 | [x] Rewritten |

### Group 2 — Bug Demos (Lessons 07-09)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 07 — Memory Bugs & Leaks | 1,276 | ~6,000 | 0→8 | [ ] |
| 08 — Buffer Overflow & Uninitialized | 1,350 | ~6,000 | 0→8 | [ ] |
| 09 — Debug Allocator Canaries | 1,260 | ~6,000 | 0→8 | [ ] |

### Group 3 — Arena Core (Lessons 10-16)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 10 — Arena Basics Push/Reset | 1,564 | ~8,000 | 0→10 | [ ] |
| 11 — Arena Alignment & Type Safety | 1,186 | ~6,000 | 0→8 | [ ] |
| 12 — Arena Growth Chained Blocks | 1,194 | ~6,000 | 0→8 | [ ] |
| 13 — Arena Growth Virtual Memory | 1,294 | ~6,000 | 0→8 | [ ] |
| 14 — Arena TempMemory Checkpoints | 1,182 | ~6,000 | 0→8 | [ ] |
| 15 — Arena Patterns Per-frame | 1,083 | ~6,000 | 0→8 | [ ] |
| 16 — Arena String Handling | 1,227 | ~6,000 | 0→8 | [ ] |

### Group 4 — Advanced Allocators (Lessons 17-22)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 17 — Stretchy Buffer | 1,209 | ~6,000 | 0→8 | [ ] |
| 18 — Pool Allocator | 1,213 | ~6,000 | 0→8 | [ ] |
| 19 — Free List on Arena | 1,336 | ~6,000 | 0→8 | [ ] |
| 20 — Stack Allocator LIFO | 1,427 | ~6,000 | 0→8 | [ ] |
| 21 — Slab Allocator | 1,558 | ~6,000 | 0→8 | [ ] |
| 22 — Buddy Allocator | 1,451 | ~5,000 | 0→6 | [ ] |

### Group 5 — Tools (Lessons 23-25)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 23 — Valgrind Deep Dive | 1,363 | ~6,000 | 0→8 | [ ] |
| 24 — ASan/UBSan/MSan/TSan | 1,380 | ~6,000 | 0→8 | [ ] |
| 25 — Custom Debug Allocator | 1,414 | ~6,000 | 0→8 | [ ] |

### Group 6 — Performance (Lessons 26-28)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 26 — Cache Locality | 1,487 | ~6,000 | 0→8 | [ ] |
| 27 — SoA vs AoS | 1,319 | ~6,000 | 0→8 | [ ] |
| 28 — Batch Allocation & Pools | 1,416 | ~6,000 | 0→8 | [ ] |

### Group 7 — Capstone (Lessons 29-30)

| Lesson | Current words | Target | Steps | Status |
|---|---|---|---|---|
| 29 — Hash Map with Arena | 1,461 | ~8,000 | 0→10 | [ ] |
| 30 — JSON Parser Capstone | 1,721 | ~10,000 | 0→12 | [ ] |

## Phase 3: Labs (after Phase 2)

| Lab | Status | Notes |
|---|---|---|
| Memory Visualizer Lab | [ ] | Visual heap/stack/arena allocations |
| Fragmentation Lab | [ ] | malloc churn vs arena patterns |
| Performance Lab | [ ] | Benchmark malloc vs arena vs pool |
| Cache Lab | [ ] | Cache-friendly vs unfriendly patterns |

## Phase 4: Exercises

| Group | Lessons | Exercises per lesson | Total | Status |
|---|---|---|---|---|
| Foundations | 01-06 | 3 | 18 | [ ] |
| Bug Demos | 07-09 | 3 | 9 | [ ] |
| Arena Core | 10-16 | 3 | 21 | [ ] |
| Advanced | 17-22 | 3 | 18 | [ ] |
| Tools | 23-25 | 2 | 6 | [ ] |
| Performance | 26-28 | 3 | 9 | [ ] |
| Capstone | 29-30 | 2 | 4 | [ ] |

## Phase 5: Verification

| Task | Status |
|---|---|
| All 30 lessons compile clean | [ ] |
| Benchmark verification | [ ] |
| Valgrind clean (non-bug lessons) | [ ] |
| Word count verification (all ≥ 5,000) | [ ] |
| Step count verification (all ≥ 6) | [ ] |
| JS bridge count (all ≥ 5) | [ ] |

## Progress Summary

| Phase | Items | Done | Remaining |
|---|---|---|---|
| 1 — Prompt files | 2 | 0 | 2 |
| 2 — Lesson rewrites | 30 | 0 | 30 |
| 3 — Labs | 4 | 0 | 4 |
| 4 — Exercises | 85 | 0 | 85 |
| 5 — Verification | 6 | 0 | 6 |
| **TOTAL** | **127** | **0** | **127** |
