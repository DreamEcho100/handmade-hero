# Block 01 Summary — Foundations (Lessons 01-03)

## Concepts introduced

1. **OOP inheritance hierarchies fail for game entities** — inflexible, empty base classes, diamond problem
2. **Fat struct with kind enum** — one struct for all entity types, discriminated by `thing_kind`
3. **Static flat array** — `thing things[MAX_THINGS]`, pre-allocated, no malloc
4. **Slot 0 reserved** — foreshadowing nil sentinel (not yet implemented)
5. **thing_idx** — integer index into the array, replaces pointers
6. **Indices survive reallocation** — pointers break when array base moves; indices don't
7. **Indices trivially serialize** — just write the int to disk

## Thing struct at this point

```c
typedef struct thing {
    thing_kind kind;    /* NIL=0, PLAYER, TROLL, ... */
    thing_idx  parent;  /* index of parent (0 = no parent) */
    float x, y;
    float health;
} thing;  /* ~20 bytes */
```

## Pool state at this point

- Static array `things[MAX_THINGS]` with `next_empty_slot` starting at 1
- Simple bump allocator (no reuse, no free list yet)
- No `used[]` flags yet (coming in lesson 07)
- No nil sentinel behavior yet (coming in lesson 06)

## Key patterns established

- Problem-before-solution: every technique motivated by a concrete failure
- Zero-init: `THING_KIND_NIL = 0` ensures zeroed things are safe
- Napkin math: 1024 things * 48 bytes = ~48 KB — don't over-engineer

## Open questions

- How to handle parent-child relationships beyond just a parent index? → Lesson 04
- What happens when things are removed? → Lesson 07
- How to make index 0 actually safe to dereference? → Lesson 06
