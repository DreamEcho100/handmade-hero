# Lesson 13: Architecture Review and What's Next

> **No new code.** This lesson is a full-system review, a map to other entity systems, and a guide for where to take this architecture next. Think of it as the whiteboard session after a long sprint --- stepping back from the code to see the whole building.

---

## What you'll learn

- The complete system diagram: every piece in one picture
- What we gained: crash safety, zero allocation, trivial serialization, cache friendliness
- What we traded away: no random child access, wasted bytes in the fat struct, iterator overhead
- Four advanced variants for when the fat struct gets too big: union, char buffer, separate pools, SoA
- When NOT to use this approach: the "valley of sadness"
- The STL composition trap and why integrated systems win
- How to introduce this approach on a team
- The five laboratories and what each one teaches
- Connection to Game Programming Patterns, Game Engine Architecture, Handmade Hero, and ECS

---

## Step 1 --- The complete architecture

Here is every piece of the things pool in one picture. If you can read this diagram and explain each labeled part, you understand the entire system.

```
                           thing_pool
 ┌───────────────────────────────────────────────────────────────────────┐
 │                                                                       │
 │  things[] (MAX_THINGS = 1024)                                         │
 │  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐   │
 │  │ [0]      │ [1]      │ [2]      │ [3]      │ [4]      │ ...    │   │
 │  │ NIL      │ PLAYER   │ (dead)   │ TROLL    │ TROLL    │        │   │
 │  │ sentinel │ x=400    │ zeroed   │ x=234    │ x=567    │        │   │
 │  │ kind=0   │ y=300    │ kind=0   │ y=456    │ y=123    │        │   │
 │  │ ALWAYS   │ size=20  │          │ size=15  │ size=15  │        │   │
 │  │ ZERO     │ orange   │          │ blue     │ blue     │        │   │
 │  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘   │
 │       ▲                     │                                         │
 │       │ never used          │ next_sibling stores free list link      │
 │       │ never iterated      ▼                                         │
 │       │               free list: 2 → 0 (end)                         │
 │                                                                       │
 │  used[]                                                               │
 │  ┌──────┬──────┬──────┬──────┬──────┬────────┐                       │
 │  │  F   │  T   │  F   │  T   │  T   │ ...    │                       │
 │  └──────┴──────┴──────┴──────┴──────┴────────┘                       │
 │                                                                       │
 │  generations[]                                                        │
 │  ┌──────┬──────┬──────┬──────┬──────┬────────┐                       │
 │  │  0   │  0   │  3   │  0   │  0   │ ...    │  slot 2 recycled      │
 │  └──────┴──────┴──────┴──────┴──────┴────────┘  3 times              │
 │                                                                       │
 │  next_empty = 5  (next never-used slot)                               │
 │  first_free = 2  (head of intrusive free list)                        │
 │                                                                       │
 └───────────────────────────────────────────────────────────────────────┘

 thing_ref = { index, generation }
 ┌─────────────────┐
 │ index = 4       │───────► things[4]  (generations[4] == 0? YES → valid)
 │ generation = 0  │
 └─────────────────┘

 thing_ref = { index=2, generation=1 }
 ┌─────────────────┐
 │ index = 2       │───────► generations[2] is 3, not 1 → STALE → nil
 │ generation = 1  │
 └─────────────────┘

 thing_iter
 ┌────────────────────────────────────────────────────────────────────┐
 │ Walks index 1..next_empty, checking used[i] at each step          │
 │                                                                    │
 │ [0] skip (nil)  [1] YIELD  [2] skip (dead)  [3] YIELD  [4] ...   │
 └────────────────────────────────────────────────────────────────────┘
```

Nine concepts, one data structure. Here they are, numbered:

| # | Concept | Lesson | Where it lives in the pool |
|---|---------|--------|---------------------------|
| 1 | Fat struct | 02 | `thing` --- one struct for all entity types |
| 2 | Kind enum | 02 | `thing.kind` --- discriminates entity type |
| 3 | Integer indices | 03 | `thing_idx` --- all references are array offsets |
| 4 | Intrusive linked lists | 04--05 | `parent`, `first_child`, `next_sibling`, `prev_sibling` |
| 5 | Nil sentinel | 06 | `things[0]` --- always zero, always safe to read |
| 6 | Zero-init architecture | 06 | `memset(0)` resets everything to a valid state |
| 7 | Add / Remove / Get | 07 | Pool API with nil-safe returns |
| 8 | Intrusive free list | 08 | Dead things chain via `next_sibling`; `first_free` is the head |
| 9 | Generational IDs | 09 | `thing_ref` = index + generation; `generations[]` bumped on recycle |
| 10 | Iterator | 10 | `thing_iter` scans `used[]`, skips dead slots |

Plus the game integration from Lessons 11--12: `game_init` creates entities via the pool, `game_update` moves/spawns/removes them, `game_render` iterates the pool to draw pixels.

Every piece depends on the pieces below it in the table. The iterator depends on the used flags. The used flags depend on add/remove. Remove depends on the free list and generational IDs. The free list reuses the intrusive linked list fields. Generational IDs depend on integer indices. And at the very bottom, the fat struct and kind enum give us the foundation.

### The dependency chain

```
 Iterator (Lesson 10)
    │ needs
    ▼
 used[] flags (Lesson 07)
    │ set by
    ▼
 Add / Remove / Get (Lesson 07)
    │ uses
    ├──────────────────────┐
    ▼                      ▼
 Free list (Lesson 08)  Generational IDs (Lesson 09)
    │ reuses               │ indexed by
    ▼                      ▼
 Intrusive links        Integer indices (Lesson 03)
 (Lessons 04-05)           │
    │                      │
    ▼                      ▼
 Fat struct + Kind enum (Lesson 02)
    │
    ▼
 Nil sentinel + Zero-init (Lesson 06)
```

If you understand this dependency chain, you understand the entire system. Every concept builds on the one below it. Remove any layer and the layers above it break.

---

## Step 2 --- What we gained

