# Lesson 05: Intrusive Trees and Circular Lists

## What we're building

A full scene graph -- root node with player and monsters as children, items as grandchildren -- using a circular doubly-linked sibling list. We add one field (`prev_sibling`) to the thing struct, upgrade removal from O(n) to O(1), and learn a trick that gives us last-child access for free without storing a separate `last_child` field.

By the end of this lesson you will run a test that prints an indented scene tree and demonstrates O(1) removal and the circular last-child trick:

```
Scene tree:
  ROOT
    PLAYER
      ITEM (axe)
      ITEM (shield)
    TROLL
      ITEM (club)
Last child of PLAYER: ITEM (shield) -- accessed in O(1)
```

## What you'll learn

- Why singly-linked removal is expensive and how `prev_sibling` fixes it
- Circular doubly-linked lists: last.next = first, first.prev = last
- The trick: `first_child.prev_sibling` IS `last_child` -- no extra field needed
- `do { ... } while (node != first_child)` loop termination for circular lists
- Scene graph hierarchies: root -> entities -> sub-entities
- Polygon edge-wrapping as a motivation for circular lists (from Anton)

## Prerequisites

- Lesson 04: intrusive singly-linked lists, `first_child`, `next_sibling`, prepend/iterate/remove

---

## Step 1 -- The O(n) removal problem

> **JS bridge: Trees are the DOM.** If the linked list concept from lesson 04 felt abstract, this lesson will feel like home. A tree is a hierarchy -- a root node with children, each child can have children, and so on. You already use the most famous tree data structure in computing every single day: the DOM. `document` is the root. `document.body` is a child. `body`'s `<div>`s are children of `body`. `<span>`s inside `<div>`s are grandchildren. Our scene graph is the EXACT same structure: `ROOT -> PLAYER -> AXE`. The only difference is that DOM nodes use object references and we use integer indices into a flat array.

In lesson 04, we built a singly-linked child list. Adding a child is O(1) (prepend). Iterating is O(n). But removal has a problem.

Look at `thing_remove_child` from lesson 04:

> **Course Note: Function rename.** In lesson 04 we called it `thing_remove_child`. From this lesson forward, we rename it to `thing_unlink_child`. Why? "Remove" suggests the thing is deleted from the pool. "Unlink" correctly says we're just disconnecting it from its parent's list — the thing still exists in the pool, it just has no parent now. You can unlink and then re-link to a different parent (reparenting). The final `things.h`/`things.c` uses `thing_unlink_child`.

```c
/* To remove POTION from: FOOD -> POTION -> AXE -> 0
 * we need to find FOOD (the previous node) and set FOOD.next = AXE.
 * But POTION doesn't know who points to it!
 * We have to walk from first_child to find the predecessor. */

thing_idx prev_idx = parent->first_child;
while (prev_idx != 0) {
    thing *prev = &pool->things[prev_idx];
    if (prev->next_sibling == child_idx) {
        prev->next_sibling = child->next_sibling;
        break;
    }
    prev_idx = prev->next_sibling;
}
```

This is O(n) in the number of siblings. For a player inventory of 5 items, who cares. But what about a scene graph with 200 monsters under the root node, and you need to remove one in the middle every frame? That walk adds up.

```
The O(n) removal problem (singly-linked):
==========================================

  FOOD ──next──> POTION ──next──> AXE ──next──> 0
    ^                ^
    │                │
    │            Want to remove this.
    │            But who points to it?
    │
    Walk from here ─────────────────┘
    to find the predecessor.

  With 200 siblings, removing the 150th means
  walking past 149 nodes first. Every frame.
```

> **Anton says:** "If you only have next, you have to walk the list to find the previous node. Adding prev makes removal O(1). It's four extra bytes -- that's nothing."

## Step 2 -- Adding prev_sibling

> **JS bridge:** You already know `prev_sibling` -- it is `node.previousSibling` in the DOM. Every DOM node has both `nextSibling` and `previousSibling`, allowing you to walk the sibling list in either direction. We are now giving our thing struct the same capability. With this addition, our struct has the same five navigation links as a DOM node:
>
> ```
>     DOM property         Our field            Purpose
>     ──────────────       ──────────────       ──────────────────────
>     parentNode           parent               who owns me
>     firstChild           first_child          my first child
>     nextSibling          next_sibling         next item in parent's list
>     previousSibling      prev_sibling         previous item in parent's list
>     lastChild            (derived from circular structure -- see Step 4)
> ```

The fix is dead simple: give every thing a `prev_sibling` field. Now each node knows both its successor AND its predecessor in the sibling chain.

```c
typedef struct thing {
    thing_kind kind;

    /* Intrusive tree links */
    thing_idx parent;
    thing_idx first_child;
    thing_idx next_sibling;
    thing_idx prev_sibling;    /* <-- NEW: 4 bytes, enables O(1) removal */

    /* Common properties */
    float x, y;
    float health;
} thing;
```

Struct goes from 28 to 32 bytes. For 1024 things, that's 4 KB more. Insignificant.

> **Why?** In web terms: you upgraded from a singly-linked list to a doubly-linked list. It's like having both `node.nextSibling` AND `node.previousSibling` in the DOM. In fact, the DOM is exactly this -- a doubly-linked sibling chain with parent/firstChild pointers. We're building the DOM's internal data structure.

With `prev_sibling`, removal becomes three pointer (index) rewirings:

```c
/* O(1) removal with prev_sibling: */
prev->next_sibling = child->next_sibling;
next->prev_sibling = child->prev_sibling;
/* That's it. No walking. */
```

```
O(1) removal (doubly-linked):
==============================

  FOOD <──prev── POTION <──prev── AXE
  FOOD ──next──> POTION ──next──> AXE

  To remove POTION:
    FOOD.next = POTION.next (= AXE)
    AXE.prev  = POTION.prev (= FOOD)

  Result:
    FOOD <──prev── AXE
    FOOD ──next──> AXE

  POTION is gone. No walking needed.
```

