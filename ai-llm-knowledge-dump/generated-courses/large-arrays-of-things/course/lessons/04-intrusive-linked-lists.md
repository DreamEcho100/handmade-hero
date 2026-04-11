# Lesson 04: Intrusive Linked Lists

## What we're building

A parent-child relationship system where the player can own an inventory of items -- axe, potion, food -- using nothing but integer indices embedded directly in the thing struct. No `std::vector`. No heap allocation. No separate "inventory container." The thing IS the list node.

By the end of this lesson you will add `first_child` and `next_sibling` fields to the thing struct, implement prepend-to-list, iterate-list, and remove-from-list, and run a test that prints:

```
Player inventory: ITEM(3) -> ITEM(2) -> ITEM(1) -> (end)
After removing potion: ITEM(3) -> ITEM(1) -> (end)
```

## What you'll learn

- Why `std::vector<Item>` inside a struct breaks everything Anton cares about
- The intrusive linked list pattern: the node IS the data
- How `first_child` + `next_sibling` form a parent-child hierarchy
- Add-to-list (prepend), iterate-list, remove-from-list -- all O(1) or O(n) in the right places
- Why an item can only be in ONE list (structural loot-duplication prevention)
- The DOM `firstChild`/`nextSibling` parallel you already know

## Prerequisites

- Lesson 02: fat struct with kind enum, static array, `pool_add()`
- Lesson 03: `thing_idx` as integer index, `parent` field

---

## Step 1 -- The problem with containers inside structs

Let's say your player picks up an axe. Where does the axe live?

In a web app, you would do something like this:

```js
// JavaScript: an entity has an inventory array
const player = {
  kind: 'player',
  x: 0, y: 0,
  health: 100,
  inventory: []   // <-- dynamic array of items
};

player.inventory.push(axe);
player.inventory.push(potion);
```

Clean. Simple. The language manages the memory for you. Now here is what the C++ equivalent looks like:

```cpp
// C++: std::vector inside a struct
struct Thing {
    ThingKind kind;
    float x, y;
    float health;
    std::vector<Thing*> children;  // <-- HERE IS THE PROBLEM
};
```

This seems reasonable until you think about what `std::vector` actually does behind the scenes.

> **Anton says:** "The moment you put a vector inside your entity struct, you've lost. You can't memset it. You can't memcpy it. You can't just blast it to zero and call it dead. You need constructors, destructors, copy operators... you've invited the entire C++ runtime into your data."

Here are the concrete problems:

**Problem 1: You can't memset anymore.** Our entire architecture from lessons 02-03 is built on the idea that `memset(&thing, 0, sizeof(thing))` resets a thing to a valid nil state. A `std::vector` has internal pointers, size counters, and capacity tracking. Zeroing those bytes corrupts it. You need to call the destructor first, then the constructor. Now every "kill an entity" needs cleanup code.

**Problem 2: You can't memcpy anymore.** Want to snapshot the game state? Copy the whole pool? With plain-old-data structs you can `memcpy` the entire array in one shot. With vectors inside, you get double-frees because two vectors think they own the same heap memory.

**Problem 3: Each entity now owns a separate heap allocation.** Your 1024 things are contiguous in the array -- good for cache. But each one's children list is a separate malloc'd buffer somewhere else in memory. You've scattered your data across the heap. Every time you iterate a parent's children, you're chasing a pointer to who-knows-where.

**Problem 4: The vector can be in multiple entities.** Nothing stops you from doing:

```cpp
player.children.push_back(&axe);
chest.children.push_back(&axe);   // axe is now in TWO inventories
```

Congratulations, you just duplicated loot. Players love that bug. You don't.

```
BEFORE: std::vector approach (scattered heap allocations)
=========================================================

things[] array (contiguous, good):
  [0]       [1]        [2]       [3]       [4]
  NIL       PLAYER     AXE       POTION    FOOD
            |
            v
         children: std::vector ─────┐
                                    v
                          ┌─────────────────────┐
                          │ heap: [&AXE, &POTION]│  <-- separate allocation!
                          └─────────────────────┘
                          (could be anywhere in RAM)

Problem: player.children is a POINTER to heap memory.
        Can't memset. Can't memcpy. Can't serialize.
```

> **JS bridge:** You might think "this is a C++ problem, JS arrays just work." True -- but you are paying for it. Every JS `player.inventory = []` is a separate heap object managed by the garbage collector. When you have 1024 entities, each with an inventory array and a children array, you have 2048+ separate heap allocations scattered across memory. The GC pauses to collect them. The CPU cache misses when jumping between them. In a game loop running 60 fps, that overhead adds up. The intrusive approach puts everything in one flat array -- zero extra allocations, zero GC pressure, perfect cache locality.

So what do we do instead?

## Step 2 -- The intrusive linked list idea