### No crashes from stale references

In a naive system, removing an entity while someone holds a pointer to it creates a dangling pointer. Accessing it is undefined behavior --- usually a crash, sometimes silent memory corruption.

With the things pool:

```
1. Remove zeros the slot and bumps the generation.
2. Anyone holding a thing_ref calls thing_pool_get_ref.
3. Generation mismatch? Returns nil sentinel.
   Not NULL. Not garbage. A safe, readable struct with kind=0.
4. Code checks thing_is_valid(result) and moves on.
```

No crash. No corruption. No special error handling code. The nil sentinel absorbs every mistake.

> **Why?** In JavaScript, accessing a deleted object gives you `undefined`, and accessing `undefined.x` throws a `TypeError`. You handle this with `if (obj?.x)` or try/catch. In our C pool, the nil sentinel is like an object that exists but has all properties set to zero. You can read it, you can check it, you just do not modify it. No exceptions, no special paths.

### No allocation in the hot loop

Every entity lives in a pre-allocated flat array. Adding costs O(1) --- pop from the free list or advance `next_empty`. Removing costs O(1) --- mark unused, push to free list. No `malloc`, no `free`, no garbage collector pause, no lock contention.

The pool's memory footprint is fixed at startup:

```
things[1024]:     1024 * 48 bytes = 49,152 bytes
used[1024]:       1024 * 1 byte   =  1,024 bytes
generations[1024]: 1024 * 4 bytes  =  4,096 bytes
next_empty + first_free:              8 bytes
                                   ─────────
Total:                             ~53 KB

That is less than a single 256x256 texture.
```

### Trivial serialization

The entire pool can be saved to disk with one `fwrite`:

```c
fwrite(&pool, sizeof(thing_pool), 1, file);
```

And loaded back with one `fread`:

```c
fread(&pool, sizeof(thing_pool), 1, file);
```

This works because:

- No pointers inside the struct (only integer indices)
- No heap allocations to reconstruct
- No vtables, no function pointers, no hidden state
- The struct is POD (Plain Old Data) --- byte-for-byte copyable

> **Why?** In JavaScript, serializing your game state means `JSON.stringify` and hoping none of your objects have circular references or class instances. In C with this architecture, it is a `memcpy`. One operation. Done.

### Stable indices

A `thing_idx` does not change when other entities are added or removed. Slot 5 is always slot 5. This is unlike the Asteroids `compact_pool` approach, where indices shift on every compaction.

Stable indices mean:

- Parent/child links never go stale from compaction
- External systems can cache a `thing_ref` and use it next frame
- Network code can send "entity at index 5" and the receiver knows what you mean
- Save/load works without remapping

### Cache-friendly iteration

The iterator walks a flat, contiguous array. On modern CPUs, this is the best possible access pattern. The hardware prefetcher loads the next cache line while you are processing the current one.

```
Flat array iteration:
  things[0] things[1] things[2] things[3] things[4] ...
  ════════ ════════ ════════ ════════ ════════
  │  Cache line 0  │  Cache line 1  │  ...
  CPU reads: sequential. Prefetcher: happy. Cache misses: near zero.

Linked list iteration (the naive OOP approach):
  Entity* → [heap addr 0x7f3a...]
             Entity* → [heap addr 0x7f51...]  (random location!)
                        Entity* → [heap addr 0x7f22...]  (random location!)
  CPU reads: random. Prefetcher: confused. Cache misses: constant.
```

For 1024 entities at 48 bytes each, the flat array fits in ~50 KB --- well within L1/L2 cache on any modern CPU. A linked list of heap-allocated objects scatters that same data across megabytes of virtual memory.

Let us put numbers on this. A modern CPU has roughly:

```
L1 cache:  ~32 KB   (access: ~1 nanosecond)
L2 cache:  ~256 KB  (access: ~4 nanoseconds)
L3 cache:  ~8 MB    (access: ~12 nanoseconds)
Main RAM:  8+ GB    (access: ~100 nanoseconds)

Our pool: 1024 * 48 = ~50 KB → fits in L2 cache.
  → Iteration: ~4 nanoseconds per entity.

Heap-allocated linked list: 1024 objects scattered across MB.
  → Each pointer chase: ~100 nanoseconds (main RAM).
  → That is 25x slower per entity.
  → For 1024 entities: 4 microseconds (flat) vs 100 microseconds (linked).
```

This is not theoretical. Press L in the demo to switch to the Data Layout Lab. Toggle between AoS and SoA with Tab and watch the timing numbers change.

> **Why?** This is the core argument of data-oriented design. Jonathan Blow, Mike Acton, and Casey Muratori all make the same point: the performance of your program is determined by how you access memory, not by how clever your algorithms are. A flat array with a dumb linear scan beats a linked list with a smart algorithm, because the array stays in cache and the linked list does not.

### Instant reset

Calling `game_init` resets the entire game with `memset(state, 0, sizeof(*state))`. One function call zeros every field, every pool slot, every counter. Then `thing_pool_init` sets `next_empty = 1` and you are ready to go.

In a framework-based approach, resetting means: iterate all entities, call destructors, free allocations, clear containers, reinitialize managers, reset singletons. In our approach: `memset`. Done.

This is not just convenient --- it is a correctness guarantee. After `memset(0)`, there are no stale pointers, no half-initialized objects, no forgotten state. Everything is at its zero-value default.

---

## Step 3 --- What we traded away

Every architecture has trade-offs. Let us be honest about what the things pool does NOT give you.

### No random child access

With intrusive linked lists, you cannot say "give me the 3rd child of this entity." You have to walk the sibling list from `first_child`: first, second, third. That is O(n) where n is the number of children.

```
thing *child = &pool.things[parent.first_child];
// This is the 1st child.

child = &pool.things[child->next_sibling];
// This is the 2nd child.

child = &pool.things[child->next_sibling];
// This is the 3rd child.
// O(n) — must walk from the beginning every time.
```

