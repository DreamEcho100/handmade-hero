# Lesson 10: Pool Iterator -- Clean Iteration Over Living Things

## What we're building

A `thing_iter` struct with four functions: `iter_begin`, `iter_next`, `iter_valid`, `iter_get`. A clean for-loop pattern that visits every living thing in the pool, automatically skipping dead slots. No more raw `for` loops that reach into pool internals.

By the end of this lesson, iterating over all living entities looks like this:

```c
for (thing_iter it = iter_begin(&pool);
     iter_valid(&it);
     iter_next(&it))
{
    thing *t = iter_get(&it);
    t->x += t->dx;
}
```

This is the final piece of the pool system. After this lesson, the pool is complete.

## What you'll learn

- Why raw for-loops that dig into pool internals are a maintenance nightmare
- The `thing_iter` struct (a pool pointer + current index)
- `iter_begin`, `iter_next`, `iter_valid`, `iter_get` -- four functions that compose into a clean loop
- The complete pool system summary: everything we've built in lessons 07-10

## Prerequisites

- Lesson 09 (generational IDs). The pool is now feature-complete -- we just need a clean way to walk it.

---

## Step 1 -- The raw loop problem

Right now, if you want to visit every living thing in the pool, you write something like this:

```c
for (int i = 1; i < pool.next_empty; i++) {
    if (!pool.used[i]) continue;
    thing *t = &pool.things[i];
    /* do something with t */
}
```

This works. But it's bad. Let me count the ways.

**Problem 1: It exposes pool internals.** Your game code has to know that indexing starts at 1, that `used[]` is a boolean array, that `next_empty` is the upper bound, and that `things[]` is the data array. If you ever change the pool's internal representation -- say, switch `used[]` from a boolean array to a bitfield, or change the starting index, or swap to a different allocation scheme -- every single loop in your codebase breaks.

**Problem 2: It's error-prone.** Start at 0 instead of 1? You process the nil sentinel. Forget the `used[i]` check? You process dead entities. Use `MAX_THINGS` instead of `next_empty`? You scan hundreds of empty slots. Each mistake is subtle and silent.

**Problem 3: It's repetitive.** Every system in the game that needs to iterate -- physics, rendering, collision, AI -- repeats the same boilerplate:

```c
/* Physics update -- raw loop */
for (int i = 1; i < pool.next_empty; i++) {
    if (!pool.used[i]) continue;
    pool.things[i].x += pool.things[i].dx;
}

/* Render -- raw loop (same boilerplate) */
for (int i = 1; i < pool.next_empty; i++) {
    if (!pool.used[i]) continue;
    draw_thing(&pool.things[i]);
}

/* Collision -- raw loop (same boilerplate AGAIN) */
for (int i = 1; i < pool.next_empty; i++) {
    if (!pool.used[i]) continue;
    check_collision(&pool, i);
}
```

Three copies of the same five-line pattern. Copy-paste it wrong once, and you have a bug. And it's not a bug that crashes -- it's a bug that silently processes the nil sentinel, or silently skips the first entity, or silently scans a thousand empty slots. The kind of bug you don't notice until your game is running and "things aren't quite right."

Here's every possible mistake in the raw loop, and what happens:

```
Correct loop:
  for (int i = 1; i < pool.next_empty; i++) {
      if (!pool.used[i]) continue;

Mistake 1: Start at 0 instead of 1.
  for (int i = 0; ...)   <-- processes nil sentinel at slot 0

Mistake 2: Use <= instead of <.
  for (... i <= pool.next_empty; ...)  <-- reads one past the used region

Mistake 3: Use MAX_THINGS instead of next_empty.
  for (... i < MAX_THINGS; ...)  <-- scans 1024 slots when 5 are alive

Mistake 4: Forget the used[] check.
  (no continue)  <-- processes dead entities (zeroed data)

Mistake 5: Check things[i].kind instead of used[i].
  if (pool.things[i].kind == THING_KIND_NIL) continue;
  <-- breaks when free list stores data in dead things' fields
```

> **Why?** In JS, you'd never write `for (let i = 0; i < array.length; i++) { if (array[i] === null) continue; ... }` when you could use `for (const item of livingEntities)`. The `for...of` loop hides the iteration details. We want the same thing in C -- hide the "start at 1, check used, skip dead" details behind a clean API.

```
The mess we want to clean up:

  Game code currently reaches INTO the pool:

    +-------------------------------------------+
    |  game_update()                            |
    |                                           |
    |  for (i = 1; i < pool.next_empty; i++)    |  <-- knows about next_empty
    |      if (!pool.used[i]) continue;         |  <-- knows about used[]
    |      t = &pool.things[i];                 |  <-- knows about things[]
    +-------------------------------------------+

  After adding an iterator:

    +-------------------------------------------+
    |  game_update()                            |
    |                                           |
    |  for (it = begin(&pool);                  |
    |       valid(&it);                         |  <-- knows NOTHING about
    |       next(&it))                          |      pool internals
    |      t = get(&it);                        |
    +-------------------------------------------+
```

