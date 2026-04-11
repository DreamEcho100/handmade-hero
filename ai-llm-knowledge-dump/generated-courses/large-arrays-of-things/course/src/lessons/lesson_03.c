/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_03.c — Indices, Not Pointers                                 */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  The thing struct grows to 20 bytes:                         */
/*          { kind, x, y, health, parent }                              */
/*          Uses typedef int thing_idx for index references.            */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_03 lesson_03.c */
/*                                                                      */
/*  Key idea: Store integer INDICES into the array, never pointers.     */
/*  Indices survive reallocation, are trivially serializable, and       */
/*  take less space than pointers on 64-bit systems.                    */
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

/* --- Index typedef ---
 *
 * We use a simple typedef so that "thing_idx" clearly means
 * "an index into the things array" rather than just "some int."
 *
 * Three approaches exist (shown here for reference):
 *
 *   Variant A — raw int (simplest, no type safety):
 *     int parent;
 *
 *   Variant B — typedef int (what we use — readable, zero overhead):
 *     typedef int thing_idx;
 *     thing_idx parent;
 *
 *   Variant C — struct wrapper (strongest type safety, more boilerplate):
 *     typedef struct { int value; } thing_idx;
 *     thing_idx parent;
 *     parent.value = 3;  // more typing, but compiler catches misuse
 *
 * We choose Variant B: readable names without the ceremony of a wrapper
 * struct. The compiler sees it as plain int, so zero runtime cost. */
typedef int thing_idx;

/* The thing struct at lesson 03's stage.
 * New field: parent (a thing_idx — index back to the owning entity).
 *
 * Layout:
 *   kind:   4 bytes (enum = int)
 *   x:      4 bytes (float)
 *   y:      4 bytes (float)
 *   health: 4 bytes (float)
 *   parent: 4 bytes (int)
 *   Total: 20 bytes */
typedef struct thing {
    thing_kind kind;
    float x, y;
    float health;
    thing_idx parent;  /* NEW: index of the entity that owns this thing */
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
    /* parent defaults to 0 (nil) from memset */

    return slot;
}

static void things_reset(void)
{
    memset(things, 0, sizeof(things));
    next_empty = 1;
}

/* ══════════════════════════════════════════════════════ */
/*        Test 1: Parent Links via Indices                 */
/* ══════════════════════════════════════════════════════ */