For most game use cases (inventories, attached effects, turret mounts), you rarely need random child access. You iterate all children, or you access the first child, or you hold a direct `thing_ref` to the child you want. The linked list is the right structure for these patterns.

If you genuinely need random child access, you would store child indices in a fixed-size array inside the thing: `thing_idx children[8]`. But that wastes space in entities that do not have 8 children.

### Wasted space in the fat struct

Every entity type carries every field. A troll has `parent`, `first_child`, `prev_sibling`, `next_sibling` --- even if it never has children. A player has `dx`, `dy` --- even if it only uses `x`, `y` for position. Unused fields sit at zero, taking up bytes.

Let us do the napkin math:

```
thing struct: 48 bytes
Useful fields for a troll: kind(4) + x(4) + y(4) + size(4) + color(4) = 20 bytes
Wasted per troll: 48 - 20 = 28 bytes

At 1024 entities, maximum waste: 28 * 1024 = 28,672 bytes = ~28 KB

28 KB on a machine with 8+ GB of RAM = 0.0003% of memory.
```

It is not a problem. You would need tens of thousands of entities with dozens of unused fields before the waste matters. And by then, you would refactor to one of the variants in Step 4.

### Iterator overhead for sparse pools

If 100 entities are alive in a 1024-slot pool, the iterator scans 924 `used[i] == false` checks. Each check is a single byte comparison --- fast --- but it is still work proportional to pool capacity, not to living entity count.

```
Dense pool (900 of 1024 slots alive):
  Iterator checks 1024 used[] flags, yields 900 things.
  Overhead: 124 wasted checks = ~12%  → negligible

Sparse pool (10 of 1024 slots alive):
  Iterator checks 1024 used[] flags, yields 10 things.
  Overhead: 1014 wasted checks = ~99%  → still fast (1024 byte comparisons
  take ~1 microsecond) but feels wrong

Very sparse pool (10 of 65536 slots alive):
  Now it matters. 65526 wasted checks per frame.
  Fix: maintain a packed iteration list alongside the pool.
  But that reintroduces the compaction complexity we avoided.
```

For our pool (1024 slots, typically 10--50 alive), the overhead is unmeasurable. The entire scan takes under a microsecond.

### Slightly complex insert (circular list surgery)

Adding a child to a parent requires updating four pointers (new child's prev, new child's next, old first's prev, old last's next). This is more complex than `array.push(child)` in JavaScript. But it is O(1) --- constant time regardless of how many children the parent has. And it requires zero allocation.

The complexity is in the code, not in the runtime cost. Once you have written `thing_add_child` (Lesson 05), you call it and it works. The four-pointer update happens in a few nanoseconds.

### No built-in type safety for kind access

When you access `troll->health`, the compiler does not check that the troll actually HAS health (all things have all fields). If you access `troll->data.player.mana` in the union variant, the compiler does not check that the thing is actually a player. You must check `kind` yourself.

In TypeScript:

```ts
if (thing.kind === 'player') {
    thing.mana -= 10; // TypeScript knows `thing` is PlayerThing here
}
```

In C:

```c
if (t->kind == THING_KIND_PLAYER) {
    t->data.player.mana -= 10.0f; // YOU know it's a player. Compiler doesn't check.
}
```

This is a real trade-off. The lack of compiler-enforced type narrowing means you can make mistakes that TypeScript would catch. The mitigation: always `switch` on `kind`, and in debug builds, `assert(t->kind == THING_KIND_PLAYER)` before accessing type-specific data.

---

## Step 4 --- When the fat struct gets too big

As your game grows, the thing struct accumulates fields:

```c
typedef struct thing {
    thing_kind kind;
    thing_idx parent, first_child, next_sibling, prev_sibling;
    float x, y, dx, dy;
    float health;
    float size;
    uint32_t color;
    /* ... and then the game grows ... */
    float armor, magic, stamina;
    float anim_timer, anim_frame;
    int inventory[8];
    char name[32];
    /* ... */
} thing;  /* now 256 bytes */
```

At 256 bytes per thing, your iteration is slower because each cache line (64 bytes on x86) holds less than one entity. If your movement loop only reads `x`, `y`, `dx`, `dy` --- that is 16 bytes of useful data buried in 256 bytes of irrelevant fields.

There are four approaches to fix this. Each trades simplicity for efficiency in a different way.

### Variant A: SoA (Structure of Arrays)

Split the fat struct into per-property arrays, all indexed by the same `thing_idx`:

```c
/* Current AoS (Array of Structs): */
thing things[MAX_THINGS];  /* one array of fat structs */

/* Refactored SoA (Structure of Arrays): */
thing_kind kinds[MAX_THINGS];
float      xs[MAX_THINGS];
float      ys[MAX_THINGS];
float      dxs[MAX_THINGS];
float      dys[MAX_THINGS];
float      healths[MAX_THINGS];
float      sizes[MAX_THINGS];
uint32_t   colors[MAX_THINGS];
/* ... one array per property */
```

Now the movement loop reads only `xs`, `ys`, `dxs`, `dys` --- four tightly packed float arrays. The CPU prefetcher loads consecutive positions into the cache with zero waste.

```
AoS iteration (current):
  for each entity: read 256 bytes, use 16 bytes.
  Cache utilization: 16/256 = 6.25%

SoA iteration:
  for each entity: read 4 floats from 4 contiguous arrays.
  Cache utilization: 16/16 = 100%
```

The key insight: **`thing_idx` still works.** Whether you have `things[5].x` (AoS) or `xs[5]` (SoA), the index is the same. All the pool machinery --- `used[]`, `generations[]`, free list, nil sentinel --- still works unchanged. You are just reorganizing storage, not changing the addressing scheme.

### Variant B: Union for type-specific data

A C `union` overlays multiple struct layouts in the same memory. You pay the size of the LARGEST member, not the sum:

```c
typedef struct thing {
    thing_kind kind;
    thing_idx parent, first_child, next_sibling, prev_sibling;
    float x, y;
    /* Common fields above. Type-specific fields below: */
    union {
        struct { float health; float mana; int level; } player;
        struct { float aggro_range; float patrol_x; }   troll;
        struct { int stack_count; int item_id; }         item;
    } data;
} thing;
```

