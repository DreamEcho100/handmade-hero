/* ══════════════════════════════════════════════════════════════════════ */
/*               test_harness.c — Standalone Pool Exerciser              */
/*                                                                       */
/*  Exercises every feature of the things pool system:                   */
/*    - Add, remove, get (lessons 02, 07)                                */
/*    - Indices not pointers (lesson 03)                                 */
/*    - Intrusive linked lists (lesson 04)                               */
/*    - Intrusive trees with circular lists (lesson 05)                  */
/*    - Nil sentinel and zero initialization (lesson 06)                 */
/*    - Free list slot reuse (lesson 08)                                 */
/*    - Generational IDs (lesson 09)                                     */
/*    - Pool iterator (lesson 10)                                        */
/*                                                                       */
/*  No platform dependency. Compiles with:                               */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG \                      */
/*        -o test_harness test_harness.c things/things.c                 */
/* ══════════════════════════════════════════════════════════════════════ */
#include "things/things.h"
#include <stdio.h>
#include <assert.h>

/* ── Helper: print a horizontal rule ── */
static void hr(const char *title)
{
    printf("\n══════ %s ══════\n", title);
}

/* ── Helper: print pool contents ── */
static void print_pool(const thing_pool *pool)
{
    printf("  Pool state (next_empty=%d, first_free=%d):\n",
           pool->next_empty, pool->first_free);
    for (int i = 0; i < pool->next_empty && i < MAX_THINGS; i++) {
        const thing *t = &pool->things[i];
        printf("    [%2d] kind=%-7s used=%d gen=%u",
               i, thing_kind_name(t->kind), pool->used[i],
               pool->generations[i]);
        if (i == 0) printf("  ← NIL SENTINEL");
        printf("\n");
    }
}

/* ── Helper: print a thing's children by walking the intrusive list ── */
static void print_children(thing_pool *pool, thing_idx parent_idx)
{
    thing *parent = thing_pool_get(pool, parent_idx);
    printf("  %s (slot %d) children: ",
           thing_kind_name(parent->kind), parent_idx);

    thing_idx first = parent->first_child;
    if (first == 0) {
        printf("(none)\n");
        return;
    }

    /* Walk circular sibling list starting at first_child. */
    thing_idx current = first;
    int safety = 0;
    do {
        thing *child = thing_pool_get(pool, current);
        printf("%s(slot %d)", thing_kind_name(child->kind), current);
        current = child->next_sibling;
        if (current != first) printf(" -> ");
        if (++safety > MAX_THINGS) { printf(" [INFINITE LOOP!]"); break; }
    } while (current != first);
    printf(" -> (circle)\n");
}

/* ══════════════════════════════════════════════════════ */
/*                   Test: Basic Add/Get                   */
/* ══════════════════════════════════════════════════════ */

static void test_basic_add_get(void)
{
    hr("Test: Basic Add / Get (Lesson 02, 07)");

    thing_pool pool;
    thing_pool_init(&pool);

    thing_ref player_ref = thing_pool_add(&pool, THING_KIND_PLAYER);
    thing_ref troll_ref  = thing_pool_add(&pool, THING_KIND_TROLL);
    thing_ref goblin_ref = thing_pool_add(&pool, THING_KIND_GOBLIN);

    printf("  Added PLAYER at slot %d (gen %u)\n",
           player_ref.index, player_ref.generation);
    printf("  Added TROLL  at slot %d (gen %u)\n",
           troll_ref.index, troll_ref.generation);
    printf("  Added GOBLIN at slot %d (gen %u)\n",
           goblin_ref.index, goblin_ref.generation);

    /* Verify get returns the right things. */
    thing *player = thing_pool_get(&pool, player_ref.index);
    assert(player->kind == THING_KIND_PLAYER);
    printf("  Get slot %d -> kind=%s ✓\n",
           player_ref.index, thing_kind_name(player->kind));

    /* Verify getting nil sentinel. */
    thing *nil = thing_pool_get(&pool, 0);
    assert(!thing_is_valid(nil));
    printf("  Get slot 0 -> kind=%s (nil) ✓\n", thing_kind_name(nil->kind));

    /* Verify out-of-bounds returns nil. */
    thing *oob = thing_pool_get(&pool, 9999);
    assert(!thing_is_valid(oob));
    printf("  Get slot 9999 -> kind=%s (nil, out-of-bounds) ✓\n",
           thing_kind_name(oob->kind));

    print_pool(&pool);
    thing_pool_assert_nil_clean(&pool);
    printf("  PASSED\n");
}

/* ══════════════════════════════════════════════════════ */
/*                   Test: Intrusive Lists                 */
/* ══════════════════════════════════════════════════════ */

