/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_10.c — Pool Iterator + Complete System                      */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  COMPLETE pool. All features from lessons 07-09 integrated.  */
/*                                                                      */
/*  Thing struct: 32 bytes (kind, parent, first_child, next_sibling,    */
/*                prev_sibling, x, y, health)                           */
/*  Pool struct:  things[], used[], generations[], first_free,          */
/*                next_empty — the full system.                         */
/*  Iterator:     thing_iter { pool, current } — skips dead slots.      */
/*                                                                      */
/*  Complete API:                                                       */
/*    pool_init, pool_add, pool_remove, pool_get, pool_get_ref          */
/*    thing_add_child (circular), thing_unlink_child                    */
/*    thing_iter_begin, thing_iter_next, thing_iter_valid, thing_iter_get*/
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_10 lesson_10.c */
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

typedef struct thing_ref {
    thing_idx index;
    uint32_t  generation;
} thing_ref;

/* Thing struct: 32 bytes. */
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;

/* Complete pool: all features. */
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    uint32_t  generations[MAX_THINGS];
    thing_idx next_empty;
    thing_idx first_free;
} thing_pool;

/* Iterator: walks only living slots. */
typedef struct thing_iter {
    thing_pool *pool;
    thing_idx   current;
} thing_iter;

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
/*                Pool Lifecycle + CRUD                    */
/* ══════════════════════════════════════════════════════ */

static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}

static thing_ref pool_add(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    if (pool->first_free != 0) {
        slot = pool->first_free;
        pool->first_free = pool->things[slot].next_sibling;
    } else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    } else {
        return (thing_ref){0, 0};
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return (thing_ref){slot, pool->generations[slot]};
}

static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));
    pool->generations[idx]++;
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}

static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}

static thing *pool_get_ref(thing_pool *pool, thing_ref ref)
{
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]
        && pool->generations[ref.index] == ref.generation) {
        return &pool->things[ref.index];
    }
    return &pool->things[0];
}

/* ══════════════════════════════════════════════════════ */
/*           Intrusive Circular List (Parent/Child)        */
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