If `player` is 12 bytes, `troll` is 8 bytes, and `item` is 8 bytes, the union is 12 bytes total --- not 28. Switch on `kind` to know which member is valid:

```c
switch (t->kind) {
    case THING_KIND_PLAYER:
        t->data.player.mana -= 10.0f;
        break;
    case THING_KIND_TROLL:
        if (distance < t->data.troll.aggro_range) { /* chase */ }
        break;
}
```

> **Why?** This is the C equivalent of a TypeScript discriminated union: `type Thing = { kind: 'player'; mana: number } | { kind: 'troll'; aggroRange: number }`. In TypeScript, the compiler enforces which branch you are in. In C, you check `kind` yourself. Accessing the wrong member reads garbage (the bytes are shared). It will not crash, but the value is nonsense.

### Variant C: Raw char buffer

The most flexible approach. Store a fixed-size raw byte buffer and cast to whatever you need:

```c
typedef struct thing {
    thing_kind kind;
    thing_idx parent, first_child, next_sibling, prev_sibling;
    float x, y;
    char custom_data[104]; /* raw bytes — cast per type */
} thing;
```

Define per-type structs separately and cast:

```c
typedef struct player_data {
    float health, mana;
    int level;
    char name[32];
} player_data;

/* Writing: */
player_data *pd = (player_data *)thing->custom_data;
pd->health = 100.0f;
pd->mana = 50.0f;

/* Reading: */
player_data *pd = (player_data *)thing->custom_data;
float h = pd->health;
```

Maximum flexibility. You can store ANYTHING in those bytes. Zero-init still works (`memset` zeros the buffer). But you MUST check `kind` before casting, and you MUST ensure your per-type struct fits in the buffer.

> **Why?** This is what Dreams (PS4) used for truly dynamic entity properties. The thing struct has a fixed layout that the pool can manage (zero-init, memset, iterate), but the custom data region is completely type-specific. Anton: "You could just do a char buffer, 104 bytes, custom data. That's the end goal if you really want to get super spicy."

### Variant D: Separate pools per entity type

Instead of one big pool for all entity types, use per-type pools:

```c
thing_pool players;    /* only player things */
thing_pool monsters;   /* only monster things */
thing_pool items;      /* only item things */
```

Each pool's thing struct can be specialized. The player pool stores player-specific fields. The monster pool stores monster-specific fields. No wasted space, no unions.

The trade-off: cross-pool references get complicated. If a monster wants to reference a player, it needs both a pool ID AND an index. Parent-child relationships that span types (a player holding an item) need inter-pool links.

```
Single pool:
  thing_idx child = player->first_child;
  thing *item = pool_get(&pool, child);
  // Works: both are in the same pool.

Separate pools:
  // Player is in players pool, item is in items pool.
  // player->first_child is an index into... which pool?
  // Need: pool_id + index, or separate link fields per pool.
```

Press 9 in the demo to see Scene 9: Separate Pools in action. It runs a player pool and a troll pool side by side with cross-pool collision detection.

### Summary table

| Approach | When to use | Struct waste | Complexity |
|---|---|---|---|
| Fat struct (current) | < 20 fields, few entity types | All fields * all entities | Simplest |
| SoA | 1000s of entities, hot-loop optimization | Zero (iterate only what you need) | Moderate |
| Union | 3--10 types, large type-specific data | `max(type data)` per entity | Moderate |
| Char buffer | Many types, very different data | Fixed buffer per entity | High |
| Separate pools | Very different sizes per type | Zero per type, cross-ref complexity | High |

Start with the fat struct. It works. Refactor when profiling shows you need it --- not before.

---

## Step 5 --- When NOT to use this approach

We have spent 13 lessons building this architecture and showing why it works. It is honest to also talk about where it does NOT work --- not because of technical limitations, but because of human and organizational ones.

### Enterprise teams demand frameworks

If you walk into a studio that uses Unreal or Unity and say "we are going to manage entities with flat arrays, intrusive lists, and memset instead of the engine's component system," you will be met with resistance. Not because your approach is wrong, but because teams standardize on tools. The ECS framework has documentation, training materials, and a decade of institutional knowledge. Your 300-line pool does not.

### Legacy codebases cannot be retrofitted

You cannot incrementally convert a C++ codebase that uses `std::vector<std::unique_ptr<Entity>>` to flat-array pools. The pointer-based architecture is woven through every system: rendering holds `Entity*`, physics holds `Entity*`, scripting holds `Entity*`. Switching to integer indices means changing every reference in the entire codebase. It is all or nothing for the data layer.

### The "valley of sadness"

Transitioning FROM modern C++ to this approach means going through a phase where your code looks like "bad C" before it looks like "clean LOATs." You have removed the smart pointers and RAII but have not yet built the pool and nil sentinel. During that transition, your code is genuinely more crash-prone than what you started with.

```
                Modern C++         LOATs
Safety:         ████████░░         ████████░░

                   ↓ remove RAII, smart pointers
                   ↓ but haven't built pool yet
                   ▼
                "Valley of Bad C"
Safety:         ███░░░░░░░     ← crashes, dangling pointers,
                                  manual memory mismanagement

                   ↓ build pool, nil sentinel, gen IDs
                   ▼
                LOATs Complete
Safety:         ████████░░     ← back to safe, but now fast
```

You have to push through that valley to reach the other side. If you stop halfway, you have the worst of both worlds.

> **Why?** Anton: "Even if I imagine that modern C++ is a sort of hill where memory safety is a thing... to get from there to here you go through this valley of bad C where it's like crashes exist and memory is unsafe... if you keep going down that hill you can get back up to a similarly high quality hill where you don't crash and it's fast."

### Team buy-in required