static void test_intrusive_lists(void)
{
    hr("Test: Intrusive Linked Lists (Lesson 04)");

    thing_pool pool;
    thing_pool_init(&pool);

    thing_ref player = thing_pool_add(&pool, THING_KIND_PLAYER);
    thing_ref axe    = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_ref potion = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_ref food   = thing_pool_add(&pool, THING_KIND_ITEM);

    /* Give the items names via position (hack for testing). */
    thing_pool_get(&pool, axe.index)->x    = 1.0f; /* "axe"    */
    thing_pool_get(&pool, potion.index)->x = 2.0f; /* "potion" */
    thing_pool_get(&pool, food.index)->x   = 3.0f; /* "food"   */

    /* Add items to player's inventory (prepend order: food, potion, axe). */
    thing_add_child(&pool, player.index, axe.index);
    thing_add_child(&pool, player.index, potion.index);
    thing_add_child(&pool, player.index, food.index);

    printf("  After adding 3 items to player's inventory:\n");
    print_children(&pool, player.index);

    /* Verify parent links. */
    assert(thing_pool_get(&pool, axe.index)->parent == player.index);
    assert(thing_pool_get(&pool, potion.index)->parent == player.index);
    assert(thing_pool_get(&pool, food.index)->parent == player.index);
    printf("  All parent links correct ✓\n");

    /* Remove middle item (potion). */
    thing_unlink_child(&pool, potion.index);
    printf("  After unlinking potion:\n");
    print_children(&pool, player.index);

    /* Verify potion's parent is cleared. */
    assert(thing_pool_get(&pool, potion.index)->parent == 0);
    printf("  Potion parent cleared ✓\n");

    thing_pool_assert_nil_clean(&pool);
    printf("  PASSED\n");
}

/* ══════════════════════════════════════════════════════ */
/*                   Test: Nil Sentinel                    */
/* ══════════════════════════════════════════════════════ */

static void test_nil_sentinel(void)
{
    hr("Test: Nil Sentinel (Lesson 06)");

    thing_pool pool;
    thing_pool_init(&pool);

    thing_ref player = thing_pool_add(&pool, THING_KIND_PLAYER);

    /* Scenario 1: Out of bounds — returns nil, doesn't crash. */
    thing *t1 = thing_pool_get(&pool, -5);
    printf("  [1] Out-of-bounds (-5): kind=%s ✓\n", thing_kind_name(t1->kind));
    assert(!thing_is_valid(t1));

    /* Scenario 2: Unused slot — returns nil. */
    thing *t2 = thing_pool_get(&pool, 500);
    printf("  [2] Unused slot (500): kind=%s ✓\n", thing_kind_name(t2->kind));
    assert(!thing_is_valid(t2));

    /* Scenario 3: Removed entity — returns nil. */
    thing_pool_remove(&pool, player.index);
    thing *t3 = thing_pool_get(&pool, player.index);
    printf("  [3] Removed entity (%d): kind=%s ✓\n",
           player.index, thing_kind_name(t3->kind));
    assert(!thing_is_valid(t3));

    /* Scenario 4: Stale ref (gen mismatch) — returns nil. */
    thing_ref new_ref = thing_pool_add(&pool, THING_KIND_TROLL);
    thing *t4 = thing_pool_get_ref(&pool, player); /* old ref, old gen */
    printf("  [4] Stale ref (gen %u vs %u): kind=%s ✓\n",
           player.generation, pool.generations[player.index],
           thing_kind_name(t4->kind));
    assert(!thing_is_valid(t4));
    (void)new_ref;

    /* Scenario 5: Chain dereference through nil. */
    thing *chain = thing_pool_get(&pool,
                     thing_pool_get(&pool,
                       thing_pool_get(&pool, 0)->first_child
                     )->next_sibling
                   );
    printf("  [5] Chain deref through nil: kind=%s ✓\n",
           thing_kind_name(chain->kind));
    assert(!thing_is_valid(chain));

    thing_pool_assert_nil_clean(&pool);
    printf("  All 5 crash scenarios returned nil safely. PASSED\n");
}

/* ══════════════════════════════════════════════════════ */
/*                   Test: Free List                       */
/* ══════════════════════════════════════════════════════ */