To see why this matters, picture a pool with gaps from removal. This is the kind of array the iterator must handle:

```
Pool state (gaps from removal):
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ [0] │ [1] │ [2] │ [3] │ [4] │ [5] │ [6] │ [7] │
│ NIL │PLYR │     │TRLL │     │ITEM │TRLL │     │
│     │used │dead │used │dead │used │used │dead │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
Iterator must visit: [1], [3], [5], [6]  — skip: [0], [2], [4], [7]
```

The iterator's job is to hand you exactly those four living things, in order, without you ever having to think about the dead slots between them.

---

## Step 2 -- The thing_iter struct

Let's start building. Open `lesson_10.c` and set up the foundation. Everything from lesson 09 carries forward -- the same types, the same pool operations. I'll show the complete type setup, and then we'll add the iterator on top.

```c
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_THINGS 64
```

All the types from lesson 09 (enum, idx, ref, thing, pool) stay the same. Add them now:

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
```

Now the new type. The iterator is a tiny struct with two fields:

```c
typedef struct thing_iter {
    thing_pool *pool;       /* which pool we're iterating */
    thing_idx   current;    /* which slot we're pointing at */
} thing_iter;
```

That's it. Two fields. A pointer to the pool, and the current slot index.

```
thing_iter layout:

  +-----------+-----------+
  | pool      | current   |
  | (pointer) | (int)     |
  +-----------+-----------+
  | 8 bytes   | 4 bytes   |    (on a 64-bit system)
  +-----------+-----------+

  It's a cursor: "I'm pointing at slot N in this pool."
```

The iterator doesn't OWN anything. It doesn't allocate memory. It doesn't copy the pool. It's just a cursor -- a bookmark that says "I'm currently looking at slot N." You advance it, check if it's valid, and dereference it to get the thing.

> **Why?** In JS terms, this is like the iterator object you get from `Symbol.iterator`. When you write `for (const item of myArray)`, JS creates an iterator `{ next() { return { value, done } } }` behind the scenes. Our `thing_iter` is the C equivalent -- explicit instead of hidden, struct instead of object, four named functions instead of one `next()` method.

> **Why a struct and not just a function?** Because iteration has state -- the current position. A function can't remember where it left off between calls (not without `static` variables, which would break if you had two iterators running at the same time). A struct carries the state explicitly, on the stack, and you can have as many simultaneous iterators as you want.

Now add the helper functions and pool operations from lesson 09 (these are identical):

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

static thing_ref pool_add(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    } else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    } else {
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
```

**Compile checkpoint.** Save and compile (with an empty main) to make sure everything still works before we add the iterator functions.

---

## Step 3 -- iter_begin: start at the first living thing

The first iterator function creates an iterator and positions it at the first living slot:

```c
static thing_iter iter_begin(thing_pool *pool)
{
    thing_iter it = {pool, 1};   /* start at index 1 (skip nil sentinel at 0) */

    /* Advance to the first used slot */
    while (it.current < MAX_THINGS && !pool->used[it.current]) {
        it.current++;
    }

    return it;
}
```

Start at index 1 (not 0 -- remember, slot 0 is the nil sentinel). Then scan forward, skipping dead slots, until we find one that's alive.

Why scan? Because slot 1 might be dead. Slots 1, 2, and 3 might all be dead. The first living thing might be at slot 47. We need to skip past all the dead slots.

```
Example pool:

  Index: [0]  [1]   [2]   [3]   [4]     [5]   [6]    [7]  ...
  Used:  ---  false false false  true    false  true   false

  iter_begin scans:
    Start at 1.
    used[1]? No. Advance to 2.
    used[2]? No. Advance to 3.
    used[3]? No. Advance to 4.
    used[4]? Yes. STOP.

  Returns: thing_iter { pool, current = 4 }
                                     ^
                                     |
                            iterator points HERE
```

Visualizing the iterator position after `iter_begin`:

```
Iterator begins at first used slot (skips nil at 0, dead at 1-3):
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ [0] │ [1] │ [2] │ [3] │ [4] │ [5] │ [6] │
│ NIL │     │     │     │PLYR │     │TRLL │
└─────┴─────┴─────┴─────┴──▲──┴─────┴─────┘
                            │
                       iter.current = 4 (first used slot)
```

Let me trace through a second example -- what happens when the pool is completely empty:

```
Empty pool (no living things):

  Index: [0]  [1]   [2]   [3]   ...  [63]
  Used:  ---  false false false       false

  iter_begin scans:
    Start at 1.
    used[1]? No. Advance to 2.
    used[2]? No. Advance to 3.
    ...
    used[63]? No. Advance to 64.
    64 >= MAX_THINGS (64). STOP.

  Returns: thing_iter { pool, current = 64 }

  iter_valid({ pool, 64 }):
    64 < MAX_THINGS?  NO.
    Returns false.

  The for-loop body NEVER executes. Zero things visited.
  No crash, no special case, no "check if pool is empty first."
```

