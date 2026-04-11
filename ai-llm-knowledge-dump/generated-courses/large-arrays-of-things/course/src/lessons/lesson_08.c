/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_08.c — Free List for Slot Reuse (WITH/WITHOUT COMPARISON)   */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  Pool grows: add first_free field.                           */
/*          Dead things store next_free in next_sibling (intrusive).    */
/*                                                                      */
/*  Thing struct: same 32 bytes as lesson 07.                           */
/*  Pool struct:  things[], used[], next_empty, first_free (NEW).       */
/*                                                                      */
/*  TWO add strategies compared side-by-side:                           */
/*    - pool_add_no_freelist(): always bumps next_empty (wasteful)      */
/*    - pool_add_with_freelist(): checks first_free first (efficient)   */
/*                                                                      */
/*  Compile:                                                            */
/*    gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_08 lesson_08.c */
/* ══════════════════════════════════════════════════════════════════════ */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 512

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

/* Thing struct: 32 bytes — same as lesson 07. */
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;   /* DOUBLE DUTY: sibling link when alive,
                                  free list link when dead */
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;

/* Pool struct: NOW includes first_free for the free list. */
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    thing_idx next_empty;        /* next never-used slot (starts at 1) */
    thing_idx first_free;        /* head of intrusive free list (0 = empty) */
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
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
    /* pool->first_free is already 0 from memset (empty free list) */
}

/* Remove: adds slot to the intrusive free list.
 * The dead thing's next_sibling field stores the next free slot index.
 * This is the "intrusive" trick — no external data structure needed. */
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    /* Push this slot onto the free list (LIFO stack). */
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}

/* ── Strategy A: Add WITHOUT free list (wasteful) ── */

