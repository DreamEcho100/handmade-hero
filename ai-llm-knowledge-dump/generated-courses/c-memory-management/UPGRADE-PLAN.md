# C Memory Management Course — Upgrade Plan

## Current State Summary

The course has **excellent content** (30 lessons, 30 compilable source files, 9 libraries, professional build system, JS bridges, ASCII diagrams) but is written as a **reference course** (read-only essays), not an **interactive tutorial course** (step-by-step build-along with active learning).

### What's GOOD (keep):
- 30 complete lessons covering memory layout → arenas → advanced allocators → production patterns
- All 30 source files compile clean (0 warnings, 29/30 ASan-clean)
- Rich JS/TS bridges in every lesson
- 14-28 ASCII diagram lines per lesson
- Professional build system (--lesson=NN, --bench, --valgrind, --all)
- 9 library implementations (arena.h, pool, freelist, slab, etc.)
- Clear progressive difficulty (foundations → bugs → arenas → advanced → tools → performance → capstone)

### What's MISSING (add):
1. **Numbered Steps** — 0/30 lessons have `## Step N` sections (all are essay-style)
2. **Interactive labs** — no hands-on challenges, no scene selector
3. **Noob-friendly explanations** — lessons assume some C familiarity despite having JS bridges
4. **Exercises** — no guided practice problems
5. **Prompt files** — no prompt/specification files exist for this course
6. **Lesson word counts too low** — 1,083-1,721 words per lesson (LOATs standard: 5,000-10,000)

## Upgrade Phases

### Phase 1: Create Prompt Files
Create the specification files that should have existed before building the course:
- `prompt/build-c-memory-management-course/_base.md` — main course spec
- Lab spec files if labs are added

### Phase 2: Rewrite All 30 Lessons as Step-by-Step Tutorials
Convert every lesson from essay-style to numbered `## Step N` build-along format:
- Each step has 5-20 lines of code
- Line-by-line explanation after each step
- "Build and run" compile checkpoint after each step
- `> **Why?**` callout with JS analogy for every C concept
- ASCII diagram for every memory operation (before + after)
- Complete compilable file at end of each lesson
- Target: 5,000-8,000 words per lesson (current: ~1,300)

### Phase 3: Add Labs
Create interactive laboratory scenes similar to LOATs:
- **Memory Visualizer Lab** — visual representation of heap/stack/arena allocations
- **Fragmentation Lab** — compare malloc churn vs arena allocation patterns
- **Performance Lab** — benchmark malloc vs arena vs pool vs slab side by side
- **Cache Lab** — visualize cache-friendly vs cache-unfriendly access patterns

### Phase 4: Add Exercises
Add 2-3 guided exercises per lesson:
- Skeleton code provided
- Clear instructions
- Expected output described

### Phase 5: Verify All Benchmarks + Valgrind
Complete the unfinished Phase 5 from the original course build:
- Run benchmarks on all benchmark-capable lessons
- Verify Valgrind-clean for all non-bug lessons
- Document results

## Priority Order

| Priority | Phase | Estimated effort | Impact |
|---|---|---|---|
| 1 | Phase 1 (Prompt files) | 4-6 hours | Foundation for all future work |
| 2 | Phase 2 (Lesson rewrites) | 60-90 hours | Highest impact — transforms from reference to tutorial |
| 3 | Phase 4 (Exercises) | 20-30 hours | Active learning |
| 4 | Phase 3 (Labs) | 40-60 hours | Advanced interactive learning |
| 5 | Phase 5 (Verification) | 2-4 hours | Quality assurance |

## Lesson-by-Lesson Rewrite Scope

Each of the 30 lessons needs:

| Current | Target | Change needed |
|---|---|---|
| ~1,300 words | ~6,000 words | 4-5x expansion |
| 0 Steps | 6-10 Steps | Full restructure |
| Essay narrative | Build-along tutorial | Complete rewrite |
| 1-2 JS bridges | 5-10 JS bridges | Add bridges for every C concept |
| 14-28 diagram lines | 30-60 diagram lines | Double the diagrams |
| 3-8 code blocks | 10-20 code blocks | More granular chunks |
| 1 compile checkpoint | 5-8 compile checkpoints | After every step |

## Group-by-Group Rewrite Plan

| Group | Lessons | Current words | Target words | Topics |
|---|---|---|---|---|
| Foundations | 01-06 | ~8,300 | ~36,000 | Memory layout, stack, heap, alignment, virtual memory, mmap |
| Bug Demos | 07-09 | ~3,900 | ~18,000 | Leaks, overflows, debug allocators |
| Arena Core | 10-16 | ~8,700 | ~42,000 | Arena push/reset, alignment, chaining, mmap, checkpoints, strings |
| Advanced | 17-22 | ~8,200 | ~36,000 | Stretchy buf, pool, freelist, stack alloc, slab, buddy |
| Tools | 23-25 | ~4,200 | ~18,000 | Valgrind, sanitizers, custom debug |
| Performance | 26-28 | ~4,200 | ~18,000 | Cache, SoA/AoS, batch allocation |
| Capstone | 29-30 | ~3,200 | ~16,000 | Hash map, JSON parser |
| **TOTAL** | **30** | **~41,000** | **~184,000** | |

## Quality Standards (from LOATs course)

Every rewritten lesson MUST have:
1. `## What we're building` — one paragraph
2. `## What you'll learn` — bullet list
3. `## Prerequisites` — which lessons first
4. Numbered `## Step N — [name]` sections
5. 5-20 lines of code per step with line-by-line explanation
6. "Build and run" after every code-adding step
7. Complete compilable file at end
8. `> **Why?**` for every C concept (with JS analogy)
9. ASCII diagram for every memory operation
10. `## Common mistakes` section
11. `## Exercises` section
12. `## Connection to your work` section
13. `## What's next` section
