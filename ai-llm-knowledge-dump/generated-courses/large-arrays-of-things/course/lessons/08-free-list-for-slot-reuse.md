# Lesson 08: Free List for Slot Reuse

## What we're building

An intrusive free list inside the pool. When a thing is removed, its slot joins a linked list of free slots. When a new thing is added, the pool checks the free list FIRST before bumping `next_empty`. Dead things store the "next free" index in their `next_sibling` field -- the field is unused since the thing is dead, so we repurpose it for free list bookkeeping. By the end of this lesson, removed slots get recycled instead of wasted, and the pool only grows `next_empty` when the free list is empty.

## What you'll learn

- The slot waste problem from lesson 07 (and why it matters for real games)
- What a "linked list" is and how it works (from scratch, for JS devs)
- The intrusive free list trick: storing bookkeeping data inside dead things
- Updating `pool_remove` to push slots onto the free list
- Updating `pool_add` to pop slots from the free list before bumping `next_empty`
- Side-by-side comparison: with vs without free list
- Rapid spawn/kill stress test showing the dramatic difference

## Prerequisites

- Lesson 07 (thing_pool, pool_add, pool_get, pool_remove)

---

## Step 1 -- The problem: slot waste

Let's look at what happened at the end of lesson 07. We added 5 things, removed 2, then added 1 more:

```
After: add 5, remove slots 2 and 4, add 1 more

Index:    [0]    [1]      [2]     [3]      [4]     [5]     [6]     [7]
Things:   NIL    PLAYER   ----    GOBLIN   ----    ITEM    ITEM    ----
Used:     false  true     false   true     false   true    true    false
                                                                   ^
                                                                   next_empty = 7
```

Slots 2 and 4 are dead. Empty. Sitting right there. But the new item went into slot 6 instead. We burned through a fresh slot while two perfectly good recycled slots gathered dust.

This is a real problem. In a game with lots of spawning and killing -- bullets firing, enemies dying, particles fading -- you're constantly adding and removing. If every add bumps `next_empty` forward, you'll hit `MAX_THINGS` fast, even though the array is mostly empty.

Let me show you the math. Say your game has 100 enemies alive at any time, but they're constantly being killed and replaced. After 1000 enemies have lived and died:

```
Without free list:
  next_empty = 1001  (1 player + 1000 enemies that came and went)
  Slots 1-1000 allocated, ~900 of them dead
  Only 101 actually alive
  Pool is nearly FULL (if MAX_THINGS = 1024)

With free list:
  next_empty = ~101  (never advanced past the initial batch)
  Removed slots get recycled
  New enemies reuse old slots
  Pool stays compact
```

> **Why?** Think of a database connection pool in Node.js (like `pg-pool`). When a request finishes, its connection goes back into the pool for reuse. You don't create a new TCP connection for every query -- you recycle. Same idea here: removed slots go back into the "available" pool for reuse. If you didn't recycle connections, your database server would hit `max_connections` and refuse new queries, even though 90% of the connections are idle.

We need a way to remember which slots are dead so `pool_add` can reuse them.

---

## Step 2 -- The idea: a linked list of dead slots

Before I show you the code, I need to explain what a linked list is. If you've only worked in JavaScript, you might never have built one from scratch.

A linked list is a chain of elements where each element points to the next one. In JS, you'd write it like this:

```javascript
// JS linked list
const node1 = { value: "A", next: node2 };
const node2 = { value: "B", next: node3 };
const node3 = { value: "C", next: null };

// Traversal: start at node1, follow .next until null
// node1 -> node2 -> node3 -> null
```

Each node has a `value` and a `next` pointer. You start at the head, follow `next` to the next node, and keep going until `next` is `null`.

Our free list works the same way, with one twist: **the nodes ARE the dead slots.** We don't allocate new node objects. We reuse the dead thing's own fields to store the "next" pointer.

Here's the key insight: when a thing is dead, ALL its fields are meaningless. The thing has been zeroed by `memset`. Its `kind`, `parent`, `first_child`, `next_sibling`, `x`, `y`, `health` -- all zero, all unused. So we can **repurpose** one of those fields to store free list bookkeeping.

Specifically: we'll use the dead thing's `next_sibling` field to store the index of the NEXT free slot. The pool struct gets a new field -- `first_free` -- that points to the head of this chain:

```
first_free -> dead slot A -> dead slot B -> dead slot C -> 0 (end)
              (next_sibling   (next_sibling   (next_sibling
               = B's index)    = C's index)    = 0)
```

This is called an **intrusive** free list because the list pointers live INSIDE the data they're managing, not in a separate data structure.