But wait -- we have a decision to make. Should `prev_sibling` at the beginning of the list be 0 (nil) and `next_sibling` at the end be 0 (nil)? Or should we make the list circular?

## Step 3 -- Going circular

> **JS bridge: What is a circular list?** JavaScript has no built-in circular data structure -- arrays have a start and an end. A circular list is like a circle of people holding hands: there is no "first" or "last" -- if you keep walking, you end up back where you started. The closest JS analogy is modular arithmetic: `(index + 1) % array.length` wraps around to the beginning. Or think of a clock face -- after 12 comes 1, not "end." In our circular list, the last child's `next_sibling` points back to the first child, and the first child's `prev_sibling` points to the last child. There is no 0 (nil) terminator in the chain.

Here's the insight that makes Anton's design elegant. Instead of a linear list that ends at 0:

```
Linear: FOOD -> POTION -> AXE -> 0
                                  ^
                            prev of FOOD = 0
```

We make the last node's `next_sibling` point back to the first, and the first node's `prev_sibling` point to the last:

```
Circular: FOOD -> POTION -> AXE -> (back to FOOD)
          ^                   |
          └───── prev ────────┘
```

> **New technique:** Circular doubly-linked list -- a linked list where the last element's `next` points to the first, and the first element's `prev` points to the last. There is no "end" -- you go around forever.
>
> **What it does:** Eliminates all boundary edge cases. There's no "beginning" or "end" to special-case. Every node has a valid prev and next.
>
> **Why:** Simplifies insertion and removal code. No special cases for "inserting at the end" or "removing the last element." The circular structure handles everything uniformly.
>
> **JS equivalent:** There is no direct JS equivalent. The closest thing is modular arithmetic: `(index + 1) % array.length` wraps around. Or think of a circular buffer / ring buffer. But JS doesn't have circular linked lists in its standard library because garbage collection would keep circular references alive -- not a concern in C.
>
> **Worked example:** Three children (slots 2, 3, 4) in a circular list: `2.next=3, 3.next=4, 4.next=2, 2.prev=4, 4.prev=3, 3.prev=2`.

Here is what the circular list looks like in the array:

```
Circular doubly-linked sibling list:
=====================================

Parent (slot 1):
  first_child = 4  (FOOD -- most recently prepended)

                ┌────── next ──────┐
                │                  │
  ┌──── next ───┤                  │
  │             │                  │
  ▼             │                  │
 [4] FOOD ─────[3] POTION ───────[2] AXE
  ▲             │                  │
  │             │                  │
  │──── prev ───┤                  │
  │             │                  │
  └───── prev ──┘                  │
                                   │
  ┌──────────── prev ──────────────┘
  │
  └─────────────────────────── (back to FOOD)

things[4]: next_sib=3, prev_sib=2
things[3]: next_sib=2, prev_sib=4
things[2]: next_sib=4, prev_sib=3
                    ↑           ↑
              wraps to first   wraps to last

Every node has a valid next AND prev. No nulls. No edge cases.
```

## Step 4 -- The last-child trick

Here's the payoff of going circular. Look at this:

```
first_child = 4 (FOOD)
things[4].prev_sibling = 2 (AXE)

AXE is the LAST child. We got it in O(1).
```

Because the list is circular, the first child's `prev_sibling` always points to the last child. We don't need a separate `last_child` field -- it's encoded in the circular structure.

```
The last-child trick:
=====================

  parent->first_child             = first child
  things[first_child].prev_sibling = last child

  This is a consequence of the circular structure:
    first.prev = last  (by definition of circular list)

  last_child = things[parent->first_child].prev_sibling

  No extra field. No walking the list. O(1).
```

> **Anton says:** "You don't need a last_child pointer. If your list is circular, first_child.prev IS last_child. That's one of the nice things about circular lists -- you get last-child access for free."

When would you need last_child? Appending. If you want to add a child at the END of the list (preserving insertion order), you need the last child so you can insert after it. With the circular trick, append is O(1) too:

```c
/* Append to end of circular list (O(1)):
 * 1. last = first->prev_sibling
 * 2. Insert new_child between last and first
 * Done. */
```

We'll implement prepend in this lesson (same as lesson 04, most common operation), but know that append is equally cheap with circular lists.

## Step 5 -- Rewriting add_child for circular lists

The prepend logic changes to maintain circular links. There are two cases: empty list (first child) and non-empty list (insert before current first).

**Case 1: Adding to an empty list.** The child becomes the only child, and its prev and next both point to itself.

```
BEFORE: Parent has no children.
  parent->first_child = 0

AFTER: Child is the sole member of a circular list of one.
  parent->first_child = child_idx
  child->next_sibling = child_idx  (points to itself)
  child->prev_sibling = child_idx  (points to itself)

  ┌───────────┐
  │           │
  └─ [child] ─┘
     next = self
     prev = self
```

> **Why?** A circular list of one element points to itself. This is not a bug -- it's a consistent base case. `node.next == node` means "I'm the only one." This eliminates the need to check for NULL/0 during removal. Every node always has valid prev/next links.

**Case 2: Adding to a non-empty list.** We insert the new child between the last node and the old first node (because "before first" is "after last" in a circle).

```
BEFORE: first_child = A, chain is ... -> last -> A -> B -> ... -> last -> A -> ...

  We want to insert NEW between last and A:
    last -> NEW -> A

  And make NEW the new first_child.

AFTER:
  last.next      = NEW
  NEW.prev       = last
  NEW.next       = old_first (A)
  old_first.prev = NEW
  parent->first_child = NEW
```

Here's the code:

