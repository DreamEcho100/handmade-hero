/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_06.c — Nil Sentinel and Zero Initialization                  */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  Thing struct is 32 bytes (same as lesson 05).               */
/*          The POOL struct is introduced: { things[], used[],          */
/*          next_empty }.                                               */
/*          Slot 0 is the NIL SENTINEL: always zero, always safe to    */
/*          read, never crashes.                                        */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_06 lesson_06.c */
/*                                                                      */
/*  Key idea: Every bad index (out-of-bounds, removed, uninitialized)  */
/*  returns a pointer to things[0] — the nil sentinel. Reading nil     */
/*  gives zero values. You NEVER get NULL. You NEVER crash.            */
/* ══════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 8  /* Tiny pool to demonstrate "pool full" scenario */

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

/* The thing struct — same 32-byte layout as lesson 05.
 * DESIGN RULE: all-zeros MUST be a valid nil state.
 *   - kind = 0 = THING_KIND_NIL
 *   - all indices = 0 = "points to nil sentinel"
 *   - all floats = 0.0f = safe defaults */
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float x, y;
    float health;
} thing;

/* ══════════════════════════════════════════════════════ */
/*                   The Thing Pool                        */
/* ══════════════════════════════════════════════════════ */

/* The pool wraps the array + metadata.
 * things[0] is the NIL SENTINEL: always zero-init, always safe to read.
 * Valid slots start at index 1. */
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    thing_idx next_empty;
} thing_pool;

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

/* ══════════════════════════════════════════════════════ */
/*                   Pool API                              */
/* ══════════════════════════════════════════════════════ */

/* Reset pool to empty state. All things become nil. */
static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;  /* slot 0 reserved for nil sentinel */
}

/* Add a new thing. Returns index, or 0 if pool is full. */
static thing_idx pool_add(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) return 0;  /* full → nil index */

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

/* Get a thing by index. Returns pointer to the thing if valid,
 * or pointer to things[0] (nil sentinel) if invalid/unused.
 * NEVER returns NULL. NEVER crashes. */
static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];  /* nil sentinel */
}

/* Remove a thing by marking it unused and zeroing it. */
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));
}

/* Check if a thing is valid (not nil). */
static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

/* ══════════════════════════════════════════════════════ */
/*              Debug: Assert Nil Is Clean                  */
/* ══════════════════════════════════════════════════════ */

#ifdef DEBUG
static void pool_assert_nil_clean(const thing_pool *pool)
{
    /* The nil sentinel (things[0]) should always be all-zeros.
     * If someone accidentally wrote to it, this fires. */
    const unsigned char *bytes = (const unsigned char *)&pool->things[0];
    for (size_t i = 0; i < sizeof(thing); i++) {
        assert(bytes[i] == 0 && "NIL sentinel was written to! "
               "A nil reference is being used as a write target.");
    }
}
#endif

/* ══════════════════════════════════════════════════════ */
/*   Intrusive List — Circular Doubly-Linked (from L05)    */
/* ══════════════════════════════════════════════════════ */

static void thing_add_child(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx)
{
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent = parent_idx;

    if (parent->first_child == 0) {
        parent->first_child  = child_idx;
        child->next_sibling  = child_idx;
        child->prev_sibling  = child_idx;
    } else {
        thing_idx old_first = parent->first_child;
        thing_idx last      = pool->things[old_first].prev_sibling;

        pool->things[last].next_sibling      = child_idx;
        child->prev_sibling                  = last;
        child->next_sibling                  = old_first;
        pool->things[old_first].prev_sibling = child_idx;

        parent->first_child = child_idx;
    }
}

/* ══════════════════════════════════════════════════════ */
/*   Scenario 1: Out-of-Bounds Index → Returns Nil         */
/* ══════════════════════════════════════════════════════ */

