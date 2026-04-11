# Block 03 Summary — Pool System (Lessons 07-09)

## Concepts introduced

11. **Slot map** — thing_pool struct with things[], used[], next_empty
12. **pool_add** — find slot, zero it, set kind, mark used, return index/ref
13. **pool_remove** — bounds check, mark unused, zero the thing
14. **pool_get** — bounds + used check, return thing or nil sentinel
15. **Intrusive free list** — dead things' next_sibling stores next free slot index
16. **LIFO slot reuse** — last removed = first reused (stack order)
17. **Generational IDs** — thing_ref { index, generation } detects stale references
18. **Generation bumping** — pool_remove increments generation; old refs fail the check

## Thing struct at this point

Unchanged from block 02 (32 bytes). Pool management is EXTERNAL to the thing struct.

## Pool state at this point

```c
typedef struct thing_pool {
    thing     things[MAX_THINGS];       /* things[0] = nil sentinel */
    bool      used[MAX_THINGS];         /* liveness flag per slot */
    uint32_t  generations[MAX_THINGS];  /* bumped on each recycle */
    thing_idx next_empty;               /* next never-used slot */
    thing_idx first_free;               /* free list head */
} thing_pool;
```

Full lifecycle: add → use → remove → free list → reuse. Generational IDs prevent stale refs.

## Key patterns established

- Free list is intrusive — reuses dead things' fields, no extra data structure
- Generations only go up — memset zeros generations on init but preserves them on per-slot remove
- pool_get_ref is the "production" accessor — checks generation match
- Three levels of safety: bounds check → used check → generation check → nil sentinel

## Open questions

- How to iterate all living things cleanly? → Lesson 10
- How does this integrate into a real game? → Lessons 11-12