```c
void thing_add_child(thing_pool *pool, thing_idx parent_idx,
                     thing_idx child_idx)
{
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx  <= 0 || child_idx  >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent = parent_idx;

    if (parent->first_child == 0) {
        /* First child: circular list of one */
        parent->first_child = child_idx;
        child->next_sibling = child_idx;
        child->prev_sibling = child_idx;
    } else {
        /* Insert before current first (= after last) */
        thing_idx old_first_idx = parent->first_child;
        thing *old_first = &pool->things[old_first_idx];

        /* last_child trick: first->prev IS last */
        thing_idx last_idx = old_first->prev_sibling;
        thing *last = &pool->things[last_idx];

        /* Wire in the new child */
        last->next_sibling      = child_idx;
        child->prev_sibling     = last_idx;
        child->next_sibling     = old_first_idx;
        old_first->prev_sibling = child_idx;

        /* New child is the new first */
        parent->first_child = child_idx;
    }
}
```

Let's trace through three adds:

```
Add AXE (slot 2) to PLAYER (slot 1):
  parent->first_child == 0, so:
    parent->first_child = 2
    AXE.next = 2 (self)
    AXE.prev = 2 (self)

    ┌──── [2] AXE ────┐
    └── next=2 prev=2 ─┘

Add POTION (slot 3) to PLAYER (slot 1):
  parent->first_child = 2 (AXE), not 0
    old_first = AXE (slot 2)
    last_idx  = AXE.prev = 2 (AXE itself -- it was alone)
    last      = AXE

    AXE.next      = 3 (POTION)    [was: 2 (self)]
    POTION.prev   = 2 (AXE)
    POTION.next   = 2 (AXE)       [old_first]
    AXE.prev      = 3 (POTION)    [was: 2 (self)]
    first_child   = 3 (POTION)

    ┌── [3] POTION ──── [2] AXE ──┐
    └─────── prev ◄──── next ──────┘

    3.next=2, 3.prev=2
    2.next=3, 2.prev=3

Add FOOD (slot 4) to PLAYER (slot 1):
  parent->first_child = 3 (POTION)
    old_first = POTION (slot 3)
    last_idx  = POTION.prev = 2 (AXE)
    last      = AXE

    AXE.next        = 4 (FOOD)     [was: 3 (POTION)]
    FOOD.prev       = 2 (AXE)
    FOOD.next       = 3 (POTION)   [old_first]
    POTION.prev     = 4 (FOOD)     [was: 2 (AXE)]
    first_child     = 4 (FOOD)

    ┌── [4] FOOD ── [3] POTION ── [2] AXE ──┐
    └──────────── prev ◄──── next ───────────┘

    4.next=3, 4.prev=2
    3.next=2, 3.prev=4
    2.next=4, 2.prev=3
```

> **Common mistake:** When inserting into a list with exactly one element, forgetting that `old_first` and `last` are the SAME node. In our trace above, when we add POTION and `last_idx = AXE.prev = 2 (AXE)`, last and old_first are both AXE. The code still works because we set `last->next_sibling` (= AXE.next) and `old_first->prev_sibling` (= AXE.prev) -- both fields of the same struct get updated. This is correct. But if you tried to "optimize" by skipping the last != old_first case, you'd break it for lists of size >= 2.

## Step 6 -- O(1) removal with prev_sibling

Now for the payoff. Removal no longer needs to walk the list.

```c
void thing_unlink_child(thing_pool *pool, thing_idx child_idx)
{
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
```

First check: is the child the ONLY child? In a circular list of one, `next_sibling == self`.

```c
    if (child->next_sibling == child_idx) {
        /* Only child -- empty the list */
        parent->first_child = 0;
    } else {
```

Otherwise, stitch the neighbors together and fix `first_child` if needed:

```c
        /* Fix neighbors */
        thing *prev = &pool->things[child->prev_sibling];
        thing *next = &pool->things[child->next_sibling];
        prev->next_sibling = child->next_sibling;
        next->prev_sibling = child->prev_sibling;

        /* If we removed the first child, update parent */
        if (parent->first_child == child_idx) {
            parent->first_child = child->next_sibling;
        }
    }

    /* Clear the removed child's links */
    child->parent       = 0;
    child->next_sibling = 0;
    child->prev_sibling = 0;
}
```

That's it. No walking. Three index rewirings plus a conditional first_child update. O(1).

Let's trace removal of POTION (slot 3) from the circular list:

```
BEFORE removing POTION (slot 3):
=================================

  first_child = 4 (FOOD)

  ┌── [4] FOOD ── [3] POTION ── [2] AXE ──┐
  └──────────── prev ◄──── next ───────────┘

  4.next=3, 4.prev=2
  3.next=2, 3.prev=4
  2.next=4, 2.prev=3

Remove POTION:
  child = POTION (slot 3)
  child->next_sibling = 2 (AXE) != 3 (self), so not only child

  prev = things[3.prev] = things[4] = FOOD
  next = things[3.next] = things[2] = AXE

  FOOD.next = 3.next = 2 (AXE)       [was: 3 (POTION)]
  AXE.prev  = 3.prev = 4 (FOOD)      [was: 3 (POTION)]

  first_child = 4 != 3, no update needed

  Clear POTION: parent=0, next=0, prev=0

AFTER removing POTION:
=======================

  first_child = 4 (FOOD)

  ┌── [4] FOOD ── [2] AXE ──┐
  └───── prev ◄──── next ────┘

  4.next=2, 4.prev=2
  2.next=4, 2.prev=4

  POTION (slot 3) is fully unlinked.
```

Compare this to lesson 04's removal: we went from a `while` loop to two assignments. For 200 siblings, that's 200x faster in the worst case.

## Step 7 -- Iterating a circular list

Iteration with a circular list is slightly different from the linear version. There's no "0 means end" anymore -- every next_sibling points to a real node. You stop when you get back to where you started.