Everyone on the team needs to understand zero-init, nil sentinels, intrusive lists, and the "no constructors" rule. One person adding `std::vector` to the thing struct breaks the memset contract. One person returning `NULL` from a pool function breaks the nil sentinel guarantee. The architecture is simple, but it requires shared understanding.

On a team of 2--3 people who have all read this course, that is easy. On a team of 20 where half joined last month, it is harder.

None of this means the architecture is wrong. It means you need to pick your battles. For a personal project, a small team, a game jam, a learning exercise, or a performance-critical system where you control the codebase --- this is the right approach.

---

## Step 6 --- The STL composition trap

The deepest philosophical split in C++ game programming is not "OOP vs data-oriented." It is "compose from generic building blocks" vs "build integrated systems."

**The STL philosophy says:** Here are 30 building blocks --- `vector`, `map`, `unordered_map`, `optional`, `variant`. Compose your data structures from them. Need an entity system? `std::unordered_map<EntityID, std::unique_ptr<Entity>>`. Need a free list? `std::stack<int>`. Every piece is generic and individually well-tested.

**The LOATs philosophy says:** Build the few systems you need as integrated units. Need an entity system? Build `thing_pool` --- one struct that contains the array, the used flags, the generations, the free list, and the iteration logic. Everything co-designed to work together.

Why the integrated approach wins for game engines:

**1. Composing from generic containers fragments memory.** `std::unordered_map<int, Entity>` allocates bucket arrays, linked list nodes, and Entity objects separately. Three allocations per entity, scattered across the heap. Our pool: one contiguous allocation. Period.

**2. Generic containers add abstraction overhead.** `std::vector::push_back` checks capacity, maybe reallocates, maybe moves elements. Our `pool_add`: pop from free list, write to a known slot. The generic version handles cases we will never hit (reallocation) and pays for flexibility we do not need.

**3. Business logic gets buried in container gymnastics.** Spawning an entity with the STL approach:

```cpp
entities.emplace(next_id++, std::make_unique<Entity>(kind));
```

Spawning with our pool:

```c
thing_pool_add(&pool, kind);
```

The actual LOGIC is the same: "create an entity of this kind." But the STL version wraps it in template instantiation, smart pointer management, and iterator invalidation rules.

The things pool is 300 lines of code. It does exactly what we need and nothing more. You can read every line. You can debug it with `printf`. You can explain it to a new team member in 30 minutes.

Let us see the same operation in both styles to make the difference concrete:

```cpp
/* STL-composed entity removal: */
auto it = entities.find(entityId);       // hash lookup
if (it != entities.end()) {
    auto& entity = it->second;           // dereference unique_ptr
    // Notify observers
    for (auto& observer : observers) {
        observer->onEntityRemoved(entity.get());
    }
    freeList.push(entityId);             // push to separate free list
    generations[entityId]++;             // update separate gen array
    entities.erase(it);                  // erase from map
    // unique_ptr destructor runs, calls delete
    // memory returned to heap allocator
}
```

```c
/* LOATs entity removal: */
thing_pool_remove(&pool, idx);
// Inside that one call:
//   1. pool->used[idx] = false
//   2. memset(&pool->things[idx], 0, sizeof(thing))
//   3. pool->generations[idx]++
//   4. pool->things[idx].next_sibling = pool->first_free
//   5. pool->first_free = idx
// Done. No allocation. No deallocation. No observers.
```

The LOATs version is 5 lines of implementation. The STL version involves a hash lookup, a smart pointer deallocation, an observer notification loop, and three different data structure operations. Both achieve the same result: the entity is gone, the slot is recyclable, and stale references are detectable.

> **Why?** This is not about STL being "bad." `std::vector` is a fine container. The argument is that when you COMPOSE generic containers into a system (entity pool = vector + map + stack + optional), the resulting system is harder to understand, harder to optimize, and harder to debug than a purpose-built alternative. The integrated system has fewer moving parts, fewer allocation points, and fewer places where things can go wrong.

---

## Step 7 --- Performance as the adoption inroad

If you want to introduce these ideas in an existing team or organization, the most effective entry point is performance. Not philosophy, not "code purity," not "this is how Casey does it." Performance.

When a competing tool is 10x faster, people pay attention. When the profiler shows 40% of frame time spent in cache misses from pointer-chasing entity hierarchies, people are ready for alternatives.

The conversation usually goes:

1. **Profile first.** Show that the current entity system is a bottleneck. Not "I think it's slow" --- actual profiler data. Cache miss rates, frame time breakdowns, allocation counts per frame.

2. **Prototype the alternative.** Build a small flat-array pool alongside the existing system. Port ONE hot path (e.g., collision detection) to use it. Measure the improvement.

3. **Show the numbers.** "Collision went from 4ms to 0.3ms per frame by switching from hash map lookups to flat array iteration." That is a 13x improvement. That gets attention.

4. **Expand gradually.** Once one system proves the approach, migrate others. Physics, rendering, AI --- each one becomes a case study.

You do not need to convert the entire codebase. Even one subsystem using flat arrays can demonstrate the principle. And once people see the performance difference, the philosophical arguments suddenly seem more reasonable.

> **Why?** "Flat arrays are faster than scattered heap objects" is a measurable, provable claim. You do not need anyone to take it on faith. Just show the profiler output.

---

## Step 8 --- The laboratories

The five laboratories in this course take the pool system you built and push it to scale. Each one demonstrates a specific aspect of the architecture under pressure.

### Lesson 14 --- Particle Laboratory (press P)

1000 particles with 6 movement archetypes (bounce, wander, seek player, predictive seek, orbit, drift noise). Demonstrates:

- **Parallel arrays** extending pool entities with per-particle data (archetype, aggression, phase)
- **Behavior dispatch via data**: `switch (archetype_id)` instead of virtual methods
- **Mutation on collision**: data rewrite (change archetype, color, aggression) instead of object creation
- **Pool under 97.8% sustained load**: 1000 of 1024 slots occupied continuously

This is the fat struct and iterator under real load. The update loop processes 1000 entities with 6 different behaviors, all dispatched through a kind check, all at 60 FPS.