This is important: the iterator gracefully handles empty pools, all-dead pools, and single-entity pools without any special-case code. The while loop in `iter_begin` and `iter_next` handles it all.

---

## Step 4 -- iter_next: advance to the next living thing

After processing the current slot, we need to advance to the next living one:

```c
static void iter_next(thing_iter *it)
{
    it->current++;   /* move past the current slot */

    /* Skip dead slots */
    while (it->current < MAX_THINGS && !it->pool->used[it->current]) {
        it->current++;
    }
}
```

Same scanning logic as `iter_begin`, but starting from `current + 1` instead of index 1. The key detail: we ALWAYS increment at least once (`it->current++` before the while loop). This guarantees we move forward -- without it, calling `iter_next` repeatedly would get stuck on the current slot forever.

```
Iterator at slot 4. Call iter_next:

  Index: [0]  [1]   [2]   [3]   [4]     [5]   [6]    [7]  ...
  Used:  ---  false false false  true    false  true   false

  Step 1: it->current++ (4 becomes 5)
  Step 2: while loop:
    used[5]? No. Advance to 6.
    used[6]? Yes. STOP.

  Iterator now: { pool, current = 6 }
```

Visualizing the skip:

```
iter_next skips dead slots:
  current=4 → increment → 5 (dead, skip) → 6 (alive, stop!)
┌─────┬─────┬─────┬─────┬─────┐
│ [4] │ [5] │ [6] │ [7] │ ... │
│PLYR │     │TRLL │     │     │
└─────┴─────┴──▲──┴─────┴─────┘
                │
           iter.current = 6
```

What happens when we're at the last living thing?

```
Iterator at slot 6 (the last living thing). Call iter_next:

  Index: [0]  ...  [6]    [7]   [8]   ...  [63]
  Used:  ---       true   false false       false

  Step 1: it->current++ (6 becomes 7)
  Step 2: while loop:
    used[7]? No. Advance to 8.
    used[8]? No. Advance to 9.
    ...
    used[63]? No. Advance to 64.
    64 >= MAX_THINGS (64). STOP.

  Iterator now: { pool, current = 64 }
  iter_valid: 64 < 64?  NO. Iteration is DONE.
```

If there are no more living things, `current` advances past `MAX_THINGS` and the iterator becomes invalid. The for-loop condition (`iter_valid`) catches this and the loop ends.

> **Why does it take a pointer (`thing_iter *it`) instead of returning a new value?** Because we're modifying the iterator in place. In C, if we returned a new `thing_iter`, the caller would need to capture it: `it = iter_next(it)`. Taking a pointer lets us just call `iter_next(&it)` and the change happens automatically. This also matches how the `for` loop's increment clause works -- `iter_next(&it)` is a side-effect, not a value expression.

> **Why?** In JS, `iterator.next()` returns a new object `{ value, done }` every time. That works because JS has garbage collection -- the old object gets cleaned up. In C, we don't want to return new structs from `next` because it's called on every iteration. Modifying in place (via pointer) is cleaner and makes the intent obvious: "advance this iterator."

---

## Step 5 -- iter_valid: are we still in bounds?

The simplest function of the four:

```c
static bool iter_valid(const thing_iter *it)
{
    return it->current > 0 && it->current < MAX_THINGS;
}
```

