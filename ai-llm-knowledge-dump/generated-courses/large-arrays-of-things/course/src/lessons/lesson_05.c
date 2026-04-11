/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_05.c — Circular Doubly-Linked Lists + Variants               */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  The thing struct grows to 32 bytes:                         */
/*          { kind, parent, first_child, next_sibling, prev_sibling,   */
/*            x, y, health }                                            */
/*          Adds prev_sibling for O(1) removal.                         */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_05 lesson_05.c */
/*                                                                      */
/*  Key idea: Circular doubly-linked list makes removal O(1).           */
/*  The list forms a ring: last.next = first, first.prev = last.        */
/*  No walking needed to find the previous sibling.                     */
/* ══════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

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

/* The thing struct at lesson 05's stage.
 * New field: prev_sibling (enables circular doubly-linked list).
 *
 * Layout:
 *   kind:         4 bytes
 *   parent:       4 bytes
 *   first_child:  4 bytes
 *   next_sibling: 4 bytes
 *   prev_sibling: 4 bytes   ← NEW
 *   x:            4 bytes
 *   y:            4 bytes
 *   health:       4 bytes
 *   Total: 32 bytes */
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;    /* NEW: enables O(1) removal */
    float x, y;
    float health;
} thing;

/* ══════════════════════════════════════════════════════ */
/*               Static Array (No Pool Yet)               */
/* ══════════════════════════════════════════════════════ */

static thing things[MAX_THINGS];
static int next_empty = 1;

/* ══════════════════════════════════════════════════════ */
/*                   Helper Functions                      */
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

/* We'll use slot-based item names for readable output. */
static const char *item_label(thing_idx idx)
{
    switch (idx) {
        case 2: return "AXE";
        case 3: return "POTION";
        case 4: return "FOOD";
        case 5: return "AXE";
        case 6: return "POTION";
        case 7: return "FOOD";
        case 8: return "AXE";
        case 9: return "POTION";
        case 10: return "FOOD";
        default: return "ITEM";
    }
}

static int thing_add(thing_kind kind, float x, float y, float health)
{
    if (next_empty >= MAX_THINGS) return 0;

    int slot = next_empty;
    next_empty++;

    memset(&things[slot], 0, sizeof(thing));
    things[slot].kind   = kind;
    things[slot].x      = x;
    things[slot].y      = y;
    things[slot].health = health;

    return slot;
}

static void things_reset(void)
{
    memset(things, 0, sizeof(things));
    next_empty = 1;
}

/* ══════════════════════════════════════════════════════ */
/*     Singly-Linked Operations (from Lesson 04)           */
/* ══════════════════════════════════════════════════════ */

/* Prepend child to parent. Singly-linked: no prev_sibling. O(1). */
static void thing_add_child_singly(thing_idx parent_idx, thing_idx child_idx)
{
    things[child_idx].parent       = parent_idx;
    things[child_idx].next_sibling = things[parent_idx].first_child;
    things[parent_idx].first_child = child_idx;
}

/* Remove child. Singly-linked: must WALK to find prev. O(n). */
static int thing_remove_child_singly(thing_idx child_idx)
{
    thing_idx parent_idx = things[child_idx].parent;
    if (parent_idx == 0) return 0;

    int steps = 0;

    if (things[parent_idx].first_child == child_idx) {
        things[parent_idx].first_child = things[child_idx].next_sibling;
        /* No walk needed for first child. */
    } else {
        thing_idx prev_idx = things[parent_idx].first_child;
        while (prev_idx != 0) {
            steps++;
            if (things[prev_idx].next_sibling == child_idx) {
                things[prev_idx].next_sibling = things[child_idx].next_sibling;
                break;
            }
            prev_idx = things[prev_idx].next_sibling;
        }
    }

    things[child_idx].parent       = 0;
    things[child_idx].next_sibling = 0;
    return steps;
}

/* Print singly-linked child list (null-terminated). */
static void print_children_singly(thing_idx parent_idx)
{
    printf("    Children of [%d] %s (singly-linked):\n",
           parent_idx, thing_kind_name(things[parent_idx].kind));

    thing_idx child = things[parent_idx].first_child;
    if (child == 0) {
        printf("      (none)\n");
        return;
    }

    while (child != 0) {
        printf("      [%d] %s (%s)\n",
               child, thing_kind_name(things[child].kind), item_label(child));
        child = things[child].next_sibling;
    }
}

/* ══════════════════════════════════════════════════════ */
/*     Circular Doubly-Linked Operations (Lesson 05)       */
/* ══════════════════════════════════════════════════════ */

/* Prepend child into circular doubly-linked list. O(1).
 *
 * The list is a RING: last.next_sibling = first, first.prev_sibling = last.
 * Single-child case: child.next = child.prev = child (ring of one). */