> **Why?** In JS, you'd keep a `Set<number>` of free indices, or an array that you push/pop from. That works but costs extra memory. Our intrusive free list costs ZERO extra memory -- we're reusing fields that are sitting there doing nothing. The dead things are already allocated (they're part of the flat array). We just write a number into one of their fields. No `new`, no `malloc`, no garbage collector involvement.
>
> This is the standard technique in systems programming. The Linux kernel uses intrusive linked lists everywhere. Every game engine uses them. You have dead memory sitting there, might as well make it useful.

**Why `next_sibling`?** We could use ANY field. But `next_sibling` is natural:

1. It's a `thing_idx` -- already the right type for storing an array index.
2. It has the right semantics: "the next one in a chain."
3. The dead thing's `next_sibling` was zeroed by `memset` in `pool_remove`, so there's no stale data to worry about.

---

## Step 3 -- Setting up the file

Let's build this from scratch. Create a new file called `lesson_08.c`. Start with the same foundation as lesson 07, but with one addition to the pool struct: `first_free`.

```c
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
```

These are the same headers from lesson 07. Same purpose: `assert` for testing, `stdbool` for `bool`, `stdint` for `uint32_t`, `stdio` for `printf`, `string` for `memset`.

```c
#define MAX_THINGS 512
```

> **Why 512?** In lesson 07 we used 64, which was fine for a small demo. For this lesson we'll do a stress test with 100+ entities to show the free list's impact. 512 gives us headroom. In a real game, you'd use 1024 or more.

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

Same as lesson 07. The kind enum with `NIL = 0`, and `thing_idx` as an integer alias.

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

Same thing struct. The important field for this lesson is `next_sibling` -- when a thing is alive, it links to a sibling in the intrusive tree. When a thing is dead, we'll repurpose this same field to link to the next free slot.

Now the pool struct -- this is where the change happens:

```c
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    thing_idx next_empty;
    thing_idx first_free;        /* NEW: head of the free list */
} thing_pool;
```

**`thing_idx first_free`** -- This is the index of the first dead slot in the free list chain. When it's 0, the free list is empty (since 0 is the nil sentinel and never a valid free slot). When it's, say, 7, that means slot 7 is dead and `things[7].next_sibling` points to the NEXT dead slot.

> **Why does 0 mean "empty"?** It's the same trick we use everywhere in this system. Index 0 is the nil sentinel -- it means "nothing." A free list head of 0 means "no free slots." The zero-init architecture keeps paying off: when `pool_init` zeros everything, `first_free` becomes 0 automatically, which correctly means "empty free list."

Let's add the helper function (same as lesson 07):

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
```

And `pool_init` -- identical to lesson 07 except now `first_free` also gets zeroed by `memset`:

```c
static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
    /* pool->first_free is already 0 from memset (empty free list) */
}
```

And `pool_get` -- unchanged from lesson 07:

```c
static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}
```

And `thing_is_valid` -- unchanged:

```c
static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}
```

**Build checkpoint.** Your file has the includes, constant, types, helper, `pool_init`, `pool_get`, and `thing_is_valid`. We still need `pool_remove` and `pool_add` -- those are the ones that change. Let's write them.

---

## Step 4 -- Updating pool_remove to build the free list

In lesson 07, `pool_remove` just zeroed the slot and marked it unused. Now we add TWO lines: push the dead slot onto the free list.

```c
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    /* Mark unused and zero the thing data. */
    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    /* Push this slot onto the free list. */
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}
```

The first four lines are identical to lesson 07. The new part is the last two lines. Let me explain them carefully.

**`pool->things[idx].next_sibling = pool->first_free;`** -- The dead thing's `next_sibling` field now stores whatever was previously at the head of the free list. If the free list was empty, `first_free` is 0 (end of list). If the free list already had some slots, this new dead slot now links to them.

> **Why?** "But wait -- we just zeroed the thing with `memset`! Isn't `next_sibling` zero?" Yes, `memset` set it to zero. Now we're writing a NEW value into it. The `memset` happens first (clean slate), then we write the free list link. The order matters.
>
> **Common mistake:** Doing it the other way around -- setting the free list link BEFORE zeroing. If you do `pool->things[idx].next_sibling = pool->first_free` and THEN `memset(...)`, the memset wipes out the link you just wrote. Boom, free list corrupted.

**`pool->first_free = idx;`** -- This slot is now the head of the free list.

These two lines together form a **push** operation on a singly-linked list:

```
new_node.next = head
head = new_node
```

This is O(1) -- no scanning, no searching, no reallocation. Just write two integers.

> **Why?** In JS, pushing onto the front of a linked list is:
> ```javascript
> newNode.next = head;
> head = newNode;
> ```
> Exact same pattern. Two pointer updates. In C we're writing to integer fields instead of object references, but the logic is identical.

Let's walk through a complete example with ASCII diagrams. Starting from an empty pool:

**Remove step 1: Start with 5 things, remove slot 2**

```
BEFORE removing slot 2:

first_free = 0  (empty free list)

Index:    [0]    [1]      [2]     [3]      [4]     [5]
Things:   NIL    PLAYER   TROLL   GOBLIN   TROLL   ITEM
Used:     false  true     true    true     true    true

                 next_empty = 6


AFTER pool_remove(pool, 2):

  1. used[2] = false
  2. memset(&things[2], 0, sizeof(thing))   <- all fields zeroed
  3. things[2].next_sibling = first_free    <- things[2].next_sibling = 0
  4. first_free = 2

first_free = 2 -----> [2]
                       next_sibling = 0  (end of list)

Index:    [0]    [1]      [2]          [3]      [4]     [5]
Things:   NIL    PLAYER   (dead)       GOBLIN   TROLL   ITEM
Used:     false  true     false        true     true    true
                          next_sib=0

Free list chain:  first_free(2) -> 0 (end)
```

Slot 2 is dead. Its `next_sibling` stores 0 (end of list). `first_free` points to slot 2.

**Remove step 2: Remove slot 4**

```
BEFORE removing slot 4:

first_free = 2

AFTER pool_remove(pool, 4):

  1. used[4] = false
  2. memset(&things[4], 0, sizeof(thing))
  3. things[4].next_sibling = first_free    <- things[4].next_sibling = 2
  4. first_free = 4

first_free = 4 -----> [4] -----> [2] -----> 0 (end)
                       next_sib=2  next_sib=0

Index:    [0]    [1]      [2]          [3]      [4]          [5]
Things:   NIL    PLAYER   (dead)       GOBLIN   (dead)       ITEM
Used:     false  true     false        true     false        true
                          next_sib=0            next_sib=2

Free list chain:  first_free(4) -> [4] -> [2] -> 0 (end)
```

Slot 4's `next_sibling` stores 2 (the previous `first_free`). `first_free` is now 4. The chain is: 4 -> 2 -> 0 (end).

This is **LIFO** (Last In, First Out) -- like a stack. The most recently freed slot is the first one we'll reuse. That's good for CPU cache: the most recently used memory is most likely still in the CPU's fast cache memory.

> **Why?** LIFO means "Last In, First Out." It's like a stack of plates: the last plate you put on top is the first one you take off. We push dead slots onto the front of the free list, and we pop from the front too. Both operations are O(1).
>
> In JS, `Array.push()` and `Array.pop()` are LIFO on the end of the array. Our free list is LIFO on the head of a linked list. Same idea, different data structure.

**Remove step 3: Remove slot 3**

```
AFTER pool_remove(pool, 3):

