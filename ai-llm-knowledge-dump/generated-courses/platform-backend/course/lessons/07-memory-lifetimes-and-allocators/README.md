# Module 07: Memory Lifetimes and Allocators

This module is where the runtime stops treating memory as a hidden implementation detail and starts treating lifetime, reuse, and reset behavior as part of the architecture.

Everything before this module already depended on memory discipline. This is the point where the course finally makes that discipline explicit.

---

## Observable Outcome

By the end of Module 07, you should be able to:

- explain why the runtime uses `perm`, `level`, and `scratch` instead of scattered heap ownership
- describe an arena as real block-backed memory with a cursor, not just a vague "fast allocator"
- explain how alignment, temp checkpoints, pool reuse, and slab page growth solve different memory problems
- connect allocator strategy back to scene rebuilds and frame cleanup in the live runtime

---

## Lesson Order

1. [lesson-30-arenas-lifetimes-and-platformgameprops.md](lesson-30-arenas-lifetimes-and-platformgameprops.md)
   - start with the lifetime problem and the three arena buckets
2. [lesson-31-arenablock-bootstrap-and-guard-pages.md](lesson-31-arenablock-bootstrap-and-guard-pages.md)
   - see what an arena physically is in memory
3. [lesson-32-arena-push-alignment-and-typed-macros.md](lesson-32-arena-push-alignment-and-typed-macros.md)
   - follow one allocation request through alignment, fit checks, and typed helpers
4. [lesson-33-tempmemory-checkpoints-and-frame-scratch.md](lesson-33-tempmemory-checkpoints-and-frame-scratch.md)
   - add scoped rewind for temporary work inside a still-live arena
5. [lesson-34-poolallocator-free-lists-and-reuse.md](lesson-34-poolallocator-free-lists-and-reuse.md)
   - switch from lifetime grouping to fixed-slot reuse
6. [lesson-35-slab-pages-and-scene-rebuild-integration.md](lesson-35-slab-pages-and-scene-rebuild-integration.md)
   - keep same-size reuse while allowing page growth and reconnect it to scene rebuild ownership

---

## Why This Module Matters

Without this module, later scene labs would feel like magic.

The labs do not only visualize gameplay state. They also visualize allocator pressure, scene-lifetime rebuild rules, nested scratch scopes, and object reuse. This module gives the learner the vocabulary and mental models needed to read that behavior without guessing.

---

## How To Work Through It

For each lesson in this module:

1. ask what problem shape the allocator is solving before reading implementation details
2. separate lifetime questions from reuse questions
3. draw one memory diagram or free-list diagram by hand
4. connect the allocator behavior back to one live runtime call site in `game/main.c`

If you only read the allocator code as pointer tricks, the labs will still feel abstract. If you keep tying each trick back to one runtime ownership problem, the design stays clear.

---

## Verification Before Module 08

Do not move on until you can explain, from memory:

- why `perm`, `level`, and `scratch` are separate lifetime buckets
- what an `ArenaBlock` header stores and why the `Arena` handle is separate from it
- why `arena_begin_temp(...)` and `arena_end_temp(...)` are not the same as `arena_reset(...)`
- when the runtime should choose an arena, a pool, or a slab

If those answers are clear, you are ready to move from allocator mechanics into facade, scene, and proof architecture.
