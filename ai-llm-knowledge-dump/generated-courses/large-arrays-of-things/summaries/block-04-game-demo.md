# Block 04 Summary — Game Demo (Lessons 10-13)

## Concepts introduced

19. **Pool iterator** — thing_iter { pool*, current } skips dead slots
20. **Platform integration** — pool library plugged into dual-backend game demo
21. **Visual proof** — pool operations visible on screen (spawning, removal, reuse)
22. **SoA refactoring** (conceptual) — split fat struct into per-property arrays when it gets too large
23. **Union for type-specific data** (conceptual) — overlay different struct layouts for different kinds
24. **Architecture mapping** — every LOATs concept mapped to GPP, GEA, and Handmade Hero equivalents

## Final system architecture

```
┌────────────────────────────────────────────────────────┐
│                    thing_pool                            │
│                                                          │
│  things[0]  = NIL SENTINEL (all zeros, always safe)      │
│  things[1..N] = living and dead things                   │
│  used[0..N]   = liveness flags                           │
│  generations[] = stale reference detection                │
│  first_free   = intrusive free list head                 │
│  next_empty   = bump allocator for fresh slots           │
│                                                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ thing[1] │──│ thing[2] │──│ thing[3] │  (siblings)    │
│  │ PLAYER   │  │ TROLL    │  │ ITEM     │               │
│  │ parent:0 │  │ parent:0 │  │ parent:1 │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│       │                                                  │
│       └── first_child ──▶ thing[3] (ITEM)                │
│                                                          │
│  Access: pool_get_ref(ref) → bounds + used + gen check   │
│          → returns thing* or &things[0] (nil)            │
│  Never returns NULL. Never crashes.                       │
└────────────────────────────────────────────────────────┘
```

## Course complete

- 13 lessons, ~53,000 words
- Reusable things.h/things.c library
- Dual-backend game demo (X11 + Raylib)
- Test harness with 6 test suites
- Every concept mapped to GPP, GEA, and Handmade Hero

## Where this goes next

- **Handmade Hero days 33+** — entity system with similar patterns
- **GPP Object Pool, Data Locality, Component** — named patterns for what we built
- **GEA Ch 15-16** — entity systems in production engines
- **SoA refactoring** — when the fat struct grows past cache line size
- **ECS** — the formalized version of LOATs (more structure, more complexity)