first_free = 3 -----> [3] -----> [4] -----> [2] -----> 0 (end)
                       next_sib=4  next_sib=2  next_sib=0

Index:    [0]    [1]      [2]          [3]          [4]          [5]
Things:   NIL    PLAYER   (dead)       (dead)       (dead)       ITEM
Used:     false  true     false        false        false        true
                          next_sib=0   next_sib=4   next_sib=2

Free list chain:  first_free(3) -> [3] -> [4] -> [2] -> 0 (end)
```

Three dead slots, chained together. The free list remembers all of them.

Let's add a temporary `main()` to verify the remove works. Add this to your file:

```c
int main(void)
{
    thing_pool pool;
    pool_init(&pool);

    /* Add 5 things */
    thing_idx s1 = pool_add_with_freelist(&pool, THING_KIND_PLAYER);
    thing_idx s2 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    thing_idx s3 = pool_add_with_freelist(&pool, THING_KIND_GOBLIN);
    thing_idx s4 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    thing_idx s5 = pool_add_with_freelist(&pool, THING_KIND_ITEM);

    printf("Added 5 things. next_empty = %d, first_free = %d\n",
           pool.next_empty, pool.first_free);

    /* Remove slots 2 and 4 */
    pool_remove(&pool, s2);
    printf("Removed slot %d. first_free = %d\n", s2, pool.first_free);

    pool_remove(&pool, s4);
    printf("Removed slot %d. first_free = %d\n", s4, pool.first_free);

    /* Walk the free list */
    printf("Free list chain: ");
    thing_idx current = pool.first_free;
    while (current != 0) {
        printf("[%d]", current);
        current = pool.things[current].next_sibling;
        if (current != 0) printf(" -> ");
    }
    printf(" -> 0 (end)\n");

    (void)s1; (void)s3; (void)s5;
    return 0;
}
```

Wait -- we haven't written `pool_add_with_freelist` yet. Let me do that next. For now, keep this `main()` in mind; we'll use it once both functions are ready.

---

## Step 5 -- Updating pool_add to check the free list first

Here's the lesson 07 `pool_add` for reference:

```c
/* Lesson 07 version -- NO free list */
static thing_idx pool_add_no_freelist(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) {
        return 0;
    }

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}
```

This always bumps `next_empty`. It never checks the free list. Dead slots are ignored forever.

Here's the new version:

```c
/* Lesson 08 version -- WITH free list */
static thing_idx pool_add_with_freelist(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    /* Step 1: Try the free list first (recycled slots). */
    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    }
    /* Step 2: No free slots -- use the never-used region. */
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    /* Step 3: Pool is full. */
    else {
        return 0;
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}
```

The new part is Step 1. Let me walk through it line by line.

**`thing_idx slot = 0;`** -- Start with 0 (nil). If we can't find a slot, we return 0.

**`if (pool->first_free != 0)`** -- Is the free list non-empty? `first_free = 0` means empty (since 0 is the nil sentinel and never a valid free slot). If it's non-zero, there's at least one dead slot we can reuse.

**`slot = pool->first_free;`** -- Take the slot at the head of the free list. This is the most recently freed slot.

**`pool->first_free = pool->things[slot].next_sibling;`** -- Pop the head. Advance `first_free` to whatever the dead thing was pointing to. This is the classic **pop** operation on a linked list:

```
slot = head
head = head.next
```

If this was the LAST free slot, its `next_sibling` was 0, so `first_free` becomes 0 -- free list is empty. The next add will fall through to Step 2 (`next_empty`).

> **Why?** In JS, popping from a linked list is:
> ```javascript
> const slot = head;
> head = head.next;
> ```
> Identical logic. One assignment to grab the value, one assignment to advance the head. O(1).

**`else if (pool->next_empty < MAX_THINGS)`** -- No free slots available. Fall back to the never-used region, same as lesson 07.

**`else { return 0; }`** -- Both the free list AND the fresh region are exhausted. Pool is genuinely full. Return 0 (nil).

After getting a slot (from either source), the rest is the same as lesson 07: zero the slot, set the kind, mark it used.

> **Why `memset` even for recycled slots?** The dead slot already has `next_sibling` set to a free list pointer. If we don't zero it, the new thing would have a `next_sibling` that points to some random slot -- corrupting the intrusive tree. `memset` wipes the free list link along with everything else, giving us a clean slate.

**Why check the free list first?** Two reasons:

1. **Memory efficiency.** Recycled slots are in the low-index region, which we've already touched. They're likely still in the CPU's cache. Fresh slots at `next_empty` might be memory we've never accessed -- pulling them into cache for the first time is slower.

2. **Pool longevity.** If we always use `next_empty` first, the pool fills up linearly regardless of removals. Checking the free list first means we only advance `next_empty` when we truly need virgin slots.

Let's trace through the add-from-free-list process:

```
BEFORE pool_add(GOBLIN):

first_free = 4 -----> [4] -----> [2] -----> 0 (end)
                       next_sib=2  next_sib=0

Index:    [0]    [1]      [2]          [3]      [4]          [5]
Things:   NIL    PLAYER   (dead)       GOBLIN   (dead)       ITEM
Used:     false  true     false        true     false        true

next_empty = 6


pool_add_with_freelist(pool, THING_KIND_GOBLIN):

  1. first_free != 0? YES (first_free = 4)
  2. slot = 4
  3. first_free = things[4].next_sibling = 2   (advance the list)
  4. memset(&things[4], 0, sizeof(thing))       (clean slate)
  5. things[4].kind = THING_KIND_GOBLIN
  6. used[4] = true

AFTER:

first_free = 2 -----> [2] -----> 0 (end)
                       next_sib=0

Index:    [0]    [1]      [2]          [3]      [4]      [5]
Things:   NIL    PLAYER   (dead)       GOBLIN   GOBLIN   ITEM
Used:     false  true     false        true     true     true