> **JS bridge: `do...while` is the same syntax.** JavaScript has `do { } while (condition)` too -- you just rarely use it because JS arrays have `.forEach()`. The C `do...while` works identically to JS: execute the body once, THEN check the condition. If the condition is true, loop again. If false, stop. The key difference from a regular `while` loop: the body always executes at least once. This is exactly what we need for circular list traversal -- we always want to process the first node before checking if we have looped back to the start.

The idiom is `do...while`:

```c
thing_idx first = parent->first_child;
if (first == 0) {
    /* No children */
} else {
    thing_idx current = first;
    do {
        thing *child = &pool->things[current];
        /* ... process child ... */
        current = child->next_sibling;
    } while (current != first);
}
```

> **Why `do...while` and not `while`?** In a linear list, the loop condition checks for 0 (end of list). In a circular list, the "end" is when you return to the start. But if you check `current != first` as the WHILE condition, you'd never enter the loop at all (because current starts AS first). `do...while` executes the body first, THEN checks. So you process the first node, advance, and stop when you've come full circle.

```
Circular iteration trace:
=========================

  first = 4 (FOOD)
  current = 4

  do:
    process FOOD (slot 4)
    current = 4.next = 3 (POTION)
  while (3 != 4) -> continue

  do:
    process POTION (slot 3)
    current = 3.next = 2 (AXE)
  while (2 != 4) -> continue

  do:
    process AXE (slot 2)
    current = 2.next = 4 (FOOD)
  while (4 != 4) -> STOP

  Visited: FOOD, POTION, AXE -- all 3 children exactly once.
```

> **Common mistake:** Using `while (current != first)` instead of `do...while`. This skips the entire list for a single-child case (where first.next == first, so current already equals first before the first iteration). `do...while` is the correct pattern for circular list traversal.

## Step 8 -- Scene graph: building a full tree

> **JS bridge:** A scene graph is structurally identical to the DOM tree you work with every day:
>
> ```
>     DOM:                         Game scene graph:
>     document                     ROOT
>       body                         PLAYER
>         div.sidebar                  ITEM (axe)
>           span.icon                  ITEM (shield)
>         div.content                TROLL
>           p.text                     ITEM (club)
> ```
>
> In both cases: a root node has children, children have children, and you traverse top-down for rendering. The DOM renders HTML; the scene graph renders game entities. Same tree, same traversal, same operations (add child, remove child, reparent).

Now let's use intrusive trees for what they're really for: a scene graph. Every game has a hierarchy of entities. The root scene has children (player, monsters), and those have children (inventory items, attached weapons).

```
Scene graph example:
====================

  [1] ROOT
   ├── [2] PLAYER
   │    ├── [4] ITEM (axe)
   │    └── [5] ITEM (shield)
   └── [3] TROLL
        └── [6] ITEM (club)

  Implementation in the flat array:
    things[1] ROOT:   first_child=3 (TROLL, most recent prepend)
    things[2] PLAYER: parent=1, first_child=5 (shield), next/prev in ROOT's list
    things[3] TROLL:  parent=1, first_child=6 (club), next/prev in ROOT's list
    things[4] AXE:    parent=2, next/prev in PLAYER's list
    things[5] SHIELD: parent=2, next/prev in PLAYER's list
    things[6] CLUB:   parent=3, next/prev in TROLL's list (only child: self-loop)
```

Building this tree is just repeated calls to `thing_add_child`:

```c
thing_idx root   = thing_pool_add(&pool, THING_KIND_NIL);  /* root = slot 1 */
thing_idx player = thing_pool_add(&pool, THING_KIND_PLAYER);
thing_idx troll  = thing_pool_add(&pool, THING_KIND_TROLL);
thing_idx axe    = thing_pool_add(&pool, THING_KIND_ITEM);
thing_idx shield = thing_pool_add(&pool, THING_KIND_ITEM);
thing_idx club   = thing_pool_add(&pool, THING_KIND_ITEM);

/* Build hierarchy */
thing_add_child(&pool, root, player);
thing_add_child(&pool, root, troll);
thing_add_child(&pool, player, axe);
thing_add_child(&pool, player, shield);
thing_add_child(&pool, troll, club);
```

To print the tree recursively:

```c
void print_tree(thing_pool *pool, thing_idx idx, int depth)
{
    thing *t = &pool->things[idx];

    /* Print indentation */
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s (slot %d)\n", thing_kind_name(t->kind), idx);

    /* Recurse into children using circular list traversal */
    thing_idx first = t->first_child;
    if (first == 0) return;

    thing_idx current = first;
    do {
        print_tree(pool, current, depth + 1);
        current = pool->things[current].next_sibling;
    } while (current != first);
}
```

> **JS bridge:** This recursive traversal is the same thing React does when it "walks the fiber tree" during reconciliation. It is also what `document.querySelectorAll('*')` does internally: visit a node, recurse into its children via `firstChild`, then follow `nextSibling`. If you have ever written a recursive function to render a nested comment thread or a file tree in React, you have written exactly this algorithm. The only C-specific detail is the `do...while` loop for circular list iteration instead of the `while (child !== null)` pattern you would use in JS.

> **Why?** This is exactly how the DOM's `TreeWalker` works: visit a node, then visit its children, then follow next_sibling. And it's exactly how game engines traverse their scene graphs for rendering: walk down from root, transform coordinates at each level, draw leaves.

> **Handmade Hero connection:** Casey builds a scene graph for entity hierarchies in a similar way -- parent/child relationships with flat-array indices. His "entity storage" is essentially this same pattern.

## Step 9 -- Anton's polygon edge-wrapping motivation

Anton gives a concrete example of why circular lists are useful beyond trees.

> **Anton says:** "Think about a polygon. You have vertices connected by edges. The last edge connects the last vertex back to the first. If your list isn't circular, you need a special case: 'if this is the last vertex, the next edge goes back to vertex 0.' With a circular list, there's no special case. last.next is first. You just keep going."