static void test_out_of_bounds(thing_pool *pool)
{
    printf("=== Scenario 1: Out-of-Bounds Index → Returns Nil ===\n\n");

    thing *t = pool_get(pool, 9999);
    printf("  pool_get(pool, 9999) → kind = %s, health = %.1f\n",
           thing_kind_name(t->kind), t->health);
    printf("  thing_is_valid() = %s\n", thing_is_valid(t) ? "true" : "false");

    t = pool_get(pool, -1);
    printf("  pool_get(pool, -1)   → kind = %s, health = %.1f\n",
           thing_kind_name(t->kind), t->health);
    printf("  thing_is_valid() = %s\n", thing_is_valid(t) ? "true" : "false");

    printf("  No crash. No NULL pointer. Just zero values.\n");
    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Scenario 2: Removed Entity → Returns Nil              */
/* ══════════════════════════════════════════════════════ */

static void test_removed_entity(thing_pool *pool)
{
    printf("=== Scenario 2: Removed Entity → Returns Nil ===\n\n");

    thing_idx troll = pool_add(pool, THING_KIND_TROLL);
    printf("  Added troll at index %d\n", troll);

    thing *t = pool_get(pool, troll);
    printf("  Before removal: kind = %s, valid = %s\n",
           thing_kind_name(t->kind), thing_is_valid(t) ? "true" : "false");

    pool_remove(pool, troll);

    t = pool_get(pool, troll);
    printf("  After removal:  kind = %s, valid = %s\n",
           thing_kind_name(t->kind), thing_is_valid(t) ? "true" : "false");
    printf("  Removed entity returns nil — same as never existed.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Scenario 3: Pool Full → Add Returns 0                 */
/* ══════════════════════════════════════════════════════ */

static void test_pool_full(void)
{
    printf("=== Scenario 3: Pool Full → Add Returns 0, Get Returns Nil ===\n\n");

    thing_pool small_pool;
    pool_init(&small_pool);

    printf("  Pool capacity: %d slots (slot 0 = nil, slots 1-%d usable)\n",
           MAX_THINGS, MAX_THINGS - 1);

    /* Fill the pool completely. */
    for (int i = 0; i < MAX_THINGS; i++) {
        thing_idx idx = pool_add(&small_pool, THING_KIND_GOBLIN);
        if (idx != 0) {
            /* slot filled */
        } else {
            printf("  Slot %d: pool_add() returned 0 — pool is FULL\n", i + 1);
            break;
        }
        printf("  Slot %d: added goblin at index %d\n", i + 1, idx);
    }

    /* Try to add one more. */
    thing_idx overflow = pool_add(&small_pool, THING_KIND_TROLL);
    printf("\n  Tried to add one more: pool_add() returned %d\n", overflow);

    thing *t = pool_get(&small_pool, overflow);
    printf("  pool_get(pool, %d) → kind = %s, valid = %s\n",
           overflow, thing_kind_name(t->kind), thing_is_valid(t) ? "true" : "false");
    printf("  Pool full → get nil. No crash, no undefined behavior.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Scenario 4: Uninitialized Index (0) → Returns Nil     */
/* ══════════════════════════════════════════════════════ */

static void test_uninitialized(thing_pool *pool)
{
    printf("=== Scenario 4: Uninitialized Index (0) → Returns Nil ===\n\n");

    /* A freshly zeroed thing_idx is 0. pool_get(0) returns nil. */
    thing_idx uninit = 0;  /* default zero-init value */

    thing *t = pool_get(pool, uninit);
    printf("  thing_idx uninit = 0;  (zero-initialized)\n");
    printf("  pool_get(pool, 0) → kind = %s, valid = %s\n",
           thing_kind_name(t->kind), thing_is_valid(t) ? "true" : "false");
    printf("  Zero-init automatically means \"points to nil.\" No special setup needed.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/* Scenario 5: Chain-Dereference Through Nil               */
/* ══════════════════════════════════════════════════════ */

static void test_chain_deref(thing_pool *pool)
{
    printf("=== Scenario 5: Chain-Dereference Through Nil ===\n\n");

    /* Reset and add a player with no children. */
    pool_init(pool);
    thing_idx player = pool_add(pool, THING_KIND_PLAYER);
    pool_get(pool, player)->health = 100.0f;

    /* player has no children, so first_child = 0 (nil).
     * Chain: pool_get(pool_get(player).first_child).health
     *   → pool_get(nil.first_child).health
     *   → pool_get(0).health
     *   → nil.health
     *   → 0.0f
     * No NULL check needed anywhere in the chain. */

    thing *player_thing = pool_get(pool, player);
    thing_idx child_idx = player_thing->first_child;  /* 0 — no children */
    thing *child = pool_get(pool, child_idx);           /* returns nil sentinel */
    float child_health = child->health;                 /* 0.0f — safe */

    printf("  Player at [%d], first_child = %d (no children)\n",
           player, player_thing->first_child);
    printf("  pool_get(pool, %d) → nil sentinel\n", child_idx);
    printf("  nil.health = %.1f  (safe zero value)\n", child_health);

    printf("\n  The full chain:\n");
    printf("    pool_get(pool, pool_get(pool, player)->first_child)->health\n");
    printf("    = pool_get(pool, 0)->health\n");
    printf("    = nil->health\n");
    printf("    = 0.0f\n");
    printf("\n  No NULL checks. No crashes. The nil sentinel absorbs everything.\n");

    /* Also demonstrate with add_child for a valid chain. */
    thing_idx sword = pool_add(pool, THING_KIND_ITEM);
    pool_get(pool, sword)->health = 25.0f;
    thing_add_child(pool, player, sword);

    thing *first_child_thing = pool_get(pool, pool_get(pool, player)->first_child);
    printf("\n  With a real child: pool_get(first_child)->health = %.1f (valid)\n",
           first_child_thing->health);
    printf("  Same code path, no branching on NULL.\n");

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*   Scenario 6: Debug Assert — Nil Is Clean               */
/* ══════════════════════════════════════════════════════ */

static void test_nil_clean(thing_pool *pool)
{
    printf("=== Scenario 6: Debug Assert — Nil Sentinel Is Clean ===\n\n");

    pool_init(pool);

    /* Add some things, do some work. */
    pool_add(pool, THING_KIND_PLAYER);
    pool_add(pool, THING_KIND_TROLL);

    /* Assert that nobody accidentally wrote to things[0]. */
#ifdef DEBUG
    pool_assert_nil_clean(pool);
    printf("  pool_assert_nil_clean() passed — nil sentinel is all zeros.\n");
    printf("  If ANY byte of things[0] were non-zero, this assert would fire.\n");
    printf("  This catches bugs where code writes through a nil reference.\n");
#else
    printf("  (DEBUG not defined — assert is a no-op in release builds)\n");
#endif

    printf("\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                              */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 06 — Nil Sentinel and Zero Initialization               ║\n");
    printf("║  struct thing = 32 bytes (same as lesson 05)                    ║\n");
    printf("║  Pool struct introduced: { things[], used[], next_empty }       ║\n");
    printf("║  things[0] = nil sentinel — always zero, always safe            ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add a player so scenarios have something to work with. */
    pool_add(&pool, THING_KIND_PLAYER);

    test_out_of_bounds(&pool);
    test_removed_entity(&pool);
    test_pool_full();

    /* Re-init for remaining tests (pool_full used a separate pool). */
    pool_init(&pool);
    pool_add(&pool, THING_KIND_PLAYER);

    test_uninitialized(&pool);
    test_chain_deref(&pool);
    test_nil_clean(&pool);

    printf("All 6 scenarios returned nil safely — zero crashes.\n\n");
    return 0;
}
