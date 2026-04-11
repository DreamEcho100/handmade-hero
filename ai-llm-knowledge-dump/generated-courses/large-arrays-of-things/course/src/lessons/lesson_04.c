/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_04.c — Intrusive Linked Lists (Singly-Linked Only)           */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  The thing struct grows to 28 bytes:                         */
/*          { kind, parent, first_child, next_sibling, x, y, health }  */
/*          NO prev_sibling — this lesson uses singly-linked only.      */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_04 lesson_04.c */
/*                                                                      */
/*  Key idea: Entity relationships (parent/child) stored AS FIELDS      */
/*  inside the entity struct itself — "intrusive" because the list      */
/*  links live inside the data, not in an external container.           */
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

/* The thing struct at lesson 04's stage.
 * New fields: first_child, next_sibling (singly-linked child list).
 * NO prev_sibling yet — that comes in lesson 05.
 *
 * Layout:
 *   kind:         4 bytes
 *   parent:       4 bytes
 *   first_child:  4 bytes
 *   next_sibling: 4 bytes
 *   x:            4 bytes
 *   y:            4 bytes
 *   health:       4 bytes
 *   Total: 28 bytes */
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
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

static const char *item_name(thing_idx idx)
{
    /* Fake item names based on slot index for readable output.
     * In a real game, you'd have a name field or a separate name table. */
    switch (idx) {
        case 2: return "AXE";
        case 3: return "POTION";
        case 4: return "FOOD";
        default: return "UNKNOWN_ITEM";
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
/*          Singly-Linked List Operations                  */
/* ══════════════════════════════════════════════════════ */

/* Add child as the FIRST child of parent. O(1) — just update two links.
 *
 * Before: parent.first_child → A → B → 0 (null-terminated)
 * After:  parent.first_child → child → A → B → 0 */
static void thing_add_child_singly(thing_idx parent_idx, thing_idx child_idx)
{
    things[child_idx].parent       = parent_idx;
    things[child_idx].next_sibling = things[parent_idx].first_child;
    things[parent_idx].first_child = child_idx;
}

/* Remove child from its parent's singly-linked child list.
 * O(n) — must WALK the list to find the previous sibling.
 *
 * This is the fundamental problem with singly-linked lists:
 * removal requires finding "the node before me", but we only
 * have forward links. We must start at first_child and walk. */
static void thing_remove_child_singly(thing_idx child_idx)
{
    thing_idx parent_idx = things[child_idx].parent;
    if (parent_idx == 0) return;  /* no parent — nothing to unlink */

    if (things[parent_idx].first_child == child_idx) {
        /* Child is the first — just update parent's first_child. O(1). */
        printf("    (removing first child — O(1), no walk needed)\n");
        things[parent_idx].first_child = things[child_idx].next_sibling;
    } else {
        /* Must WALK to find the previous sibling. This is the O(n) part. */
        thing_idx prev_idx = things[parent_idx].first_child;
        int steps = 0;
        while (prev_idx != 0) {
            steps++;
            if (things[prev_idx].next_sibling == child_idx) {
                printf("    (walked %d step(s) to find previous sibling — O(n))\n", steps);
                things[prev_idx].next_sibling = things[child_idx].next_sibling;
                break;
            }
            prev_idx = things[prev_idx].next_sibling;
        }
    }

    things[child_idx].parent       = 0;
    things[child_idx].next_sibling = 0;
}

/* Print all children of a parent by walking the singly-linked list. */
static void print_children(thing_idx parent_idx)
{
    printf("    Children of [%d] %s:\n",
           parent_idx, thing_kind_name(things[parent_idx].kind));

    thing_idx child = things[parent_idx].first_child;
    if (child == 0) {
        printf("      (none)\n");
        return;
    }

    while (child != 0) {
        printf("      [%d] %s (%s)  → next: %d\n",
               child,
               thing_kind_name(things[child].kind),
               item_name(child),
               things[child].next_sibling);
        child = things[child].next_sibling;
    }
}

/* ══════════════════════════════════════════════════════ */
/*      Test 1: Build Inventory with 3 Items               */
/* ══════════════════════════════════════════════════════ */

static void test_build_inventory(void)
{
    printf("=== Test 1: Player Inventory — Add 3 Items ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx axe    = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx potion = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx food   = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    /* Prepend each item. Order will be REVERSED (last added = first child). */
    thing_add_child_singly(player, axe);     /* list: axe */
    thing_add_child_singly(player, potion);  /* list: potion → axe */
    thing_add_child_singly(player, food);    /* list: food → potion → axe */

    printf("  Added axe, potion, food (prepend = reversed order).\n\n");
    print_children(player);

    printf("\n  Note: prepend reverses insertion order. Last added (food) is first.\n");
    printf("  For inventories, order usually doesn't matter — O(1) add is what counts.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Test 2: Remove Middle Item — Show O(n) Walk           */
/* ══════════════════════════════════════════════════════ */

static void test_remove_middle(void)
{
    printf("=== Test 2: Remove Middle Item (Potion) — O(n) Walk ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx axe    = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx potion = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx food   = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    thing_add_child_singly(player, axe);
    thing_add_child_singly(player, potion);
    thing_add_child_singly(player, food);

    printf("  Before removal:\n");
    print_children(player);

    printf("\n  Removing potion (index %d)...\n", potion);
    thing_remove_child_singly(potion);

    printf("\n  After removal:\n");
    print_children(player);

    printf("\n  The O(n) walk is the cost of singly-linked removal.\n");
    printf("  With 3 items it's trivial, but with 100+ children it adds up.\n");
    printf("  Lesson 05 introduces prev_sibling to make this O(1).\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*     Test 3: Domain-Named List Fields (Concept)          */
/* ══════════════════════════════════════════════════════ */

static void test_domain_named_lists(void)
{
    printf("=== Test 3: Domain-Named List Fields (Concept) ===\n\n");

    printf("  Our current struct uses generic names:\n");
    printf("    thing_idx first_child;\n");
    printf("    thing_idx next_sibling;\n\n");

    printf("  In a real game, you might use DOMAIN-SPECIFIC names:\n\n");

    printf("    typedef struct thing {\n");
    printf("        thing_kind kind;\n");
    printf("        thing_idx  parent;\n");
    printf("        /* --- Inventory list --- */\n");
    printf("        thing_idx  first_inventory;  /* first item I own */\n");
    printf("        thing_idx  next_inventory;   /* next item in same owner's bag */\n");
    printf("        /* --- World children --- */\n");
    printf("        thing_idx  first_world_child; /* first entity in my \"room\" */\n");
    printf("        thing_idx  next_world_child;  /* next entity in same room */\n");
    printf("        float x, y;\n");
    printf("        float health;\n");
    printf("    } thing;\n\n");

    printf("  Each pair of (first_X, next_X) fields IS a linked list.\n");
    printf("  One thing can be in MULTIPLE lists simultaneously —\n");
    printf("  it just has multiple link pairs.\n\n");

    printf("  But for this course, we keep it simple: one pair\n");
    printf("  (first_child / next_sibling) for the entity tree.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*        Test 4: One Thing, One List                      */
/* ══════════════════════════════════════════════════════ */

static void test_no_duplication(void)
{
    printf("=== Test 4: One Thing, One List — No Duplication ===\n\n");

    printf("  Each thing has exactly ONE set of list links.\n");
    printf("  A sword is either in the player's inventory OR on the ground — never both.\n");
    printf("  To move it: unlink from ground list, link into inventory list.\n");
    printf("  The thing itself never gets copied or duplicated.\n\n");

    printf("  One thing, one list — no duplication bugs.\n");
    printf("  (Diablo loot duping eliminated: an item can't be in two inventories\n");
    printf("   because it has only one next_sibling link.)\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                              */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 04 — Intrusive Linked Lists (Singly-Linked Only)    ║\n");
    printf("║  struct thing { kind, parent, first_child, next_sibling,    ║\n");
    printf("║                 x, y, health } = 28 bytes                   ║\n");
    printf("║  NO prev_sibling — removal is O(n)                          ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    test_build_inventory();
    test_remove_middle();
    test_domain_named_lists();
    test_no_duplication();

    printf("All tests passed.\n\n");
    return 0;
}
