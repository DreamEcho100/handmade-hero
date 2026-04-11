/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_07.c — Slot Map: Add, Remove, Get (WITH VARIANTS)           */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  Pool with things[], used[], next_empty.                     */
/*          NO generations yet. NO free list yet.                       */
/*                                                                      */
/*  Thing struct (32 bytes):                                            */
/*    kind, parent, first_child, next_sibling, prev_sibling, x, y,     */
/*    health                                                            */
/*                                                                      */
/*  TWO liveness-check variants:                                        */
/*    A) pool_get_by_kind()  — check things[idx].kind != NIL            */
/*    B) pool_get_by_used()  — check used[idx]                          */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c */
/* ══════════════════════════════════════════════════════════════════════ */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 64

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

/* Thing struct: 32 bytes (8 ints/floats)
 * At this lesson's stage: flat data, no generations, no free list. */
typedef struct thing {
    thing_kind kind;           /* 4 bytes — discriminated union tag */
    thing_idx  parent;         /* 4 bytes — index of parent (0 = nil) */
    thing_idx  first_child;    /* 4 bytes — first child index (0 = nil) */
    thing_idx  next_sibling;   /* 4 bytes — next sibling index (0 = nil) */
    thing_idx  prev_sibling;   /* 4 bytes — prev sibling index (0 = nil) */
    float      x;              /* 4 bytes — position X */
    float      y;              /* 4 bytes — position Y */
    float      health;         /* 4 bytes — hit points */
} thing;

/* Pool struct: things[], used[], next_empty.
 * No generations. No free list. This is the SIMPLEST pool. */
typedef struct thing_pool {
    thing     things[MAX_THINGS];  /* flat array — things[0] = nil sentinel */
    bool      used[MAX_THINGS];    /* explicit liveness flag per slot */
    thing_idx next_empty;          /* next never-used slot (starts at 1) */
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
    /* Zero EVERYTHING. This makes things[0] the nil sentinel
     * (all zeros = THING_KIND_NIL, all indices = 0).
     * Slot 0 is reserved — never used for real entities. */
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1; /* slot 0 is the nil sentinel */
}

static thing_idx pool_add(thing_pool *pool, thing_kind kind)
{
    /* At this stage: no free list. Always grab the next empty slot. */
    if (pool->next_empty >= MAX_THINGS) {
        return 0; /* pool full — return nil index */
    }

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    /* Zero the slot, then set the kind. */
    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

static void pool_remove(thing_pool *pool, thing_idx idx)
{
    /* Removing nil (index 0) is a no-op. Removing out-of-bounds is a no-op.
     * Removing an already-unused slot is a no-op.
     * Defensive: game code should not crash on double-remove. */
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));
    /* No free list yet — the slot is just dead. Wasted until lesson 08. */
}

/* ── Variant A: Liveness via kind-check (no used[] needed) ── */

static thing *pool_get_by_kind(thing_pool *pool, thing_idx idx)
{
    /* Check if things[idx].kind != NIL. Simpler — no extra data needed.
     * BUT: can't distinguish "removed and zeroed" from "never allocated".
     * Also breaks when free list stores data in dead things. */
    if (idx > 0 && idx < MAX_THINGS
        && pool->things[idx].kind != THING_KIND_NIL) {
        return &pool->things[idx];
    }
    return &pool->things[0]; /* nil sentinel */
}

/* ── Variant B: Liveness via used[] flag (explicit) ── */

static thing *pool_get_by_used(thing_pool *pool, thing_idx idx)
{
    /* Check the explicit used[] flag. More robust — the flag is independent
     * of the thing's data. Works even when dead things store free list info. */
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0]; /* nil sentinel */
}

static bool thing_is_valid(const thing *t)
{
    /* A thing is valid if its kind is not NIL.
     * The nil sentinel has kind = 0 = THING_KIND_NIL, so this
     * returns false for any nil-sentinel-returned reference. */
    return t->kind != THING_KIND_NIL;
}

/* ══════════════════════════════════════════════════════ */
/*                      Tests                             */
/* ══════════════════════════════════════════════════════ */

static void test_1_both_variants_agree(void)
{
    printf("=== Test 1: Add 5 things — both get variants return same results ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    thing_kind kinds[5] = {
        THING_KIND_PLAYER, THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM, THING_KIND_TROLL
    };
    thing_idx slots[5];

    for (int i = 0; i < 5; i++) {
        slots[i] = pool_add(&pool, kinds[i]);
        printf("  Added %-7s at slot %d\n", thing_kind_name(kinds[i]), slots[i]);
    }

    printf("\n  Checking both variants:\n");
    for (int i = 0; i < 5; i++) {
        thing *by_kind = pool_get_by_kind(&pool, slots[i]);
        thing *by_used = pool_get_by_used(&pool, slots[i]);

        bool kind_valid = thing_is_valid(by_kind);
        bool used_valid = thing_is_valid(by_used);
        bool agree = (kind_valid == used_valid);

        printf("  Slot %d: kind-check=%s, used-check=%s  %s\n",
               slots[i],
               kind_valid ? "VALID" : "nil",
               used_valid ? "VALID" : "nil",
               agree ? "[AGREE]" : "[DISAGREE!]");

        assert(agree);
    }
    printf("\n  Both variants agree on all 5 living things.\n\n");
}