static void test_free_list(void)
{
    hr("Test: Free List Slot Reuse (Lesson 08)");

    thing_pool pool;
    thing_pool_init(&pool);

    /* Add 5 things. */
    thing_ref refs[5];
    for (int i = 0; i < 5; i++) {
        refs[i] = thing_pool_add(&pool, THING_KIND_TROLL);
        printf("  Add: slot %d\n", refs[i].index);
    }

    /* Remove slots 2 and 4 (they go onto the free list). */
    printf("  Remove: slot %d\n", refs[1].index); /* slot 2 */
    thing_pool_remove(&pool, refs[1].index);
    printf("  Remove: slot %d\n", refs[3].index); /* slot 4 */
    thing_pool_remove(&pool, refs[3].index);

    printf("  Free list head: %d\n", pool.first_free);

    /* Add 2 new things — should reuse freed slots. */
    thing_ref new1 = thing_pool_add(&pool, THING_KIND_GOBLIN);
    thing_ref new2 = thing_pool_add(&pool, THING_KIND_GOBLIN);
    printf("  Add (reuse): slot %d (expected recycled)\n", new1.index);
    printf("  Add (reuse): slot %d (expected recycled)\n", new2.index);

    /* The recycled slots should be 4 and 2 (LIFO order). */
    assert(new1.index == refs[3].index || new1.index == refs[1].index);
    assert(new2.index == refs[3].index || new2.index == refs[1].index);
    assert(new1.index != new2.index);

    /* Add one more — should use next_empty (slot 6). */
    thing_ref new3 = thing_pool_add(&pool, THING_KIND_ITEM);
    printf("  Add (fresh): slot %d (expected %d)\n", new3.index, 6);
    assert(new3.index == 6);

    print_pool(&pool);
    thing_pool_assert_nil_clean(&pool);
    printf("  PASSED\n");
}

/* ══════════════════════════════════════════════════════ */
/*                   Test: Generational IDs                */
/* ══════════════════════════════════════════════════════ */

static void test_generational_ids(void)
{
    hr("Test: Generational IDs (Lesson 09)");

    thing_pool pool;
    thing_pool_init(&pool);

    /* Add a troll, save ref. */
    thing_ref troll = thing_pool_add(&pool, THING_KIND_TROLL);
    printf("  Added TROLL at slot %d gen %u\n", troll.index, troll.generation);

    /* Remove it. */
    thing_pool_remove(&pool, troll.index);
    printf("  Removed slot %d (gen bumped to %u)\n",
           troll.index, pool.generations[troll.index]);

    /* Add a goblin — reuses the same slot. */
    thing_ref goblin = thing_pool_add(&pool, THING_KIND_GOBLIN);
    printf("  Added GOBLIN at slot %d gen %u\n",
           goblin.index, goblin.generation);
    assert(goblin.index == troll.index); /* same slot reused */

    /* Old troll ref is STALE — generation doesn't match. */
    thing *stale = thing_pool_get_ref(&pool, troll);
    printf("  Old troll ref (gen %u) -> kind=%s (should be NIL)\n",
           troll.generation, thing_kind_name(stale->kind));
    assert(!thing_is_valid(stale));

    /* New goblin ref is VALID. */
    thing *valid = thing_pool_get_ref(&pool, goblin);
    printf("  New goblin ref (gen %u) -> kind=%s (should be GOBLIN)\n",
           goblin.generation, thing_kind_name(valid->kind));
    assert(valid->kind == THING_KIND_GOBLIN);

    thing_pool_assert_nil_clean(&pool);
    printf("  PASSED\n");
}

/* ══════════════════════════════════════════════════════ */
/*                   Test: Iterator                        */
/* ══════════════════════════════════════════════════════ */

static void test_iterator(void)
{
    hr("Test: Pool Iterator (Lesson 10)");

    thing_pool pool;
    thing_pool_init(&pool);

    /* Add 5 things, remove 2 to create gaps. */
    thing_ref r0 = thing_pool_add(&pool, THING_KIND_PLAYER);
    thing_ref r1 = thing_pool_add(&pool, THING_KIND_TROLL);
    thing_ref r2 = thing_pool_add(&pool, THING_KIND_GOBLIN);
    thing_ref r3 = thing_pool_add(&pool, THING_KIND_TROLL);
    thing_ref r4 = thing_pool_add(&pool, THING_KIND_ITEM);
    (void)r0; (void)r2; (void)r4;

    thing_pool_remove(&pool, r1.index);
    thing_pool_remove(&pool, r3.index);

    printf("  Living things (should skip removed slots):\n");
    int count = 0;
    for (thing_iter it = thing_iter_begin(&pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *t = thing_iter_get(&it);
        printf("    slot %d: %s\n", it.current, thing_kind_name(t->kind));
        assert(thing_is_valid(t));
        count++;
    }
    printf("  Iterated %d things (expected 3)\n", count);
    assert(count == 3);

    thing_pool_assert_nil_clean(&pool);
    printf("  PASSED\n");
}

/* ══════════════════════════════════════════════════════ */
/*                        Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("Large Arrays of Things — Test Harness\n");
    printf("sizeof(thing) = %zu bytes\n", sizeof(thing));
    printf("sizeof(thing_pool) = %zu bytes (~%.1f KB)\n",
           sizeof(thing_pool), (double)sizeof(thing_pool) / 1024.0);

    test_basic_add_get();
    test_intrusive_lists();
    test_nil_sentinel();
    test_free_list();
    test_generational_ids();
    test_iterator();

    hr("ALL TESTS PASSED");
    return 0;
}