```
Polygon with circular list (Anton's example):
==============================================

  Vertices: A, B, C, D (forming a quadrilateral)

  Linear list (needs special case):
    A -> B -> C -> D -> NULL
    Edge D->A requires: if (next == NULL) next = first;

  Circular list (no special case):
    A -> B -> C -> D -> A
    Edge D->A is just: D.next = A
    Every edge is "current -> current.next". Done.

  This is the same principle: circular lists eliminate
  boundary conditions. The code that processes each
  element is identical for ALL elements, including
  the first and last.
```

This matters for game engines because polygons, splines, animation cycles, and ring buffers all have circular topology. The circular doubly-linked list handles all of these uniformly.

> **Cross-reference:** GEA Chapter 16 (Scene Graph) covers hierarchical scene graphs used in engines like Unity and Unreal. The underlying data structure is always some form of parent/child tree. Our intrusive circular list is one of the most memory-efficient ways to implement it.

## Step 10 -- Putting it all together: the compilable file

```c
/* lesson_05.c -- Intrusive Trees and Circular Lists
 *
 * Compile:
 *   gcc -Wall -Wextra -std=c11 -o lesson_05 lesson_05.c
 *
 * Run:
 *   ./lesson_05
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

    /* Intrusive tree links (circular doubly-linked sibling list) */
    thing_idx parent;
    thing_idx first_child;
    thing_idx next_sibling;
    thing_idx prev_sibling;   /* NEW in lesson 05 */

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
/*      Intrusive List Operations (Circular Doubly-Linked) */
/* ══════════════════════════════════════════════════════ */

/* Prepend child to front of parent's circular child list. O(1).
 *
 * Two cases:
 *   1. Empty list: child becomes sole member, next/prev = self
 *   2. Non-empty:  insert between last and old_first, update first_child */
void thing_add_child(thing_pool *pool, thing_idx parent_idx,
                     thing_idx child_idx)
{
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx  <= 0 || child_idx  >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent = parent_idx;

    if (parent->first_child == 0) {
        /* First child: circular list of one */
        parent->first_child = child_idx;
        child->next_sibling = child_idx;
        child->prev_sibling = child_idx;
    } else {
        /* Insert before current first (= after last) */
        thing_idx old_first_idx = parent->first_child;
        thing *old_first = &pool->things[old_first_idx];

        thing_idx last_idx = old_first->prev_sibling;
        thing *last = &pool->things[last_idx];

        last->next_sibling      = child_idx;
        child->prev_sibling     = last_idx;
        child->next_sibling     = old_first_idx;
        old_first->prev_sibling = child_idx;

        parent->first_child = child_idx;
    }
}

/* Remove child from its parent's circular sibling list. O(1).
 *
 * Does NOT remove from pool -- just unlinks from the tree.
 * WHY separate? Reparenting (move item from ground to inventory)
 * needs unlink without destroy. */
void thing_unlink_child(thing_pool *pool, thing_idx child_idx)
{
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];

    if (child->next_sibling == child_idx) {
        /* Only child -- empty the list */
        parent->first_child = 0;
    } else {
        /* Stitch neighbors together (O(1) -- no walking!) */
        thing *prev = &pool->things[child->prev_sibling];
        thing *next = &pool->things[child->next_sibling];
        prev->next_sibling = child->next_sibling;
        next->prev_sibling = child->prev_sibling;

        if (parent->first_child == child_idx) {
            parent->first_child = child->next_sibling;
        }
    }

    child->parent       = 0;
    child->next_sibling = 0;
    child->prev_sibling = 0;
}

/* ══════════════════════════════════════════════════════ */
/*              Print Helpers                               */
/* ══════════════════════════════════════════════════════ */

/* Print a single thing's link state */
void print_thing(thing_pool *pool, thing_idx idx)
{
    thing *t = thing_pool_get(pool, idx);
    printf("  [%d] kind=%-7s parent=%d first_child=%d next=%d prev=%d\n",
           idx, thing_kind_name(t->kind),
           t->parent, t->first_child, t->next_sibling, t->prev_sibling);
}

/* Print children of a parent (circular traversal) */
void print_children(thing_pool *pool, thing_idx parent_idx)
{
    thing *parent = thing_pool_get(pool, parent_idx);
    printf("  %s(slot %d) children: ",
           thing_kind_name(parent->kind), parent_idx);

    thing_idx first = parent->first_child;
    if (first == 0) {
        printf("(none)\n");
        return;
    }

    thing_idx current = first;
    int safety = 0;
    do {
        thing *child = thing_pool_get(pool, current);
        printf("%s(%d)", thing_kind_name(child->kind), current);
        current = child->next_sibling;
        if (current != first) printf(" -> ");
        if (++safety > MAX_THINGS) { printf(" [LOOP!]"); break; }
    } while (current != first);
    printf(" -> (circle)\n");
}

/* Recursive tree printer */
void print_tree(thing_pool *pool, thing_idx idx, int depth)
{
    thing *t = &pool->things[idx];

    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s (slot %d)\n", thing_kind_name(t->kind), idx);

    thing_idx first = t->first_child;
    if (first == 0) return;

    thing_idx current = first;
    int safety = 0;
    do {
        print_tree(pool, current, depth + 1);
        current = pool->things[current].next_sibling;
        if (++safety > MAX_THINGS) break;
    } while (current != first);
}

/* ══════════════════════════════════════════════════════ */
/*                        Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("Lesson 05: Intrusive Trees and Circular Lists\n");
    printf("sizeof(thing) = %zu bytes\n\n", sizeof(thing));

    thing_pool pool;
    thing_pool_init(&pool);

    /* ── Part 1: Circular list basics ── */
    printf("--- Part 1: Circular list prepend and iterate ---\n\n");

    thing_idx player = thing_pool_add(&pool, THING_KIND_PLAYER);
    thing_idx axe    = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_idx potion = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_idx food   = thing_pool_add(&pool, THING_KIND_ITEM);

    printf("Created: PLAYER=%d, AXE=%d, POTION=%d, FOOD=%d\n\n", 
           player, axe, potion, food);

    thing_add_child(&pool, player, axe);
    printf("After adding AXE:\n");
    print_children(&pool, player);
    printf("  AXE: next=%d prev=%d (should both be %d -- self-loop)\n\n",
           pool.things[axe].next_sibling, pool.things[axe].prev_sibling, axe);

    thing_add_child(&pool, player, potion);
    printf("After adding POTION:\n");
    print_children(&pool, player);

    thing_add_child(&pool, player, food);
    printf("After adding FOOD:\n");
    print_children(&pool, player);

    /* Show circular structure */
    printf("\nCircular link verification:\n");
    print_thing(&pool, food);
    print_thing(&pool, potion);
    print_thing(&pool, axe);

    /* ── Part 2: The last-child trick ── */
    printf("\n--- Part 2: Last-child trick ---\n\n");

    thing_idx first = pool.things[player].first_child;
    thing_idx last  = pool.things[first].prev_sibling;
    printf("  first_child = %d (%s)\n",
           first, thing_kind_name(pool.things[first].kind));
    printf("  last_child  = first.prev = %d (%s)\n",
           last, thing_kind_name(pool.things[last].kind));
    printf("  (O(1) access, no walking needed)\n");

    /* ── Part 3: O(1) removal ── */
    printf("\n--- Part 3: O(1) removal ---\n\n");

    printf("Removing POTION (middle element)...\n");
    thing_unlink_child(&pool, potion);
    print_children(&pool, player);

    printf("\nRemoving FOOD (first child)...\n");
    thing_unlink_child(&pool, food);
    print_children(&pool, player);

    printf("\nRemoving AXE (only child)...\n");
    thing_unlink_child(&pool, axe);
    print_children(&pool, player);

    /* ── Part 4: Scene graph ── */
    printf("\n--- Part 4: Scene graph ---\n\n");

    /* Reset pool */
    thing_pool_init(&pool);

    thing_idx root   = thing_pool_add(&pool, THING_KIND_NIL);    /* 1 */
    thing_idx p2     = thing_pool_add(&pool, THING_KIND_PLAYER); /* 2 */
    thing_idx troll  = thing_pool_add(&pool, THING_KIND_TROLL);  /* 3 */
    thing_idx axe2   = thing_pool_add(&pool, THING_KIND_ITEM);   /* 4 */
    thing_idx shield = thing_pool_add(&pool, THING_KIND_ITEM);   /* 5 */
    thing_idx club   = thing_pool_add(&pool, THING_KIND_ITEM);   /* 6 */

    /* Build the tree */
    thing_add_child(&pool, root, p2);
    thing_add_child(&pool, root, troll);
    thing_add_child(&pool, p2, axe2);
    thing_add_child(&pool, p2, shield);
    thing_add_child(&pool, troll, club);

    printf("Scene tree:\n");
    print_tree(&pool, root, 1);

    /* Demonstrate last-child trick on the tree */
    printf("\nLast-child trick on scene graph:\n");
    thing_idx p2_first = pool.things[p2].first_child;
    thing_idx p2_last  = pool.things[p2_first].prev_sibling;
    printf("  PLAYER first_child = slot %d (%s)\n",
           p2_first, thing_kind_name(pool.things[p2_first].kind));
    printf("  PLAYER last_child  = slot %d (%s)  [via first.prev]\n",
           p2_last, thing_kind_name(pool.things[p2_last].kind));

    /* ── Part 5: Reparenting demonstration ── */
    printf("\n--- Part 5: Reparenting (move item between owners) ---\n\n");

    printf("Moving CLUB from TROLL to PLAYER...\n");
    thing_unlink_child(&pool, club);
    thing_add_child(&pool, p2, club);

    printf("\nUpdated scene tree:\n");
    print_tree(&pool, root, 1);

    printf("\nSlot details after reparent:\n");
    for (int i = 1; i <= 6; i++) {
        print_thing(&pool, i);
    }

    printf("\nDone.\n");
    return 0;
}
```