> **JS bridge: What is a linked list?** JavaScript has no built-in linked list -- you use arrays for everything. A linked list is a different way to store a sequence: instead of items sitting next to each other in contiguous memory (like an array), each item stores a "link" (pointer or index) to the next item. Think of a scavenger hunt: each clue tells you where to find the next clue. You cannot jump to "the 5th item" directly (no `list[4]`) -- you have to follow the chain from the start. The tradeoff: arrays are fast for random access (`O(1)`) and slow for insertion/removal in the middle (`O(n)`). Linked lists are the opposite: slow for random access (`O(n)`) but fast for insertion/removal (`O(1)`) because you just rewire the links.

> **JS bridge: What does "intrusive" mean?** In a normal (non-intrusive) linked list, you wrap each piece of data in a "list node" container: `class Node<T> { data: T; next: Node<T> | null; }`. The list machinery is separate from your data. In an *intrusive* linked list, the list links (`next`, `prev`) live INSIDE your data struct. The thing IS the node. There is no wrapper. This is exactly how the DOM works: every DOM node has `firstChild`, `nextSibling`, `previousSibling`, and `parentNode` built directly into the node object -- not in a separate container. You have been using intrusive trees your entire web dev career. You just did not know the name.

Instead of giving each entity a separate container to hold its children, we put the list links directly inside the thing struct. The thing IS the list node. There is no separate "list" object.

> **New technique:** Intrusive linked list -- a linked list where the next/prev pointers (or in our case, indices) live inside the data struct itself, rather than in an external wrapper node.
>
> **What it does:** Lets you chain things together into parent-child lists without any heap allocation or external containers.
>
> **Why:** Preserves memset/memcpy safety. The struct stays POD (plain old data). Zero-init still works.
>
> **JS equivalent:** The DOM. Seriously. `node.firstChild` and `node.nextSibling` are intrusive tree links -- they live ON the node, not in a separate container. You've been using intrusive trees your entire career.
>
> **Worked example:** A player (slot 1) owns three items (slots 2, 3, 4). The player's `first_child = 4` (most recently added), and items chain: 4 -> 3 -> 2 -> 0 (end). No heap involved.

Here is the mental model. We add two fields to every thing:

```c
thing_idx first_child;    // index of my first child (0 = no children)
thing_idx next_sibling;   // index of my next sibling in parent's list (0 = end)
```

> **JS bridge: `firstChild` and `nextSibling` -- you know these.** Open your browser console and type `document.body.firstChild`. That gives you the body's first child node. Then `node.nextSibling` gives you the next one. That is EXACTLY what `first_child` and `next_sibling` are. The DOM stores these links directly on every node -- no separate "children array" object. Our thing struct does the same thing. The only difference: DOM uses object references (managed by the GC), and we use integer indices (managed by us).
>
> ```
>     DOM:                          Our struct:
>     node.firstChild               things[parent].first_child
>     node.nextSibling              things[child].next_sibling
>     node.parentNode               things[child].parent
>     node.firstChild === null      things[parent].first_child == 0
> ```

That's it. Two integers. They join the flat array -- they don't point to heap memory. They survive reallocation (lesson 03). They can be memset to zero. They serialize trivially.

```
AFTER: Intrusive linked list (everything stays in the flat array)
================================================================

things[] array:
  [0]       [1]         [2]       [3]        [4]
  NIL       PLAYER      AXE       POTION     FOOD
            first_child ────────────────────> [4]
                         next_sib  next_sib   next_sib
                         = 0       = 2        = 3
                         (end)

Reading: PLAYER.first_child = 4 (FOOD)
         FOOD.next_sibling  = 3 (POTION)
         POTION.next_sibling = 2 (AXE)
         AXE.next_sibling   = 0 (end of list)

Traversal: 4 -> 3 -> 2 -> 0 (stop)

No heap. No pointers. Just indices into the same flat array.
```

> **Why?** In JS, arrays auto-resize and the garbage collector handles cleanup. In C, if you want a dynamic list inside a struct, you need `malloc` + `free` + careful lifetime management. The intrusive approach eliminates ALL of that -- the "list" is just a chain of indices through existing array slots. The cost? Two extra `int` fields per thing (8 bytes). For 1024 things that's 8 KB total. Nothing.

> **Handmade Hero connection:** Casey uses a very similar pattern for entity relationships in Handmade Hero. Entities store parent/child indices and chain together via next pointers in a flat array. He calls this approach "handles" -- integer references into a flat table.

## Step 3 -- Growing the thing struct

In lesson 03, our thing struct looked like this (20 bytes):

```c
typedef struct thing {
    thing_kind kind;       // 4 bytes
    float x, y;            // 8 bytes
    float health;          // 4 bytes
    thing_idx parent;      // 4 bytes
} thing;                   // 20 bytes total
```

We now add two fields:

```c
typedef struct thing {
    thing_kind kind;         // 4 bytes
    float x, y;              // 8 bytes
    float health;            // 4 bytes
    thing_idx parent;        // 4 bytes
    thing_idx first_child;   // 4 bytes  <-- NEW
    thing_idx next_sibling;  // 4 bytes  <-- NEW
} thing;                     // 28 bytes total
```

