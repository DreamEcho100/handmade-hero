# Lesson 09: Generational IDs -- Safe References

## What we're building

A versioned reference system. Instead of referencing a thing by just its array index (`5`), we reference it by index AND a version number: `{ index: 5, generation: 3 }`. Every time a slot is recycled (removed and reused), its version counter bumps up. When you try to look up a thing using an old reference, the system checks if the generation matches -- if it doesn't, the reference is stale, and you get the nil sentinel instead of the wrong entity.

By the end of this lesson, you can hand out references to entities and know FOR CERTAIN whether they're still pointing at the thing you think they're pointing at.

## What you'll learn

- The use-after-free problem with raw indices (how recycled slots trick your old references)
- The `thing_ref` struct: `{ index, generation }`
- The `generations[]` array inside the pool (bumped on each slot recycle)
- `pool_get_ref`: checks generation match, returns nil if mismatch
- Side-by-side comparison: WITH vs WITHOUT generation checking

## Prerequisites

- Lesson 08 (free list). Slots now get recycled, which is exactly what creates the stale reference problem we're solving here.

---

## Step 1 -- The stale reference problem

Let me show you a bug that will ruin your afternoon. It's subtle, it's silent, and it's a direct consequence of the free list we built in lesson 08.

Watch this scenario carefully:

```
Step 1: You add a troll. It lands in slot 5.

  Index:    [0]   [1]      [2]   ...  [5]      [6]  ...
  Things:   NIL   PLAYER   ---        TROLL    ----
  Used:     ---   true     false       true    false

  Your game code saves:  troll_idx = 5
```

So far so good. You have a variable holding the number 5, meaning "my troll is at slot 5."

```
Step 2: The player kills the troll. You call pool_remove(pool, 5).

  Index:    [0]   [1]      [2]   ...  [5]      [6]  ...
  Things:   NIL   PLAYER   ---        (dead)   ----
  Used:     ---   true     false       false   false

  Slot 5 goes onto the free list.
  But your game code STILL holds:  troll_idx = 5
```

The troll is dead. Its slot is on the free list. But your variable `troll_idx` still says 5. Nobody told it the troll died.

```
Step 3: A goblin spawns. The free list hands out slot 5.

  Index:    [0]   [1]      [2]   ...  [5]      [6]  ...
  Things:   NIL   PLAYER   ---        GOBLIN   ----
  Used:     ---   true     false       true    false

  Your game code STILL holds:  troll_idx = 5
```

Now here's the disaster:

```
Step 4: Your code tries to "damage the troll" using the saved index.

  thing *t = pool_get(pool, troll_idx);   // troll_idx = 5
  t->health -= 50;

  What pool_get checks:
    - Index 5 in bounds?   YES
    - used[5]?             YES (the goblin is alive!)
    - Returns:             &things[5]  ... which is the GOBLIN

  You just damaged the GOBLIN, not the troll.
```

That's the bug. The troll is dead. Its slot got recycled. A goblin lives there now. But the old index 5 passes every check -- it's in bounds and the slot is in use. `pool_get` happily returns the goblin, and your game code damages the wrong entity.

This is worse than a crash. A crash tells you something is wrong. You open the debugger, you see the stack trace, you fix it in ten minutes. Silent corruption hides for hours, days, weeks. The goblin takes random damage. The AI path-finds to the wrong entity. The inventory screen shows the wrong item. The player reports "my game is acting weird" and you have no stack trace, no error log, nothing.

```
The bug visualized:

  troll_idx = 5  ------>  points to slot 5
                                |
  Slot 5 was: TROLL           now: GOBLIN
                                |
  pool_get(pool, 5):      returns &things[5]
                                |
                           things[5] IS the goblin
                                |
  Your code thinks it's     WRONG ENTITY!
  the troll.                    |
                           t->health -= 50
                                |
                           Goblin takes damage meant for the troll.
                           No crash. No error. Just wrong.
```

Here's the pool state at each step, shown as a flat array:

```
BEFORE removal (troll alive at slot 5):
┌─────┬─────┬─────┬─────┬─────┬─────┐
│ [0] │ [1] │ [2] │ [3] │ [4] │ [5] │
│ NIL │PLYR │GBLN │     │     │TROLL│  ← saved ref: {idx:5, gen:0}
└─────┴─────┴─────┴─────┴─────┴─────┘

AFTER removal + reuse (goblin now occupies slot 5):
┌─────┬─────┬─────┬─────┬─────┬─────┐
│ [0] │ [1] │ [2] │ [3] │ [4] │ [5] │
│ NIL │PLYR │GBLN │     │     │GBLN2│  ← slot 5 is now a DIFFERENT entity!
└─────┴─────┴─────┴─────┴─────┴─────┘
                                  ^
                    saved ref {idx:5, gen:0} hits WRONG entity!
```

Let me be very clear about WHY `pool_get` doesn't catch this. Walk through the checks in `pool_get`:

```
pool_get(pool, 5):

  Check 1:  5 > 0?         YES (5 is a valid index)
  Check 2:  5 < MAX_THINGS? YES (5 is in bounds)
  Check 3:  used[5]?        YES (the GOBLIN is alive!)

  All checks pass!  Returns &things[5]  (the goblin)

  The problem: pool_get has NO WAY to know that you
  meant the troll. It just sees "give me slot 5" and
  slot 5 IS alive. It's alive with the wrong entity.
```

The `used` check only tells you "is something alive in this slot?" It doesn't tell you "is the SAME thing alive that was there when you saved this index." That's the gap we need to fill.

> **Why?** If you're coming from JS/TS, think about this: you store a database row ID `{ id: 5 }`. Someone deletes row 5 (the troll). A new goblin gets inserted and the database happens to assign it id 5 (because it recycles IDs). Now your cached `{ id: 5 }` silently resolves to the goblin instead of the troll. In real databases, auto-increment IDs solve this by never reusing numbers. But we WANT to reuse slot numbers -- that's the whole point of our free list from lesson 08. So we need a different solution.

---

## Step 2 -- The solution: version numbers

The fix is beautifully simple. Instead of referencing a thing by just its slot index, we reference it by the slot index AND a version counter.

Each slot in the pool has a generation counter. It starts at 0. Every time that slot is recycled (removed), the counter goes up by one.

When you add a thing, you get back a reference that includes the slot index AND whatever the generation counter is right now. When you try to look up a thing using that reference later, the system compares the generation in your reference against the current generation in the pool. If they don't match, the slot has been recycled since you saved your reference. Your reference is stale. Return nil.

Here is the generations array after the troll is removed from slot 5:

```
generations[]: [0, 0, 0, 0, 0, 1]  ← slot 5 bumped from 0 to 1
                                ^
saved ref {idx:5, gen:0} vs generations[5]=1  → MISMATCH → nil!
```

Let me replay the troll/goblin scenario with this fix:

```
Step 1: Add troll at slot 5. Generation for slot 5 starts at 0.

  Slot 5:  TROLL, generation = 0
  Your ref:  { index: 5, generation: 0 }

Step 2: Remove troll. Generation bumps from 0 to 1.

  Slot 5:  (dead), generation = 1
  Your ref:  { index: 5, generation: 0 }    <-- STALE

Step 3: Goblin spawns in slot 5. Generation stays at 1.

  Slot 5:  GOBLIN, generation = 1
  New ref:   { index: 5, generation: 1 }
  Your ref:  { index: 5, generation: 0 }    <-- still stale

Step 4: You try to look up your old ref.

  Your ref says:     generation = 0
  Pool says slot 5:  generation = 1

  0 != 1  -->  GENERATION MISMATCH  -->  return nil sentinel!
```

Instead of silently returning the goblin, the system returns nil. Your code sees `kind = THING_KIND_NIL`, calls `thing_is_valid`, gets false, and knows the troll is gone. Bug caught.

```
The fix:

  your_ref = { index: 5, gen: 0 }  --- check ---  slot 5 gen = 1
                                         |
                                    0 != 1  -> STALE
                                         |
                                  return nil sentinel
                                         |
  Your code:  thing_is_valid(t) = false  -> "troll is gone"
```

Here's the entire flow compared side by side:

```
WITHOUT generational IDs:                  WITH generational IDs:

  save:    troll_idx = 5                     save:    troll_ref = { 5, gen=0 }
  remove:  pool_remove(pool, 5)              remove:  pool_remove(pool, 5)
           slot 5 generation bumps: 0 -> 1            slot 5 generation bumps: 0 -> 1
  add:     goblin takes slot 5               add:     goblin takes slot 5, gen=1
  lookup:  pool_get(pool, 5)                 lookup:  pool_get_ref(pool, {5, gen=0})
           bounds OK, used OK                         bounds OK, used OK
           returns GOBLIN                             gen 0 != gen 1 -> MISMATCH
           WRONG ENTITY                               returns NIL SENTINEL
                                                      SAFE
```

> **Why?** Think of it like React's `key` prop. When React sees that a component's key changed, it knows the old component is gone and a new one has taken its place -- even if it's in the same position in the list. Same idea: same slot index, but the generation changed, so it's a different entity. Or think of database row versioning: `WHERE id = 5 AND version = 3`. If someone deleted row 5 and a new row 5 was created with version 4, your query returns zero rows.

---

## Step 3 -- Adding generations[] to the pool

Let's start building. Open `lesson_09.c` and start with the same foundation we've had since lesson 07 -- the includes, constants, and types.

```c
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_THINGS 64
```

Nothing new here. Same headers, same pool size. Now the types:

```c
typedef enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

typedef int thing_idx;
```

Same enum. Same index type. Now the thing struct -- unchanged from lesson 08:

```c
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;
```

And now the pool struct. This is where the first new piece goes:

```c
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    uint32_t  generations[MAX_THINGS];   /* NEW: bumped on each slot recycle */
    thing_idx next_empty;
    thing_idx first_free;
} thing_pool;
```

One new array: `generations[MAX_THINGS]`. That's it. One array of 32-bit unsigned integers, one per slot.

```
Pool struct with generations:

  +---things[64]---+---used[64]---+---generations[64]---+---next_empty---+---first_free---+
  | 64 things      | 64 bools     | 64 uint32_t's      | int            | int            |
  | (entity data)  | (alive/dead) | (version counters)  | (fresh slots)  | (recycled)     |
  +----------------+--------------+--------------------+----------------+----------------+
```

When the pool is initialized with `memset(pool, 0, sizeof(*pool))`, every generation starts at 0. That's the starting version for every slot. Zero-init strikes again -- no extra initialization code needed.

Let me walk through what each array in the pool means now:

```
After pool_init (memset to 0, next_empty = 1):

  things[0]:       all zeros (nil sentinel -- NEVER changes)
  things[1..63]:   all zeros (not yet used)
  used[0..63]:     all false (nothing is alive)
  generations[0..63]: all 0   (no slot has ever been recycled)
  next_empty:      1          (first available fresh slot)
  first_free:      0          (free list is empty)

  Memory breakdown:
    things[64]:       64 * 32 bytes = 2,048 bytes  (entity data)
    used[64]:         64 * 1 byte   =    64 bytes  (alive/dead flags)
    generations[64]:  64 * 4 bytes  =   256 bytes  (version counters) <-- NEW
    next_empty:       4 bytes
    first_free:       4 bytes
    -----------------------------------------------
    Total:                           2,376 bytes

  The generations array adds 256 bytes. That's 10% overhead.
  For the real pool (MAX_THINGS=1024): 4 KB. Still nothing.
```

> **Why?** The memory cost is `64 * 4 bytes = 256 bytes` for our lesson (or `1024 * 4 = 4 KB` in the real header with MAX_THINGS=1024). That's nothing. One comparison per lookup, 4 bytes per slot. Cheap insurance against an entire class of silent bugs.

> **Why `uint32_t` and not `int`?** A `uint32_t` is an unsigned 32-bit integer. It can count up to 4,294,967,295. If a slot got recycled once per frame at 60fps, it would take 2.27 YEARS to wrap around back to 0. You'd have to deliberately stress-test to hit overflow. And even if you did, the worst case is a false "valid" result for one frame -- not great, but not catastrophic. If you're paranoid, use `uint64_t` -- that's 9.2 quintillion recycles before wrapping.

Let's add the helper functions and `pool_init`. These are the same as before:

```c
static const char *thing_kind_name(thing_kind kind)
{
    switch (kind) {
        case THING_KIND_NIL:    return "NIL";
        case THING_KIND_PLAYER: return "PLAYER";
        case THING_KIND_TROLL:  return "TROLL";
        case THING_KIND_GOBLIN: return "GOBLIN";
        case THING_KIND_ITEM:   return "ITEM";
        default:                return "UNKNOWN";
    }
}

static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}
```

**Compile checkpoint.** Save the file and compile:

```
gcc -Wall -Wextra -Werror -std=c11 -o lesson_09 lesson_09.c
```

You'll get an error about missing `main`. That's fine -- we're building incrementally. The compiler is confirming the types and functions are valid. Add an empty main to silence it if you want:

```c
int main(void) { return 0; }
```

---

## Step 4 -- Building the thing_ref struct

Now we build the reference type. Add this right after the `thing_idx` typedef:

```c
typedef struct thing_ref {
    thing_idx index;       /* which slot */
    uint32_t  generation;  /* which "version" of that slot */
} thing_ref;
```

Two fields. That's the whole thing. Eight bytes total (4 for the index, 4 for the generation).

```
thing_ref layout (8 bytes):

  +--------+-----------+
  | index  | generation|
  |  (int) | (uint32_t)|
  +--------+-----------+
  | 4 bytes| 4 bytes   |
  +--------+-----------+

  Example:  { index: 5, generation: 3 }
  Meaning:  "slot 5, version 3"
```

A `thing_ref` is trivially copyable. You can pass it by value, store it in an array, save it to disk, send it over the network. It's just two integers. No pointers, no allocations, no cleanup needed.