If `current` has gone past `MAX_THINGS`, iteration is done. If `current` is somehow 0 (which shouldn't happen since we start at 1, but safety first), we also stop.

That's it. One comparison.

---

## Step 6 -- iter_get: dereference to get the thing

Returns a pointer to the thing at the current slot:

```c
static thing *iter_get(thing_iter *it)
{
    if (iter_valid(it)) {
        return &it->pool->things[it->current];
    }
    return &it->pool->things[0];  /* nil sentinel */
}
```

If the iterator is valid, return the thing. If it's exhausted (past the end), return the nil sentinel.

> **Why?** Consistent with everything else in this pool system -- no NULL pointers, ever. If someone calls `iter_get` after iteration is done, they get nil, not a crash.

Here's the complete picture of how these four functions work together:

```
The four iterator functions:

  iter_begin(&pool)     Create iterator, find first living slot
       |
       v
  +---------+
  |  check  |<------+
  |  valid  |       |
  +----+----+       |
       |            |
    YES |        iter_next(&it)
       v            ^
  +---------+       |
  |   get   |-------+
  |  thing  |  (process, then advance)
  +---------+
       |
    NO |
       v
     done
```

---

## Step 7 -- Using the iterator in a clean loop

Now we compose these four functions into a `for` loop. This is the pattern you'll use everywhere.

```c
for (thing_iter it = iter_begin(&pool);
     iter_valid(&it);
     iter_next(&it))
{
    thing *t = iter_get(&it);
    /* t is guaranteed to be a living thing */
    printf("slot %d: %s at (%.0f, %.0f)\n",
           it.current, thing_kind_name(t->kind), t->x, t->y);
}
```

Each part of the `for` loop maps to one iterator function:

```
for (
    thing_iter it = iter_begin(&pool);    <-- INIT: create iterator at first living slot
    iter_valid(&it);                      <-- CONDITION: is iterator in bounds?
    iter_next(&it)                        <-- ADVANCE: move to next living slot
)
{
    thing *t = iter_get(&it);             <-- BODY: get the thing
}
```

> **Why?** This is the C equivalent of the JS iterator protocol. In JS, you implement `[Symbol.iterator]()` returning `{ next() { return { value, done }; } }`. In C, you split that into four explicit function calls. The semantics are identical -- create a cursor, check if it's done, get the value, advance.

Let's compare the raw loop and the iterator loop side by side:

```c
/* RAW LOOP (exposes pool internals): */
for (int i = 1; i < pool.next_empty; i++) {
    if (!pool.used[i]) continue;
    thing *t = &pool.things[i];
    t->x += t->dx;
}

/* ITERATOR LOOP (clean API): */
for (thing_iter it = iter_begin(&pool);
     iter_valid(&it);
     iter_next(&it))
{
    thing *t = iter_get(&it);
    t->x += t->dx;
}
```

The iterator loop is one line longer. The point is NOT to save lines -- it's to **hide implementation details**. Let me show you why that matters:

```
Scenario: You decide to change used[] from a bool array
to a bitfield (to save memory).

RAW LOOP IMPACT:
  Every raw loop in the codebase breaks.
  You grep for "pool.used[" and find 47 occurrences.
  You rewrite all 47 to use bit operations.
  You miss one. It compiles (bitfield is still an integer).
  It silently does the wrong thing at runtime.

ITERATOR LOOP IMPACT:
  You change iter_begin and iter_next (2 functions).
  All 47 loops in the codebase keep working.
  Zero changes outside the iterator implementation.
```

The iterator is a boundary. On one side: the pool's internal representation (how things are stored, how liveness is tracked). On the other side: game code that just wants to visit living things. The iterator translates between them. Change one side, the other doesn't notice.

**Compile checkpoint.** Everything compiles. Now let's test.

---

## Step 8 -- Testing: iterate with gaps

Here's the main function. We'll add 8 things, remove 3 to create gaps, and show that the iterator correctly skips the dead slots.

```c
int main(void)
{
    printf("\n=== Lesson 10: Pool Iterator ===\n\n");

    thing_pool pool;
    pool_init(&pool);
```

**Setup: add 8 things, remove 3 to create gaps.**

```c
    /* ---- Setup: add 8 things ---- */
    printf("--- Setup: Add 8 things ---\n");

    thing_kind kinds[8] = {
        THING_KIND_PLAYER, THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_PLAYER
    };
    thing_ref refs[8];

    for (int i = 0; i < 8; i++) {
        refs[i] = pool_add(&pool, kinds[i]);
        thing *t = pool_get(&pool, refs[i].index);
        t->x = (float)(i * 10);
        t->y = (float)(i * 5);
        printf("  [%d] %s at (%.0f, %.0f)\n",
               refs[i].index, thing_kind_name(t->kind), t->x, t->y);
    }
```

Now remove 3 of them (slots 2, 4, 6) to create gaps:

```c
    /* ---- Remove 3 to create gaps ---- */
    printf("\n--- Remove slots 2, 4, 6 (create gaps) ---\n");

    pool_remove(&pool, refs[1].index);   /* slot 2: TROLL */
    pool_remove(&pool, refs[3].index);   /* slot 4: ITEM */
    pool_remove(&pool, refs[5].index);   /* slot 6: GOBLIN */

    printf("  Removed: slot %d (TROLL), slot %d (ITEM), slot %d (GOBLIN)\n",
           refs[1].index, refs[3].index, refs[5].index);
```

Let's print the pool state so we can see the gaps:

```c
    printf("\n  Pool state:\n");
    for (int i = 0; i < 9; i++) {
        printf("    [%d] kind=%-7s used=%d\n",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
    }
```

The pool should look like this:

```
  Pool state:
    [0] kind=NIL     used=0      <-- nil sentinel
    [1] kind=PLAYER  used=1
    [2] kind=NIL     used=0      <-- gap (was TROLL)
    [3] kind=GOBLIN  used=1
    [4] kind=NIL     used=0      <-- gap (was ITEM)
    [5] kind=TROLL   used=1
    [6] kind=NIL     used=0      <-- gap (was GOBLIN)
    [7] kind=ITEM    used=1
    [8] kind=PLAYER  used=1
```

Five living things, three dead gaps. This is the classic scenario the iterator needs to handle.

**Test 1: Raw loop (the gross way).**

```c
    /* ---- Test 1: Raw loop ---- */
    printf("\n--- Test 1: Raw loop (the gross way) ---\n");
    int raw_count = 0;
    for (int i = 1; i < pool.next_empty; i++) {
        if (!pool.used[i]) continue;
        thing *t = &pool.things[i];
        printf("  slot %d: %s at (%.0f, %.0f)\n",
               i, thing_kind_name(t->kind), t->x, t->y);
        raw_count++;
    }
    printf("  Raw loop visited %d things\n", raw_count);
```

**Test 2: Iterator (the clean way).**

```c
    /* ---- Test 2: Iterator (the clean way) ---- */
    printf("\n--- Test 2: Iterator (the clean way) ---\n");
    int iter_count = 0;
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("  slot %d: %s at (%.0f, %.0f)\n",
               it.current, thing_kind_name(t->kind), t->x, t->y);
        iter_count++;
    }
    printf("  Iterator visited %d things\n", iter_count);

    assert(raw_count == iter_count);
    assert(iter_count == 5);
    printf("  Both methods visited the same %d things!\n", iter_count);
```

Both loops visit exactly the same 5 living things. The iterator just wraps the skipping logic so you don't repeat it.

**Test 3: Empty pool -- iterator handles it gracefully.**

```c
    /* ---- Test 3: Empty pool ---- */
    printf("\n--- Test 3: Empty pool iteration ---\n");
    thing_pool empty_pool;
    pool_init(&empty_pool);

    int empty_count = 0;
    for (thing_iter it = iter_begin(&empty_pool);
         iter_valid(&it);
         iter_next(&it))
    {
        empty_count++;
    }
    printf("  Empty pool: visited %d things (expected 0)\n", empty_count);
    assert(empty_count == 0);
```

When the pool is empty, `iter_begin` scans forward and never finds a used slot. The iterator starts exhausted. The loop body never runs. Zero things visited. No crash, no special case.

**Test 4: All-dead pool -- same graceful handling.**

```c
    /* ---- Test 4: All-dead pool ---- */
    printf("\n--- Test 4: All-dead pool iteration ---\n");
    thing_pool dead_pool;
    pool_init(&dead_pool);
    thing_ref temp = pool_add(&dead_pool, THING_KIND_TROLL);
    pool_remove(&dead_pool, temp.index);

    int dead_count = 0;
    for (thing_iter it = iter_begin(&dead_pool);
         iter_valid(&it);
         iter_next(&it))
    {
        dead_count++;
    }
    printf("  All-dead pool: visited %d things (expected 0)\n", dead_count);
    assert(dead_count == 0);
```

A pool where everything has been removed. All slots are dead. Iterator finds nothing. Zero things visited. Correct.

**Test 5: Accessing the slot index during iteration.**

Sometimes you need the slot index, not just the thing pointer. For example, to remove an entity during iteration, or to build a reference to it. The iterator's `current` field is public:

```c
    /* ---- Test 5: Access slot index ---- */
    printf("\n--- Test 5: Access slot index during iteration ---\n");
    printf("  Living things with their slot indices:\n");
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("    slot %d -> %s (health=%.0f)\n",
               it.current, thing_kind_name(t->kind), t->health);
    }
```

The `it.current` field gives you the slot index at any time. You can use it to call `pool_remove`, to build a `thing_ref`, or just for printing.

**Test 6: Iterator-based physics update.**

This is what real game code looks like -- using the iterator for a physics step:

```c
    /* ---- Test 6: Physics update via iterator ---- */
    printf("\n--- Test 6: Iterator-based physics update ---\n");

    /* Give living things velocity */
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        t->dx = 1.0f;
        t->dy = 0.5f;
    }

    printf("  Before update:\n");
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("    slot %d: (%.0f, %.0f)\n", it.current, t->x, t->y);
    }

    /* Run one physics frame */
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        t->x += t->dx;
        t->y += t->dy;
    }

    printf("  After update (dx=1, dy=0.5):\n");
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("    slot %d: (%.0f, %.1f)\n", it.current, t->x, t->y);
    }
```

Three clean loops. Set velocity, then update position, then print. Each uses the exact same iterator pattern. No pool internals visible anywhere.

Close out main:

```c
    /* Suppress unused warnings */
    (void)pool_get_ref;

    printf("\n=== Lesson 10 PASSED ===\n\n");
    return 0;
}
```

---

## Step 9 -- The complete pool system

That's it. The pool system is done. Let's look at everything we've built across lessons 07-10:

```
thing_pool (COMPLETE)
|
|-- things[MAX_THINGS]          Flat array of entities (Lesson 02)
|   +-- things[0]               Nil sentinel: always zero, never crashes (Lesson 06)
|
|-- used[MAX_THINGS]            Boolean liveness per slot (Lesson 07)
|
|-- generations[MAX_THINGS]     Stale-reference detection per slot (Lesson 09)
|
|-- next_empty                  Next never-used slot index (Lesson 07)
|
+-- first_free                  Head of intrusive free list in dead things (Lesson 08)


API (COMPLETE)
|
|-- pool_init(pool)             Zero everything, set next_empty=1 (Lesson 07)
|
|-- pool_add(pool, kind)        Free list first, then next_empty (Lessons 07-08)
|   +-- Returns thing_ref       { index, generation } (Lesson 09)
|
|-- pool_remove(pool, idx)      Mark dead, zero, bump gen, push free list (Lessons 07-09)
|
|-- pool_get(pool, idx)         Raw index access -> thing* or nil (Lesson 07)
|
|-- pool_get_ref(pool, ref)     Generation-checked access -> thing* or nil (Lesson 09)
|
|-- thing_is_valid(t)           kind != THING_KIND_NIL (Lesson 07)
|
|-- iter_begin(pool)            Iterator at first living slot (Lesson 10)
|-- iter_next(it)               Advance to next living slot (Lesson 10)
|-- iter_valid(it)              Is iterator still in bounds? (Lesson 10)
+-- iter_get(it)                Dereference -> thing* or nil (Lesson 10)
```

Every operation is O(1) except iteration (which is O(n) where n is `next_empty`, not MAX_THINGS -- you only scan slots that were ever used). No malloc. No free. No garbage collection. No constructors. No destructors. No virtual dispatch. No hash maps. No search trees.

Just a flat array, some booleans, some integers, and clean APIs.

Let me put the performance characteristics in a table:

```
+-------------------+------+-------------------------------------------+
| Operation         | Cost | Notes                                     |
+-------------------+------+-------------------------------------------+
| pool_init         | O(1) | One memset, one assignment                |
| pool_add          | O(1) | Pop from free list or bump next_empty     |
| pool_remove       | O(1) | Mark dead, zero, bump gen, push free list |
| pool_get          | O(1) | Bounds check + used check                 |
| pool_get_ref      | O(1) | Bounds + used + generation check          |
| thing_is_valid    | O(1) | One comparison                            |
| iter_begin        | O(n) | Scan to first used slot                   |
| iter_next         | O(k) | Scan to next used slot (k = gap size)     |
| Full iteration    | O(n) | Visit every slot up to next_empty         |
+-------------------+------+-------------------------------------------+

  n = next_empty (not MAX_THINGS)
  k = number of dead slots between current and next living slot

  For 1024 entities at 60fps:
    Full iteration scans at most 1024 booleans = 1 KB.
    That's 16 cache lines. Takes microseconds.
    Not a performance concern until you have tens of thousands of entities.
```

> **Why?** If you're coming from a web background where you worry about O(n) on every operation, relax. Scanning 1024 booleans is so fast it doesn't show up in a profiler. The CPU prefetcher handles sequential array scans perfectly. This is one of those cases where the theoretical complexity doesn't matter -- the constant factors are tiny.

---

## Build and run

Save the complete file and compile:

```
gcc -Wall -Wextra -Werror -std=c11 -o lesson_10 lesson_10.c
```

Then run:

```
./lesson_10
```

## Expected output

```
=== Lesson 10: Pool Iterator ===

--- Setup: Add 8 things ---
  [1] PLAYER at (0, 0)
  [2] TROLL at (10, 5)
  [3] GOBLIN at (20, 10)
  [4] ITEM at (30, 15)
  [5] TROLL at (40, 20)
  [6] GOBLIN at (50, 25)
  [7] ITEM at (60, 30)
  [8] PLAYER at (70, 35)

--- Remove slots 2, 4, 6 (create gaps) ---
  Removed: slot 2 (TROLL), slot 4 (ITEM), slot 6 (GOBLIN)

  Pool state:
    [0] kind=NIL     used=0
    [1] kind=PLAYER  used=1
    [2] kind=NIL     used=0
    [3] kind=GOBLIN  used=1
    [4] kind=NIL     used=0
    [5] kind=TROLL   used=1
    [6] kind=NIL     used=0
    [7] kind=ITEM    used=1
    [8] kind=PLAYER  used=1

--- Test 1: Raw loop (the gross way) ---
  slot 1: PLAYER at (0, 0)
  slot 3: GOBLIN at (20, 10)
  slot 5: TROLL at (40, 20)
  slot 7: ITEM at (60, 30)
  slot 8: PLAYER at (70, 35)
  Raw loop visited 5 things

--- Test 2: Iterator (the clean way) ---
  slot 1: PLAYER at (0, 0)
  slot 3: GOBLIN at (20, 10)
  slot 5: TROLL at (40, 20)
  slot 7: ITEM at (60, 30)
  slot 8: PLAYER at (70, 35)
  Iterator visited 5 things
  Both methods visited the same 5 things!

--- Test 3: Empty pool iteration ---
  Empty pool: visited 0 things (expected 0)

--- Test 4: All-dead pool iteration ---
  All-dead pool: visited 0 things (expected 0)

--- Test 5: Access slot index during iteration ---
  Living things with their slot indices:
    slot 1 -> PLAYER (health=0)
    slot 3 -> GOBLIN (health=0)
    slot 5 -> TROLL (health=0)
    slot 7 -> ITEM (health=0)
    slot 8 -> PLAYER (health=0)

--- Test 6: Iterator-based physics update ---
  Before update:
    slot 1: (0, 0)
    slot 3: (20, 10)
    slot 5: (40, 20)
    slot 7: (60, 30)
    slot 8: (70, 35)
  After update (dx=1, dy=0.5):
    slot 1: (1, 0.5)
    slot 3: (21, 10.5)
    slot 5: (41, 20.5)
    slot 7: (61, 30.5)
    slot 8: (71, 35.5)

=== Lesson 10 PASSED ===
```

---

## Common mistakes

**1. Starting at index 0 instead of 1.** The nil sentinel lives at slot 0. If your `iter_begin` starts at 0 and slot 0 happens to pass the `used` check (it shouldn't -- `used[0]` is false), you'd process the nil sentinel. Our implementation starts at 1 and `iter_valid` also checks `> 0`, so this is double-protected.

**2. Forgetting to advance past dead slots.** If you write `iter_next` as just `it->current++` without the while-loop skip, the iterator will land on dead slots. The caller would get nil from `iter_get` (since the slot is unused, `iter_valid` could still return true if the slot is in bounds but dead). You need the skip loop.

**3. Using the thing pointer after removing during iteration.** If you remove a thing inside the loop body, the pointer `t` becomes stale immediately (the slot was zeroed by memset). Don't read from `t` after the remove. The ITERATOR is still fine -- `iter_next` will correctly advance past the now-dead slot -- but the dereferenced pointer is garbage.

```c
for (thing_iter it = iter_begin(&pool);
     iter_valid(&it);
     iter_next(&it))
{
    thing *t = iter_get(&it);
    if (t->health <= 0) {
        pool_remove(&pool, it.current);
        /* t is now INVALID -- don't use it after this line! */
        /* The iterator is fine. iter_next will skip past this slot. */
    }
}
```

**4. Calling `iter_get` without checking `iter_valid`.** If the pool is empty, `iter_begin` returns an exhausted iterator. Calling `iter_get` on it returns nil (safe -- won't crash), but it's still a logic error. Always check `iter_valid` first, which the for-loop pattern does automatically.

---

## Exercises

1. **Count by kind.** Write a function `count_kind(thing_pool *pool, thing_kind kind)` that uses the iterator to count how many living things of a specific kind exist. Test it: add 3 trolls and 2 goblins, remove 1 troll, verify `count_kind(pool, THING_KIND_TROLL)` returns 2.

2. **Find first of kind.** Write `find_first(thing_pool *pool, thing_kind kind)` that returns the `thing_idx` of the first living thing of a given kind, or 0 if not found. Use the iterator.

3. **Reverse iteration.** The current iterator goes forward (index 1 to MAX_THINGS). Write a reverse iterator that starts at the highest used slot and works backward. You'll need a different `begin` (start at `next_empty - 1`, scan backward) and a different `next` (decrement and scan backward).

4. **Removal during iteration.** Add 10 things, then iterate and remove every TROLL. After the loop, iterate again and verify no trolls remain. Make sure the iterator correctly handles gaps created by the removals.

---

## Connection to your work

If you've worked with JS's `for...of`, Python's `for item in collection`, or any language's iterator pattern, this is the C version. The difference is that C makes you build the iterator by hand -- there's no language-level protocol. But the result is the same: a clean abstraction that hides the messy details of "how to walk this data structure" behind a simple begin/valid/next/get interface.

In real game engines, iterators over entity pools are everywhere. Unity's `foreach (var entity in entities)` is exactly this pattern. Unreal's `TActorIterator` is this pattern. The raw data structure might be different (sparse sets, archetypes, bitfield arrays), but the iterator API is always the same: start, check, advance, get.

---

## What's next

The pool system is complete. Lessons 11-12 integrate it into a real game demo with a window, sprites, and input. Lesson 13 reviews the full architecture. Then the labs push the system further: particles, data layout experiments, spatial partitioning.

But the core -- the pool with add, remove, get, safe references, free list, generational IDs, and iteration -- is done. Everything from here builds on top of this foundation.

---

## Complete file: lesson_10.c

```c
/* lesson_10.c -- Pool Iterator + Complete System
 *
 * Compile:
 *   gcc -Wall -Wextra -Werror -std=c11 -o lesson_10 lesson_10.c
 *
 * Run:
 *   ./lesson_10
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

typedef struct thing_iter {
    thing_pool *pool;
    thing_idx   current;
} thing_iter;

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
    } else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    } else {
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

/* ---- Iterator ---- */

static thing_iter iter_begin(thing_pool *pool)
{
    thing_iter it = {pool, 1};
    while (it.current < MAX_THINGS && !pool->used[it.current]) {
        it.current++;
    }
    return it;
}

static void iter_next(thing_iter *it)
{
    it->current++;
    while (it->current < MAX_THINGS && !it->pool->used[it->current]) {
        it->current++;
    }
}

static bool iter_valid(const thing_iter *it)
{
    return it->current > 0 && it->current < MAX_THINGS;
}

static thing *iter_get(thing_iter *it)
{
    if (iter_valid(it)) {
        return &it->pool->things[it->current];
    }
    return &it->pool->things[0];
}

/* ---- Main ---- */

int main(void)
{
    printf("\n=== Lesson 10: Pool Iterator ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* ---- Setup: add 8 things ---- */
    printf("--- Setup: Add 8 things ---\n");

    thing_kind kinds[8] = {
        THING_KIND_PLAYER, THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_PLAYER
    };
    thing_ref refs[8];

    for (int i = 0; i < 8; i++) {
        refs[i] = pool_add(&pool, kinds[i]);
        thing *t = pool_get(&pool, refs[i].index);
        t->x = (float)(i * 10);
        t->y = (float)(i * 5);
        printf("  [%d] %s at (%.0f, %.0f)\n",
               refs[i].index, thing_kind_name(t->kind), t->x, t->y);
    }

    /* ---- Remove 3 to create gaps ---- */
    printf("\n--- Remove slots 2, 4, 6 (create gaps) ---\n");

    pool_remove(&pool, refs[1].index);
    pool_remove(&pool, refs[3].index);
    pool_remove(&pool, refs[5].index);

    printf("  Removed: slot %d (TROLL), slot %d (ITEM), slot %d (GOBLIN)\n",
           refs[1].index, refs[3].index, refs[5].index);

    printf("\n  Pool state:\n");
    for (int i = 0; i < 9; i++) {
        printf("    [%d] kind=%-7s used=%d\n",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
    }

    /* ---- Test 1: Raw loop ---- */
    printf("\n--- Test 1: Raw loop (the gross way) ---\n");
    int raw_count = 0;
    for (int i = 1; i < pool.next_empty; i++) {
        if (!pool.used[i]) continue;
        thing *t = &pool.things[i];
        printf("  slot %d: %s at (%.0f, %.0f)\n",
               i, thing_kind_name(t->kind), t->x, t->y);
        raw_count++;
    }
    printf("  Raw loop visited %d things\n", raw_count);

    /* ---- Test 2: Iterator ---- */
    printf("\n--- Test 2: Iterator (the clean way) ---\n");
    int iter_count = 0;
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("  slot %d: %s at (%.0f, %.0f)\n",
               it.current, thing_kind_name(t->kind), t->x, t->y);
        iter_count++;
    }
    printf("  Iterator visited %d things\n", iter_count);

    assert(raw_count == iter_count);
    assert(iter_count == 5);
    printf("  Both methods visited the same %d things!\n", iter_count);

    /* ---- Test 3: Empty pool ---- */
    printf("\n--- Test 3: Empty pool iteration ---\n");
    thing_pool empty_pool;
    pool_init(&empty_pool);

    int empty_count = 0;
    for (thing_iter it = iter_begin(&empty_pool);
         iter_valid(&it);
         iter_next(&it))
    {
        empty_count++;
    }
    printf("  Empty pool: visited %d things (expected 0)\n", empty_count);
    assert(empty_count == 0);

    /* ---- Test 4: All-dead pool ---- */
    printf("\n--- Test 4: All-dead pool iteration ---\n");
    thing_pool dead_pool;
    pool_init(&dead_pool);
    thing_ref temp = pool_add(&dead_pool, THING_KIND_TROLL);
    pool_remove(&dead_pool, temp.index);

    int dead_count = 0;
    for (thing_iter it = iter_begin(&dead_pool);
         iter_valid(&it);
         iter_next(&it))
    {
        dead_count++;
    }
    printf("  All-dead pool: visited %d things (expected 0)\n", dead_count);
    assert(dead_count == 0);

    /* ---- Test 5: Access slot index ---- */
    printf("\n--- Test 5: Access slot index during iteration ---\n");
    printf("  Living things with their slot indices:\n");
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("    slot %d -> %s (health=%.0f)\n",
               it.current, thing_kind_name(t->kind), t->health);
    }

    /* ---- Test 6: Physics update via iterator ---- */
    printf("\n--- Test 6: Iterator-based physics update ---\n");

    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        t->dx = 1.0f;
        t->dy = 0.5f;
    }

    printf("  Before update:\n");
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("    slot %d: (%.0f, %.0f)\n", it.current, t->x, t->y);
    }

    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        t->x += t->dx;
        t->y += t->dy;
    }

    printf("  After update (dx=1, dy=0.5):\n");
    for (thing_iter it = iter_begin(&pool);
         iter_valid(&it);
         iter_next(&it))
    {
        thing *t = iter_get(&it);
        printf("    slot %d: (%.0f, %.1f)\n", it.current, t->x, t->y);
    }

    /* Suppress unused warnings */
    (void)pool_get_ref;
    (void)thing_is_valid;

    printf("\n=== Lesson 10 PASSED ===\n\n");
    return 0;
}
```
