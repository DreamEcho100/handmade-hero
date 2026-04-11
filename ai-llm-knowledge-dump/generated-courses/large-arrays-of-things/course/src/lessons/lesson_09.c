/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_09.c — Generational IDs (WITH/WITHOUT COMPARISON)           */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  Pool grows: add generations[MAX] array and thing_ref struct.*/
/*          Free list from lesson 08 is present.                        */
/*                                                                      */
/*  Thing struct: same 32 bytes.                                        */
/*  Pool struct:  things[], used[], generations[] (NEW), next_empty,    */
/*                first_free.                                           */
/*  thing_ref:    { index, generation } — a safe handle.                */
/*                                                                      */
/*  TWO get strategies compared:                                        */
/*    - pool_get_ref():    checks generation match (SAFE)               */
/*    - pool_get_no_gen(): ignores generation (UNSAFE — stale ref bug)  */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_09 lesson_09.c */
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

/* NEW: Generational reference — index + generation counter.
 * If the generation doesn't match the pool's generation for that slot,
 * the ref is stale (the slot was recycled). */
typedef struct thing_ref {
    thing_idx index;
    uint32_t  generation;
} thing_ref;

/* Thing struct: 32 bytes — same as lessons 07-08. */
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;

/* Pool struct: NOW includes generations[]. */
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    uint32_t  generations[MAX_THINGS];  /* NEW: bumped on each slot recycle */
    thing_idx next_empty;
    thing_idx first_free;
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

static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

/* ══════════════════════════════════════════════════════ */
/*                   Pool Operations                      */
/* ══════════════════════════════════════════════════════ */

static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}

/* Add: returns thing_ref { index, generation }.
 * Uses free list from lesson 08. */
static thing_ref pool_add(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    /* Try free list first */
    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    }
    /* Then try never-used region */
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    /* Pool full */
    else {
        return (thing_ref){0, 0};
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    /* Return ref with CURRENT generation of this slot.
     * If recycled, generation was already bumped by remove(). */
    return (thing_ref){slot, pool->generations[slot]};
}

/* Remove: marks unused, zeros thing, bumps generation, pushes to free list. */
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    /* BUMP GENERATION: any existing thing_ref to this slot becomes stale.
     * Old ref has generation N, pool now has generation N+1.
     * This is the core of the generational ID system. */
    pool->generations[idx]++;

    /* Push to free list (intrusive, same as lesson 08). */
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}

/* ── WITH generation check (SAFE) ── */

static thing *pool_get_ref(thing_pool *pool, thing_ref ref)
{
    /* Check: index in range, slot is used, AND generation matches.
     * If the slot was recycled, generation won't match -> stale ref -> nil. */
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]
        && pool->generations[ref.index] == ref.generation) {
        return &pool->things[ref.index];
    }
    return &pool->things[0]; /* nil sentinel */
}

/* ── WITHOUT generation check (UNSAFE — for comparison) ── */

static thing *pool_get_no_gen(thing_pool *pool, thing_ref ref)
{
    /* Ignores generation entirely. Returns whatever is in the slot.
     * If the slot was recycled (removed + re-added), you silently
     * get the WRONG entity. This is the stale-reference bug. */
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]) {
        return &pool->things[ref.index];
    }
    return &pool->things[0]; /* nil sentinel */
}

/* ══════════════════════════════════════════════════════ */
/*                      Tests                             */
/* ══════════════════════════════════════════════════════ */