next_empty = 6  (UNCHANGED! We reused a dead slot!)
```

Slot 4 got RECYCLED. `next_empty` didn't move. The new goblin lives in the space the old troll vacated.

```
One more add -- pool_add(ITEM):

  1. first_free != 0? YES (first_free = 2)
  2. slot = 2
  3. first_free = things[2].next_sibling = 0   (free list now EMPTY)
  4. memset, set kind, set used

AFTER:

first_free = 0  (empty!)

Index:    [0]    [1]      [2]     [3]      [4]      [5]
Things:   NIL    PLAYER   ITEM    GOBLIN   GOBLIN   ITEM
Used:     false  true     true    true     true     true

next_empty = 6  (STILL unchanged!)
```

Both dead slots reused. `next_empty` never moved. Zero waste.

```
One MORE add -- pool_add(TROLL):

  1. first_free != 0? NO (first_free = 0, free list empty)
  2. next_empty < MAX_THINGS? YES (next_empty = 6)
  3. slot = 6, next_empty = 7

AFTER:

Index:    [0]    [1]      [2]     [3]      [4]      [5]     [6]
Things:   NIL    PLAYER   ITEM    GOBLIN   GOBLIN   ITEM    TROLL
Used:     false  true     true    true     true     true    true

next_empty = 7
first_free = 0 (empty)
```

Only NOW does `next_empty` advance -- when there are zero recycled slots available.

**Build checkpoint.** Add both `pool_add_no_freelist` and `pool_add_with_freelist` to your file. We need both because we're going to compare them side by side. Now let's write the test.

---

## Step 6 -- Putting it together: free list in action

Now add the `main()` function. We'll run three tests: demonstrate the free list chain, show reuse happening, and then stress test.

```c
int main(void)
{
    printf("=== Lesson 08: Free List for Slot Reuse ===\n\n");

    thing_pool pool;
    pool_init(&pool);
```

> **Why?** Same as always: declare the pool on the stack, zero-initialize it. `first_free` starts at 0 (empty free list) automatically.

```c
    /* --- Step 1: Add 5 things --- */
    printf("--- Step 1: Add 5 things ---\n");
    thing_idx s1 = pool_add_with_freelist(&pool, THING_KIND_PLAYER);
    thing_idx s2 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    thing_idx s3 = pool_add_with_freelist(&pool, THING_KIND_GOBLIN);
    thing_idx s4 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    thing_idx s5 = pool_add_with_freelist(&pool, THING_KIND_ITEM);

    printf("  Added: PLAYER(%d) TROLL(%d) GOBLIN(%d) TROLL(%d) ITEM(%d)\n",
           s1, s2, s3, s4, s5);
    printf("  next_empty = %d, first_free = %d\n\n",
           pool.next_empty, pool.first_free);
```

At this point, the free list is empty because we haven't removed anything yet. All 5 adds went to fresh slots.

```
After Step 1:

Index:    [0]    [1]      [2]     [3]      [4]     [5]
Things:   NIL    PLAYER   TROLL   GOBLIN   TROLL   ITEM
Used:     false  true     true    true     true    true

next_empty = 6, first_free = 0 (empty)
```

```c
    /* --- Step 2: Remove slots 2 and 4 --- */
    printf("--- Step 2: Remove slots %d and %d ---\n", s2, s4);
    pool_remove(&pool, s2);
    printf("  Removed slot %d. first_free = %d\n", s2, pool.first_free);

    pool_remove(&pool, s4);
    printf("  Removed slot %d. first_free = %d\n", s4, pool.first_free);

    /* Walk and print the free list */
    printf("  Free list: ");
    thing_idx current = pool.first_free;
    while (current != 0) {
        printf("[%d]", current);
        current = pool.things[current].next_sibling;
        if (current != 0) printf(" -> ");
    }
    printf(" -> 0 (end)\n\n");
```

> **Why?** We walk the free list by starting at `first_free` and following `next_sibling` links until we hit 0 (end). This is the standard linked list traversal: `while (current != null) { ... current = current.next; }`. In C, our "null" is 0.

```
After Step 2:

Index:    [0]    [1]      [2]          [3]      [4]          [5]
Things:   NIL    PLAYER   (dead)       GOBLIN   (dead)       ITEM
Used:     false  true     false        true     false        true
                          next_sib=0            next_sib=2

next_empty = 6, first_free = 4
Free list: [4] -> [2] -> 0 (end)
```

```c
    /* --- Step 3: Add 2 new things -- should reuse freed slots --- */
    printf("--- Step 3: Add 2 new things (should reuse freed slots) ---\n");
    thing_idx n1 = pool_add_with_freelist(&pool, THING_KIND_GOBLIN);
    printf("  Added GOBLIN at slot %d", n1);
    if (n1 == s4 || n1 == s2) printf(" (RECYCLED!)");
    printf("\n");

    thing_idx n2 = pool_add_with_freelist(&pool, THING_KIND_ITEM);
    printf("  Added ITEM   at slot %d", n2);
    if (n2 == s4 || n2 == s2) printf(" (RECYCLED!)");
    printf("\n");

    printf("  next_empty = %d (should still be %d -- no growth!)\n\n",
           pool.next_empty, 6);

    assert(n1 == s4 || n1 == s2);
    assert(n2 == s4 || n2 == s2);
    assert(n1 != n2);
    assert(pool.next_empty == 6);
```

Both new things reused the dead slots. `next_empty` didn't move.

```
After Step 3:

Index:    [0]    [1]      [2]     [3]      [4]      [5]
Things:   NIL    PLAYER   ITEM    GOBLIN   GOBLIN   ITEM
Used:     false  true     true    true     true     true

next_empty = 6 (unchanged!), first_free = 0 (empty, both slots reused)
```

```c
    /* --- Step 4: Add one more -- free list empty, must use fresh slot --- */
    printf("--- Step 4: Add one more (free list empty) ---\n");
    thing_idx n3 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    printf("  Added TROLL at slot %d (fresh slot, next_empty advanced)\n", n3);
    printf("  next_empty = %d\n\n", pool.next_empty);

    assert(n3 == 6);
    assert(pool.next_empty == 7);

    (void)s1; (void)s3; (void)s5;
```

With the free list exhausted, we fall back to `next_empty`.

```
After Step 4:

Index:    [0]    [1]      [2]     [3]      [4]      [5]     [6]
Things:   NIL    PLAYER   ITEM    GOBLIN   GOBLIN   ITEM    TROLL
Used:     false  true     true    true     true     true    true

next_empty = 7 (advanced by 1), first_free = 0 (empty)
```

---

## Step 7 -- Comparing with and without free list

Now the dramatic comparison. We'll run the same scenario twice: add 10 things, remove 5, add 5 more. Once without the free list, once with.

Add this to your `main()`:

```c
    /* ═══════════════════════════════════════════════ */
    printf("=== Comparison: WITH vs WITHOUT free list ===\n\n");

    /* --- WITHOUT free list --- */
    thing_pool pool_no_fl;
    pool_init(&pool_no_fl);

    thing_idx no_fl_slots[10];
    for (int i = 0; i < 10; i++) {
        no_fl_slots[i] = pool_add_no_freelist(&pool_no_fl, THING_KIND_TROLL);
    }
    printf("  [No FL] Added 10 things. next_empty = %d\n",
           pool_no_fl.next_empty);

    /* Remove 5 (even-indexed: slots 1, 3, 5, 7, 9) */
    for (int i = 0; i < 10; i += 2) {
        pool_remove(&pool_no_fl, no_fl_slots[i]);
    }
    printf("  [No FL] Removed 5. next_empty = %d\n", pool_no_fl.next_empty);

    /* The no-freelist version can't use the free list that pool_remove built,
     * so we clear it to simulate the lesson-07 behavior. */
    pool_no_fl.first_free = 0;

    for (int i = 0; i < 5; i++) {
        pool_add_no_freelist(&pool_no_fl, THING_KIND_GOBLIN);
    }
    printf("  [No FL] Added 5 more. next_empty = %d\n\n",
           pool_no_fl.next_empty);
```

> **Why clear `first_free`?** `pool_remove` always builds the free list (because that's how we wrote it). But `pool_add_no_freelist` ignores the free list. To show pure "no free list" behavior, we manually zero `first_free` so the removed slots are truly forgotten.

```c
    /* --- WITH free list --- */
    thing_pool pool_fl;
    pool_init(&pool_fl);

    thing_idx fl_slots[10];
    for (int i = 0; i < 10; i++) {
        fl_slots[i] = pool_add_with_freelist(&pool_fl, THING_KIND_TROLL);
    }
    printf("  [With FL] Added 10 things. next_empty = %d\n",
           pool_fl.next_empty);

    for (int i = 0; i < 10; i += 2) {
        pool_remove(&pool_fl, fl_slots[i]);
    }
    printf("  [With FL] Removed 5. next_empty = %d\n", pool_fl.next_empty);

    for (int i = 0; i < 5; i++) {
        pool_add_with_freelist(&pool_fl, THING_KIND_GOBLIN);
    }
    printf("  [With FL] Added 5 more. next_empty = %d\n\n",
           pool_fl.next_empty);
```

```c
    /* --- Count living and wasted --- */
    int no_fl_alive = 0, fl_alive = 0;
    for (int i = 1; i < pool_no_fl.next_empty; i++) {
        if (pool_no_fl.used[i]) no_fl_alive++;
    }
    for (int i = 1; i < pool_fl.next_empty; i++) {
        if (pool_fl.used[i]) fl_alive++;
    }

    printf("  Results:\n");
    printf("  +------------------+---------+----------+\n");
    printf("  | Metric           | No FL   | With FL  |\n");
    printf("  +------------------+---------+----------+\n");
    printf("  | next_empty       | %-7d | %-8d |\n",
           pool_no_fl.next_empty, pool_fl.next_empty);
    printf("  | Living things    | %-7d | %-8d |\n",
           no_fl_alive, fl_alive);
    printf("  | Wasted gaps      | %-7d | %-8d |\n",
           pool_no_fl.next_empty - 1 - no_fl_alive,
           pool_fl.next_empty - 1 - fl_alive);
    printf("  +------------------+---------+----------+\n\n");

    assert(pool_no_fl.next_empty > pool_fl.next_empty);
```

Expected comparison:

```
  Results:
  +------------------+---------+----------+
  | Metric           | No FL   | With FL  |
  +------------------+---------+----------+
  | next_empty       | 16      | 11       |
  | Living things    | 10      | 10       |
  | Wasted gaps      | 5       | 0        |
  +------------------+---------+----------+
```

Without the free list: `next_empty` grew to 16 (10 original + 5 new + the nil slot). 5 dead gaps sitting unused.

With the free list: `next_empty` stayed at 11. All 5 new things reused the freed slots. Zero waste.

---

## Step 8 -- Rapid spawn/kill stress test

Now the real test. 100 things added, 80 removed, 80 added again. This simulates a game where enemies are constantly spawning and dying.

```c
    /* ═══════════════════════════════════════════════ */
    printf("=== Stress test: Add 100, remove 80, add 80 ===\n\n");

    /* --- Without free list --- */
    thing_pool stress_no_fl;
    pool_init(&stress_no_fl);

    thing_idx stress_no_fl_slots[100];
    for (int i = 0; i < 100; i++) {
        stress_no_fl_slots[i] = pool_add_no_freelist(
            &stress_no_fl, THING_KIND_TROLL);
    }
    for (int i = 0; i < 80; i++) {
        pool_remove(&stress_no_fl, stress_no_fl_slots[i]);
    }
    stress_no_fl.first_free = 0;  /* simulate no free list */
    for (int i = 0; i < 80; i++) {
        pool_add_no_freelist(&stress_no_fl, THING_KIND_GOBLIN);
    }

    printf("  [No FL] Add 100, remove 80, add 80:\n");
    printf("    next_empty = %d\n", stress_no_fl.next_empty);
```

```c
    /* --- With free list --- */
    thing_pool stress_fl;
    pool_init(&stress_fl);

    thing_idx stress_fl_slots[100];
    for (int i = 0; i < 100; i++) {
        stress_fl_slots[i] = pool_add_with_freelist(
            &stress_fl, THING_KIND_TROLL);
    }
    for (int i = 0; i < 80; i++) {
        pool_remove(&stress_fl, stress_fl_slots[i]);
    }
    for (int i = 0; i < 80; i++) {
        pool_add_with_freelist(&stress_fl, THING_KIND_GOBLIN);
    }

    printf("  [With FL] Add 100, remove 80, add 80:\n");
    printf("    next_empty = %d\n\n", stress_fl.next_empty);
```

```c
    /* --- Summary --- */
    printf("  Without free list: next_empty = %d (burned through %d slots!)\n",
           stress_no_fl.next_empty, stress_no_fl.next_empty - 1);
    printf("  With free list:    next_empty = %d (recycled %d slots)\n\n",
           stress_fl.next_empty, 80);

    assert(stress_no_fl.next_empty == 181);
    assert(stress_fl.next_empty == 101);

    printf("=== Lesson 08 PASSED ===\n");
    return 0;
}
```

Expected output for the stress test:

```
  [No FL] Add 100, remove 80, add 80:
    next_empty = 181
  [With FL] Add 100, remove 80, add 80:
    next_empty = 101

  Without free list: next_empty = 181 (burned through 180 slots!)
  With free list:    next_empty = 101 (recycled 80 slots)