---

## Build and run

```bash
gcc -Wall -Wextra -std=c11 -o lesson_05 lesson_05.c
./lesson_05
```

## Expected result

```
Lesson 05: Intrusive Trees and Circular Lists
sizeof(thing) = 32 bytes

--- Part 1: Circular list prepend and iterate ---

Created: PLAYER=1, AXE=2, POTION=3, FOOD=4

After adding AXE:
  PLAYER(slot 1) children: ITEM(2) -> (circle)
  AXE: next=2 prev=2 (should both be 2 -- self-loop)

After adding POTION:
  PLAYER(slot 1) children: ITEM(3) -> ITEM(2) -> (circle)
After adding FOOD:
  PLAYER(slot 1) children: ITEM(4) -> ITEM(3) -> ITEM(2) -> (circle)

Circular link verification:
  [4] kind=ITEM    parent=1 first_child=0 next=3 prev=2
  [3] kind=ITEM    parent=1 first_child=0 next=2 prev=4
  [2] kind=ITEM    parent=1 first_child=0 next=4 prev=3

--- Part 2: Last-child trick ---

  first_child = 4 (ITEM)
  last_child  = first.prev = 2 (ITEM)
  (O(1) access, no walking needed)

--- Part 3: O(1) removal ---

Removing POTION (middle element)...
  PLAYER(slot 1) children: ITEM(4) -> ITEM(2) -> (circle)

Removing FOOD (first child)...
  PLAYER(slot 1) children: ITEM(2) -> (circle)

Removing AXE (only child)...
  PLAYER(slot 1) children: (none)

--- Part 4: Scene graph ---

Scene tree:
  NIL (slot 1)
    TROLL (slot 3)
      ITEM (slot 6)
    PLAYER (slot 2)
      ITEM (slot 5)
      ITEM (slot 4)

Last-child trick on scene graph:
  PLAYER first_child = slot 5 (ITEM)
  PLAYER last_child  = slot 4 (ITEM)  [via first.prev]

--- Part 5: Reparenting (move item between owners) ---

Moving CLUB from TROLL to PLAYER...

Updated scene tree:
  NIL (slot 1)
    TROLL (slot 3)
    PLAYER (slot 2)
      ITEM (slot 6)
      ITEM (slot 5)
      ITEM (slot 4)

Slot details after reparent:
  [1] kind=NIL     parent=0 first_child=3 next=0 prev=0
  [2] kind=PLAYER  parent=1 first_child=6 next=3 prev=3
  [3] kind=TROLL   parent=1 first_child=0 next=2 prev=2
  [4] kind=ITEM    parent=2 first_child=0 next=6 prev=5
  [5] kind=ITEM    parent=2 first_child=0 next=4 prev=6
  [6] kind=ITEM    parent=2 first_child=0 next=5 prev=4

Done.
```