> **Why?** Both new fields are `thing_idx` -- plain integers. They default to 0 when you memset the struct, and 0 means "nil" (no child, no sibling). The struct is still POD. Nothing changed about our initialization story.

Let's update the full header and the pool code. Here is `things_04.h` -- the version of our header for this lesson:

```c
/* things_04.h -- Lesson 04: Intrusive Linked Lists */
#ifndef THINGS_04_H
#define THINGS_04_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_THINGS 1024

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

Nothing new yet -- same as lesson 03. Now the struct:

```c
typedef struct thing {
    thing_kind kind;

    /* Intrusive tree links -- lesson 04 */
    thing_idx parent;
    thing_idx first_child;
    thing_idx next_sibling;

    /* Common properties */
    float x, y;
    float health;
} thing;
```

> **Common mistake:** Putting the tree links at the end of the struct, after x/y/health. It works, but grouping the structural links together at the top (right after `kind`) makes the "shape" of the struct readable at a glance: first the type tag, then the tree topology, then the data. When you're debugging and looking at memory, you want all the link fields adjacent.

And the pool -- same as lesson 03 but now we also need functions to manipulate the child list:

```c
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    int       next_empty;
} thing_pool;

void thing_pool_init(thing_pool *pool);
thing_idx thing_pool_add(thing_pool *pool, thing_kind kind);
thing *thing_pool_get(thing_pool *pool, thing_idx idx);
const char *thing_kind_name(thing_kind kind);

/* NEW in lesson 04: intrusive list operations */
void thing_add_child(thing_pool *pool, thing_idx parent_idx,
                     thing_idx child_idx);
void thing_remove_child(thing_pool *pool, thing_idx child_idx);

#endif
```

## Step 4 -- Pool basics (recap from lesson 03)

The pool implementation hasn't changed from lesson 03 for the core operations. Here they are for completeness:

```c
/* things_04.c -- Lesson 04: Intrusive Linked Lists */
#include "things_04.h"
#include <stdio.h>

void thing_pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1; /* slot 0 reserved for nil */
}

thing_idx thing_pool_add(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) return 0; /* pool full */
    thing_idx slot = pool->next_empty;
    pool->next_empty++;
    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    return slot;
}

thing *thing_pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return &pool->things[0];
    return &pool->things[idx];
}

const char *thing_kind_name(thing_kind kind)
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

> **Why?** `pool_get` returns `&pool->things[0]` for invalid indices instead of `NULL`. This is our nil sentinel pattern (fully explained in lesson 06). For now, just know: you never get NULL back, you get a zeroed-out thing whose `kind` is `THING_KIND_NIL`. Safe to read, meaningless data.

## Step 5 -- Add-to-list (prepend)

The most common operation: "player picks up an item." We want to add the item as a child of the player.

We prepend -- the new child becomes the first child. Why prepend instead of append?

```
Prepend: O(1) -- just update first_child and one next_sibling
Append:  O(n) -- walk the entire sibling list to find the end

For inventories, order usually doesn't matter. O(1) wins.
```

> **Anton says:** "Insert at the beginning -- it's O(1). If order doesn't matter, and it usually doesn't for inventories, just prepend."

Here's what happens step by step. Starting state: player has no children.

```
BEFORE: Player has no children
================================
things[]:
  [0]       [1]              [2]
  NIL       PLAYER           AXE
            first_child=0    parent=0
            (no children)    next_sib=0

We want: AXE becomes a child of PLAYER.
```

```c
void thing_add_child(thing_pool *pool, thing_idx parent_idx,
                     thing_idx child_idx)
{
    /* Safety: don't operate on nil or out-of-bounds indices. */
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];
```

First, get direct pointers to the parent and child. We work with pointers here (not indices) because we're going to modify the structs.

```c
    /* Set the child's parent link */
    child->parent = parent_idx;

    /* Prepend: new child's next_sibling = old first_child */
    child->next_sibling = parent->first_child;

    /* New child becomes the new first_child */
    parent->first_child = child_idx;
}
```

That's it. Three assignments. Let's trace through three consecutive adds.

**Add AXE (slot 2) to PLAYER (slot 1):**

```
Step: child->parent = 1
      child->next_sibling = parent->first_child (= 0, no children yet)
      parent->first_child = 2

AFTER adding AXE:
  [1] PLAYER          [2] AXE
  first_child = 2     parent = 1
                      next_sib = 0 (end)

  List: PLAYER -> AXE -> (end)
```

**Add POTION (slot 3) to PLAYER (slot 1):**

```
Step: child->parent = 1
      child->next_sibling = parent->first_child (= 2, AXE)
      parent->first_child = 3

AFTER adding POTION:
  [1] PLAYER          [3] POTION      [2] AXE
  first_child = 3     parent = 1      parent = 1
                      next_sib = 2    next_sib = 0

  List: PLAYER -> POTION -> AXE -> (end)
```