```

Without the free list, `next_empty` reached 181. That's 180 slots touched, with only 100 alive. With `MAX_THINGS = 512`, we've burned through a third of the pool.

With the free list, `next_empty` is still 101. All 80 new entities reused dead slots. The pool didn't grow at all. In a game with thousands of add/remove cycles per minute, this is the difference between a pool that stays healthy and one that runs out of space.

---

## Build and run

Full compile command:

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_08 lesson_08.c
./lesson_08
```

## Expected result

```
=== Lesson 08: Free List for Slot Reuse ===

--- Step 1: Add 5 things ---
  Added: PLAYER(1) TROLL(2) GOBLIN(3) TROLL(4) ITEM(5)
  next_empty = 6, first_free = 0

--- Step 2: Remove slots 2 and 4 ---
  Removed slot 2. first_free = 2
  Removed slot 4. first_free = 4
  Free list: [4] -> [2] -> 0 (end)

--- Step 3: Add 2 new things (should reuse freed slots) ---
  Added GOBLIN at slot 4 (RECYCLED!)
  Added ITEM   at slot 2 (RECYCLED!)
  next_empty = 6 (should still be 6 -- no growth!)

--- Step 4: Add one more (free list empty) ---
  Added TROLL at slot 6 (fresh slot, next_empty advanced)
  next_empty = 7

=== Comparison: WITH vs WITHOUT free list ===

  [No FL] Added 10 things. next_empty = 11
  [No FL] Removed 5. next_empty = 11
  [No FL] Added 5 more. next_empty = 16

  [With FL] Added 10 things. next_empty = 11
  [With FL] Removed 5. next_empty = 11
  [With FL] Added 5 more. next_empty = 11

  Results:
  +------------------+---------+----------+
  | Metric           | No FL   | With FL  |
  +------------------+---------+----------+
  | next_empty       | 16      | 11       |
  | Living things    | 10      | 10       |
  | Wasted gaps      | 5       | 0        |
  +------------------+---------+----------+

=== Stress test: Add 100, remove 80, add 80 ===

  [No FL] Add 100, remove 80, add 80:
    next_empty = 181
  [With FL] Add 100, remove 80, add 80:
    next_empty = 101

  Without free list: next_empty = 181 (burned through 180 slots!)
  With free list:    next_empty = 101 (recycled 80 slots)

=== Lesson 08 PASSED ===
```

