/* ══════════════════════════════════════════════════════════════════════ */
/*               things.c — Large Arrays of Things                       */
/*                                                                       */
/*  Implementation of the thing pool system.                             */
/*  See things.h for the public API and design philosophy.               */
/* ══════════════════════════════════════════════════════════════════════ */
#include "things.h"

#include <assert.h>
#include <string.h> /* memset */

/* ══════════════════════════════════════════════════════ */
/*                   Pool Lifecycle                        */
/* ══════════════════════════════════════════════════════ */

void thing_pool_init(thing_pool *pool)
{
    /* WHY: Zero EVERYTHING — things, used flags, generations, free list.
     * This makes things[0] the nil sentinel (all zeros = THING_KIND_NIL).
     * We zero the entire struct in one shot because that's the whole point
     * of the zero-init architecture: memset is our universal reset. */
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;  /* slot 0 is reserved for nil sentinel */
    /* pool->first_free is already 0 from memset (empty free list) */

    /* JS: Array.fill(null) — everything starts as "nothing".
     * C:  memset to zero achieves this because THING_KIND_NIL = 0. */
}

/* ══════════════════════════════════════════════════════ */
/*                   Add / Remove / Get                    */
/* ══════════════════════════════════════════════════════ */

thing_ref thing_pool_add(thing_pool *pool, thing_kind kind)
{
    thing_idx slot = 0; /* default: nil (pool full) */

    /* Step 1: Try the free list first (recycled slots).
     * JS: connection pool — reuse freed connections before creating new ones.
     * WHY: Free list gives O(1) slot reuse without scanning the array. */
    if (pool->first_free != 0) {
        slot = pool->first_free;
        /* The free list is intrusive: dead things store the next free index
         * in their next_sibling field (it's unused since the thing is dead). */
        pool->first_free = pool->things[slot].next_sibling;
    }
    /* Step 2: If no free slots, try the never-used region. */
    else if (pool->next_empty < MAX_THINGS) {
        slot = pool->next_empty;
        pool->next_empty++;
    }
    /* Step 3: Pool is full. Return nil ref. */
    else {
        return (thing_ref){0, 0};
    }

    /* Zero the slot, then set kind. Zeroing ensures all tree links,
     * position, velocity, etc. start at safe zero values. */
    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    /* Return a ref with the CURRENT generation of this slot.
     * If this slot was recycled, generation was already bumped in remove(). */
    return (thing_ref){slot, pool->generations[slot]};
}

void thing_pool_remove(thing_pool *pool, thing_idx idx)
{
    /* Removing nil (index 0) is a no-op. Removing out-of-bounds is a no-op.
     * Removing an already-unused slot is a no-op.
     * WHY: Defensive — game code shouldn't crash on double-remove. */
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    /* Mark unused and zero the thing. */
    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));

    /* Bump generation so any existing thing_ref to this slot becomes stale.
     * JS: like auto-increment ID — old refs to "row 5 gen 3" won't match
     *     the new "row 5 gen 4". */
    pool->generations[idx]++;

    /* Add to intrusive free list. We reuse the dead thing's next_sibling
     * field to store the next free slot index (the thing is dead so
     * next_sibling has no meaning anymore).
     * WHY: This is the intrusive free list trick — no external data
     *      structure needed, just reuse fields inside the dead thing. */
    pool->things[idx].next_sibling = pool->first_free;
    pool->first_free = idx;
}

thing *thing_pool_get(thing_pool *pool, thing_idx idx)
{
    /* Valid range: 1..MAX_THINGS-1. Index 0 = nil sentinel.
     * Out-of-bounds or unused → return nil sentinel.
     * WHY: NEVER returns NULL. NEVER crashes. The nil sentinel is always
     *      safe to read (all zeros). Writing to it is harmless in release
     *      (values get overwritten) and caught in debug (assert). */
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0]; /* nil sentinel */
}

thing *thing_pool_get_ref(thing_pool *pool, thing_ref ref)
{
    /* Same as pool_get but also checks generation.
     * WHY: If the slot was recycled (removed and re-added), the generation
     *      won't match. This detects stale references — the caller thinks
     *      they're accessing "troll #5 gen 2" but slot 5 is now "goblin gen 3".
     * JS: Like checking if a database row's auto-increment ID still matches
     *     what you cached — if it changed, someone deleted and recreated. */
    if (ref.index > 0 && ref.index < MAX_THINGS
        && pool->used[ref.index]
        && pool->generations[ref.index] == ref.generation) {
        return &pool->things[ref.index];
    }
    return &pool->things[0]; /* nil sentinel */
}