**Add FOOD (slot 4) to PLAYER (slot 1):**

```
Step: child->parent = 1
      child->next_sibling = parent->first_child (= 3, POTION)
      parent->first_child = 4

AFTER adding FOOD:
  [1] PLAYER          [4] FOOD       [3] POTION      [2] AXE
  first_child = 4     parent = 1     parent = 1       parent = 1
                      next_sib = 3   next_sib = 2     next_sib = 0

  List: PLAYER -> FOOD -> POTION -> AXE -> (end)
```

Notice the order: items appear in reverse order of insertion. FOOD was added last but appears first. This is exactly like pushing onto a stack -- LIFO (last in, first out). For an inventory, this is fine. The player doesn't care which item was picked up first.

> **Why?** In JS, `Array.unshift(item)` prepends to the front but is O(n) because it shifts every element. Our prepend is O(1) because a linked list doesn't need to move anything -- we just rewire two indices.

## Step 6 -- Iterate the child list

Now let's walk a parent's children. This is the operation you'll use to draw inventory items, apply damage to all children, check collision with child entities, etc.

```c
/* Print all children of parent_idx */
void print_children(thing_pool *pool, thing_idx parent_idx)
{
    thing *parent = thing_pool_get(pool, parent_idx);
    printf("%s(slot %d) children: ",
           thing_kind_name(parent->kind), parent_idx);

    thing_idx current = parent->first_child;
```

We start at `first_child`. If it's 0, there are no children.

```c
    if (current == 0) {
        printf("(none)\n");
        return;
    }
```

Otherwise, we walk the sibling chain:

```c
    while (current != 0) {
        thing *child = thing_pool_get(pool, current);
        printf("%s(slot %d)", thing_kind_name(child->kind), current);

        current = child->next_sibling;

        if (current != 0) printf(" -> ");
    }
    printf(" -> (end)\n");
}
```

That's the whole iteration. Start at `first_child`, follow `next_sibling` until you hit 0.

```
Iteration trace for PLAYER's children:
=======================================

  current = parent->first_child = 4 (FOOD)
    print "FOOD(slot 4)"
    current = things[4].next_sibling = 3

  current = 3 (POTION)
    print "POTION(slot 3)"
    current = things[3].next_sibling = 2

  current = 2 (AXE)
    print "AXE(slot 2)"
    current = things[2].next_sibling = 0

  current = 0 -> stop

  Output: FOOD(slot 4) -> POTION(slot 3) -> AXE(slot 2) -> (end)
```

> **Why?** Compare this to iterating children in the DOM:
> ```js
> let child = parent.firstChild;
> while (child !== null) {
>     console.log(child.tagName);
>     child = child.nextSibling;
> }
> ```
> Identical pattern. `null` in JS = index 0 in our system. `node.firstChild` = `thing.first_child`. `node.nextSibling` = `thing.next_sibling`. You've done this a thousand times traversing DOM trees.

## Step 7 -- Remove from list

Now the tricky one. The player drops the potion. We need to unlink it from the sibling chain without breaking the chain for the remaining items.

Here's the state before removal:

```
BEFORE removing POTION (slot 3):
=================================
  [1] PLAYER          [4] FOOD       [3] POTION      [2] AXE
  first_child = 4     parent = 1     parent = 1       parent = 1
                      next_sib = 3   next_sib = 2     next_sib = 0

  Chain: 4 -> 3 -> 2 -> 0
```

We need to make FOOD's `next_sibling` skip over POTION and point directly to AXE:

```
AFTER removing POTION (slot 3):
================================
  [1] PLAYER          [4] FOOD       [3] POTION      [2] AXE
  first_child = 4     parent = 1     parent = 0       parent = 1
                      next_sib = 2   next_sib = 0     next_sib = 0
                              ↑       (unlinked)
                              └── skips over slot 3

  Chain: 4 -> 2 -> 0
  POTION is now an orphan (parent=0, next_sib=0)
```

There's a catch with singly-linked lists: to remove a node, you need to find the **previous** node so you can update its `next_sibling`. That means walking from the start.

```c
void thing_remove_child(thing_pool *pool, thing_idx child_idx)
{
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
```

Now we need to handle two cases. Case 1: the child we're removing IS the first child.

```c
    if (parent->first_child == child_idx) {
        /* Removing the first child: parent's first_child
         * becomes the removed child's next sibling */
        parent->first_child = child->next_sibling;
    }
```

Case 2: the child is somewhere in the middle (or end) of the list. We have to walk from `first_child` to find the node that points to our child.

```c
    else {
        /* Walk the list to find the previous sibling */
        thing_idx prev_idx = parent->first_child;
        while (prev_idx != 0) {
            thing *prev = &pool->things[prev_idx];
            if (prev->next_sibling == child_idx) {
                /* Found it -- skip over the removed child */
                prev->next_sibling = child->next_sibling;
                break;
            }
            prev_idx = prev->next_sibling;
        }
    }
```