## Common mistakes

1. **Zeroing AFTER setting the free list link.** If you write `pool->things[idx].next_sibling = pool->first_free` and THEN `memset(...)`, the memset wipes out the link. Always zero FIRST, then set the link:
   ```c
   /* WRONG order: */
   pool->things[idx].next_sibling = pool->first_free;  /* set link */
   memset(&pool->things[idx], 0, sizeof(thing));        /* OOPS -- zeroed it */

   /* RIGHT order: */
   memset(&pool->things[idx], 0, sizeof(thing));        /* clean slate */
   pool->things[idx].next_sibling = pool->first_free;   /* set link */
   ```

2. **Forgetting to zero the slot in pool_add.** When you reuse a slot from the free list, the slot's `next_sibling` contains the old free list pointer. If you don't `memset` it, the new thing has a `next_sibling` that points to some random slot. The intrusive tree is now corrupted. Always zero before setting the kind.

3. **Checking `next_empty` before `first_free`.** The free list should be checked FIRST. If you check `next_empty` first, you'll burn through fresh slots while perfectly good recycled slots sit unused. The whole point of the free list is to be checked first.

4. **Using `first_free == -1` as "empty."** We use 0, not -1. Zero is the nil sentinel, and it's the natural "nothing" value in our system. Using -1 would be inconsistent with everything else and would require special initialization (memset gives you 0, not -1).

5. **Forgetting that `pool_remove` already handles the guards.** The `idx <= 0` and `!pool->used[idx]` checks in `pool_remove` mean you can't accidentally push the nil sentinel onto the free list, and you can't double-push a dead slot. The free list stays clean automatically.

## Exercises

1. **Walk the free list.** After removing 5 things, write a loop that follows the free list chain and prints each index. Verify it has exactly 5 entries and ends at 0.