### Lesson 15 --- Data Layout Toggle Lab (press L, Tab to toggle)

2000 entities with identical logic running on three different memory layouts:

- **AoS** (Array of Structs) --- the fat struct approach from this course
- **SoA** (Structure of Arrays) --- per-property arrays
- **Hybrid** --- hot data (position, velocity) in one struct, cold data (color, archetype) in another

You can toggle between layouts with Tab and see the performance difference on the HUD. This is the cache locality argument from Step 2 made measurable.

### Lesson 16 --- Spatial Partition Lab (press G, Tab to cycle)

2000 entities with four spatial query strategies:

- **Naive O(N^2)** --- check every pair (intentionally slow baseline)
- **Uniform Grid** --- divide screen into cells, only check within-cell pairs
- **Spatial Hash** --- hash position to bucket, check within-bucket pairs
- **Quadtree** --- adaptive subdivision, check within-node pairs

The naive mode performs ~4,000,000 pair checks per frame and stutters visibly. Grid partitioning drops to ~10K--50K checks. A debug overlay draws grid lines, hash bucket highlights, and quadtree subdivision boxes.

This demonstrates that the pool's flat-array storage works perfectly with spatial acceleration structures. The entities live in the pool; the spatial structure just provides fast lookup by position.

### Lesson 17 --- Memory Arena Lab (press M, Tab to cycle)

Entity churn workload (hundreds of spawns/destroys per second) run through four allocation strategies:

- **malloc/free** --- standard allocator (intentionally spiky baseline)
- **Linear arena** --- bump pointer, bulk reset
- **Pool allocator** --- free list, zero alloc in game loop
- **Scratch arena** --- reset every frame

The malloc baseline shows frame-time spikes from lock contention and fragmentation. The arena modes show rock-solid frame times. A memory visualization overlay renders arena fill level and pool slot occupancy.

This is the memory lifetime argument made measurable --- the same permanent + transient arena model Casey uses in Handmade Hero.

### Lesson 18 --- Infinite Growth Lab (press I, Tab to cycle)

The capstone. Entities spawn forever at ~50/sec into the real `thing_pool`. The unbounded baseline fills the pool in ~20 seconds and silently stops spawning. Then toggle through stabilization strategies:

- **Lifetime cleanup** --- entities expire, slots return to pool
- **Budget caps** --- hard max active count, spawner adapts
- **Spatial streaming** --- only simulate within a radius
- **Simulation LOD** --- near: full update, far: every 4th frame
- **Pool recycling** --- oldest entities evicted for new ones
- **Amortized work** --- stagger 1/4 of entities per frame
- **Production mode** --- all strategies combined

Each mode is a real engine technique for keeping a long-running session stable under infinite pressure. This lab integrates every concept from lessons 02--17.

### How the labs connect

The labs form a progression:

```
Particle Lab (L14)     → Can the pool handle 1000 entities?
                          YES. Fat struct + iterator work at scale.
                            │
                            ▼
Data Layout Lab (L15)  → What if we need 2000+ entities and cache matters?
                          SoA is faster. Know when to refactor.
                            │
                            ▼
Spatial Lab (L16)      → What if we need collision between 2000 entities?
                          Grid/hash/quadtree reduce N² to ~N.
                            │
                            ▼
Memory Arena Lab (L17) → What if entities churn rapidly (spawn/die 100/sec)?
                          Arenas eliminate allocation jitter.
                            │
                            ▼
Growth Lab (L18)       → What if entities grow forever?
                          Lifetime, budget, streaming, LOD, amortization.
                          All techniques combined.
```

Each lab addresses the next bottleneck you would hit as your game grows. The pool itself does not change --- the labs add techniques that work alongside it.

---

## Step 9 --- Map to Game Programming Patterns, GEA, and Handmade Hero

The things pool is not a toy. These exact techniques appear in professional game development literature and practice. Here is how they map.

### Game Programming Patterns (Robert Nystrom)

Three chapters directly relate:

- **Chapter 19: Object Pool** --- the things pool IS an object pool. Pre-allocate a fixed array, use a free list for reuse, avoid runtime allocation. Our pool adds generational IDs and nil sentinel on top of the basic pattern.

- **Chapter 17: Data Locality** --- the flat array is the key to data locality. The book explains why iterating a linked list of heap-allocated objects thrashes the cache, while iterating a flat array is cache-friendly. Our pool iterator (Lesson 10) is the practical implementation.

- **Chapter 14: Component** --- the fat struct is a single-component approach. The Component pattern says: compose entities from small pieces instead of using a big class hierarchy. The fat struct achieves this at a coarser granularity --- one struct holds all pieces, type-tagged by `kind`. ECS takes the Component pattern further.

### Game Engine Architecture (Jason Gregory, 3rd edition)

Two chapters cover our territory:

- **Chapter 15: Runtime Object Model** --- entity handles (our `thing_ref`), ID recycling (our free list + generation), object-centric vs property-centric models (our AoS fat struct vs SoA refactoring).

- **Chapter 16: Runtime Gameplay Foundation** --- scene graphs and entity hierarchies (our intrusive parent/child lists), entity lifecycle (our add/remove/init), and the question of "what is an entity?" (our answer: a slot in a flat array, identified by an index).

### Handmade Hero (Casey Muratori)

Casey builds an entity system starting around day 33. The parallels:

| Handmade Hero concept | LOATs equivalent |
|---|---|
| Entity struct with type field | `thing` with `thing_kind` |
| Entity storage in a flat array | `thing things[MAX_THINGS]` |
| Entity ID (index-based handle) | `thing_idx` / `thing_ref` |
| Null entity at index 0 | Nil sentinel at `things[0]` |
| Entity removal by marking inactive | `thing_pool_remove` + `used[i] = false` |
| Entity iteration skipping inactive | `thing_iter` scanning `used[]` |

Casey does not use a free list (he uses a simpler linear scan for empty slots) or generational IDs (he uses his own handle scheme). But the core architecture --- flat array, integer indices, type tag, nil at zero --- is the same approach.