Finally, clear the removed child's links:

```c
    /* Clean up the removed child's links */
    child->parent = 0;
    child->next_sibling = 0;
}
```

> **Common mistake:** Forgetting to clear `child->parent` and `child->next_sibling` after removal. If you don't, the orphaned child still "remembers" its old parent and sibling. Later code that checks `child->parent != 0` will think it's still attached. Always zero out stale links.

> **Why?** In a singly-linked list, removing a middle node is O(n) because you have to find the previous node. This is the classic singly-linked-list tax. In lesson 05, we'll add `prev_sibling` to make this O(1). But for now, with typical inventory sizes (5-20 items), O(n) removal is fine. Don't optimize what doesn't hurt.

Let's trace through removing POTION (slot 3):

```
Remove POTION (slot 3):
  child->parent = 1 (PLAYER)
  parent->first_child = 4 (FOOD) != 3, so we walk:

  prev_idx = 4 (FOOD)
    things[4].next_sibling = 3 == child_idx!
    Found it. Set things[4].next_sibling = things[3].next_sibling = 2
    Break.

  Clear: things[3].parent = 0, things[3].next_sibling = 0

  Result: 4 -> 2 -> 0 (POTION is unlinked)
```

## Step 8 -- The correctness advantage: one item, one parent

Here's something subtle that the intrusive approach gives you for free.

Each thing has exactly ONE `parent` field and ONE `next_sibling` field. An item can only be in one list at a time. If the axe is in the player's inventory, it CANNOT simultaneously be in a chest's inventory. The struct physically doesn't allow it.

```
Structural invariant:
  thing.parent       = the ONE parent that owns this thing
  thing.next_sibling = the ONE next item in that parent's list

  An item cannot be in two lists simultaneously.
  Trying to add it to a second list overwrites the first link.
```

With `std::vector<Thing*>` you could push the same pointer into multiple vectors -- the classic loot duplication bug. With intrusive links, the structure enforces uniqueness. If you want to move an item from player to chest, you remove it from one and add it to the other:

```c
/* Move axe from player to chest */
thing_remove_child(&pool, axe_idx);         /* unlink from player */
thing_add_child(&pool, chest_idx, axe_idx); /* add to chest */
```

The item is atomically transferred. There is no moment where it exists in both or neither (assuming single-threaded game logic, which is what we have).

> **Anton says:** "The nice thing about intrusive lists is that an object can only be in one list. If you want it in two lists, you need two sets of next/prev fields. But for parent-child, one set is exactly right."

## Step 9 -- Putting it all together: the compilable file

Here is the complete file you can compile and run. It builds the player inventory, prints it, removes an item, and prints again.