2. **Track the high-water mark.** Add a `max_used` field to the pool. Update it in `pool_add` if the new slot index is higher than `max_used`. After the stress test, compare `max_used` with and without the free list.

3. **FIFO instead of LIFO.** Implement a FIFO free list (first freed = first reused). You'll need a `last_free` pointer in addition to `first_free`. Push to the tail, pop from the head. Compare cache behavior with LIFO. (Hint: LIFO is almost always better for cache.)

4. **Count recycled vs fresh allocations.** Add two counters to the pool: `recycled_count` and `fresh_count`. Increment the appropriate one in `pool_add_with_freelist`. After the stress test, print both. You should see that most allocations after the initial batch are recycled.

## Connection to your work

If you've ever used a connection pool in a web server (`pg-pool`, `redis`, database connection pools), you've used this pattern. Connections are "things." When a request finishes, the connection goes back to the pool. When a new request comes in, the pool hands out a recycled connection instead of creating a new one from scratch.

The free list is the mechanism behind that recycling. In our case, the "connections" are array slots, and the "recycling" is tracking dead slots for reuse. Same concept, different domain.

This pattern also shows up in memory allocators (like `malloc` and `free`). When you `free()` a block of memory, many allocators put it on a free list. When you `malloc()` again, they check the free list first. We've just built a tiny version of the same thing, specialized for our fixed-size thing slots.

## What's next

We now have a pool that can add, remove, get, and recycle slots. But there's a subtle bug lurking. Imagine: you save a reference to "slot 5, which is a troll." Then slot 5 gets removed and reused for a goblin. You look up slot 5 using your saved reference... and get the goblin. You think it's the troll. Your code does troll-specific logic on a goblin. Chaos.

Lesson 09 fixes this with **generational IDs**. Each slot has a generation counter. When a slot is recycled, the counter increments. Your saved reference includes both the index AND the generation. If the generation doesn't match, the reference is stale -- you know the original thing was removed.

---

## Complete file: lesson_08.c

Copy-paste this entire file. Compile with `gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_08 lesson_08.c`. Run with `./lesson_08`.

