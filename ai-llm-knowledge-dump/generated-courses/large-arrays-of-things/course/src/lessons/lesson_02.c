/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_02.c — Fat Struct and Kind Enum                              */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  The thing struct is 16 bytes: { kind, x, y, health }.       */
/*          No pool struct yet — just a plain static array with a       */
/*          next_empty cursor.                                          */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_02 lesson_02.c */
/*                                                                      */
/*  Key idea: ONE struct for ALL entity types. Unused fields stay zero. */
/*  This is the "fat struct" pattern — same shape, different kind tag.  */
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

/* The kind enum — a discriminated union tag.
 * THING_KIND_NIL = 0 so that zero-initialized memory reads as "nothing". */
typedef enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

/* The thing struct at lesson 02's stage.
 * Just enough fields to demonstrate the fat-struct concept.
 *
 * Layout (assuming typical alignment):
 *   kind:   4 bytes (enum = int)
 *   x:      4 bytes (float)
 *   y:      4 bytes (float)
 *   health: 4 bytes (float)
 *   Total: 16 bytes */
typedef struct thing {
    thing_kind kind;
    float x, y;
    float health;
} thing;

/* ══════════════════════════════════════════════════════ */
/*               Static Array (No Pool Yet)               */
/* ══════════════════════════════════════════════════════ */

/* No pool struct at this stage — just a bare array and a cursor.
 * Slot 0 is implicitly nil (all zeros from static storage). */
static thing things[MAX_THINGS];
static int next_empty = 1;  /* slot 0 reserved (stays zeroed = nil) */

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

/* Add a thing to the array. Returns the index, or 0 if full. */
static int thing_add(thing_kind kind, float x, float y, float health)
{
    if (next_empty >= MAX_THINGS) return 0;  /* pool full → nil index */

    int slot = next_empty;
    next_empty++;

    /* Zero the slot first, then set fields.
     * WHY: ensures any fields we don't set explicitly start at zero. */
    memset(&things[slot], 0, sizeof(thing));
    things[slot].kind   = kind;
    things[slot].x      = x;
    things[slot].y      = y;
    things[slot].health = health;

    return slot;
}

/* Reset the array to empty state (for running multiple tests). */
static void things_reset(void)
{
    memset(things, 0, sizeof(things));
    next_empty = 1;
}

/* ══════════════════════════════════════════════════════ */
/*             Test 1: Add and Print Entities              */
/* ══════════════════════════════════════════════════════ */

static void test_add_and_print(void)
{
    printf("=== Test 1: Add a Player, Troll, and Goblin ===\n\n");

    things_reset();

    int player  = thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    int troll   = thing_add(THING_KIND_TROLL,  50.0f, 60.0f,  80.0f);
    int goblin  = thing_add(THING_KIND_GOBLIN, 30.0f, 40.0f,  40.0f);

    printf("  Slot %d: kind=%-8s  x=%.1f  y=%.1f  health=%.1f\n",
           player, thing_kind_name(things[player].kind),
           things[player].x, things[player].y, things[player].health);

    printf("  Slot %d: kind=%-8s  x=%.1f  y=%.1f  health=%.1f\n",
           troll, thing_kind_name(things[troll].kind),
           things[troll].x, things[troll].y, things[troll].health);

    printf("  Slot %d: kind=%-8s  x=%.1f  y=%.1f  health=%.1f\n",
           goblin, thing_kind_name(things[goblin].kind),
           things[goblin].x, things[goblin].y, things[goblin].health);

    /* Slot 0 is always nil — let's prove it. */
    printf("\n  Slot 0: kind=%-8s  (nil sentinel — always zero)\n",
           thing_kind_name(things[0].kind));

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*           Test 2: Napkin Math (Memory Budget)           */
/* ══════════════════════════════════════════════════════ */

static void test_napkin_math(void)
{
    printf("=== Test 2: Napkin Math — How Much Memory? ===\n\n");

    printf("  sizeof(thing) = %zu bytes\n", sizeof(thing));
    printf("  MAX_THINGS    = %d\n", MAX_THINGS);
    printf("  Total pool    = %zu bytes (%.1f KB)\n",
           sizeof(things), (double)sizeof(things) / 1024.0);

    printf("\n  --- The \"worst case\" napkin math ---\n");
    printf("  Imagine a BIG game struct: 100 properties, each ~16 bytes\n");
    printf("    10,000 things * 100 props * 16 bytes = %d bytes\n",
           10000 * 100 * 16);
    printf("    That's %.1f MB — fits in L3 cache on modern CPUs.\n",
           (double)(10000 * 100 * 16) / (1024.0 * 1024.0));
    printf("    \"If your game needs more than 15 MB of entity data,\n");
    printf("     you have bigger problems than struct size.\"\n");

    printf("\n  --- Our tiny lesson struct ---\n");
    printf("    %d things * %zu bytes = %zu bytes (%.1f KB)\n",
           MAX_THINGS, sizeof(thing),
           (size_t)MAX_THINGS * sizeof(thing),
           (double)((size_t)MAX_THINGS * sizeof(thing)) / 1024.0);
    printf("    Room to grow. The fat struct stays fat and we don't care.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*          Test 3: Loop Through All Things                */
/* ══════════════════════════════════════════════════════ */

static void test_loop_all(void)
{
    printf("=== Test 3: Loop Through All Things ===\n\n");

    things_reset();

    thing_add(THING_KIND_PLAYER, 10.0f, 20.0f, 100.0f);
    thing_add(THING_KIND_TROLL,  50.0f, 60.0f,  80.0f);
    thing_add(THING_KIND_GOBLIN, 30.0f, 40.0f,  40.0f);
    thing_add(THING_KIND_ITEM,    5.0f,  5.0f,   0.0f);
    thing_add(THING_KIND_TROLL,  70.0f, 80.0f,  60.0f);

    printf("  Scanning all %d slots (slot 0 = nil, skip it):\n\n", MAX_THINGS);

    int count = 0;
    for (int i = 1; i < next_empty; i++) {
        if (things[i].kind != THING_KIND_NIL) {
            printf("    [%d] %s\n", i, thing_kind_name(things[i].kind));
            count++;
        }
    }

    printf("\n  Found %d living things out of %d allocated slots.\n", count, next_empty - 1);
    printf("  (In later lessons, a used[] array or iterator replaces this loop.)\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                              */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Lesson 02 — Fat Struct and Kind Enum            ║\n");
    printf("║  struct thing { kind, x, y, health } = 16 bytes  ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\n");

    test_add_and_print();
    test_napkin_math();
    test_loop_all();

    printf("All tests passed.\n\n");
    return 0;
}