static thing_idx pool_add_no_freelist(thing_pool *pool, thing_kind kind)
{
    /* Always bump next_empty. Ignores freed slots entirely.
     * Removed slots become permanent dead gaps. */
    if (pool->next_empty >= MAX_THINGS) {
        return 0; /* pool full */
    }

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

/* ── Strategy B: Add WITH free list (efficient) ── */

static thing_idx pool_add_with_freelist(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0;

    /* Step 1: Try the free list first (recycled slots). */
    if (pool->first_free != 0) {
        slot = pool->first_free;
        /* Follow the intrusive linked list to the next free slot. */
        pool->first_free = pool->things[slot].next_sibling;
    }
    /* Step 2: No free slots — use the never-used region. */
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    /* Step 3: Pool is full. */
    else {
        return 0;
    }

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

/* ══════════════════════════════════════════════════════ */
/*                      Tests                             */
/* ══════════════════════════════════════════════════════ */

static void test_1_without_freelist(void)
{
    printf("=== Test 1: WITHOUT free list — slot waste ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add 10 things */
    thing_idx slots[10];
    for (int i = 0; i < 10; i++) {
        slots[i] = pool_add_no_freelist(&pool, THING_KIND_TROLL);
    }
    printf("  Added 10 things. next_empty = %d\n", pool.next_empty);

    /* Remove 5 (even-indexed) */
    for (int i = 0; i < 10; i += 2) {
        pool_remove(&pool, slots[i]);
    }
    printf("  Removed 5 things (slots %d, %d, %d, %d, %d).\n",
           slots[0], slots[2], slots[4], slots[6], slots[8]);
    printf("  first_free = %d (free list built from removals)\n", pool.first_free);

    /* Add 5 more — but without free list, they go to fresh slots */
    /* Reset first_free to simulate no-freelist behavior */
    pool.first_free = 0;
    for (int i = 0; i < 5; i++) {
        pool_add_no_freelist(&pool, THING_KIND_GOBLIN);
    }
    printf("  Added 5 more WITHOUT free list.\n");
    printf("  next_empty = %d\n", pool.next_empty);

    /* Count living things */
    int living = 0;
    for (int i = 1; i < pool.next_empty; i++) {
        if (pool.used[i]) living++;
    }
    int wasted = pool.next_empty - 1 - living;
    printf("  Living: %d, Wasted gaps: %d\n", living, wasted);
    printf("  \"%d slots used, %d wasted.\"\n\n", living, wasted);
}

static void test_2_with_freelist(void)
{
    printf("=== Test 2: WITH free list — slot reuse ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add 10 things */
    thing_idx slots[10];
    for (int i = 0; i < 10; i++) {
        slots[i] = pool_add_with_freelist(&pool, THING_KIND_TROLL);
    }
    printf("  Added 10 things. next_empty = %d\n", pool.next_empty);

    /* Remove 5 (even-indexed) */
    for (int i = 0; i < 10; i += 2) {
        pool_remove(&pool, slots[i]);
    }
    printf("  Removed 5 things. first_free = %d\n", pool.first_free);

    /* Add 5 more — WITH free list, they reuse freed slots */
    for (int i = 0; i < 5; i++) {
        thing_idx s = pool_add_with_freelist(&pool, THING_KIND_GOBLIN);
        printf("  Reused slot %d for GOBLIN\n", s);
    }
    printf("  next_empty = %d (still %d!)\n", pool.next_empty, pool.next_empty);

    /* Count living things */
    int living = 0;
    for (int i = 1; i < pool.next_empty; i++) {
        if (pool.used[i]) living++;
    }
    int wasted = pool.next_empty - 1 - living;
    printf("  Living: %d, Wasted gaps: %d\n", living, wasted);
    printf("  \"Freed slots reused! %d wasted.\"\n\n", wasted);
}

static void test_3_free_list_chain(void)
{
    printf("=== Test 3: Free list chain visualization ===\n\n");

    thing_pool pool;
    pool_init(&pool);

    /* Add 10 things (slots 1..10) */
    for (int i = 0; i < 10; i++) {
        pool_add_with_freelist(&pool, THING_KIND_TROLL);
    }

    /* Remove slots 3, 5, 7 in that order.
     * Free list is LIFO: last removed = head. */
    pool_remove(&pool, 3);
    printf("  Removed slot 3. Free list: first_free = %d\n", pool.first_free);
    pool_remove(&pool, 5);
    printf("  Removed slot 5. Free list: first_free = %d\n", pool.first_free);
    pool_remove(&pool, 7);
    printf("  Removed slot 7. Free list: first_free = %d\n", pool.first_free);

    /* Walk the free list to show the chain */
    printf("\n  Free list chain: ");
    thing_idx current = pool.first_free;
    while (current != 0) {
        printf("%d", current);
        current = pool.things[current].next_sibling;
        if (current != 0) printf(" -> ");
    }
    printf(" -> 0 (end)\n");
    printf("  first_free=%d -> %d -> %d -> 0\n\n",
           pool.first_free,
           pool.things[pool.first_free].next_sibling,
           pool.things[pool.things[pool.first_free].next_sibling].next_sibling);
}

static void test_4_rapid_spawn_kill(void)
{
    printf("=== Test 4: Rapid spawn/kill cycle comparison ===\n\n");

    /* --- Without free list --- */
    thing_pool pool_no_fl;
    pool_init(&pool_no_fl);

    /* Add 100 */
    thing_idx no_fl_slots[100];
    for (int i = 0; i < 100; i++) {
        no_fl_slots[i] = pool_add_no_freelist(&pool_no_fl, THING_KIND_TROLL);
    }
    /* Remove 80 */
    for (int i = 0; i < 80; i++) {
        pool_remove(&pool_no_fl, no_fl_slots[i]);
    }
    /* Disable the free list that pool_remove built */
    pool_no_fl.first_free = 0;
    /* Add 80 more */
    for (int i = 0; i < 80; i++) {
        pool_add_no_freelist(&pool_no_fl, THING_KIND_GOBLIN);
    }

    printf("  WITHOUT free list: Add 100, remove 80, add 80.\n");
    printf("    next_empty = %d\n", pool_no_fl.next_empty);

    int no_fl_living = 0;
    for (int i = 1; i < pool_no_fl.next_empty; i++) {
        if (pool_no_fl.used[i]) no_fl_living++;
    }
    printf("    Living: %d, Wasted: %d\n\n", no_fl_living,
           pool_no_fl.next_empty - 1 - no_fl_living);

    /* --- With free list --- */
    thing_pool pool_fl;
    pool_init(&pool_fl);

    /* Add 100 */
    thing_idx fl_slots[100];
    for (int i = 0; i < 100; i++) {
        fl_slots[i] = pool_add_with_freelist(&pool_fl, THING_KIND_TROLL);
    }
    /* Remove 80 */
    for (int i = 0; i < 80; i++) {
        pool_remove(&pool_fl, fl_slots[i]);
    }
    /* Add 80 more — these reuse freed slots */
    for (int i = 0; i < 80; i++) {
        pool_add_with_freelist(&pool_fl, THING_KIND_GOBLIN);
    }

    printf("  WITH free list: Add 100, remove 80, add 80.\n");
    printf("    next_empty = %d\n", pool_fl.next_empty);

    int fl_living = 0;
    for (int i = 1; i < pool_fl.next_empty; i++) {
        if (pool_fl.used[i]) fl_living++;
    }
    printf("    Living: %d, Wasted: %d\n\n", fl_living,
           pool_fl.next_empty - 1 - fl_living);

    assert(pool_no_fl.next_empty > pool_fl.next_empty);
}

static void test_5_summary_table(void)
{
    printf("=== Test 5: Summary comparison ===\n\n");

    printf("  +-------------------+-----------+----------+\n");
    printf("  | Scenario          | No FL     | With FL  |\n");
    printf("  |                   | next_empty| next_empty|\n");
    printf("  +-------------------+-----------+----------+\n");
    printf("  | Add 10, rm 5,     |           |          |\n");
    printf("  |   add 5           |    16     |    11    |\n");
    printf("  +-------------------+-----------+----------+\n");
    printf("  | Add 100, rm 80,   |           |          |\n");
    printf("  |   add 80          |   181     |   101    |\n");
    printf("  +-------------------+-----------+----------+\n\n");

    printf("  Without free list: removed slots are permanent dead gaps.\n");
    printf("    next_empty grows forever. Pool runs out of space faster.\n\n");
    printf("  With free list: removed slots are recycled.\n");
    printf("    next_empty only grows when the free list is empty.\n");
    printf("    Pool stays compact.\n\n");

    printf("  The free list is INTRUSIVE: dead things store the\n");
    printf("  \"next free\" index in their next_sibling field.\n");
    printf("  Zero external storage. Zero allocation. O(1) push/pop.\n\n");
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Lesson 08: Free List for Slot Reuse (WITH/WITHOUT)       ║\n");
    printf("║  Pool: things[] + used[] + next_empty + first_free (NEW)  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    /* Suppress unused warning for thing_kind_name */
    (void)thing_kind_name;

    test_1_without_freelist();
    test_2_with_freelist();
    test_3_free_list_chain();
    test_4_rapid_spawn_kill();
    test_5_summary_table();

    printf("══════════════════════════════════════════════════════\n");
    printf("  All lesson 08 tests passed.\n");
    printf("══════════════════════════════════════════════════════\n\n");

    return 0;
}
