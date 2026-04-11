/* ══════════════════════════════════════════════════════════════════════ */
/*               things.h — Large Arrays of Things                       */
/*                                                                       */
/*  A flat-array entity pool with:                                       */
/*    - nil sentinel at index 0 (never crashes)                          */
/*    - intrusive linked lists (parent/child/sibling)                    */
/*    - free list for O(1) slot reuse                                    */
/*    - generational IDs for stale reference detection                   */
/*    - clean iterator over living things                                */
/*                                                                       */
/*  Inspired by Anton Mikhailov (Media Molecule, Dreams PS4).            */
/*  No malloc. No constructors. No STL. Zero-init is the architecture.   */
/*                                                                       */
/*  This file compiles without ANY platform headers.                     */
/* ══════════════════════════════════════════════════════════════════════ */
#ifndef THINGS_H
#define THINGS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h> /* memset */

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 1024

/* ══════════════════════════════════════════════════════ */
/*                      Types                             */
/* ══════════════════════════════════════════════════════ */

/* JS: TypeScript discriminated union tag — `type Kind = 'player' | 'troll'`
 * C:  Integer enum. THING_KIND_NIL = 0 so zero-init produces nil. */
typedef enum thing_kind {
    THING_KIND_NIL = 0, /* zero-init produces this — the "nothing" kind */
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

/* JS: Array index `entities[id]` — just a number, not a reference.
 * C:  typedef'd int. Survives reallocation. Trivially serializable. */
typedef int thing_idx;

/* JS: Database row { id: 5, version: 3 } — prevents confusing recycled rows.
 * C:  Index + generation. If generations don't match, the ref is stale. */
typedef struct thing_ref {
    thing_idx index;
    uint32_t  generation;
} thing_ref;

/* ══════════════════════════════════════════════════════ */
/*                   The Thing Struct                      */
/* ══════════════════════════════════════════════════════ */

/* JS: { kind: 'player', x: 0, y: 0, health: 100 } — fat object.
 * C:  One struct for ALL entity types. Unused fields stay zero.
 *
 * DESIGN RULE: all-zeros MUST be a valid nil state.
 *   - kind = 0 = THING_KIND_NIL
 *   - all indices = 0 = "points to nil sentinel"
 *   - all floats = 0.0f = safe defaults
 *
 * WHY: memset(&pool, 0, sizeof(pool)) resets EVERYTHING.
 *      No constructors needed. No destructor cleanup. */
typedef struct thing {
    /* ── Type tag ── */
    thing_kind kind;

    /* ── Intrusive tree links (indices, not pointers) ── */
    /* JS: DOM node.parentNode / node.firstChild / node.nextSibling
     * C:  Integer indices into the same flat array. Zero = nil. */
    thing_idx parent;
    thing_idx first_child;
    thing_idx next_sibling;
    thing_idx prev_sibling; /* enables doubly-linked circular list */

    /* ── Common properties (all entity types) ── */
    float x, y;       /* position */
    float dx, dy;     /* velocity */
    float health;
    float size;        /* collision radius / draw scale */
    uint32_t color;    /* RGBA for rendering */
} thing;

/* ══════════════════════════════════════════════════════ */
/*                   The Thing Pool                        */
/* ══════════════════════════════════════════════════════ */

/* JS: Map<number, Entity> with set()/delete() — but ours is a flat array.
 * C:  Static array + used flags + free list + generation counters.
 *
 * things[0] is the NIL SENTINEL: always zero-init, always safe to read.
 * Valid slots start at index 1. */
typedef struct thing_pool {
    thing     things[MAX_THINGS];       /* the flat array — things[0] = nil */
    bool      used[MAX_THINGS];         /* true if slot holds a living thing */
    uint32_t  generations[MAX_THINGS];  /* incremented on each slot recycle */
    thing_idx next_empty;               /* next never-used slot (starts at 1) */
    thing_idx first_free;               /* head of intrusive free list (0 = empty) */
} thing_pool;

/* ══════════════════════════════════════════════════════ */
/*                   Pool Iterator                         */
/* ══════════════════════════════════════════════════════ */

/* JS: for (const entity of pool) — but we manually advance.
 * C:  Struct holding pool pointer + current index. Skips dead slots. */
typedef struct thing_iter {
    thing_pool *pool;
    thing_idx   current;
} thing_iter;

/* ══════════════════════════════════════════════════════ */
/*                   Pool API                              */
/* ══════════════════════════════════════════════════════ */

/* Reset pool to empty state. All things become nil. Generations preserved
 * for safety (stale refs from before reset will still be detected). */
void thing_pool_init(thing_pool *pool);

/* Add a new thing of the given kind. Returns a thing_ref (index + generation).
 * If the pool is full, returns {0, 0} — which resolves to the nil sentinel. */
thing_ref thing_pool_add(thing_pool *pool, thing_kind kind);

/* Remove a thing by index. Marks it unused, zeros it, adds slot to free list,
 * bumps generation. Removing index 0 (nil) is a no-op. */
void thing_pool_remove(thing_pool *pool, thing_idx idx);

/* Get a thing by raw index. Returns pointer to the thing if valid and used,
 * or pointer to things[0] (nil sentinel) if invalid/unused.
 * NEVER returns NULL. NEVER crashes. */
thing *thing_pool_get(thing_pool *pool, thing_idx idx);

/* Get a thing by ref (index + generation). Same as pool_get but also checks
 * that the generation matches — detects stale references to recycled slots.
 * Returns pointer to nil sentinel if generation mismatch. */
thing *thing_pool_get_ref(thing_pool *pool, thing_ref ref);

/* Check if a thing is valid (not nil). Equivalent to thing->kind != THING_KIND_NIL.
 * JS: if (entity) — truthy check. */
bool thing_is_valid(const thing *t);

/* ══════════════════════════════════════════════════════ */
/*              Intrusive List Operations                   */
/* ══════════════════════════════════════════════════════ */

/* Add child_idx as the FIRST child of parent_idx (O(1) prepend).
 * Sets up parent, first_child, next_sibling, prev_sibling links.
 * Both parent and child must be valid pool indices. */
void thing_add_child(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx);

/* Remove child_idx from its parent's child list.
 * Fixes sibling and parent links. Does NOT remove from pool — just unlinks. */
void thing_unlink_child(thing_pool *pool, thing_idx child_idx);

/* ══════════════════════════════════════════════════════ */
/*         Variant Functions (for scene comparisons)       */
/* ══════════════════════════════════════════════════════ */

/* --- Singly-linked list variants (Lesson 04) --- */

/* Add child using singly-linked list ONLY (no prev_sibling).
 * Prepends to front. O(1). */
void thing_add_child_singly(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx);

/* Remove child using singly-linked list ONLY. Must WALK the list
 * to find the previous sibling. O(n). This is why doubly-linked exists. */
void thing_remove_child_singly(thing_pool *pool, thing_idx child_idx);

/* --- Append variant (Lesson 05) --- */

/* Add child at END of parent's child list (append, not prepend).
 * Uses circular list's last_child trick: first_child.prev_sibling = last.
 * O(1) because of circular structure. */
void thing_add_child_append(thing_pool *pool, thing_idx parent_idx, thing_idx child_idx);

/* --- Kind-check liveness (Lesson 07 Variant A) --- */

/* Get a thing by checking kind != NIL instead of used[] array.
 * Simpler (no extra data), but can't distinguish "removed" from "never allocated". */
thing *thing_pool_get_by_kind(thing_pool *pool, thing_idx idx);

/* --- No-freelist add (Lesson 08 comparison) --- */

/* Add WITHOUT checking the free list. Always bumps next_empty.
 * Demonstrates slot waste when free list is disabled. */
thing_ref thing_pool_add_no_freelist(thing_pool *pool, thing_kind kind);

/* --- No-generation get (Lesson 09 comparison) --- */

/* Get by ref WITHOUT checking generation. Returns the thing even if
 * the slot was recycled. Demonstrates the stale-reference problem. */
thing *thing_pool_get_no_gen(thing_pool *pool, thing_ref ref);

/* ══════════════════════════════════════════════════════ */
/*              Iterator API                               */
/* ══════════════════════════════════════════════════════ */

/* Begin iteration at the first used slot (skipping nil at 0). */
thing_iter thing_iter_begin(thing_pool *pool);

/* Advance to next used slot. */
void thing_iter_next(thing_iter *it);

/* Returns true if iterator points to a valid slot. */
bool thing_iter_valid(const thing_iter *it);

/* Dereference iterator — returns pointer to current thing. */
thing *thing_iter_get(thing_iter *it);

/* ══════════════════════════════════════════════════════ */
/*              Debug Helpers                               */
/* ══════════════════════════════════════════════════════ */

#ifdef DEBUG
/* Assert that the nil sentinel is still zeroed. If someone wrote to it,
 * this fires — tells you there's a bug where code writes to a nil ref. */
void thing_pool_assert_nil_clean(const thing_pool *pool);
#else
#define thing_pool_assert_nil_clean(pool) ((void)0)
#endif

/* Return a human-readable name for a thing_kind. */
const char *thing_kind_name(thing_kind kind);

#endif /* THINGS_H */