```c
/* lesson_04.c -- Intrusive Linked Lists
 *
 * Compile:
 *   gcc -Wall -Wextra -std=c11 -o lesson_04 lesson_04.c
 *
 * Run:
 *   ./lesson_04
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 1024

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

    /* Intrusive tree links */
    thing_idx parent;
    thing_idx first_child;
    thing_idx next_sibling;

    /* Common properties */
    float x, y;
    float health;
} thing;

typedef struct thing_pool {
    thing things[MAX_THINGS];
    int   next_empty;
} thing_pool;

/* ══════════════════════════════════════════════════════ */
/*                   Pool Operations                      */
/* ══════════════════════════════════════════════════════ */

void thing_pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}

thing_idx thing_pool_add(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) return 0;
    thing_idx slot = pool->next_empty;
    pool->next_empty++;
    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    return slot;
}

thing *thing_pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return &pool->things[0];
    return &pool->things[idx];
}

const char *thing_kind_name(thing_kind kind)
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
/*              Intrusive List Operations                   */
/* ══════════════════════════════════════════════════════ */

/* JS: parent.prepend(child) -- but with index wiring instead of DOM nodes.
 *
 * Prepend child to front of parent's child list. O(1).
 * After this call:
 *   child->parent       = parent_idx
 *   child->next_sibling = old first_child
 *   parent->first_child = child_idx */
void thing_add_child(thing_pool *pool, thing_idx parent_idx,
                     thing_idx child_idx)
{
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx  <= 0 || child_idx  >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent       = parent_idx;
    child->next_sibling = parent->first_child;
    parent->first_child = child_idx;
}

/* JS: parent.removeChild(child) -- unlink from sibling chain.
 *
 * Remove child from its parent's child list by index.
 * Walks the sibling chain to find the previous node (O(n)).
 * Clears child's parent and next_sibling links.
 * Does NOT remove from pool -- the thing still exists, just unlinked. */
void thing_remove_child(thing_pool *pool, thing_idx child_idx)
{
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];

    if (parent->first_child == child_idx) {
        /* Removing the first child */
        parent->first_child = child->next_sibling;
    } else {
        /* Walk to find previous sibling */
        thing_idx prev_idx = parent->first_child;
        while (prev_idx != 0) {
            thing *prev = &pool->things[prev_idx];
            if (prev->next_sibling == child_idx) {
                prev->next_sibling = child->next_sibling;
                break;
            }
            prev_idx = prev->next_sibling;
        }
    }

    child->parent       = 0;
    child->next_sibling = 0;
}

/* ══════════════════════════════════════════════════════ */
/*              Print Helpers                               */
/* ══════════════════════════════════════════════════════ */

void print_children(thing_pool *pool, thing_idx parent_idx)
{
    thing *parent = thing_pool_get(pool, parent_idx);
    printf("  %s(slot %d) children: ",
           thing_kind_name(parent->kind), parent_idx);

    thing_idx current = parent->first_child;
    if (current == 0) {
        printf("(none)\n");
        return;
    }

    while (current != 0) {
        thing *child = thing_pool_get(pool, current);
        printf("%s(slot %d)", thing_kind_name(child->kind), current);
        current = child->next_sibling;
        if (current != 0) printf(" -> ");
    }
    printf(" -> (end)\n");
}

void print_thing(thing_pool *pool, thing_idx idx)
{
    thing *t = thing_pool_get(pool, idx);
    printf("  [%d] kind=%-7s parent=%d first_child=%d next_sib=%d\n",
           idx, thing_kind_name(t->kind),
           t->parent, t->first_child, t->next_sibling);
}

/* ══════════════════════════════════════════════════════ */
/*                        Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("Lesson 04: Intrusive Linked Lists\n");
    printf("sizeof(thing) = %zu bytes\n\n", sizeof(thing));

    thing_pool pool;
    thing_pool_init(&pool);

    /* Create entities */
    thing_idx player = thing_pool_add(&pool, THING_KIND_PLAYER);
    thing_idx axe    = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_idx potion = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_idx food   = thing_pool_add(&pool, THING_KIND_ITEM);

    printf("Created: PLAYER=%d, AXE=%d, POTION=%d, FOOD=%d\n\n",
           player, axe, potion, food);

    /* ── Build the inventory ── */
    printf("Adding items to player inventory...\n");

    thing_add_child(&pool, player, axe);
    printf("  After adding AXE:\n");
    print_children(&pool, player);

    thing_add_child(&pool, player, potion);
    printf("  After adding POTION:\n");
    print_children(&pool, player);

    thing_add_child(&pool, player, food);
    printf("  After adding FOOD:\n");
    print_children(&pool, player);

    /* ── Show all slot states ── */
    printf("\nSlot details:\n");
    for (int i = 0; i <= 4; i++) {
        print_thing(&pool, i);
    }

    /* ── Verify parent links ── */
    printf("\nParent verification:\n");
    printf("  AXE.parent    = %d (expected %d)\n",
           pool.things[axe].parent, player);
    printf("  POTION.parent = %d (expected %d)\n",
           pool.things[potion].parent, player);
    printf("  FOOD.parent   = %d (expected %d)\n",
           pool.things[food].parent, player);

    /* ── Remove middle item ── */
    printf("\nRemoving POTION from inventory...\n");
    thing_remove_child(&pool, potion);

    printf("  After removing POTION:\n");
    print_children(&pool, player);

    /* ── Verify potion is unlinked ── */
    printf("\n  POTION state after removal:\n");
    print_thing(&pool, potion);
    printf("  parent=%d (should be 0), next_sib=%d (should be 0)\n",
           pool.things[potion].parent, pool.things[potion].next_sibling);

    /* ── Remove first item ── */
    printf("\nRemoving FOOD (first child) from inventory...\n");
    thing_remove_child(&pool, food);
    printf("  After removing FOOD:\n");
    print_children(&pool, player);

    /* ── Remove last item ── */
    printf("\nRemoving AXE (last remaining) from inventory...\n");
    thing_remove_child(&pool, axe);
    printf("  After removing AXE:\n");
    print_children(&pool, player);

    /* ── Demonstrate the uniqueness invariant ── */
    printf("\n--- Demonstrating uniqueness invariant ---\n");
    thing_idx chest = thing_pool_add(&pool, THING_KIND_ITEM);
    printf("Created CHEST at slot %d\n", chest);

    thing_add_child(&pool, player, axe);
    printf("  AXE added to PLAYER:\n");
    print_children(&pool, player);

    /* Move axe from player to chest */
    thing_remove_child(&pool, axe);
    thing_add_child(&pool, chest, axe);
    printf("  AXE moved to CHEST:\n");
    print_children(&pool, player);
    print_children(&pool, chest);

    printf("\nDone.\n");
    return 0;
}
```

---

## Build and run

```bash
gcc -Wall -Wextra -std=c11 -o lesson_04 lesson_04.c
./lesson_04
```

## Expected result