static void thing_unlink_child(thing_pool *pool, thing_idx child_idx)
{
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];

    if (child->next_sibling == child_idx) {
        parent->first_child = 0;
    } else {
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
/*                   Pool Iterator                        */
/* ══════════════════════════════════════════════════════ */

static thing_iter thing_iter_begin(thing_pool *pool)
{
    /* Start at index 1 (skip nil sentinel at 0).
     * Advance to the first used slot. */
    thing_iter it = {pool, 1};
    while (it.current < MAX_THINGS && !pool->used[it.current]) {
        it.current++;
    }
    return it;
}

static void thing_iter_next(thing_iter *it)
{
    it->current++;
    while (it->current < MAX_THINGS && !it->pool->used[it->current]) {
        it->current++;
    }
}

static bool thing_iter_valid(const thing_iter *it)
{
    return it->current > 0 && it->current < MAX_THINGS;
}

static thing *thing_iter_get(thing_iter *it)
{
    if (thing_iter_valid(it)) {
        return &it->pool->things[it->current];
    }
    return &it->pool->things[0];
}

/* ══════════════════════════════════════════════════════ */
/*                      Tests                             */
/* ══════════════════════════════════════════════════════ */

static void test_1_iterate_with_gaps(void)
{
    printf("=== Test 1: Add 8 things, remove 3 — iterate only living things ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    thing_kind kinds[8] = {
        THING_KIND_PLAYER, THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_PLAYER
    };
    thing_ref refs[8];
    for (int i = 0; i < 8; i++) {
        refs[i] = pool_add(&pool, kinds[i]);
        pool.things[refs[i].index].health = (float)((i + 1) * 10);
    }

    /* Remove slots 2, 4, 6 (0-indexed in refs array) to create gaps */
    thing_idx removed[3] = {refs[2].index, refs[4].index, refs[6].index};
    for (int i = 0; i < 3; i++) {
        pool_remove(&pool, removed[i]);
    }

    printf("  Pool state (slots 0..%d):\n", pool.next_empty - 1);
    printf("    Slot  Kind       used   health\n");
    printf("    ----  --------   -----  ------\n");
    for (int i = 0; i < pool.next_empty; i++) {
        printf("    [%d]   %-8s   %-5s  ",
               i, thing_kind_name(pool.things[i].kind),
               pool.used[i] ? "true" : "false");
        if (pool.used[i]) {
            printf("%.0f", pool.things[i].health);
        } else {
            printf("-");
        }
        if (i == 0) printf("   <- nil sentinel");
        printf("\n");
    }

    printf("\n  Iterator output (only living things):\n");
    for (thing_iter it = thing_iter_begin(&pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *t = thing_iter_get(&it);
        printf("    [%d] %-8s health=%.0f\n",
               it.current, thing_kind_name(t->kind), t->health);
    }
    printf("\n");
}

static void test_2_raw_loop_vs_iterator(void)
{
    printf("=== Test 2: Raw loop vs iterator ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    pool_add(&pool, THING_KIND_PLAYER);
    pool_add(&pool, THING_KIND_TROLL);
    thing_ref r3 = pool_add(&pool, THING_KIND_GOBLIN);
    pool_add(&pool, THING_KIND_ITEM);
    thing_ref r5 = pool_add(&pool, THING_KIND_TROLL);

    pool_remove(&pool, r3.index);
    pool_remove(&pool, r5.index);

    /* Raw loop: reaches into pool internals */
    printf("  Raw loop (reaches into pool internals):\n");
    printf("    for (int i = 1; i < pool.next_empty; i++) {\n");
    printf("        if (!pool.used[i]) continue;\n");
    printf("        thing *t = &pool.things[i];\n");
    printf("        // use t...\n");
    printf("    }\n");
    printf("    Result: ");
    for (int i = 1; i < pool.next_empty; i++) {
        if (!pool.used[i]) continue;
        printf("[%d]=%s ", i, thing_kind_name(pool.things[i].kind));
    }
    printf("\n\n");

    /* Iterator: clean API */
    printf("  Iterator (clean API):\n");
    printf("    for (thing_iter it = thing_iter_begin(&pool);\n");
    printf("         thing_iter_valid(&it);\n");
    printf("         thing_iter_next(&it)) {\n");
    printf("        thing *t = thing_iter_get(&it);\n");
    printf("        // use t...\n");
    printf("    }\n");
    printf("    Result: ");
    for (thing_iter it = thing_iter_begin(&pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *t = thing_iter_get(&it);
        printf("[%d]=%s ", it.current, thing_kind_name(t->kind));
    }
    printf("\n\n");

    printf("  Both produce the same output.\n");
    printf("  The iterator hides pool internals (used[], next_empty).\n");
    printf("  Game code uses the iterator. Pool internals stay private.\n\n");
}

static void test_3_count_via_iterator(void)
{
    printf("=== Test 3: Count living things via iterator ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add 8 things */
    thing_ref refs[8];
    thing_kind kinds[8] = {
        THING_KIND_PLAYER, THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_TROLL, THING_KIND_GOBLIN,
        THING_KIND_ITEM,   THING_KIND_PLAYER
    };
    for (int i = 0; i < 8; i++) {
        refs[i] = pool_add(&pool, kinds[i]);
    }

    /* Remove 3 things (indices 1, 3, 5 in our refs array) */
    pool_remove(&pool, refs[1].index);
    pool_remove(&pool, refs[3].index);
    pool_remove(&pool, refs[5].index);

    int total_slots = pool.next_empty - 1; /* excluding nil sentinel */
    int living = 0;

    for (thing_iter it = thing_iter_begin(&pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        living++;
    }

    int dead = total_slots - living;

    printf("  Total slots allocated: %d\n", total_slots);
    printf("  Living (via iterator): %d\n", living);
    printf("  Dead (gaps):           %d\n\n", dead);
    printf("  \"Iterator found %d living things, skipped %d dead slots.\"\n\n",
           living, dead);

    assert(living == 5);
    assert(dead == 3);
}

static void test_4_complete_api_demo(void)
{
    printf("=== Test 4: Complete pool API demonstration ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* --- pool_add --- */
    thing_ref player_ref = pool_add(&pool, THING_KIND_PLAYER);
    thing_ref sword_ref  = pool_add(&pool, THING_KIND_ITEM);
    thing_ref shield_ref = pool_add(&pool, THING_KIND_ITEM);
    thing_ref troll_ref  = pool_add(&pool, THING_KIND_TROLL);

    pool.things[player_ref.index].health = 100.0f;
    pool.things[player_ref.index].x = 5.0f;
    pool.things[player_ref.index].y = 3.0f;
    pool.things[sword_ref.index].health = 50.0f;
    pool.things[shield_ref.index].health = 80.0f;
    pool.things[troll_ref.index].health = 60.0f;
    pool.things[troll_ref.index].x = 10.0f;

    printf("  pool_add:  Created PLAYER(slot %d), SWORD(slot %d), SHIELD(slot %d), TROLL(slot %d)\n",
           player_ref.index, sword_ref.index, shield_ref.index, troll_ref.index);

    /* --- thing_add_child (circular doubly-linked) --- */
    thing_add_child(&pool, player_ref.index, sword_ref.index);
    thing_add_child(&pool, player_ref.index, shield_ref.index);
    printf("  thing_add_child: SWORD and SHIELD are now children of PLAYER\n");

    /* Verify circular list */
    thing *player = pool_get(&pool, player_ref.index);
    thing_idx first = player->first_child;
    printf("    PLAYER.first_child = slot %d (%s)\n",
           first, thing_kind_name(pool.things[first].kind));
    thing_idx second = pool.things[first].next_sibling;
    printf("    -> next_sibling = slot %d (%s)\n",
           second, thing_kind_name(pool.things[second].kind));
    thing_idx back_to_first = pool.things[second].next_sibling;
    printf("    -> next_sibling = slot %d (circular, back to first)\n", back_to_first);
    assert(back_to_first == first);

    /* --- thing_unlink_child --- */
    thing_unlink_child(&pool, sword_ref.index);
    printf("  thing_unlink_child: SWORD unlinked from PLAYER\n");
    player = pool_get(&pool, player_ref.index);
    printf("    PLAYER.first_child = slot %d (%s) — only SHIELD remains\n",
           player->first_child,
           thing_kind_name(pool.things[player->first_child].kind));
    assert(player->first_child == shield_ref.index);

    /* --- pool_get --- */
    thing *t = pool_get(&pool, player_ref.index);
    printf("  pool_get(%d): %s at (%.0f, %.0f) health=%.0f\n",
           player_ref.index, thing_kind_name(t->kind), t->x, t->y, t->health);

    /* --- pool_get_ref (generational) --- */
    thing *t2 = pool_get_ref(&pool, player_ref);
    printf("  pool_get_ref({%d, %u}): %s (generation matches)\n",
           player_ref.index, player_ref.generation, thing_kind_name(t2->kind));
    assert(thing_is_valid(t2));

    /* --- pool_remove + stale ref --- */
    pool_remove(&pool, troll_ref.index);
    thing *stale = pool_get_ref(&pool, troll_ref);
    printf("  pool_remove(%d): TROLL removed\n", troll_ref.index);
    printf("  pool_get_ref(old_troll_ref): %s (stale ref caught)\n",
           thing_is_valid(stale) ? thing_kind_name(stale->kind) : "nil");
    assert(!thing_is_valid(stale));

    /* --- Iterator --- */
    printf("  Iterating all living things:\n");
    for (thing_iter it = thing_iter_begin(&pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        thing *ithing = thing_iter_get(&it);
        printf("    [%d] %-8s health=%.0f\n",
               it.current, thing_kind_name(ithing->kind), ithing->health);
    }

    printf("\n  Complete pool API summary:\n");
    printf("    +---------------------------+----------------------------------+\n");
    printf("    | Function                  | Purpose                          |\n");
    printf("    +---------------------------+----------------------------------+\n");
    printf("    | pool_init                 | Zero everything, set next_empty=1|\n");
    printf("    | pool_add                  | Alloc slot (free list -> fresh)  |\n");
    printf("    | pool_remove               | Free slot, bump gen, push free  |\n");
    printf("    | pool_get                  | Get by raw index (bounds+used)  |\n");
    printf("    | pool_get_ref              | Get by ref (+ generation check) |\n");
    printf("    | thing_add_child           | Circular doubly-linked prepend  |\n");
    printf("    | thing_unlink_child        | Remove from parent's child list |\n");
    printf("    | thing_iter_begin          | Start iterator at first living  |\n");
    printf("    | thing_iter_next           | Advance to next living slot     |\n");
    printf("    | thing_iter_valid          | Check if iterator is in range   |\n");
    printf("    | thing_iter_get            | Dereference iterator -> thing*  |\n");
    printf("    +---------------------------+----------------------------------+\n\n");
}

static void test_5_final_message(void)
{
    printf("=== Test 5: Final verification ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Full cycle: add, parent, remove, iterate, ref check */
    thing_ref a = pool_add(&pool, THING_KIND_PLAYER);
    thing_ref b = pool_add(&pool, THING_KIND_TROLL);
    thing_ref c = pool_add(&pool, THING_KIND_GOBLIN);
    thing_ref d = pool_add(&pool, THING_KIND_ITEM);
    thing_ref e = pool_add(&pool, THING_KIND_TROLL);

    pool.things[a.index].health = 100.0f;
    pool.things[b.index].health = 80.0f;
    pool.things[c.index].health = 50.0f;
    pool.things[d.index].health = 0.0f;
    pool.things[e.index].health = 90.0f;

    thing_add_child(&pool, a.index, d.index);

    pool_remove(&pool, b.index);
    pool_remove(&pool, c.index);

    /* Verify: refs to removed things are stale */
    assert(!thing_is_valid(pool_get_ref(&pool, b)));
    assert(!thing_is_valid(pool_get_ref(&pool, c)));

    /* Verify: refs to living things work */
    assert(thing_is_valid(pool_get_ref(&pool, a)));
    assert(thing_is_valid(pool_get_ref(&pool, d)));
    assert(thing_is_valid(pool_get_ref(&pool, e)));

    /* Verify: free list reuse */
    thing_ref f = pool_add(&pool, THING_KIND_GOBLIN);
    assert(f.index == c.index || f.index == b.index); /* reused a freed slot */
    assert(f.generation > 0); /* generation was bumped */

    /* Verify: iterator skips dead slots */
    int count = 0;
    for (thing_iter it = thing_iter_begin(&pool);
         thing_iter_valid(&it);
         thing_iter_next(&it))
    {
        count++;
    }
    assert(count == 4); /* a, d, e, f — b and c were removed, one slot reused */

    /* Verify: parent-child link */
    thing *player = pool_get(&pool, a.index);
    assert(player->first_child == d.index);

    printf("  Full cycle verified:\n");
    printf("    - Add 5 things                   [OK]\n");
    printf("    - Parent-child linking            [OK]\n");
    printf("    - Remove 2 (stale refs detected)  [OK]\n");
    printf("    - Free list reuse                 [OK]\n");
    printf("    - Generational ID safety          [OK]\n");
    printf("    - Iterator skips dead slots        [OK]\n\n");

    printf("  Pool system complete. All features working.\n");
    printf("  Ready for game integration.\n\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 10: Pool Iterator + Complete System               ║\n");
    printf("║  Complete pool: things[] + used[] + generations[]         ║\n");
    printf("║  + free list + iterator + parent-child tree               ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    test_1_iterate_with_gaps();
    test_2_raw_loop_vs_iterator();
    test_3_count_via_iterator();
    test_4_complete_api_demo();
    test_5_final_message();

    printf("══════════════════════════════════════════════════════\n");
    printf("  All lesson 10 tests passed.\n");
    printf("══════════════════════════════════════════════════════\n\n");

    return 0;
}