Compare this to how you'd do the same thing in different languages:

```
C (our approach):    thing_ref = { index: 5, generation: 3 }
                     8 bytes. Copy with =. Compare field by field.

JS/TS:               { id: 5, version: 3 }
                     ~40 bytes (object overhead). GC-managed.

C++ shared_ptr:      shared_ptr<Entity>
                     16 bytes + heap allocation + atomic ref count.

Rust:                slotmap::Key
                     8 bytes. Same concept, language-level safety.
```

Our approach is the lightest possible. Two integers. No indirection. No reference counting. No garbage collection. Just data.

> **Why?** In JS terms, `thing_ref` is like `{ id: 5, version: 3 }`. It's a value object -- no methods, no prototype chain, no garbage collector involvement. Just two numbers that together uniquely identify a specific entity at a specific point in time. If the entity at slot 5 dies and a new one takes its place, the version changes, and your old `{ id: 5, version: 3 }` no longer matches.

There's also a natural "nil" value: `{ index: 0, generation: 0 }`. Since slot 0 is the nil sentinel and never holds a real entity, any ref with index 0 automatically resolves to nil. And since structs in C are zero-initialized by `memset`, an uninitialized ref is `{ 0, 0 }` -- which is nil. Safe by default.

```
Nil ref behavior:

  thing_ref my_ref;                        // uninitialized
  memset(&my_ref, 0, sizeof(my_ref));      // or zero-init
  // my_ref = { index: 0, generation: 0 }

  pool_get_ref(pool, my_ref):
    ref.index = 0
    0 > 0?  NO.  Return nil sentinel.

  thing_is_valid(result):  false

  Safe. Predictable. No crash.
```

---

## Step 5 -- pool_add returns thing_ref (not just index)

Now let's write `pool_add`. In lesson 08, this function returned a `thing_ref` but we hadn't really talked about what the generation field meant. Now it matters.

```c
static thing_ref pool_add(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    /* Try free list first (recycled slots) */
    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    }
    /* Then try never-used region */
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    /* Pool full */
    else {
        return (thing_ref){0, 0};
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    /* Return ref with CURRENT generation of this slot */
    return (thing_ref){slot, pool->generations[slot]};
}
```

The last line is the key: `return (thing_ref){slot, pool->generations[slot]};`

When a slot is brand new (never recycled), its generation is 0. The ref you get back is `{ slot, 0 }`.