## Common mistakes

**1. Using `while (current != first)` instead of `do...while (current != first)`.**

The `while` loop never executes for a single-child list because `current` starts equal to `first`. Always use `do...while` for circular list traversal.

**2. Forgetting that a one-element circular list points to itself.**

When a child is added to an empty list, both `next_sibling` and `prev_sibling` are set to the child's own index. This is not a bug. `child->next_sibling == child_idx` is how you detect "only child."

**3. Not updating `first_child` when removing the first child.**

If you remove the node that `parent->first_child` points to, you must update `first_child` to point to the removed node's next_sibling. Otherwise the parent still points to a disconnected node.

**4. Infinite loops from broken circular links.**

If prev/next get out of sync (e.g., you set `A.next = B` but forgot to set `B.prev = A`), traversal in one direction works but the other direction skips nodes or loops forever. Always update BOTH next and prev in the same operation. The safety counter (`if (++safety > MAX_THINGS) break;`) in our print functions catches this during debugging.

## Exercises

1. **Append instead of prepend.** Write `thing_append_child` that adds the new child as the LAST child (insertion order preserved). Use the last-child trick: `last = things[first_child].prev_sibling`, then insert after `last`. Verify that children print in insertion order.

2. **Count descendants.** Write `int count_descendants(thing_pool *pool, thing_idx idx)` that recursively counts ALL descendants (children, grandchildren, etc.). Test it on the scene graph.

3. **Find root.** Write `thing_idx find_root(thing_pool *pool, thing_idx idx)` that walks up via `parent` until it reaches a thing with `parent == 0`. This is the equivalent of `node.getRootNode()` in the DOM.