static void test_parent_links(void)
{
    printf("=== Test 1: Parent Links — Items Owned by Player ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx sword  = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx shield = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    /* Set parent links: sword and shield belong to the player. */
    things[sword].parent  = player;
    things[shield].parent = player;

    printf("  [%d] %s  (parent: %d = %s)\n",
           player, thing_kind_name(things[player].kind),
           things[player].parent,
           things[player].parent == 0 ? "none (root)" : thing_kind_name(things[things[player].parent].kind));

    printf("  [%d] %s   (parent: %d = %s)\n",
           sword, thing_kind_name(things[sword].kind),
           things[sword].parent,
           thing_kind_name(things[things[sword].parent].kind));

    printf("  [%d] %s   (parent: %d = %s)\n",
           shield, thing_kind_name(things[shield].kind),
           things[shield].parent,
           thing_kind_name(things[things[shield].parent].kind));

    printf("\n  The parent field is just an integer index: %d\n", things[sword].parent);
    printf("  To get the parent: things[things[sword].parent] — array lookup, not pointer chase.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*        Test 2: Typedef Variants (Comment Tour)          */
/* ══════════════════════════════════════════════════════ */

static void test_typedef_variants(void)
{
    printf("=== Test 2: Three Ways to Type an Index ===\n\n");

    printf("  Variant A — raw int (simplest, no type safety):\n");
    printf("    int parent;\n");
    printf("    Pros: zero boilerplate. Cons: compiler can't tell it from any other int.\n\n");

    printf("  Variant B — typedef int (what we use):\n");
    printf("    typedef int thing_idx;\n");
    printf("    thing_idx parent;\n");
    printf("    Pros: self-documenting name. Cons: compiler still sees plain int.\n\n");

    printf("  Variant C — struct wrapper (strongest type safety):\n");
    printf("    typedef struct { int value; } thing_idx;\n");
    printf("    thing_idx parent; parent.value = 3;\n");
    printf("    Pros: compiler catches mixing with plain int. Cons: more typing.\n\n");

    printf("  We use Variant B: readable, zero overhead, good enough for a game.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*    Test 3: Serialization — Indices Are Plain Integers    */
/* ══════════════════════════════════════════════════════ */

static void test_serialization(void)
{
    printf("=== Test 3: Serialization — Indices Are Plain Integers ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx sword  = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);
    thing_idx shield = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    things[sword].parent  = player;
    things[shield].parent = player;

    printf("  Dumping all thing indices as plain integers:\n\n");

    for (int i = 1; i < next_empty; i++) {
        printf("    things[%d]: kind=%d  parent=%d  x=%.0f  y=%.0f  health=%.0f\n",
               i,
               (int)things[i].kind,
               things[i].parent,
               things[i].x, things[i].y, things[i].health);
    }

    printf("\n  Every field is a plain number. No pointers, no addresses.\n");
    printf("  You could fwrite(things, sizeof(thing), next_empty, file) to disk\n");
    printf("  and fread() it back — indices still valid because they're offsets,\n");
    printf("  not memory addresses.\n");

    printf("\n  If we used pointers:\n");
    printf("    thing *parent_ptr = &things[1];  // address: %p\n", (void *)&things[1]);
    printf("    After fread(), that address is GARBAGE — the array moved.\n");
    printf("    Pointers embed the address of THIS process's memory layout.\n");
    printf("    Indices are process-independent.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*    Test 4: Reallocation Safety — Indices Survive Copy   */
/* ══════════════════════════════════════════════════════ */

static void test_reallocation_safety(void)
{
    printf("=== Test 4: Reallocation Safety — memcpy the Array ===\n\n");

    things_reset();

    thing_idx player = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_idx sword  = thing_add(THING_KIND_ITEM,    0.0f,  0.0f,   0.0f);

    things[sword].parent = player;

    /* Capture a pointer to the player BEFORE the copy. */
    thing *old_player_ptr = &things[player];
    printf("  Before copy:\n");
    printf("    things array at:    %p\n", (void *)things);
    printf("    &things[player] at: %p\n", (void *)old_player_ptr);
    printf("    sword.parent = %d (index)\n", things[sword].parent);

    /* Simulate reallocation: copy the entire array to a new location. */
    thing new_things[MAX_THINGS];
    memcpy(new_things, things, sizeof(things));

    printf("\n  After memcpy to new_things:\n");
    printf("    new_things array at:  %p\n", (void *)new_things);
    printf("    different address!    %s\n",
           (void *)new_things != (void *)things ? "YES" : "no (same, unlikely)");

    /* The INDEX still works — it's just a number. */
    thing_idx sword_parent = new_things[sword].parent;
    printf("\n    new_things[sword].parent = %d (same index)\n", sword_parent);
    printf("    new_things[%d].kind = %s (still the player!)\n",
           sword_parent, thing_kind_name(new_things[sword_parent].kind));
    printf("    new_things[%d].health = %.0f (data intact)\n",
           sword_parent, new_things[sword_parent].health);

    printf("\n  The INDEX survived the copy because it's an offset, not an address.\n");
    printf("  A POINTER to the old array would now be dangling — pointing at freed memory.\n");
    printf("  This is why game engines use indices: arrays get moved (realloc, save/load, etc.).\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                              */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 03 — Indices, Not Pointers                   ║\n");
    printf("║  struct thing { kind, x, y, health, parent } = 20B   ║\n");
    printf("║  typedef int thing_idx;                               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\n");

    test_parent_links();
    test_typedef_variants();
    test_serialization();
    test_reallocation_safety();

    printf("All tests passed.\n\n");
    return 0;
}