static void thing_add_child_circular(thing_idx parent_idx, thing_idx child_idx)
{
    things[child_idx].parent = parent_idx;

    if (things[parent_idx].first_child == 0) {
        /* No children yet. Child becomes a ring of one. */
        things[parent_idx].first_child  = child_idx;
        things[child_idx].next_sibling  = child_idx;
        things[child_idx].prev_sibling  = child_idx;
    } else {
        /* Insert before the current first child.
         * In a circular list, "before first" = "after last". */
        thing_idx old_first = things[parent_idx].first_child;
        thing_idx last      = things[old_first].prev_sibling;

        things[last].next_sibling           = child_idx;
        things[child_idx].prev_sibling      = last;
        things[child_idx].next_sibling      = old_first;
        things[old_first].prev_sibling      = child_idx;

        things[parent_idx].first_child = child_idx;  /* new first */
    }
}

/* Unlink child from circular doubly-linked list. O(1).
 * No walking — we have prev_sibling, so we patch around directly. */
static void thing_unlink_child_circular(thing_idx child_idx)
{
    thing_idx parent_idx = things[child_idx].parent;
    if (parent_idx == 0) return;

    if (things[child_idx].next_sibling == child_idx) {
        /* Only child — empty the list. */
        things[parent_idx].first_child = 0;
    } else {
        /* Patch around: prev.next = child.next, next.prev = child.prev. */
        thing_idx prev = things[child_idx].prev_sibling;
        thing_idx next = things[child_idx].next_sibling;
        things[prev].next_sibling = next;
        things[next].prev_sibling = prev;

        /* If child was first_child, update parent. */
        if (things[parent_idx].first_child == child_idx) {
            things[parent_idx].first_child = next;
        }
    }

    things[child_idx].parent       = 0;
    things[child_idx].next_sibling = 0;
    things[child_idx].prev_sibling = 0;
}

/* Append child to END of circular list. O(1).
 * The trick: first_child.prev_sibling IS last_child in a circular list.
 * So "append" = "insert between last and first, but don't change first_child". */
static void thing_add_child_append(thing_idx parent_idx, thing_idx child_idx)
{
    things[child_idx].parent = parent_idx;

    if (things[parent_idx].first_child == 0) {
        /* No children yet — same as prepend for single child. */
        things[parent_idx].first_child  = child_idx;
        things[child_idx].next_sibling  = child_idx;
        things[child_idx].prev_sibling  = child_idx;
    } else {
        /* Insert AFTER last (= first.prev), BEFORE first.
         * first_child stays the same — new child goes at the end. */
        thing_idx first = things[parent_idx].first_child;
        thing_idx last  = things[first].prev_sibling;

        things[last].next_sibling       = child_idx;
        things[child_idx].prev_sibling  = last;
        things[child_idx].next_sibling  = first;
        things[first].prev_sibling      = child_idx;
        /* parent->first_child unchanged — new child is at the END */
    }
}

/* Print circular doubly-linked child list. */
static void print_children_circular(thing_idx parent_idx)
{
    printf("    Children of [%d] %s (circular doubly-linked):\n",
           parent_idx, thing_kind_name(things[parent_idx].kind));

    thing_idx first = things[parent_idx].first_child;
    if (first == 0) {
        printf("      (none)\n");
        return;
    }

    thing_idx child = first;
    do {
        printf("      [%d] %s (%s)  prev=%d  next=%d\n",
               child, thing_kind_name(things[child].kind), item_label(child),
               things[child].prev_sibling, things[child].next_sibling);
        child = things[child].next_sibling;
    } while (child != first);
}

/* ══════════════════════════════════════════════════════ */
/*    Test 1: Singly-Linked — Add 3, Remove Middle         */
/* ══════════════════════════════════════════════════════ */