4. **Depth calculation.** Write `int thing_depth(thing_pool *pool, thing_idx idx)` that returns how many levels deep a thing is in the tree (root = 0, root's children = 1, grandchildren = 2, etc.).

## Connection to your work

The circular doubly-linked list might feel exotic if you've only worked with JavaScript arrays. But you have used the result of this pattern every single day: the browser's DOM is an intrusive tree with `parentNode`, `firstChild`, `lastChild`, `nextSibling`, and `previousSibling` -- the exact same five links we just built (and we got `lastChild` for free from the circular structure without a dedicated field). When you write `document.body.firstChild.nextSibling`, you are traversing an intrusive tree. We just built the same thing in C with integer indices instead of object references.

React's fiber tree, which drives reconciliation and rendering, is also an intrusive tree. Each fiber node has `child`, `sibling`, and `return` (parent) pointers. When React "walks the fiber tree" to find work, it's doing the same traversal we wrote in `print_tree`.

The game engine parallel is the scene graph (GEA Chapter 16). Unity's `Transform` component stores parent/child relationships and uses them for coordinate transforms: a sword's position is relative to the hand, which is relative to the arm, which is relative to the player, which is relative to the world. Our tree structure supports exactly this hierarchy. In lessons 11-12, we'll use it for the game demo.

## Step 11 -- The variant progression: pick your level

We jumped from singly-linked (lesson 04) to circular doubly-linked in this lesson. But those aren't the only two options. There's a full spectrum of list variants, and which one you pick depends on your game's actual needs.

Here's the complete progression:

```
┌───────────────────┬───────────────────────────┬────────┬────────────┬───────────┬──────────────────────────────────┐
│ Variant           │ Fields per node           │ Add    │ Remove     │ Backward  │ When to use                      │
│                   │                           │ cost   │ cost       │ iteration │                                  │
├───────────────────┼───────────────────────────┼────────┼────────────┼───────────┼──────────────────────────────────┤
│ A. Singly-linked  │ first_child, next_sibling │ O(1)   │ O(n) --    │ No        │ < 10 children, removal rare      │
│                   │                           │prepend │ walk to    │           │                                  │
│                   │                           │        │ find prev  │           │                                  │
├───────────────────┼───────────────────────────┼────────┼────────────┼───────────┼──────────────────────────────────┤
│ B. +last_child    │ A + last_child            │ O(1)   │ O(n)       │ No        │ Need append, removal rare        │
│                   │                           │either  │            │           │                                  │
│                   │                           │end     │            │           │                                  │
├───────────────────┼───────────────────────────┼────────┼────────────┼───────────┼──────────────────────────────────┤
│ C. Doubly-linked  │ A + prev_sibling          │ O(1)   │ O(1)       │ Yes       │ Frequent removal                 │
│                   │                           │prepend │            │           │                                  │
├───────────────────┼───────────────────────────┼────────┼────────────┼───────────┼──────────────────────────────────┤
│ D. Circular       │ first_child, next_sibling,│ O(1)   │ O(1)       │ Yes       │ Eliminate edge cases;            │
│    doubly-linked  │ prev_sibling (circular)   │either  │            │           │ last_child is derived            │
│    (this course)  │                           │end     │            │           │                                  │
└───────────────────┴───────────────────────────┴────────┴────────────┴───────────┴──────────────────────────────────┘
```

> **Alternative approach:** Variant A (singly-linked from lesson 04) is perfectly valid for small games. If your player never has more than 10 inventory items and you rarely remove from the middle, the O(n) removal cost is negligible -- you're walking past 9 nodes at most. The extra `prev_sibling` field in variants C and D costs 4 bytes per entity. For 1024 entities that's 4 KB. Not much. But if you're building a simple game and want the minimum viable list, variant A is your happy cabin.

> **Alternative approach:** Variant B (adding a `last_child` field) is useful when you need insertion-order preservation without going circular. You can append to the end in O(1) by maintaining `last_child` explicitly. The downside is one more field to maintain on every add/remove. The circular approach (variant D) gives you `last_child` for free via `first_child.prev_sibling`, which is why Anton prefers it.

This course implements variant D because it gives the best combination: O(1) everything, backward iteration, derived last-child, and the fewest edge cases in the code. But if you're looking at this table and thinking "variant A is all I need" -- go with variant A. You can always upgrade later by adding `prev_sibling`. The upgrade path is additive, not destructive.

---

## Step 12 -- Deriving first_child from parent pointers

Here's a radical alternative Anton has used in practice: what if you didn't store `first_child` at all?

In his UI system, things only had `parent` pointers. There was no `first_child` link. Instead, right before rendering, the system would loop through ALL things and collect children for each parent dynamically:

```c
/* No first_child stored anywhere. Derive it on the fly. */
for (int i = 1; i < pool->count; i++) {
    thing *t = &pool->things[i];
    if (t->parent == some_parent_idx) {
        /* process this child */
    }
}
```

This is an O(n) scan over the entire pool to find one parent's children. Sounds terrible, right? But consider the context: if you're already iterating every thing every frame (for update, for rendering, for physics), you're doing the O(n) scan anyway. Adding a "while I'm here, note who my parent is" check costs almost nothing on top of the scan you're already doing.

> **Anton says:** "Actually if you want to go full lean you don't actually need the first item link. You can establish it dynamically... the parent didn't know who the kids were, it's just the kids knew who the parent was."

**Pro:** Fewer fields per entity. No need to maintain `first_child`, `next_sibling`, `prev_sibling` on every add/remove. The thing struct stays smaller. The add/remove code stays simpler (just set `parent`).

**Con:** O(n) per-parent lookup instead of O(1). You can't walk from a specific parent to its children without scanning. Random parent-to-child access is expensive.

**When to use:** When you iterate everything anyway and don't need random parent-to-child access mid-frame. UI systems, particle systems, and simple 2D games where you loop all entities every frame are good candidates. Scene graphs where you need to walk subtrees on demand (e.g., "find all descendants of this transform node") are not.

For this course, we keep the intrusive links because the scene graph and inventory systems need random access to children. But it's worth knowing this lean alternative exists -- sometimes the simplest data structure is no data structure at all, just a field and a scan.

---

## Step 13 -- "If you can remove an if, that's a good day"

This is Anton's design heuristic, and it runs deeper than it sounds.

Every `if` statement in your code is a place where a bug can hide. An `if` that checks for null. An `if` that checks for the last element. An `if` that checks for the first element. Each one is a branch the CPU has to predict, a path your tests need to cover, and a condition that might be wrong in an edge case you didn't think of.

> **Anton says:** "If you can remove an if, that's a good day. That is a good day."

Circular lists remove the "am I at the last element?" check. In a non-circular list, iteration looks like:

```c
/* Non-circular: must check for 0 (end of list) */
thing_idx cursor = parent->first_child;
while (cursor != 0) {
    /* process */
    cursor = pool->things[cursor].next_sibling;
}
```

In a circular list:

```c
/* Circular: must check if we've wrapped back to first */
thing_idx cursor = parent->first_child;
if (cursor != 0) {
    do {
        /* process */
        cursor = pool->things[cursor].next_sibling;
    } while (cursor != parent->first_child);
}
```

Wait -- the circular version still has an `if`! The outer `if (cursor != 0)` handles the empty-children case. But what if we go one step further and use a nil sentinel (lesson 06) where `things[0].next_sibling` points back to 0? Then even the empty case is handled: following the nil sentinel's next_sibling gives you nil again, and the `do/while` loop body executes zero times because `cursor == first_child` immediately.

This is the philosophy: every special case you eliminate through structural design (circular links, nil sentinels, zero-initialization) is a bug you can never have. Not "a bug you tested for" -- a bug that is *structurally impossible*.

The nil sentinel from lesson 06 removes the "is this null?" check. Circular lists remove the "is this the last element?" check. Zero-initialization removes the "did I forget to initialize this?" check. Each one is an `if` deleted from the codebase. Each deletion is a good day.

Circular lists are optional, though. Wesh was not convinced during the stream, and that's fine -- singly-linked (variant A) works perfectly for many games. It could be your happy cabin. The important thing is understanding the design principle: when you have a choice between "add a check at runtime" and "make the structure handle it automatically," prefer the structure.

> **Anton says (pro tip):** "You can also rename children to kids and siblings to sibs and then everything lines up." If your struct has both `first_child` and `next_sibling`, they're different lengths and misalign visually. Rename to `first_kid` / `next_sib` and they line up in a column. Minor style choice — your cabin, your naming.

---

## What's next

We have the tree structure. But there's a gaping safety hole: what happens when you access `things[0]`? What happens when an index is out of bounds? What happens when you chain-dereference through a dead entity? Right now, `pool_get` returns `&things[0]`, but we haven't really thought about what that means. Lesson 06 turns slot 0 from "reserved but vague" into a first-class architectural feature: the nil sentinel. It's the lesson that makes the entire pool system crash-proof.