If you continue watching Handmade Hero past day 33, everything will feel familiar. Casey builds toward the exact system you already built. His entity struct accumulates fields over episodes --- the fat struct growing organically as the game needs it. His null entity at index 0 catches stale references. His iteration loop skips inactive slots. You have already internalized the pattern.

The biggest difference: Casey builds his entity system over many episodes, discovering each piece as the game needs it. You built the entire system upfront in 13 lessons, with the benefit of hindsight. Both approaches are valid --- Casey's is more exploratory, ours is more structured. But the destination is the same.

### Entity Component Systems (ECS)

ECS is the formal, industrialized version of the ideas in this course:

| LOATs concept | ECS equivalent |
|---|---|
| `thing` (fat struct) | Entity (just an ID) + Components (separate data per aspect) |
| `thing_kind` enum | Entity has no "type" --- its component set defines behavior |
| `thing_pool` | World / Registry |
| `thing_idx` / `thing_ref` | Entity handle with generation |
| `used[]` array | Archetype alive set / bitset |
| Free list | Entity ID recycling pool |
| Pool iterator | System query ("all entities with Position and Velocity") |
| `game_update` with kind check | Systems (functions that process matching entities) |

The things pool is simpler than ECS --- one struct for all entities, one array for everything, kind-based `switch` statements. ECS is more powerful --- add behaviors by attaching components, query for specific component combinations, formal parallelism.

Here is the same "move all entities with velocity" operation in both styles:

```c
/* LOATs approach: */
for (thing_iter it = thing_iter_begin(&pool);
     thing_iter_valid(&it);
     thing_iter_next(&it))
{
    thing *t = thing_iter_get(&it);
    t->x += t->dx * dt;
    t->y += t->dy * dt;
}

/* ECS approach (pseudocode): */
ecs_query *q = ecs_query_new(world, Position, Velocity);
for (int i = 0; i < q->count; i++) {
    Position *p = ecs_get(q, i, Position);
    Velocity *v = ecs_get(q, i, Velocity);
    p->x += v->dx * dt;
    p->y += v->dy * dt;
}
```

The logic is the same. The ECS version is more explicit about which components it needs (`Position` and `Velocity`), which lets the framework skip entities that have Position but not Velocity. Our version iterates all entities and relies on the fact that all things have `x`, `y`, `dx`, `dy`.

For 50 entities, the difference in code complexity is not worth the ECS overhead. For 50,000 entities where only 10,000 have Velocity, the ECS query avoids iterating 40,000 irrelevant entities.

You do not need ECS for small-to-medium games. The things pool scales to thousands of entities before you would want the added complexity. If you want to explore ECS, start with the "Entity Component Systems FAQ" by Sander Mertens (creator of Flecs).

---

## Step 10 --- The "log cabin" philosophy

Anton, Casey, Jonathan Blow, and Ryan Fleury share a development philosophy worth stating explicitly:

**Build what you need. Own it. Keep your sanity.**

Do not import a 50,000-line ECS framework to manage 200 entities. Do not write an abstract "component registry" when a flat array with a kind enum does the job. Do not optimize for 100,000 entities when your game has 50.

The things pool is about 300 lines of code. You understand every line. You can debug it by reading the array in a debugger. You can extend it by adding a field to the struct. You can throw it away and rewrite it in an afternoon.

This is not "unsophisticated." This is the same architecture that shipped Dreams on PS4 --- a game with millions of procedural objects. Anton did not use a generic ECS framework. He used large arrays of things with intrusive linked lists and integer indices. The exact same ideas you built in 13 lessons.

> **Why?** Anton: "You don't need the engine. You need the data structure. Once you have the data structure, the 'engine' is just a for loop over it."

The progression from here:

1. **Add more thing kinds.** Goblins, items, projectiles. Same struct, new `kind` values, new behavior in `game_update`.
2. **Add intrusive tree relationships.** Items in player inventory via `thing_add_child`. Turrets mounted on vehicles. Particle effects attached to entities.
3. **Add type-specific data.** Either via the union trick, or by adding fields to the fat struct as needed.
4. **Profile, then optimize.** If iteration gets slow, split hot fields into SoA arrays. If the pool fills up, increase `MAX_THINGS`. If you need per-type queries, add a per-kind linked list.
5. **Move to ECS only if the fat struct genuinely runs out of room.** For most indie games and learning projects, it never will.

### The size of the code

Let us count:

```
things.h:  ~230 lines  (types, API declarations)
things.c:  ~434 lines  (implementation + variants)
game.h:    ~337 lines  (game types, lab types)
game.c:    ~3400 lines (14 scenes + 5 labs + core)

Core pool system alone: ~660 lines (things.h + things.c)
Without variants/labs:  ~300 lines
```

300 lines for a complete entity pool with nil sentinel, free list, generational IDs, intrusive linked lists, and a clean iterator. You can read it in 15 minutes. You can explain it in 30. You can rewrite it from scratch in an afternoon.

Compare that to:
- Unity's ECS framework: tens of thousands of lines across dozens of files
- entt (popular C++ ECS): ~15,000 lines of template metaprogramming
- Flecs: ~60,000 lines

Those frameworks solve harder problems (arbitrary component types, parallel scheduling, live editor integration). But if you do not need those features, you are carrying 50,000 lines of code you did not write, do not understand, and cannot debug. The things pool is 300 lines you wrote yourself, understand completely, and can debug with `printf`.

---

## Connection to your work

If you came from web development, here is the full mapping of everything you built:

| LOATs concept | Web/JS equivalent |
|---|---|
| `thing things[1024]` | `const entities = new Array(1024).fill(null)` |
| `thing_kind` enum | TypeScript discriminated union tag |
| `thing_idx` | Array index |
| `thing_ref {index, gen}` | Database row `{ id, version }` |
| `used[]` flags | `entities[i] !== null` check |
| Free list | Connection pool reuse |
| `memset(0)` reset | `Object.assign(state, initialState)` |
| Nil sentinel | `NullObject` pattern / safe empty object |
| Pool iterator | `entities.filter(e => e !== null).forEach(...)` |
| Intrusive linked list | Parent/child via index references (like `parentId` in a flat Redux store) |
| `game_state` | Redux store / React state |
| `game_update` | Reducer / event handlers |
| `game_render` | React `render()` / `requestAnimationFrame` callback |