```
Lesson 04: Intrusive Linked Lists
sizeof(thing) = 28 bytes

Created: PLAYER=1, AXE=2, POTION=3, FOOD=4

Adding items to player inventory...
  After adding AXE:
  PLAYER(slot 1) children: ITEM(slot 2) -> (end)
  After adding POTION:
  PLAYER(slot 1) children: ITEM(slot 3) -> ITEM(slot 2) -> (end)
  After adding FOOD:
  PLAYER(slot 1) children: ITEM(slot 4) -> ITEM(slot 3) -> ITEM(slot 2) -> (end)

Slot details:
  [0] kind=NIL     parent=0 first_child=0 next_sib=0
  [1] kind=PLAYER  parent=0 first_child=4 next_sib=0
  [2] kind=ITEM    parent=1 first_child=0 next_sib=0
  [3] kind=ITEM    parent=1 first_child=0 next_sib=2
  [4] kind=ITEM    parent=1 first_child=0 next_sib=3

Parent verification:
  AXE.parent    = 1 (expected 1)
  POTION.parent = 1 (expected 1)
  FOOD.parent   = 1 (expected 1)

Removing POTION from inventory...
  After removing POTION:
  PLAYER(slot 1) children: ITEM(slot 4) -> ITEM(slot 2) -> (end)

  POTION state after removal:
  [3] kind=ITEM    parent=0 first_child=0 next_sib=0
  parent=0 (should be 0), next_sib=0 (should be 0)

Removing FOOD (first child) from inventory...
  After removing FOOD:
  PLAYER(slot 1) children: ITEM(slot 2) -> (end)

Removing AXE (last remaining) from inventory...
  After removing AXE:
  PLAYER(slot 1) children: (none)

--- Demonstrating uniqueness invariant ---
Created CHEST at slot 5
  AXE added to PLAYER:
  PLAYER(slot 1) children: ITEM(slot 2) -> (end)
  AXE moved to CHEST:
  PLAYER(slot 1) children: (none)
  ITEM(slot 5) children: ITEM(slot 2) -> (end)

Done.
```

## Common mistakes

**1. Adding a child that's already in a list without removing it first.**

```c
thing_add_child(&pool, player, axe);
thing_add_child(&pool, chest, axe);  /* BUG: axe is still in player's list! */
```

This overwrites `axe->parent` and `axe->next_sibling`, silently corrupting the player's child list. Always call `thing_remove_child` before re-adding to a different parent.

**2. Confusing "remove from list" with "remove from pool."**

`thing_remove_child` only unlinks the thing from its parent's child list. The thing still exists in the pool at its slot. If you want to destroy the thing entirely, you would also need to mark its slot as unused (covered in lesson 07).

**3. Walking a list after modifying it.**

If you're iterating a parent's children and you remove the current child mid-iteration, `current = child->next_sibling` returns 0 (because you cleared it in `thing_remove_child`). Save `next_sibling` BEFORE removing:

```c
thing_idx current = parent->first_child;
while (current != 0) {
    thing_idx next = pool.things[current].next_sibling;  /* save BEFORE remove */
    thing_remove_child(&pool, current);
    current = next;
}
```

**4. Off-by-one with slot 0.**

Remember: slot 0 is the nil sentinel. Valid things start at slot 1. Trying to add slot 0 as a child is a no-op (our guard checks `child_idx <= 0`). This is by design.

## Exercises

1. **Count children.** Write a function `int count_children(thing_pool *pool, thing_idx parent_idx)` that walks the sibling chain and returns the number of children. Test it after each add/remove.

2. **Find child by kind.** Write `thing_idx find_child_by_kind(thing_pool *pool, thing_idx parent_idx, thing_kind kind)` that returns the first child matching the given kind, or 0 if not found.

3. **Reparent all.** Write `thing_reparent_all(thing_pool *pool, thing_idx from, thing_idx to)` that moves all children from one parent to another. Think about whether you need to walk the list or if there's a shortcut (hint: what if you just change `first_child`?).

4. **Nested hierarchy.** Create a scene: ROOT -> PLAYER, ROOT -> TROLL, PLAYER -> AXE, PLAYER -> SHIELD, TROLL -> CLUB. Print the hierarchy as an indented tree (2 spaces per level). This previews lesson 05.

## Connection to your work

If you've ever built a tree structure for a React component hierarchy, a file browser, or a nested comment thread, you've done this in JavaScript with objects and arrays. The only difference here is that instead of `node.children = [child1, child2]` (an external container), each node carries `firstChild` and `nextSibling` links. It's the same traversal pattern -- `node.firstChild`, `node.nextSibling` -- that the DOM uses internally.

The Linux kernel uses this exact pattern (`struct list_head`) for practically everything: process lists, file system entries, network packet queues. The kernel can't afford heap allocations in hot paths, so intrusive lists are the standard approach. GPP Chapter 19 (Object Pool) discusses the same idea: embed the links in the objects themselves.