bool thing_is_valid(const thing *t)
{
    /* JS: if (entity) — truthy check.
     * C:  A thing is valid if its kind is not NIL.
     *     The nil sentinel has kind = 0 = THING_KIND_NIL, so this
     *     returns false for any nil-sentinel-returned reference. */
    return t->kind != THING_KIND_NIL;
}

/* ══════════════════════════════════════════════════════ */
/*              Intrusive List Operations                   */
/* ══════════════════════════════════════════════════════ */

void thing_add_child(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx)
{
    /* Prepend child to the front of parent's child list (O(1)).
     *
     * Before: parent.first_child → A → B → (circle back to A)
     * After:  parent.first_child → child → A → B → (circle back to child)
     *
     * WHY prepend, not append? Prepend is O(1) — we just update first_child.
     * Append would require walking to the end of the list (O(n)).
     * Anton: "Insert at the beginning — it's O(1). If order doesn't matter,
     *         and it usually doesn't for inventories, just prepend." */

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    /* Set the child's parent link. */
    child->parent = parent_idx;

    if (parent->first_child == 0) {
        /* Parent has no children yet. Child becomes the only child.
         * Circular list of one: prev and next point to self. */
        parent->first_child  = child_idx;
        child->next_sibling  = child_idx;  /* circular: points to itself */
        child->prev_sibling  = child_idx;
    } else {
        /* Parent already has children. Insert child BEFORE the current first.
         * Since the list is circular, "before first" = "after last".
         *
         * ┌─ first ─── A ─── B ─── (back to first) ─┐
         * │◄───── prev_sibling ──────────────────────┘
         *
         * We insert child between last (first.prev) and first:
         *   last.next = child
         *   child.prev = last
         *   child.next = old_first
         *   old_first.prev = child
         *   parent.first_child = child  (new first) */
        thing_idx old_first_idx = parent->first_child;
        thing *old_first = &pool->things[old_first_idx];
        thing_idx last_idx = old_first->prev_sibling;
        thing *last = &pool->things[last_idx];

        last->next_sibling      = child_idx;
        child->prev_sibling     = last_idx;
        child->next_sibling     = old_first_idx;
        old_first->prev_sibling = child_idx;

        parent->first_child = child_idx; /* new first child */
    }
}

void thing_unlink_child(thing_pool *pool, thing_idx child_idx)
{
    /* Remove child from its parent's circular sibling list.
     * Does NOT pool_remove — just unlinks from the tree.
     *
     * WHY separate from pool_remove? Sometimes you want to reparent
     * (move an item from ground to inventory) without destroying it. */

    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;

    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];

    if (child->next_sibling == child_idx) {
        /* Only child — remove empties the list. */
        parent->first_child = 0;
    } else {
        /* Fix sibling links around the removed child. */
        thing *prev = &pool->things[child->prev_sibling];
        thing *next = &pool->things[child->next_sibling];
        prev->next_sibling = child->next_sibling;
        next->prev_sibling = child->prev_sibling;

        /* If the removed child was the first child, update parent. */
        if (parent->first_child == child_idx) {
            parent->first_child = child->next_sibling;
        }
    }

    /* Clear the child's tree links. */
    child->parent       = 0;
    child->next_sibling = 0;
    child->prev_sibling = 0;
}

/* ══════════════════════════════════════════════════════ */
/*         Variant Functions (for scene comparisons)       */
/* ══════════════════════════════════════════════════════ */

/* --- Singly-linked variants (no prev_sibling used) --- */

void thing_add_child_singly(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx)
{
    /* Prepend: new child becomes first_child, old first becomes next_sibling.
     * Simpler than circular — no prev_sibling management. */
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent       = parent_idx;
    child->next_sibling = parent->first_child; /* old first becomes next */
    parent->first_child = child_idx;           /* new child is now first */
    /* NOTE: prev_sibling is NOT set — singly-linked variant ignores it */
}

void thing_remove_child_singly(thing_pool *pool, thing_idx child_idx)
{
    /* O(n) removal: must WALK the sibling list from first_child to find
     * the sibling whose next_sibling == child_idx, then patch around.
     * This is WHY doubly-linked lists exist — to avoid this walk. */
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *child = &pool->things[child_idx];
    thing_idx parent_idx = child->parent;
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];

    if (parent->first_child == child_idx) {
        /* Child is the first — just update parent's first_child. */
        parent->first_child = child->next_sibling;
    } else {
        /* Walk the list to find the previous sibling. O(n). */
        thing_idx prev_idx = parent->first_child;
        while (prev_idx != 0) {
            thing *prev = &pool->things[prev_idx];
            if (prev->next_sibling == child_idx) {
                prev->next_sibling = child->next_sibling; /* patch around */
                break;
            }
            prev_idx = prev->next_sibling;
        }
    }

    child->parent       = 0;
    child->next_sibling = 0;
}