static void test_2_remove_both_agree(void)
{
    printf("=== Test 2: Remove slot 3 — both variants detect removal ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add 5 things (slots 1..5) */
    pool_add(&pool, THING_KIND_PLAYER);
    pool_add(&pool, THING_KIND_TROLL);
    thing_idx slot3 = pool_add(&pool, THING_KIND_GOBLIN);
    pool_add(&pool, THING_KIND_ITEM);
    pool_add(&pool, THING_KIND_TROLL);

    printf("  Before removal: slot %d is %s\n",
           slot3, thing_kind_name(pool.things[slot3].kind));

    pool_remove(&pool, slot3);
    printf("  After removal:  slot %d zeroed\n\n", slot3);

    thing *by_kind = pool_get_by_kind(&pool, slot3);
    thing *by_used = pool_get_by_used(&pool, slot3);

    bool kind_result = thing_is_valid(by_kind);
    bool used_result = thing_is_valid(by_used);

    printf("  pool_get_by_kind(%d) -> %s\n", slot3, kind_result ? "VALID" : "nil");
    printf("  pool_get_by_used(%d) -> %s\n", slot3, used_result ? "VALID" : "nil");

    assert(!kind_result);
    assert(!used_result);
    printf("\n  Both agree: slot %d is dead.\n\n", slot3);
}

static void test_3_why_used_is_better(void)
{
    printf("=== Test 3: WHY used[] is better ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add and remove a thing */
    thing_idx slot = pool_add(&pool, THING_KIND_TROLL);
    pool_remove(&pool, slot);

    printf("  After removing slot %d, the thing is zeroed:\n", slot);
    printf("    kind         = %s (== NIL)\n", thing_kind_name(pool.things[slot].kind));
    printf("    used[%d]     = %s\n", slot, pool.used[slot] ? "true" : "false");
    printf("    next_sibling = %d\n\n", pool.things[slot].next_sibling);

    /* Simulate what a free list would do: store a "next free" pointer
     * in the dead thing's next_sibling field. */
    printf("  Simulating free list: storing next_free=42 in dead slot's next_sibling...\n");
    pool.things[slot].next_sibling = 42;

    printf("    kind         = %s (still NIL — the slot is dead)\n",
           thing_kind_name(pool.things[slot].kind));
    printf("    next_sibling = %d (free list pointer, NOT a real sibling)\n\n",
           pool.things[slot].next_sibling);

    /* Now check both variants: */
    thing *by_kind = pool_get_by_kind(&pool, slot);
    thing *by_used = pool_get_by_used(&pool, slot);

    printf("  pool_get_by_kind(%d) -> %s\n", slot,
           thing_is_valid(by_kind) ? "VALID (WRONG!)" : "nil (correct, for now)");
    printf("  pool_get_by_used(%d) -> %s\n\n", slot,
           thing_is_valid(by_used) ? "VALID (WRONG!)" : "nil (correct)");

    printf("  Right now both return nil because kind is still NIL.\n");
    printf("  BUT: the dead slot has MEANING to the free list (next_sibling=42).\n");
    printf("  If a future optimization sets kind to a non-zero marker,\n");
    printf("  pool_get_by_kind would falsely report the slot as alive.\n");
    printf("  pool_get_by_used always works — it checks used[], not thing data.\n\n");

    printf("  BOTTOM LINE: used[] is independent of the thing's data.\n");
    printf("  Kind-check couples liveness to a field that dead things might reuse.\n\n");
}

static void test_4_variant_summary(void)
{
    printf("=== Test 4: Variant comparison summary ===\n\n");

    printf("  +-----------+--------------------------------------+\n");
    printf("  | Variant   | Characteristics                      |\n");
    printf("  +-----------+--------------------------------------+\n");
    printf("  | A (kind)  | Simpler, no extra data.              |\n");
    printf("  |           | Breaks if dead slots reuse fields.   |\n");
    printf("  +-----------+--------------------------------------+\n");
    printf("  | B (used[])| Explicit, supports free list.        |\n");
    printf("  |           | Costs 1 byte per slot (bool array).  |\n");
    printf("  +-----------+--------------------------------------+\n\n");

    printf("  Variant A (kind-check): simpler, no extra data.\n");
    printf("  Variant B (used[]): explicit, supports free list.\n\n");
}

static void test_5_preview_next_lesson(void)
{
    printf("=== Test 5: Preview ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add 5, remove 2 to create gaps */
    pool_add(&pool, THING_KIND_PLAYER);
    thing_idx s2 = pool_add(&pool, THING_KIND_TROLL);
    pool_add(&pool, THING_KIND_GOBLIN);
    thing_idx s4 = pool_add(&pool, THING_KIND_ITEM);
    pool_add(&pool, THING_KIND_TROLL);

    pool_remove(&pool, s2);
    pool_remove(&pool, s4);

    printf("  Pool state after adding 5 and removing slots %d, %d:\n\n", s2, s4);
    printf("    Slot  Kind       used[]\n");
    printf("    ----  --------   ------\n");
    for (int i = 0; i < pool.next_empty; i++) {
        printf("    [%d]   %-8s   %s",
               i, thing_kind_name(pool.things[i].kind),
               pool.used[i] ? "true" : "false");
        if (i == 0)  printf("   <- nil sentinel");
        if (i == s2) printf("   <- REMOVED (gap)");
        if (i == s4) printf("   <- REMOVED (gap)");
        printf("\n");
    }
    printf("    next_empty = %d\n\n", pool.next_empty);

    printf("  Deactivated objects are empty gaps in the array\n");
    printf("  -- lesson 08 fills them.\n\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 07: Slot Map — Add, Remove, Get (WITH VARIANTS) ║\n");
    printf("║  Thing: 32 bytes | Pool: things[] + used[] + next_empty  ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    printf("  sizeof(thing) = %zu bytes\n\n", sizeof(thing));

    test_1_both_variants_agree();
    test_2_remove_both_agree();
    test_3_why_used_is_better();
    test_4_variant_summary();
    test_5_preview_next_lesson();

    printf("══════════════════════════════════════════════════════\n");
    printf("  All lesson 07 tests passed.\n");
    printf("══════════════════════════════════════════════════════\n\n");

    return 0;
}