```c
/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_08.c -- Free List for Slot Reuse                            */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  Pool grows: adds first_free field.                          */
/*          Dead things store next_free in next_sibling (intrusive).    */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_08 lesson_08.c */
/* ══════════════════════════════════════════════════════════════════════ */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 512

/* ══════════════════════════════════════════════════════ */
/*                      Types                             */
/* ══════════════════════════════════════════════════════ */

typedef enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

typedef int thing_idx;

typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;   /* DOUBLE DUTY: sibling link when alive,
                                  free list link when dead */
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;

typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    thing_idx next_empty;
    thing_idx first_free;        /* head of intrusive free list (0 = empty) */
} thing_pool;

/* ══════════════════════════════════════════════════════ */
/*                  Helper: kind name                     */
/* ══════════════════════════════════════════════════════ */

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

/* ══════════════════════════════════════════════════════ */
/*                   Pool Operations                      */
/* ══════════════════════════════════════════════════════ */

static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
    /* pool->first_free is already 0 from memset (empty free list) */
}

static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}

static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

/* Remove: zero the slot, then push it onto the free list. */
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    /* Push this slot onto the free list (LIFO). */
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}

/* Add WITHOUT free list: always bumps next_empty (lesson 07 behavior). */
static thing_idx pool_add_no_freelist(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) {
        return 0;
    }

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

/* Add WITH free list: checks first_free before next_empty. */
static thing_idx pool_add_with_freelist(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    /* Step 1: Try the free list first (recycled slots). */
    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    }
    /* Step 2: No free slots -- use the never-used region. */
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    /* Step 3: Pool is full. */
    else {
        return 0;
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== Lesson 08: Free List for Slot Reuse ===\n\n");

    /* Suppress unused warning */
    (void)thing_kind_name;
    (void)pool_get;
    (void)thing_is_valid;

    thing_pool pool;
    pool_init(&pool);

    /* --- Step 1: Add 5 things --- */
    printf("--- Step 1: Add 5 things ---\n");
    thing_idx s1 = pool_add_with_freelist(&pool, THING_KIND_PLAYER);
    thing_idx s2 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    thing_idx s3 = pool_add_with_freelist(&pool, THING_KIND_GOBLIN);
    thing_idx s4 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    thing_idx s5 = pool_add_with_freelist(&pool, THING_KIND_ITEM);

    printf("  Added: PLAYER(%d) TROLL(%d) GOBLIN(%d) TROLL(%d) ITEM(%d)\n",
           s1, s2, s3, s4, s5);
    printf("  next_empty = %d, first_free = %d\n\n",
           pool.next_empty, pool.first_free);

    /* --- Step 2: Remove slots 2 and 4 --- */
    printf("--- Step 2: Remove slots %d and %d ---\n", s2, s4);
    pool_remove(&pool, s2);
    printf("  Removed slot %d. first_free = %d\n", s2, pool.first_free);

    pool_remove(&pool, s4);
    printf("  Removed slot %d. first_free = %d\n", s4, pool.first_free);

    /* Walk the free list */
    printf("  Free list: ");
    thing_idx current = pool.first_free;
    while (current != 0) {
        printf("[%d]", current);
        current = pool.things[current].next_sibling;
        if (current != 0) printf(" -> ");
    }
    printf(" -> 0 (end)\n\n");

    /* --- Step 3: Add 2 new things --- */
    printf("--- Step 3: Add 2 new things (should reuse freed slots) ---\n");
    thing_idx n1 = pool_add_with_freelist(&pool, THING_KIND_GOBLIN);
    printf("  Added GOBLIN at slot %d", n1);
    if (n1 == s4 || n1 == s2) printf(" (RECYCLED!)");
    printf("\n");

    thing_idx n2 = pool_add_with_freelist(&pool, THING_KIND_ITEM);
    printf("  Added ITEM   at slot %d", n2);
    if (n2 == s4 || n2 == s2) printf(" (RECYCLED!)");
    printf("\n");

    printf("  next_empty = %d (should still be %d -- no growth!)\n\n",
           pool.next_empty, 6);

    assert(n1 == s4 || n1 == s2);
    assert(n2 == s4 || n2 == s2);
    assert(n1 != n2);
    assert(pool.next_empty == 6);

    /* --- Step 4: Add one more --- */
    printf("--- Step 4: Add one more (free list empty) ---\n");
    thing_idx n3 = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    printf("  Added TROLL at slot %d (fresh slot, next_empty advanced)\n", n3);
    printf("  next_empty = %d\n\n", pool.next_empty);

    assert(n3 == 6);
    assert(pool.next_empty == 7);

    (void)s1; (void)s3; (void)s5;

    /* ═══════════════════════════════════════════════ */
    printf("=== Comparison: WITH vs WITHOUT free list ===\n\n");

    /* --- WITHOUT free list --- */
    thing_pool pool_no_fl;
    pool_init(&pool_no_fl);

    thing_idx no_fl_slots[10];
    for (int i = 0; i < 10; i++) {
        no_fl_slots[i] = pool_add_no_freelist(&pool_no_fl, THING_KIND_TROLL);
    }
    printf("  [No FL] Added 10 things. next_empty = %d\n",
           pool_no_fl.next_empty);

    for (int i = 0; i < 10; i += 2) {
        pool_remove(&pool_no_fl, no_fl_slots[i]);
    }
    printf("  [No FL] Removed 5. next_empty = %d\n", pool_no_fl.next_empty);

    pool_no_fl.first_free = 0;  /* simulate no free list */

    for (int i = 0; i < 5; i++) {
        pool_add_no_freelist(&pool_no_fl, THING_KIND_GOBLIN);
    }
    printf("  [No FL] Added 5 more. next_empty = %d\n\n",
           pool_no_fl.next_empty);

    /* --- WITH free list --- */
    thing_pool pool_fl;
    pool_init(&pool_fl);

    thing_idx fl_slots[10];
    for (int i = 0; i < 10; i++) {
        fl_slots[i] = pool_add_with_freelist(&pool_fl, THING_KIND_TROLL);
    }
    printf("  [With FL] Added 10 things. next_empty = %d\n",
           pool_fl.next_empty);

    for (int i = 0; i < 10; i += 2) {
        pool_remove(&pool_fl, fl_slots[i]);
    }
    printf("  [With FL] Removed 5. next_empty = %d\n", pool_fl.next_empty);

    for (int i = 0; i < 5; i++) {
        pool_add_with_freelist(&pool_fl, THING_KIND_GOBLIN);
    }
    printf("  [With FL] Added 5 more. next_empty = %d\n\n",
           pool_fl.next_empty);

    /* --- Results table --- */
    int no_fl_alive = 0, fl_alive = 0;
    for (int i = 1; i < pool_no_fl.next_empty; i++) {
        if (pool_no_fl.used[i]) no_fl_alive++;
    }
    for (int i = 1; i < pool_fl.next_empty; i++) {
        if (pool_fl.used[i]) fl_alive++;
    }

    printf("  Results:\n");
    printf("  +------------------+---------+----------+\n");
    printf("  | Metric           | No FL   | With FL  |\n");
    printf("  +------------------+---------+----------+\n");
    printf("  | next_empty       | %-7d | %-8d |\n",
           pool_no_fl.next_empty, pool_fl.next_empty);
    printf("  | Living things    | %-7d | %-8d |\n",
           no_fl_alive, fl_alive);
    printf("  | Wasted gaps      | %-7d | %-8d |\n",
           pool_no_fl.next_empty - 1 - no_fl_alive,
           pool_fl.next_empty - 1 - fl_alive);
    printf("  +------------------+---------+----------+\n\n");

    assert(pool_no_fl.next_empty > pool_fl.next_empty);

    /* ═══════════════════════════════════════════════ */
    printf("=== Stress test: Add 100, remove 80, add 80 ===\n\n");

    /* --- Without free list --- */
    thing_pool stress_no_fl;
    pool_init(&stress_no_fl);

    thing_idx stress_no_fl_slots[100];
    for (int i = 0; i < 100; i++) {
        stress_no_fl_slots[i] = pool_add_no_freelist(
            &stress_no_fl, THING_KIND_TROLL);
    }
    for (int i = 0; i < 80; i++) {
        pool_remove(&stress_no_fl, stress_no_fl_slots[i]);
    }
    stress_no_fl.first_free = 0;  /* simulate no free list */
    for (int i = 0; i < 80; i++) {
        pool_add_no_freelist(&stress_no_fl, THING_KIND_GOBLIN);
    }

    printf("  [No FL] Add 100, remove 80, add 80:\n");
    printf("    next_empty = %d\n", stress_no_fl.next_empty);

    /* --- With free list --- */
    thing_pool stress_fl;
    pool_init(&stress_fl);

    thing_idx stress_fl_slots[100];
    for (int i = 0; i < 100; i++) {
        stress_fl_slots[i] = pool_add_with_freelist(
            &stress_fl, THING_KIND_TROLL);
    }
    for (int i = 0; i < 80; i++) {
        pool_remove(&stress_fl, stress_fl_slots[i]);
    }
    for (int i = 0; i < 80; i++) {
        pool_add_with_freelist(&stress_fl, THING_KIND_GOBLIN);
    }

    printf("  [With FL] Add 100, remove 80, add 80:\n");
    printf("    next_empty = %d\n\n", stress_fl.next_empty);

    printf("  Without free list: next_empty = %d (burned through %d slots!)\n",
           stress_no_fl.next_empty, stress_no_fl.next_empty - 1);
    printf("  With free list:    next_empty = %d (recycled %d slots)\n\n",
           stress_fl.next_empty, 80);

    assert(stress_no_fl.next_empty == 181);
    assert(stress_fl.next_empty == 101);

    printf("=== Lesson 08 PASSED ===\n");
    return 0;
}
```
