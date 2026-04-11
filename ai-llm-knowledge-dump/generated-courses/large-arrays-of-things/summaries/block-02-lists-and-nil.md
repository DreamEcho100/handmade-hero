# Block 02 Summary — Lists and Nil (Lessons 04-06)

## Concepts introduced

4. **Intrusive singly-linked list** — `first_child` + `next_sibling` embedded in the thing struct
5. **Intrusive doubly-linked circular list** — `prev_sibling` added, last_child = first_child.prev_sibling
6. **Nil sentinel** — things[0] is permanently zeroed; all invalid accesses return it
7. **Zero initialization as architecture** — design ALL structs so memset(0) produces a valid state
8. **Chain dereferencing** — pool_get chains bottom out at nil, never crash
9. **Debug nil assert** — detect accidental writes to the nil sentinel
10. **Correctness advantage** — intrusive lists prevent items from being in two lists simultaneously

## Thing struct at this point

```c
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float x, y;
    float health;
} thing;  /* ~32 bytes */
```

## Pool state at this point

- Nil sentinel at index 0 (always zeroed)
- `pool_get()` returns nil on invalid access
- `thing_is_valid()` checks kind != NIL
- Intrusive circular sibling lists for parent-child hierarchies
- Still no `used[]` flags or free list (coming in lessons 07-08)

## Key patterns established

- Intrusive data structures: linkage lives INSIDE the data, not in external containers
- Circular lists eliminate boundary edge cases (no special first/last handling)
- Nil replaces NULL: safe to dereference, never crashes
- memset is the universal reset

## Open questions

- How to properly manage add/remove lifecycle? → Lesson 07
- How to reuse removed slots? → Lesson 08
- How to detect stale references? → Lesson 09