When a slot has been recycled once, its generation is 1 (bumped by `pool_remove`, which we'll write next). The ref you get back is `{ slot, 1 }`.

The ref always captures the CURRENT generation of that slot at the moment the entity was created. This is the "version number" that later lookups will check against.

Let me trace through what happens with a fresh slot vs a recycled slot:

```
Scenario A: Fresh slot (never used before).

  pool_add(pool, THING_KIND_TROLL):
    first_free = 0  (empty)
    next_empty = 3
    slot = 3, next_empty becomes 4

    generations[3] = 0  (never recycled)
    Returns: { index: 3, generation: 0 }

Scenario B: Recycled slot (was used, removed, now reused).

  pool_add(pool, THING_KIND_GOBLIN):
    first_free = 3  (slot 3 was freed earlier)
    slot = 3, first_free advances

    generations[3] = 2  (recycled twice before)
    Returns: { index: 3, generation: 2 }

    This ref says: "I am the entity that was placed in slot 3
                    when slot 3 was on its 2nd recycling."
    Any ref to slot 3 with generation 0 or 1 is STALE.
```

Notice that `pool_add` doesn't change the generation. It just reads it. The generation only changes in `pool_remove`. So the sequence is:

```
add   -> slot 3, gen 0.  ref_a = {3, 0}
remove -> gen bumps to 1
add   -> slot 3, gen 1.  ref_b = {3, 1}
remove -> gen bumps to 2
add   -> slot 3, gen 2.  ref_c = {3, 2}

ref_a is stale (gen 0 vs current 2)
ref_b is stale (gen 1 vs current 2)
ref_c is valid (gen 2 matches current 2)
```

> **Why?** Think of it as a receipt number at a deli counter. You take number 47, and when they call "number 47!" you know it's your order. If they start a new batch and someone else gets number 47 from the new roll, your old ticket #47 doesn't work anymore -- the batch number (generation) has changed.

---

## Step 6 -- pool_remove bumps the generation

Here's `pool_remove`, updated with one critical new line:

```c
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    /* BUMP GENERATION: every existing thing_ref to this slot is now stale */
    pool->generations[idx]++;

    /* Push onto free list (same as lesson 08) */
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}
```

One new line: `pool->generations[idx]++;`

That's it. One integer increment. Let me walk through what this line does at the moment of removal:

```
Before pool_remove(pool, 5):

  things[5]:        TROLL (kind=TROLL, health=100, x=10, ...)
  used[5]:          true
  generations[5]:   0

  troll_ref = { index: 5, generation: 0 }     <-- matches pool

After pool_remove(pool, 5):

  things[5]:        (all zeros from memset)
  used[5]:          false
  generations[5]:   1                          <-- BUMPED from 0 to 1

  troll_ref = { index: 5, generation: 0 }     <-- MISMATCH (0 != 1)

  The gap:  ref says gen=0, pool says gen=1.
  Any future pool_get_ref with this ref -> nil.
```

Every existing `thing_ref` that points to this slot now has a generation that's one behind the pool's current generation. There could be 50 different variables holding `{ 5, 0 }` scattered across your codebase -- AI target refs, physics constraint refs, inventory refs. ALL of them become stale at once, with a single `++`. No "invalidation list." No "notify observers." No "weak reference cleanup." Just one integer increment.

> **Why bump BEFORE adding to free list?** The order doesn't strictly matter (both happen before anyone can read the slot again), but conceptually the generation bump is part of "killing this entity" and the free list push is part of "making the slot available." Putting them in that order makes the code read naturally: die, then become available.

> **Why?** In JS, if you wanted to invalidate all cached references to a deleted object, you'd need some kind of pub/sub system -- event listeners, WeakRef callbacks, a global invalidation bus. In C with generational IDs, invalidation is free: bump a counter, and every old reference self-detects as stale when it's next used. No notification system needed.

**Compile checkpoint.** Your file should compile cleanly now (with an empty `main`).

---

## Step 7 -- pool_get_ref checks generation

Now the payoff. We write `pool_get_ref` -- the generation-checked lookup function.

First, let's also write the plain `pool_get` so we have both for comparison:

```c
/* Raw index lookup -- no generation check */
static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0]; /* nil sentinel */
}
```

This is the same `pool_get` from lesson 07. It checks bounds and liveness, but it does NOT check the generation. If a slot was recycled and a new entity moved in, `pool_get` with the old index returns the new entity. That's the bug from Step 1.

Now the safe version:

```c
/* Generation-checked lookup -- detects stale references */
static thing *pool_get_ref(thing_pool *pool, thing_ref ref)
{
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]
        && pool->generations[ref.index] == ref.generation) {
        return &pool->things[ref.index];
    }
    return &pool->things[0]; /* nil sentinel */
}
```

Same structure as `pool_get`, but with one extra condition: `pool->generations[ref.index] == ref.generation`. Let me break down all four checks:

```
The four checks in pool_get_ref:

  Check 1:  ref.index > 0
            Catches nil refs (index 0 is the nil sentinel, never a real entity)

  Check 2:  ref.index < MAX_THINGS
            Catches out-of-bounds (garbage index values)

  Check 3:  pool->used[ref.index]
            Catches dead slots (entity was removed but slot not yet recycled)

  Check 4:  pool->generations[ref.index] == ref.generation
            Catches recycled slots (entity was removed AND a new entity took the slot)
```

Check 3 and Check 4 cover different situations:

```
Scenario A: Troll removed, slot NOT yet reused.

  used[5] = false    <-- Check 3 catches this
  generations[5] = 1
  ref.generation = 0

  Check 3 fires first. We never even reach Check 4.

Scenario B: Troll removed, slot reused by goblin.

  used[5] = true     <-- Check 3 passes! (goblin is alive)
  generations[5] = 1
  ref.generation = 0

  Check 3 misses it. Check 4 catches it: 1 != 0.
```

Without Check 4, Scenario B is the bug from Step 1. The slot is in use (by the wrong entity), and `pool_get` would happily return the goblin. Only the generation check catches it.

Here's every possible state a stale reference can hit, and which check catches it:

```
+----------------------------------+----------------+-------------------+
| State of slot                    | used[] catches | generation catches|
|                                  | it?            | it?               |
+----------------------------------+----------------+-------------------+
| 1. Slot still holds original     | N/A            | N/A               |
|    entity (ref is VALID)         | (not stale)    | (not stale)       |
+----------------------------------+----------------+-------------------+
| 2. Entity removed, slot not yet  | YES            | (not reached)     |
|    recycled. used=false.         |                |                   |
+----------------------------------+----------------+-------------------+
| 3. Entity removed, slot recycled | NO (slot is    | YES (gen bumped)  |
|    by new entity. used=true.     |  alive again!) |                   |
+----------------------------------+----------------+-------------------+
| 4. Entity removed, slot recycled | NO (slot is    | YES (gen bumped   |
|    MULTIPLE times. used=true.    |  alive again!) |  multiple times)  |
+----------------------------------+----------------+-------------------+
```

> **Why?** Together, these two checks cover every possible state a stale reference can encounter: the slot might be dead (caught by `used`), or the slot might be alive-but-different (caught by `generation`). There's no gap. No edge case slips through.

Now let's also write a version WITHOUT the generation check, so we can compare them side by side:

```c
/* WITHOUT generation check (UNSAFE -- for comparison only) */
static thing *pool_get_no_gen(thing_pool *pool, thing_ref ref)
{
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]) {
        return &pool->things[ref.index];
    }
    return &pool->things[0]; /* nil sentinel */
}
```

This is identical to `pool_get_ref` but without the `generations` check. We'll use it in our test to show exactly what goes wrong.

**Compile checkpoint.** Everything compiles. Now let's test it.

---

## Step 8 -- Demonstrating stale reference detection

Here's our main function. We'll walk through the exact troll/goblin scenario from Step 1, showing that `pool_get_ref` catches the stale reference while `pool_get_no_gen` does NOT.

```c
int main(void)
{
    printf("\n=== Lesson 09: Generational IDs ===\n\n");

    thing_pool pool;
    pool_init(&pool);
```

**Test 1: Add a troll and verify the ref works.**

```c
    /* ---- Test 1: Add a troll, verify the ref works ---- */
    printf("--- Test 1: Add troll at slot 1 (generation 0) ---\n");

    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    pool.things[troll_ref.index].health = 100.0f;

    printf("  Added TROLL at slot %d, generation %u\n",
           troll_ref.index, troll_ref.generation);
    printf("  troll_ref = { index: %d, generation: %u }\n",
           troll_ref.index, troll_ref.generation);

    thing *t = pool_get_ref(&pool, troll_ref);
    printf("  pool_get_ref(troll_ref): kind=%s, valid=%d\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(t->kind == THING_KIND_TROLL);
    assert(troll_ref.index == 1);
    assert(troll_ref.generation == 0);
```

The troll lands in slot 1 (the first slot after the nil sentinel). Generation is 0 because slot 1 has never been recycled. The ref is `{ 1, 0 }`, and `pool_get_ref` returns the troll. Everything works.

```
Pool state after Test 1:

  Index:       [0]    [1]
  Things:      NIL    TROLL (health=100)
  Used:        false  true
  Generations: 0      0

  troll_ref = { index: 1, generation: 0 }   <-- VALID
```

**Test 2: Remove the troll. Generation bumps.**

```c
    /* ---- Test 2: Remove troll. Generation bumps. ---- */
    printf("\n--- Test 2: Remove troll -- generation bumps ---\n");

    pool_remove(&pool, troll_ref.index);

    printf("  Removed slot %d\n", troll_ref.index);
    printf("  pool.generations[%d] is now %u (was %u in ref)\n",
           troll_ref.index,
           pool.generations[troll_ref.index],
           troll_ref.generation);

    t = pool_get_ref(&pool, troll_ref);
    printf("  pool_get_ref(troll_ref): kind=%s, valid=%d  <-- STALE\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));
```

After removal, `generations[1]` bumped from 0 to 1. The troll_ref still holds generation 0. Even though the slot is dead (caught by `used` check before we even reach the generation check), the ref is stale.

```
Pool state after Test 2:

  Index:       [0]    [1]
  Things:      NIL    (dead)
  Used:        false  false
  Generations: 0      1       <-- BUMPED from 0 to 1

  troll_ref = { index: 1, generation: 0 }   <-- STALE
  first_free = 1
```

**Test 3: Add a goblin. Same slot, new generation. Old ref is still stale.**

```c
    /* ---- Test 3: Add goblin (reuses slot). Old ref still stale. ---- */
    printf("\n--- Test 3: Goblin spawns in recycled slot ---\n");

    thing_ref goblin_ref = pool_add(&pool, THING_KIND_GOBLIN);
    pool.things[goblin_ref.index].health = 30.0f;

    printf("  Added GOBLIN at slot %d, generation %u\n",
           goblin_ref.index, goblin_ref.generation);
    assert(goblin_ref.index == troll_ref.index);  /* same slot! */

    printf("  troll_ref  = { index: %d, gen: %u }  <-- STALE\n",
           troll_ref.index, troll_ref.generation);
    printf("  goblin_ref = { index: %d, gen: %u }  <-- VALID\n",
           goblin_ref.index, goblin_ref.generation);
```

The goblin lands in slot 1 (recycled from the free list). The goblin's ref is `{ 1, 1 }` -- same index, new generation.

```
Pool state after Test 3:

  Index:       [0]    [1]
  Things:      NIL    GOBLIN (health=30)
  Used:        false  true
  Generations: 0      1

  troll_ref  = { index: 1, generation: 0 }  <-- STALE (gen 0 vs pool gen 1)
  goblin_ref = { index: 1, generation: 1 }  <-- VALID (gen 1 matches pool gen 1)
```

Now the critical test. Let's try both the safe and unsafe lookup with the old troll ref:

```c
    /* ---- The critical comparison ---- */
    printf("\n--- WITH generation check (SAFE) ---\n");
    t = pool_get_ref(&pool, troll_ref);
    printf("  pool_get_ref(troll_ref): kind=%s, valid=%d\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    printf("  ref.generation=%u vs pool.generations[%d]=%u --> %s\n",
           troll_ref.generation, troll_ref.index,
           pool.generations[troll_ref.index],
           troll_ref.generation == pool.generations[troll_ref.index]
               ? "MATCH" : "MISMATCH -> nil");
    assert(!thing_is_valid(t));  /* SAFE: stale ref caught */

    printf("\n--- WITHOUT generation check (UNSAFE) ---\n");
    t = pool_get_no_gen(&pool, troll_ref);
    printf("  pool_get_no_gen(troll_ref): kind=%s, valid=%d\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    if (thing_is_valid(t)) {
        printf("  WRONG ENTITY! Got %s (health=%.0f) instead of the dead troll!\n",
               thing_kind_name(t->kind), t->health);
    }
    assert(thing_is_valid(t));               /* BUG: it IS valid... */
    assert(t->kind == THING_KIND_GOBLIN);    /* ...but it's the GOBLIN */
```

There it is. `pool_get_ref` returns nil -- stale reference detected, safe. `pool_get_no_gen` returns the goblin -- wrong entity, silent corruption.

Here is the full flow of how generational IDs detect stale references:

```
The full generational ID flow:

1. Add troll    → slot 1, gen 0  → ref = {1, 0}
2. Remove troll → gen bumps to 1 → generations[1] = 1
3. Add goblin   → slot 1, gen 1  → new ref = {1, 1}
4. Old ref {1, 0} vs gen[1]=1    → MISMATCH → return nil (SAFE!)
5. New ref {1, 1} vs gen[1]=1    → MATCH    → return goblin (correct)
```

```
The comparison:

  +---------------------+--------------------+---------------------+
  |                     | pool_get_ref       | pool_get_no_gen     |
  |                     | (WITH gen check)   | (WITHOUT gen check) |
  +---------------------+--------------------+---------------------+
  | Returns:            | nil sentinel       | GOBLIN              |
  | thing_is_valid():   | false              | true                |
  | Stale ref caught?   | YES                | NO                  |
  | Silent corruption?  | NO                 | YES                 |
  +---------------------+--------------------+---------------------+
```

**Test 4: Verify the goblin ref works fine.**

```c
    /* ---- Test 4: Goblin ref works fine ---- */
    printf("\n--- Test 4: Goblin ref works correctly ---\n");
    t = pool_get_ref(&pool, goblin_ref);
    printf("  pool_get_ref(goblin_ref): kind=%s, health=%.0f\n",
           thing_kind_name(t->kind), t->health);
    assert(t->kind == THING_KIND_GOBLIN);
    assert(t->health == 30.0f);
```

The goblin ref has generation 1, which matches slot 1's current generation. It works perfectly.

**Test 5: Multiple recycles of the same slot.**

```c
    /* ---- Test 5: Multiple recycles ---- */
    printf("\n--- Test 5: Multiple recycles of the same slot ---\n");

    thing_ref saved_refs[4];
    const char *names[] = {"TROLL", "GOBLIN", "ITEM", "PLAYER"};
    thing_kind kinds[] = {THING_KIND_TROLL, THING_KIND_GOBLIN,
                          THING_KIND_ITEM, THING_KIND_PLAYER};

    /* Remove the goblin first */
    pool_remove(&pool, goblin_ref.index);

    /* Add and remove 4 times at the same slot */
    for (int i = 0; i < 4; i++) {
        saved_refs[i] = pool_add(&pool, kinds[i]);
        printf("  Add %-7s at slot %d, gen %u\n",
               names[i], saved_refs[i].index, saved_refs[i].generation);
        if (i < 3) {
            pool_remove(&pool, saved_refs[i].index);
            printf("  Remove slot %d (gen bumped to %u)\n",
                   saved_refs[i].index,
                   pool.generations[saved_refs[i].index]);
        }
    }
```

Now we check all four saved refs. Only the last one should be valid:

```c
    printf("\n  Checking all saved refs:\n");
    for (int i = 0; i < 4; i++) {
        t = pool_get_ref(&pool, saved_refs[i]);
        printf("    %s (gen=%u): kind=%s, valid=%d",
               names[i], saved_refs[i].generation,
               thing_kind_name(t->kind), thing_is_valid(t));
        if (i == 3) {
            printf("  <-- VALID (current)\n");
            assert(thing_is_valid(t));
        } else {
            printf("  <-- STALE\n");
            assert(!thing_is_valid(t));
        }
    }
```

The output looks like this:

```
  Add TROLL   at slot 1, gen 2
  Remove slot 1 (gen bumped to 3)
  Add GOBLIN  at slot 1, gen 3
  Remove slot 1 (gen bumped to 4)
  Add ITEM    at slot 1, gen 4
  Remove slot 1 (gen bumped to 5)
  Add PLAYER  at slot 1, gen 5

  Checking all saved refs:
    TROLL (gen=2): kind=NIL, valid=0  <-- STALE
    GOBLIN (gen=3): kind=NIL, valid=0  <-- STALE
    ITEM (gen=4): kind=NIL, valid=0  <-- STALE
    PLAYER (gen=5): kind=PLAYER, valid=1  <-- VALID (current)
```

Every old ref is stale. Only the most recent ref is valid. This is automatic -- no "invalidation list," no "observer pattern," no "weak reference cleanup." Just one integer comparison.

**Test 6: Nil ref is always safe.**

```c
    /* ---- Test 6: Nil ref ---- */
    printf("\n--- Test 6: Nil ref is always safe ---\n");

    thing_ref nil_ref = {0, 0};
    t = pool_get_ref(&pool, nil_ref);
    printf("  pool_get_ref({0, 0}): kind=%s, valid=%d  <-- nil\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));
```

A ref of `{0, 0}` is the nil ref. Since `ref.index = 0` fails the `> 0` check, it always returns the nil sentinel. This is what `pool_add` returns when the pool is full, and it's what a zero-initialized `thing_ref` variable contains. Safe by default.

**Test 7: Summary comparison.**

```c
    /* ---- Test 7: Summary ---- */
    printf("\n--- Summary ---\n\n");
    printf("  +---------------------+--------------------+---------------------+\n");
    printf("  |                     | WITH gen check     | WITHOUT gen check   |\n");
    printf("  +---------------------+--------------------+---------------------+\n");
    printf("  | Stale ref returns:  | nil (SAFE)         | WRONG entity (BUG!) |\n");
    printf("  | Silent corruption?  | NO                 | YES                 |\n");
    printf("  | Cost per lookup:    | 1 extra comparison | nothing             |\n");
    printf("  | Cost per slot:      | 4 bytes (uint32_t) | nothing             |\n");
    printf("  +---------------------+--------------------+---------------------+\n\n");

    printf("=== Lesson 09 PASSED ===\n\n");
    return 0;
}
```

---

## Build and run

Save the complete file and compile:

```
gcc -Wall -Wextra -Werror -std=c11 -o lesson_09 lesson_09.c
```

Then run:

```
./lesson_09
```

## Expected output

```
=== Lesson 09: Generational IDs ===

--- Test 1: Add troll at slot 1 (generation 0) ---
  Added TROLL at slot 1, generation 0
  troll_ref = { index: 1, generation: 0 }
  pool_get_ref(troll_ref): kind=TROLL, valid=1

--- Test 2: Remove troll -- generation bumps ---
  Removed slot 1
  pool.generations[1] is now 1 (was 0 in ref)
  pool_get_ref(troll_ref): kind=NIL, valid=0  <-- STALE

--- Test 3: Goblin spawns in recycled slot ---
  Added GOBLIN at slot 1, generation 1
  troll_ref  = { index: 1, gen: 0 }  <-- STALE
  goblin_ref = { index: 1, gen: 1 }  <-- VALID

--- WITH generation check (SAFE) ---
  pool_get_ref(troll_ref): kind=NIL, valid=0
  ref.generation=0 vs pool.generations[1]=1 --> MISMATCH -> nil

--- WITHOUT generation check (UNSAFE) ---
  pool_get_no_gen(troll_ref): kind=GOBLIN, valid=1
  WRONG ENTITY! Got GOBLIN (health=30) instead of the dead troll!

--- Test 4: Goblin ref works correctly ---
  pool_get_ref(goblin_ref): kind=GOBLIN, health=30

--- Test 5: Multiple recycles of the same slot ---
  Add TROLL   at slot 1, gen 2
  Remove slot 1 (gen bumped to 3)
  Add GOBLIN  at slot 1, gen 3
  Remove slot 1 (gen bumped to 4)
  Add ITEM    at slot 1, gen 4
  Remove slot 1 (gen bumped to 5)
  Add PLAYER  at slot 1, gen 5

  Checking all saved refs:
    TROLL (gen=2): kind=NIL, valid=0  <-- STALE
    GOBLIN (gen=3): kind=NIL, valid=0  <-- STALE
    ITEM (gen=4): kind=NIL, valid=0  <-- STALE
    PLAYER (gen=5): kind=PLAYER, valid=1  <-- VALID (current)

--- Test 6: Nil ref is always safe ---
  pool_get_ref({0, 0}): kind=NIL, valid=0  <-- nil

--- Summary ---

  +---------------------+--------------------+---------------------+
  |                     | WITH gen check     | WITHOUT gen check   |
  +---------------------+--------------------+---------------------+
  | Stale ref returns:  | nil (SAFE)         | WRONG entity (BUG!) |
  | Silent corruption?  | NO                 | YES                 |
  | Cost per lookup:    | 1 extra comparison | nothing             |
  | Cost per slot:      | 4 bytes (uint32_t) | nothing             |
  +---------------------+--------------------+---------------------+

=== Lesson 09 PASSED ===
```

---

## Common mistakes

**1. Only checking generation, skipping the `used` check.** If you remove an entity but the slot hasn't been recycled yet, the generation was bumped but the slot is unused. That's fine -- the generation catches it. But what about the edge case where someone manually creates a ref `{ 5, 1 }` (maybe from a save file) and slot 5 happens to be dead with generation 1? The generation matches, but the slot is dead. The `used` check catches this. Always check both.

**2. Forgetting to bump the generation in `pool_remove`.** Without the bump, the generation stays the same after removal. When the slot is recycled, the new entity has the SAME generation as the old ref. The stale ref passes the generation check. You've just reintroduced the bug this entire lesson exists to fix.

**3. Using `pool_get` when you should use `pool_get_ref`.** `pool_get` takes a raw index and has NO generation check. It's fine within a single frame when you KNOW the entity is alive (because you just got it from the iterator). But if you're holding a reference across frames -- "my target enemy," "the item I'm carrying" -- you MUST use `pool_get_ref`. Otherwise you're one remove-and-recycle away from the silent corruption bug.

**4. Comparing `thing_ref` structs with `==`.** In C, you can't compare structs with `==` (unlike JS objects with deep equality). To check if two refs are the same, compare both fields: `ref_a.index == ref_b.index && ref_a.generation == ref_b.generation`. If you want, write a helper: `bool ref_equal(thing_ref a, thing_ref b) { return a.index == b.index && a.generation == b.generation; }`.

**5. Storing `thing_ref` in a local variable and using it after pool operations.** If you save a ref, then add or remove entities from the pool, the ref itself doesn't change -- it's just two integers on the stack. But the POOL's state may have changed. A ref that was valid before a batch of removals might be stale afterward. Always re-check with `pool_get_ref` before using a saved ref after pool modifications.

---

## When to use thing_ref vs thing_idx

Now you have two ways to reference a thing:

| Type | What it is | When to use it |
|---|---|---|
| `thing_idx` | Just the slot number | Within a single frame, when you KNOW the thing is alive (e.g., you just got it from the iterator) |
| `thing_ref` | Slot number + generation | Across frames, when the thing MIGHT have been removed (e.g., "my target enemy", "the item I'm carrying") |

Think of it this way:

- `thing_idx` is a raw address. Fast, zero overhead, but dangerous if the thing dies between when you save it and when you use it.
- `thing_ref` is a checked address. One extra integer comparison per lookup, but detects stale references.

Here are concrete examples of each:

```c
/* ---- WITHIN a single update frame -- raw index is fine ---- */

/* You're iterating the pool RIGHT NOW. Every thing you touch is
 * guaranteed alive because you just checked used[]. */
for (int i = 1; i < pool.next_empty; i++) {
    if (!pool.used[i]) continue;
    thing *t = &pool.things[i];
    t->x += t->dx;  /* t is guaranteed alive */
}

/* ---- ACROSS frames -- use thing_ref ---- */

/* These refs persist from one frame to the next. Between frames,
 * ANYTHING could happen: the target could die, the item could
 * despawn, the player could get removed by a cutscene. */
typedef struct game_state {
    thing_pool pool;
    thing_ref  player_ref;    /* might be dead next frame */
    thing_ref  target_ref;    /* enemy we're tracking -- might die */
    thing_ref  held_item_ref; /* item in player's hand -- might break */
} game_state;

void game_update(game_state *state) {
    /* Check if our target is still alive */
    thing *target = pool_get_ref(&state->pool, state->target_ref);
    if (!thing_is_valid(target)) {
        /* Target died or was recycled -- pick a new one */
        state->target_ref = find_nearest_enemy(&state->pool);
    } else {
        /* Target is still alive and it's the SAME entity we saved */
        chase_target(target);
    }
}
```

> **Why?** In JS/TS, you don't usually worry about this because object references stay valid as long as the object exists (garbage collection handles the rest). But in our pool system, slot indices can be reused. A `thing_idx` is like storing `array[5]` -- if someone replaces the object at index 5, your cached index 5 points to the new object. A `thing_ref` is like storing `{ index: 5, version: 3 }` -- you can detect when the object at index 5 has been replaced.

The rule of thumb: if the reference lives LONGER than one frame, use `thing_ref`. If it's a temporary variable within a single function, `thing_idx` is fine.

---

## What generational IDs DON'T do

Let's be honest about the limitations. This isn't magic -- it's a targeted solution for a specific problem.

**They don't prevent all bugs.** If you use `pool_get` (raw index) instead of `pool_get_ref`, you bypass the generation check entirely. The raw index access is still available and still useful -- sometimes you KNOW the thing is alive and don't want to pay for the extra comparison. But if you guess wrong, you're back to the silent corruption bug.

**They don't tell you WHAT happened.** A stale ref just says "this ref is no longer valid." It doesn't tell you whether the entity was killed by the player, despawned by a timer, removed by a level transition, or fell into a pit. If you need that information, you'd add a "death reason" enum or an event system on top. But for most game code, "is it still alive? yes/no" is all you need.

**They don't prevent writing to nil.** If you get a nil result from `pool_get_ref` and then write to it, you're writing to the nil sentinel (slot 0). In debug mode, `thing_pool_assert_nil_clean` will catch this. In release mode, the write is silently lost (overwritten by the next memset). Generational IDs prevent you from getting the WRONG entity, but they don't prevent you from ignoring the "it's nil" return value.

**They wrap around (theoretically).** After 4 billion recycles of the same slot, the generation wraps back to 0 and an ancient ref could falsely validate. In practice this never happens. But it's worth knowing the theoretical limit.

---

## Exercises

1. **Overflow thought experiment.** The generation is `uint32_t`, which overflows at ~4 billion. What happens if a slot is recycled 4,294,967,296 times and the generation wraps back to 0? Could an ancient ref falsely validate? How long would this take at 60 removes per second? (Answer: ~2.27 years of a single slot being constantly recycled. In practice, this never happens.)

2. **Add a `pool_ref_is_nil` function.** Write a function that returns true if a `thing_ref` is the nil ref `{0, 0}`. Use it to check if `pool_add` returned a valid ref. (Hint: check `ref.index == 0`.)

3. **Serialization.** A `thing_ref` is just two integers. Write a function that prints a ref as a string `"ref(5,3)"` and another that parses it back. This is how you'd save references to a file or send them over a network. The generation ensures that after loading a save file, stale refs are still detected correctly.

4. **Double-remove safety.** Call `pool_remove` on the same index twice. What happens? Does the generation bump twice? Trace through the code carefully. (Hint: the `if (!pool->used[idx]) return;` guard prevents the second remove from doing anything. The generation only bumps once.)

5. **Build a "target tracking" system.** Create a struct `tracker { thing_ref target; }`. In a loop, spawn enemies, pick one as the target (save its ref), then randomly remove enemies. After each frame, check if the target is still valid using `pool_get_ref`. Print whether the target survived or was lost. This simulates a real game AI that tracks a target enemy.

---

## Connection to your work

If you've ever built a web app with a cache that holds entity IDs -- user session refs, document IDs, WebSocket connection handles -- you've hit this exact problem. A cached ID points to something that no longer exists, and the system either crashes (null reference) or silently uses the wrong entity (reused ID).

Here are concrete parallels from web development:

```
Web:    React component key={id} detects when a list item
        at position 5 has been replaced by a different item.
Pool:   thing_ref { index: 5, gen: 3 } detects when slot 5
        has been recycled with a different entity.

Web:    Database optimistic locking: UPDATE ... WHERE version = 3.
        If someone else updated the row, version is now 4, and
        your UPDATE affects 0 rows. Stale write detected.
Pool:   pool_get_ref checks generation. If the slot was recycled,
        generation doesn't match. Stale read detected.

Web:    WebSocket connection ID. If the connection drops and
        reconnects, the server assigns a new connection ID.
        Old messages with the old ID are rejected.
Pool:   thing_ref with old generation. If the entity dies and
        the slot is reused, the old ref is rejected.
```

In game engines, every long-lived reference to an entity (AI target, physics constraint, animation attachment) uses generational IDs. Unity calls them "Entity" (with an index and a version). Unreal calls them "FName" and "FWeakObjectPtr". The concept is universal: stable slot reuse + safe stale detection.

---

## What's next

The pool now supports add, remove, raw get, and safe get-by-ref. Here's the complete picture after this lesson:

```
After Lesson 09, the pool has:

  things[MAX_THINGS]        <-- entity data (Lesson 02)
  things[0] = nil sentinel  <-- safe fallback (Lesson 06)
  used[MAX_THINGS]          <-- alive/dead tracking (Lesson 07)
  generations[MAX_THINGS]   <-- stale reference detection (THIS LESSON)
  next_empty                <-- fresh slot allocator (Lesson 07)
  first_free                <-- recycled slot free list (Lesson 08)

  pool_init()               <-- zero everything
  pool_add(kind)            <-- free list -> next_empty, returns ref
  pool_remove(idx)          <-- mark dead, zero, bump gen, push free list
  pool_get(idx)             <-- raw index access, nil fallback
  pool_get_ref(ref)         <-- generation-checked access, nil fallback
  thing_is_valid(t)         <-- kind != NIL
```

The one thing left is a clean way to iterate over all living entities. Right now you'd write a raw `for` loop checking `used[i]` at every slot -- ugly, error-prone, and leaking pool internals into game code. In Lesson 10, we wrap that into a proper iterator API, and the pool system is complete.

---

## Complete file: lesson_09.c

```c
/* lesson_09.c -- Generational IDs (WITH/WITHOUT comparison)
 *
 * Compile:
 *   gcc -Wall -Wextra -Werror -std=c11 -o lesson_09 lesson_09.c
 *
 * Run:
 *   ./lesson_09
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Constants ---- */

#define MAX_THINGS 64

/* ---- Types ---- */

typedef enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

typedef int thing_idx;

typedef struct thing_ref {
    thing_idx index;
    uint32_t  generation;
} thing_ref;

typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;

typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    uint32_t  generations[MAX_THINGS];
    thing_idx next_empty;
    thing_idx first_free;
} thing_pool;

/* ---- Helpers ---- */

static const char *thing_kind_name(thing_kind kind)
{
    switch (kind) {
        case THING_KIND_NIL:    return "NIL";
        case THING_KIND_PLAYER: return "PLAYER";
        case THING_KIND_TROLL:  return "TROLL";
        case THING_KIND_GOBLIN: return "GOBLIN";
        case THING_KIND_ITEM:   return "ITEM";
        default:                return "UNKNOWN";
    }
}

static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

/* ---- Pool operations ---- */

static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}

static thing_ref pool_add(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    }
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    else {
        return (thing_ref){0, 0};
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return (thing_ref){slot, pool->generations[slot]};
}

static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    pool->generations[idx]++;

    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}

static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}

static thing *pool_get_ref(thing_pool *pool, thing_ref ref)
{
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]
        && pool->generations[ref.index] == ref.generation) {
        return &pool->things[ref.index];
    }
    return &pool->things[0];
}

static thing *pool_get_no_gen(thing_pool *pool, thing_ref ref)
{
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]) {
        return &pool->things[ref.index];
    }
    return &pool->things[0];
}

/* ---- Main ---- */

int main(void)
{
    printf("\n=== Lesson 09: Generational IDs ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* ---- Test 1: Add a troll, verify the ref works ---- */
    printf("--- Test 1: Add troll at slot 1 (generation 0) ---\n");

    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    pool.things[troll_ref.index].health = 100.0f;

    printf("  Added TROLL at slot %d, generation %u\n",
           troll_ref.index, troll_ref.generation);
    printf("  troll_ref = { index: %d, generation: %u }\n",
           troll_ref.index, troll_ref.generation);

    thing *t = pool_get_ref(&pool, troll_ref);
    printf("  pool_get_ref(troll_ref): kind=%s, valid=%d\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(t->kind == THING_KIND_TROLL);
    assert(troll_ref.index == 1);
    assert(troll_ref.generation == 0);

    /* ---- Test 2: Remove troll. Generation bumps. ---- */
    printf("\n--- Test 2: Remove troll -- generation bumps ---\n");

    pool_remove(&pool, troll_ref.index);

    printf("  Removed slot %d\n", troll_ref.index);
    printf("  pool.generations[%d] is now %u (was %u in ref)\n",
           troll_ref.index,
           pool.generations[troll_ref.index],
           troll_ref.generation);

    t = pool_get_ref(&pool, troll_ref);
    printf("  pool_get_ref(troll_ref): kind=%s, valid=%d  <-- STALE\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    /* ---- Test 3: Add goblin (reuses slot). Old ref still stale. ---- */
    printf("\n--- Test 3: Goblin spawns in recycled slot ---\n");

    thing_ref goblin_ref = pool_add(&pool, THING_KIND_GOBLIN);
    pool.things[goblin_ref.index].health = 30.0f;

    printf("  Added GOBLIN at slot %d, generation %u\n",
           goblin_ref.index, goblin_ref.generation);
    assert(goblin_ref.index == troll_ref.index);

    printf("  troll_ref  = { index: %d, gen: %u }  <-- STALE\n",
           troll_ref.index, troll_ref.generation);
    printf("  goblin_ref = { index: %d, gen: %u }  <-- VALID\n",
           goblin_ref.index, goblin_ref.generation);

    /* ---- The critical comparison ---- */
    printf("\n--- WITH generation check (SAFE) ---\n");
    t = pool_get_ref(&pool, troll_ref);
    printf("  pool_get_ref(troll_ref): kind=%s, valid=%d\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    printf("  ref.generation=%u vs pool.generations[%d]=%u --> %s\n",
           troll_ref.generation, troll_ref.index,
           pool.generations[troll_ref.index],
           troll_ref.generation == pool.generations[troll_ref.index]
               ? "MATCH" : "MISMATCH -> nil");
    assert(!thing_is_valid(t));

    printf("\n--- WITHOUT generation check (UNSAFE) ---\n");
    t = pool_get_no_gen(&pool, troll_ref);
    printf("  pool_get_no_gen(troll_ref): kind=%s, valid=%d\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    if (thing_is_valid(t)) {
        printf("  WRONG ENTITY! Got %s (health=%.0f) instead of the dead troll!\n",
               thing_kind_name(t->kind), t->health);
    }
    assert(thing_is_valid(t));
    assert(t->kind == THING_KIND_GOBLIN);

    /* ---- Test 4: Goblin ref works fine ---- */
    printf("\n--- Test 4: Goblin ref works correctly ---\n");
    t = pool_get_ref(&pool, goblin_ref);
    printf("  pool_get_ref(goblin_ref): kind=%s, health=%.0f\n",
           thing_kind_name(t->kind), t->health);
    assert(t->kind == THING_KIND_GOBLIN);
    assert(t->health == 30.0f);

    /* ---- Test 5: Multiple recycles ---- */
    printf("\n--- Test 5: Multiple recycles of the same slot ---\n");

    thing_ref saved_refs[4];
    const char *names[] = {"TROLL", "GOBLIN", "ITEM", "PLAYER"};
    thing_kind kinds[] = {THING_KIND_TROLL, THING_KIND_GOBLIN,
                          THING_KIND_ITEM, THING_KIND_PLAYER};

    pool_remove(&pool, goblin_ref.index);

    for (int i = 0; i < 4; i++) {
        saved_refs[i] = pool_add(&pool, kinds[i]);
        printf("  Add %-7s at slot %d, gen %u\n",
               names[i], saved_refs[i].index, saved_refs[i].generation);
        if (i < 3) {
            pool_remove(&pool, saved_refs[i].index);
            printf("  Remove slot %d (gen bumped to %u)\n",
                   saved_refs[i].index,
                   pool.generations[saved_refs[i].index]);
        }
    }

    printf("\n  Checking all saved refs:\n");
    for (int i = 0; i < 4; i++) {
        t = pool_get_ref(&pool, saved_refs[i]);
        printf("    %s (gen=%u): kind=%s, valid=%d",
               names[i], saved_refs[i].generation,
               thing_kind_name(t->kind), thing_is_valid(t));
        if (i == 3) {
            printf("  <-- VALID (current)\n");
            assert(thing_is_valid(t));
        } else {
            printf("  <-- STALE\n");
            assert(!thing_is_valid(t));
        }
    }

    /* ---- Test 6: Nil ref ---- */
    printf("\n--- Test 6: Nil ref is always safe ---\n");

    thing_ref nil_ref = {0, 0};
    t = pool_get_ref(&pool, nil_ref);
    printf("  pool_get_ref({0, 0}): kind=%s, valid=%d  <-- nil\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    /* ---- Test 7: Summary ---- */
    printf("\n--- Summary ---\n\n");
    printf("  +---------------------+--------------------+---------------------+\n");
    printf("  |                     | WITH gen check     | WITHOUT gen check   |\n");
    printf("  +---------------------+--------------------+---------------------+\n");
    printf("  | Stale ref returns:  | nil (SAFE)         | WRONG entity (BUG!) |\n");
    printf("  | Silent corruption?  | NO                 | YES                 |\n");
    printf("  | Cost per lookup:    | 1 extra comparison | nothing             |\n");
    printf("  | Cost per slot:      | 4 bytes (uint32_t) | nothing             |\n");
    printf("  +---------------------+--------------------+---------------------+\n\n");

    /* Suppress unused warnings */
    (void)pool_get;

    printf("=== Lesson 09 PASSED ===\n\n");
    return 0;
}
```