Every game engine you'll encounter uses some variation of this. Unity's `Transform.GetChild()` iterates an intrusive child list. Unreal's `AActor` hierarchy is an intrusive tree. The pattern is universal because the constraints are universal: game entities live in flat arrays, and you can't afford dynamic containers for structural relationships.

## Step 10 -- Domain-named lists: inventory, scene graph, and more

So far we've used generic names: `first_child`, `next_sibling`. These work fine when a thing participates in one list at a time. But here's the real power of intrusive lists: a thing can be in MULTIPLE lists simultaneously by having separate named link fields.

Think about it. The player exists in the scene graph (child of the root). The player also HAS an inventory (a list of items). The axe is in the player's inventory AND it might also be in a "renderable items" list maintained by the renderer. These are three different lists, and the same entities participate in more than one.

With external containers (`std::vector`, JavaScript arrays), you'd need separate container objects for each list, each with its own heap allocation. With intrusive lists, you just add more link fields:

```c
struct thing {
    thing_kind kind;

    /* Scene graph links (parent-child hierarchy) */
    thing_idx scene_parent;
    thing_idx scene_first_child;
    thing_idx scene_next_sibling;

    /* Inventory links (what this entity carries) */
    thing_idx inv_owner;           /* who owns me in their inventory */
    thing_idx inv_first_item;      /* first item I'm carrying */
    thing_idx inv_next_item;       /* next item in the same inventory */

    /* Renderable list (flat list for the renderer) */
    thing_idx render_next;

    float x, y;
    float health;
};
```

Now the player's scene graph position and inventory are completely independent data structures that happen to share the same pool:

```
Scene graph:                    Inventory:

  ROOT                          PLAYER.inv_first_item
    |                               |
    +-- PLAYER                      +-- AXE
    |     |                         |     |
    |     +-- SHIELD                |     inv_next_item
    |                               |         |
    +-- TROLL                       +-- POTION
          |                               |
          +-- CLUB                        inv_next_item
                                              |
                                          FOOD
                                              |
                                             (0 = end)
```

The AXE is simultaneously:
- A child of PLAYER in the scene graph (via `scene_parent`)
- The first item in PLAYER's inventory (via `inv_first_item` / `inv_next_item`)
- An entry in the "renderable" list (via `render_next`)

Three different lists, zero extra allocations. Each list has its own set of link fields, so adding/removing from one list doesn't affect the others.

> **Anton says:** "This is implementing literally the inventory list. First inventory item belongs to the inventory owner... next inventory item belongs to the item."

In this course, we keep it simple with one set of links (`first_child`, `next_sibling`, `parent`). But as your game grows, the naming convention `<domain>_first_<thing>` / `<domain>_next_<thing>` keeps things clear. Scene graph? `scene_first_child`. Inventory? `inv_first_item`. Render queue? `render_next`. Each domain gets its own intrusive list, all living in the same flat pool.

---

## Step 11 -- Real-world usage: Linux kernel list_head

If intrusive lists feel like an obscure game-engine trick, here's a reality check: the Linux kernel has used them as its primary data structure since the 1990s.

The kernel defines a generic list node:

```c
/* From include/linux/types.h (simplified) */
struct list_head {
    struct list_head *next, *prev;
};
```

This tiny struct gets embedded inside whatever data structure needs to participate in a list:

```c
struct task_struct {
    /* ... hundreds of fields ... */
    struct list_head tasks;        /* links all processes */
    struct list_head children;     /* links child processes */
    struct list_head sibling;      /* links siblings under same parent */
    /* ... */
};
```

Every process in Linux is in at least three intrusive lists simultaneously, using exactly the pattern we just discussed (domain-named link fields). The scheduler iterates the task list. The process tree iterates children/siblings. Waitqueues use yet another embedded `list_head`. All intrusive. No heap-allocated containers anywhere.

The kernel uses pointers (not indices) because it doesn't have a single flat pool -- processes are allocated individually. But the core principle is identical: the list node is embedded in the data struct, not stored in an external container.

Our version uses indices instead of pointers, which is actually one step better for serialization and hot-reload (as lesson 03 explained). But the structural pattern -- "embed the links in the data" -- is the same pattern that runs every Linux machine on the planet.

> **Alternative approach:** If you've ever used a `Map<string, Set<string>>` in TypeScript to track relationships (parent to children, user to groups, tag to posts), you're maintaining an external container for each relationship. The intrusive list approach eliminates the container entirely -- the relationship IS the link fields in the data. Fewer allocations, no container-vs-data sync bugs, and the data knows which lists it belongs to.

---

## What's next

Our singly-linked list has one weakness: removing a child from the middle requires walking from `first_child` to find the previous sibling (O(n)). In lesson 05, we add `prev_sibling` to make removal O(1), upgrade to a circular doubly-linked list, and get a bonus trick: `first_child.prev_sibling` gives you the last child for free. We also build a full scene graph tree.