static void test_1_add_troll(void)
{
    printf("=== Test 1: Add troll at slot 1 (generation 0) ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    pool.things[troll_ref.index].health = 100.0f;

    printf("  Added TROLL at slot %d, generation %u\n",
           troll_ref.index, troll_ref.generation);
    printf("  troll_ref = { index: %d, generation: %u }\n",
           troll_ref.index, troll_ref.generation);
    printf("  pool.generations[%d] = %u\n",
           troll_ref.index, pool.generations[troll_ref.index]);
    printf("  Health: %.0f\n\n", pool.things[troll_ref.index].health);

    assert(troll_ref.index == 1);
    assert(troll_ref.generation == 0);
}

static void test_2_remove_bumps_generation(void)
{
    printf("=== Test 2: Remove slot 1 — generation bumps to 1 ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    printf("  Before removal: generations[%d] = %u\n",
           troll_ref.index, pool.generations[troll_ref.index]);

    pool_remove(&pool, troll_ref.index);
    printf("  After removal:  generations[%d] = %u  (bumped!)\n",
           troll_ref.index, pool.generations[troll_ref.index]);
    printf("  troll_ref still holds generation %u (stale now)\n\n",
           troll_ref.generation);

    assert(pool.generations[troll_ref.index] == 1);
    assert(troll_ref.generation == 0);
}

static void test_3_reuse_slot_for_goblin(void)
{
    printf("=== Test 3: Add goblin at slot 1 (generation 1, reused via free list) ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    pool.things[troll_ref.index].health = 100.0f;

    pool_remove(&pool, troll_ref.index);

    thing_ref goblin_ref = pool_add(&pool, THING_KIND_GOBLIN);
    pool.things[goblin_ref.index].health = 30.0f;

    printf("  troll_ref  = { index: %d, generation: %u } (STALE)\n",
           troll_ref.index, troll_ref.generation);
    printf("  goblin_ref = { index: %d, generation: %u } (CURRENT)\n",
           goblin_ref.index, goblin_ref.generation);
    printf("  Both point to slot %d, but different generations!\n\n",
           troll_ref.index);

    assert(troll_ref.index == goblin_ref.index);    /* same slot */
    assert(troll_ref.generation != goblin_ref.generation); /* different gen */
    assert(goblin_ref.generation == 1);
}

static void test_4_with_gen_check(void)
{
    printf("=== Test 4: WITH generation check — stale ref detected (SAFE) ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Step 1: Add troll */
    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    pool.things[troll_ref.index].health = 100.0f;
    printf("  1. Added TROLL at slot %d, gen %u. Health=100.\n",
           troll_ref.index, troll_ref.generation);

    /* Step 2: Remove troll */
    pool_remove(&pool, troll_ref.index);
    printf("  2. Removed slot %d. Gen bumped to %u.\n",
           troll_ref.index, pool.generations[troll_ref.index]);

    /* Step 3: Add goblin (reuses slot via free list) */
    thing_ref goblin_ref = pool_add(&pool, THING_KIND_GOBLIN);
    pool.things[goblin_ref.index].health = 30.0f;
    printf("  3. Added GOBLIN at slot %d, gen %u. Health=30.\n",
           goblin_ref.index, goblin_ref.generation);

    /* Step 4: Try to access troll via stale ref */
    printf("\n  Using pool_get_ref(old_troll_ref):\n");
    thing *result = pool_get_ref(&pool, troll_ref);
    bool is_valid = thing_is_valid(result);

    printf("    troll_ref.generation = %u\n", troll_ref.generation);
    printf("    pool.generations[%d] = %u\n",
           troll_ref.index, pool.generations[troll_ref.index]);
    printf("    Match? %s\n", troll_ref.generation == pool.generations[troll_ref.index] ? "YES" : "NO");
    printf("    Result: %s\n", is_valid ? thing_kind_name(result->kind) : "nil (stale ref!)");
    printf("    \"Stale ref detected! Safe.\"\n\n");

    assert(!is_valid); /* Must be nil — the ref is stale */
}

static void test_5_without_gen_check(void)
{
    printf("=== Test 5: WITHOUT generation check — stale ref NOT detected (UNSAFE) ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Same setup as test 4 */
    thing_ref troll_ref = pool_add(&pool, THING_KIND_TROLL);
    pool.things[troll_ref.index].health = 100.0f;
    printf("  1. Added TROLL at slot %d, gen %u. Health=100.\n",
           troll_ref.index, troll_ref.generation);

    pool_remove(&pool, troll_ref.index);
    printf("  2. Removed slot %d.\n", troll_ref.index);

    thing_ref goblin_ref = pool_add(&pool, THING_KIND_GOBLIN);
    pool.things[goblin_ref.index].health = 30.0f;
    printf("  3. Added GOBLIN at slot %d, gen %u. Health=30.\n\n",
           goblin_ref.index, goblin_ref.generation);

    /* Try stale ref WITHOUT generation check */
    printf("  Using pool_get_no_gen(old_troll_ref):\n");
    thing *result = pool_get_no_gen(&pool, troll_ref);
    bool is_valid = thing_is_valid(result);

    printf("    Result: %s (valid=%s)\n",
           is_valid ? thing_kind_name(result->kind) : "nil",
           is_valid ? "true" : "false");

    if (is_valid) {
        printf("    Health: %.0f (this is the GOBLIN's health, not the troll's!)\n",
               result->health);
        printf("    \"Stale ref NOT detected! WRONG entity returned!\"\n\n");
    }

    /* The stale ref should have returned the goblin (wrong entity) */
    assert(is_valid);  /* It IS valid — that's the BUG */
    assert(result->kind == THING_KIND_GOBLIN); /* Got goblin, wanted troll */

    /* Suppress unused warning */
    (void)goblin_ref;
}

static void test_6_summary(void)
{
    printf("=== Test 6: Generational ID summary ===\n\n");

    printf("  The scenario:\n");
    printf("    1. Add TROLL at slot 1 (gen 0). Save troll_ref = {1, 0}.\n");
    printf("    2. Remove slot 1. Generation bumps to 1.\n");
    printf("    3. Add GOBLIN at slot 1 (gen 1). Slot reused via free list.\n");
    printf("    4. Access via old troll_ref {1, 0}.\n\n");

    printf("  +---------------------+------------------+------------------+\n");
    printf("  |                     | With gen check   | Without gen check|\n");
    printf("  +---------------------+------------------+------------------+\n");
    printf("  | pool_get(troll_ref) | nil (SAFE)       | GOBLIN (BUG!)    |\n");
    printf("  | Stale ref detected? | YES              | NO               |\n");
    printf("  | Crash?              | No               | No               |\n");
    printf("  | Silent corruption?  | No               | YES              |\n");
    printf("  +---------------------+------------------+------------------+\n\n");

    printf("  With generational IDs:    stale refs return nil (safe).\n");
    printf("  Without generational IDs: stale refs silently return wrong entity (bug).\n\n");

    printf("  The generation counter costs 4 bytes per slot (uint32_t).\n");
    printf("  That's %zu bytes for %d slots. Cheap insurance.\n\n",
           (size_t)MAX_THINGS * sizeof(uint32_t), MAX_THINGS);
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 09: Generational IDs (WITH/WITHOUT COMPARISON)       ║\n");
    printf("║  Pool: things[] + used[] + generations[] (NEW) + free list   ║\n");
    printf("║  thing_ref: { index, generation } — safe handle             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    test_1_add_troll();
    test_2_remove_bumps_generation();
    test_3_reuse_slot_for_goblin();
    test_4_with_gen_check();
    test_5_without_gen_check();
    test_6_summary();

    printf("══════════════════════════════════════════════════════\n");
    printf("  All lesson 09 tests passed.\n");
    printf("══════════════════════════════════════════════════════\n\n");

    return 0;
}