The difference is that in JavaScript, the runtime handles memory, GC pauses happen unpredictably, and data is scattered across the heap in individually-allocated objects. In C with this architecture, you control the memory layout, allocation never happens in the game loop, and data is packed into a contiguous array that the CPU can prefetch efficiently.

You did not learn "C programming." You learned a specific architecture for managing runtime data that happens to be expressed in C. The principles --- flat arrays, integer handles, nil sentinels, intrusive structures, zero-init --- apply to any language where you care about performance and control.

### Where these ideas appear in web development

If you think this is "only for games," consider:

- **Database connection pools** --- pre-allocated array of connections, free list for reuse, no allocation per request. Exactly our pool pattern.
- **React's reconciler** --- maintains a flat array of fiber nodes, uses integer indices for parent/child/sibling links. Sound familiar?
- **V8's hidden classes** --- uses integer offsets instead of string-keyed property lookups. The same "integers not pointers" principle.
- **WebAssembly linear memory** --- one flat byte array, integer offsets for all data. No pointers. No GC. This is the thing struct paradigm at the language level.
- **Redis** --- all data in flat arrays and hash tables. No heap-allocated object graphs. This is why it is fast.

The architecture you learned is not "game-specific." It is the universal high-performance data management pattern. Games are just the domain where people talk about it most openly.

---

## The full lesson table

| Lesson | What you built | Problem it solved |
|---|---|---|
| 01 | (conceptual) | OOP inheritance does not scale for game entities |
| 02 | Fat struct + kind enum | One struct for all types, type-tagged |
| 03 | Integer indices | References survive reallocation |
| 04 | Intrusive singly-linked lists | Parent-child without external containers |
| 05 | Circular doubly-linked lists | O(1) unlinking, no boundary edge cases |
| 06 | Nil sentinel + zero init | Chain-dereference without crashes; memset as constructor |
| 07 | Slot map: add, remove, get | Full pool lifecycle with nil-safe access |
| 08 | Intrusive free list | O(1) slot reuse without scanning |
| 09 | Generational IDs | Detect stale references to recycled slots |
| 10 | Pool iterator | Clean iteration over living things, skipping dead slots |
| 11 | Game demo: window + render | Pool integrated into a real game with visible output |
| 12 | Game demo: update + collision | Movement, spawning, removal --- every pool feature live |
| 13 | Architecture review | Full system understanding, trade-offs, and what comes next |

---

## The 13-lesson journey: what changed in your thinking

When you started Lesson 01, you probably thought about game entities like this:

```
class Entity { ... }
class Player extends Entity { ... }
class Troll extends Entity { ... }

std::vector<std::unique_ptr<Entity>> entities;
entities.push_back(std::make_unique<Player>());
```

After 13 lessons, you think about them like this:

```
thing things[1024];  // flat array, zero-init
bool used[1024];     // living or dead?
uint32_t gens[1024]; // reincarnation counter

thing_ref player = thing_pool_add(&pool, THING_KIND_PLAYER);
// done. no allocation. no inheritance. no virtual dispatch.
```

That shift --- from "objects with behavior" to "data in arrays with indices" --- is the core insight of this entire course. Everything else (nil sentinel, free list, generational IDs, iterator) is infrastructure supporting that one shift.

The shift does not mean OOP is "wrong." It means that for managing runtime game entities at scale, flat arrays with integer handles are more efficient, more debuggable, and more predictable than object hierarchies with pointer graphs. Different tools for different problems.

---

## Course complete --- for the core lessons

You built a complete entity pool system from scratch. You understand every line. You saw it run in a real game demo. You know the trade-offs, the variants, and where it connects to professional game development.

The five laboratories (Lessons 14--18) continue the journey. They take the pool you built and push it to 1000 particles, 2000 collision entities, multiple memory allocation strategies, and infinite growth stress tests. Each lab demonstrates a specific aspect of the architecture at scale.

But the core is done. The thing pool --- 300 lines of code that manage a flat array of entities with zero allocation, crash-safe references, and cache-friendly iteration --- is your foundation. Everything else builds on it.

### What to do next

Here is a concrete progression, in order:

1. **Play with the demo.** Press every scene key (1--9, P, L, G, M, I). Watch each concept from the course in action. Kill trolls. Watch the free list recycle slots.

2. **Modify the game demo.** Add a new entity type (e.g., `THING_KIND_GOBLIN`). Give it a different color and size. Make it move in a pattern (e.g., bounce off walls). This exercises: adding to the kind enum, spawning via `pool_add`, iterating with the kind filter.

3. **Add parent-child relationships.** Make the player "pick up" items by calling `thing_add_child`. Display inventory count in the HUD. This exercises: intrusive linked lists from Lessons 04--05.

4. **Do the Particle Lab exercises.** Lesson 14 has specific exercises: add a new archetype, tune the aggression parameter, observe mutation on collision. This pushes the pool to 1000 entities.

5. **Do the Data Layout Lab comparison.** Lesson 15 asks you to observe the performance difference between AoS, SoA, and Hybrid layouts. This teaches you when to refactor and when to leave it alone.

6. **Continue to Game Programming Patterns.** Chapters 14, 17, and 19 will feel like a review --- you have already built the patterns they describe. The remaining chapters (Observer, State, Command) are complementary patterns that work alongside the pool.

7. **Continue to Handmade Hero.** Days 33+ build the entity system. Everything will feel familiar. You built the same foundation.

> **Why?** Anton: "That's it. That's the whole thing. A big array, some indices, a free list, a generation counter. No templates. No smart pointers. No STL. Just data."