/* --- Append variant --- */

void thing_add_child_append(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx)
{
    /* Append to END of circular list. Uses the last_child trick:
     * first_child.prev_sibling IS last_child in a circular list.
     * So we insert between last and first. Still O(1). */
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx <= 0 || child_idx >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent = parent_idx;

    if (parent->first_child == 0) {
        /* No children yet — same as prepend for single child */
        parent->first_child = child_idx;
        child->next_sibling = child_idx;
        child->prev_sibling = child_idx;
    } else {
        /* Insert AFTER last (= first.prev), BEFORE first.
         * first_child stays the same — new child goes at the end. */
        thing_idx first_idx = parent->first_child;
        thing *first = &pool->things[first_idx];
        thing_idx last_idx = first->prev_sibling;
        thing *last = &pool->things[last_idx];

        last->next_sibling  = child_idx;
        child->prev_sibling = last_idx;
        child->next_sibling = first_idx;
        first->prev_sibling = child_idx;
        /* parent->first_child unchanged — new child is at the END */
    }
}

/* --- Kind-check liveness (no used[] array) --- */

thing *thing_pool_get_by_kind(thing_pool *pool, thing_idx idx)
{
    /* Check kind != NIL instead of used[]. Simpler — no extra data needed.
     * BUT: can't distinguish "removed and zeroed" from "never allocated".
     * Also breaks when free list stores data in dead things (kind is zero
     * but next_sibling holds the free list pointer). */
    if (idx > 0 && idx < MAX_THINGS && pool->things[idx].kind != THING_KIND_NIL) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}

/* --- Add WITHOUT free list --- */

thing_ref thing_pool_add_no_freelist(thing_pool *pool, thing_kind kind)
{
    /* Always bump next_empty, never check first_free.
     * Shows how slot waste accumulates without a free list:
     * remove 50 things, add 50 more → next_empty advances by 50
     * even though 50 slots are empty. */
    if (pool->next_empty >= MAX_THINGS) {
        return (thing_ref){0, 0};
    }
    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return (thing_ref){slot, pool->generations[slot]};
}

/* --- Get WITHOUT generation check --- */

thing *thing_pool_get_no_gen(thing_pool *pool, thing_ref ref)
{
    /* Ignores generation — returns whatever is in the slot right now.
     * If the slot was recycled, you silently get the WRONG entity.
     * This is the stale-reference bug that generational IDs prevent. */
    if (ref.index > 0 && ref.index < MAX_THINGS && pool->used[ref.index]) {
        return &pool->things[ref.index];
    }
    return &pool->things[0];
}

/* ══════════════════════════════════════════════════════ */
/*              Iterator                                   */
/* ══════════════════════════════════════════════════════ */

thing_iter thing_iter_begin(thing_pool *pool)
{
    /* Start at index 1 (skip nil sentinel at 0).
     * Advance to the first used slot. */
    thing_iter it = {pool, 1};
    while (it.current < MAX_THINGS && !pool->used[it.current]) {
        it.current++;
    }
    return it;
}

void thing_iter_next(thing_iter *it)
{
    /* Advance past current, then skip unused slots. */
    it->current++;
    while (it->current < MAX_THINGS && !it->pool->used[it->current]) {
        it->current++;
    }
}

bool thing_iter_valid(const thing_iter *it)
{
    return it->current > 0 && it->current < MAX_THINGS;
}

thing *thing_iter_get(thing_iter *it)
{
    if (thing_iter_valid(it)) {
        return &it->pool->things[it->current];
    }
    return &it->pool->things[0]; /* nil */
}

/* ══════════════════════════════════════════════════════ */
/*              Debug Helpers                               */
/* ══════════════════════════════════════════════════════ */

#ifdef DEBUG
void thing_pool_assert_nil_clean(const thing_pool *pool)
{
    /* The nil sentinel (things[0]) should always be all-zeros.
     * If someone accidentally wrote to it (via a nil reference),
     * this assert fires — tells you there's a stale-write bug. */
    const unsigned char *bytes = (const unsigned char *)&pool->things[0];
    for (size_t i = 0; i < sizeof(thing); i++) {
        assert(bytes[i] == 0 && "NIL sentinel was written to! "
               "A nil reference is being used as a write target.");
    }
}
#endif

const char *thing_kind_name(thing_kind kind)
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