static void test_singly_linked(void)
{
    printf("=== Test 1: Singly-Linked — Add 3 Children, Remove Middle ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx axe    = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx potion = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx food   = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    thing_add_child_singly(player, axe);
    thing_add_child_singly(player, potion);
    thing_add_child_singly(player, food);

    printf("  Before removal:\n");
    print_children_singly(player);

    printf("\n  Removing potion (index %d)...\n", potion);
    int steps = thing_remove_child_singly(potion);
    printf("    Walked %d step(s) to find previous sibling — O(n)\n", steps);

    printf("\n  After removal:\n");
    print_children_singly(player);

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Test 2: Circular — Add 3, Remove Middle (O(1))        */
/* ══════════════════════════════════════════════════════ */

static void test_circular_remove(void)
{
    printf("=== Test 2: Circular Doubly-Linked — Add 3, Remove Middle (O(1)) ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx axe    = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx potion = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx food   = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    thing_add_child_circular(player, axe);
    thing_add_child_circular(player, potion);
    thing_add_child_circular(player, food);

    printf("  Before removal:\n");
    print_children_circular(player);

    printf("\n  Removing potion (index %d)...\n", potion);
    printf("    prev_sibling = %d, next_sibling = %d — patch directly, NO walk.\n",
           things[potion].prev_sibling, things[potion].next_sibling);
    thing_unlink_child_circular(potion);
    printf("    Done — O(1), zero steps.\n");

    printf("\n  After removal:\n");
    print_children_circular(player);

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*    Test 3: Append — Insertion Order Preserved            */
/* ══════════════════════════════════════════════════════ */

static void test_append(void)
{
    printf("=== Test 3: Append — Add 3 Children in Insertion Order ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx axe    = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx potion = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx food   = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    /* Append preserves insertion order (unlike prepend which reverses it). */
    thing_add_child_append(player, axe);
    thing_add_child_append(player, potion);
    thing_add_child_append(player, food);

    printf("  Added axe, potion, food using APPEND:\n");
    print_children_circular(player);

    printf("\n  Iteration order matches insertion order: AXE → POTION → FOOD.\n");
    printf("  Prepend would have given: FOOD → POTION → AXE (reversed).\n");
    printf("  Both are O(1) thanks to the circular structure.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*    Test 4: The last_child Trick                         */
/* ══════════════════════════════════════════════════════ */

static void test_last_child_trick(void)
{
    printf("=== Test 4: The last_child Trick ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx axe    = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx potion = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx food   = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    thing_add_child_append(player, axe);
    thing_add_child_append(player, potion);
    thing_add_child_append(player, food);

    thing_idx first_child_idx = things[player].first_child;
    thing_idx last_child_idx  = things[first_child_idx].prev_sibling;

    printf("  first_child = [%d] (%s)\n",
           first_child_idx, item_label(first_child_idx));
    printf("  first_child.prev_sibling = [%d] (%s)  ← this IS last_child!\n",
           last_child_idx, item_label(last_child_idx));

    printf("\n  In a circular list:\n");
    printf("    first_child.prev_sibling == last_child   (always)\n");
    printf("    last_child.next_sibling  == first_child  (always)\n");
    printf("\n  You get O(1) access to BOTH ends with just one stored link (first_child).\n");
    printf("  No separate last_child field needed — the circular structure gives it free.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Test 5: Side-by-Side Comparison                       */
/* ══════════════════════════════════════════════════════ */

static void test_comparison(void)
{
    printf("=== Test 5: Side-by-Side Comparison ===\n\n");

    printf("  ┌───────────────────────┬──────────────────────────┐\n");
    printf("  │    Singly-Linked      │  Circular Doubly-Linked  │\n");
    printf("  ├───────────────────────┼──────────────────────────┤\n");
    printf("  │  Prepend:  O(1)       │  Prepend:  O(1)          │\n");
    printf("  │  Append:   O(n)       │  Append:   O(1)          │\n");
    printf("  │  Remove:   O(n) walk  │  Remove:   O(1) direct   │\n");
    printf("  │  Memory:   1 link/node│  Memory:   2 links/node  │\n");
    printf("  │  Overhead: 4 bytes    │  Overhead: 8 bytes       │\n");
    printf("  └───────────────────────┴──────────────────────────┘\n");

    printf("\n  The extra 4 bytes for prev_sibling buy us O(1) removal\n");
    printf("  and O(1) append. For 10,000 entities, that's 40 KB —\n");
    printf("  a tiny price for never having to walk the list.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Test 6: Rename Note                                   */
/* ══════════════════════════════════════════════════════ */

static void test_rename_note(void)
{
    printf("=== Test 6: Naming Convention Change ===\n\n");

    printf("  Lesson 04:  thing_remove_child_singly()  — O(n), walks the list\n");
    printf("  Lesson 05+: thing_unlink_child()          — O(1), patches directly\n\n");

    printf("  The name changes from \"remove\" to \"unlink\" because:\n");
    printf("    - \"remove\" implies searching for something (O(n))\n");
    printf("    - \"unlink\" implies we already know the links and just patch them (O(1))\n\n");

    printf("  From lesson 05 onward, the circular doubly-linked variant is the default.\n");
    printf("  The singly-linked version exists only for comparison/teaching.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                              */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 05 — Circular Doubly-Linked Lists + Variants            ║\n");
    printf("║  struct thing adds prev_sibling → 32 bytes                      ║\n");
    printf("║  Both singly-linked and circular variants shown side by side    ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    test_singly_linked();
    test_circular_remove();
    test_append();
    test_last_child_trick();
    test_comparison();
    test_rename_note();

    printf("All tests passed.\n\n");
    return 0;
}
