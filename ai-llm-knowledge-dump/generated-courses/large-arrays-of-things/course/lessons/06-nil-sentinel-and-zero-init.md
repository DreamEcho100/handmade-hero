# Lesson 06: The Nil Sentinel and Zero Initialization

## What we're building

The philosophical foundation of the entire pool system. We formalize `things[0]` as the nil sentinel -- an always-zero, always-safe, always-readable entity that represents "nothing." We prove it works by running five crash scenarios that would segfault in a pointer-based system but gracefully bottom out at nil in ours. We add a debug assertion that catches accidental writes to nil, and we demonstrate chain-dereferencing -- accessing `pool_get(pool, pool_get(pool, player).first_child).health` -- where every step bottoms out safely.

By the end of this lesson:

```
[1] Out-of-bounds (-5): kind=NIL  -- safe
[2] Unused slot (500):  kind=NIL  -- safe
[3] Removed entity:     kind=NIL  -- safe
[4] Pool full:          kind=NIL  -- safe
[5] Chain deref:        kind=NIL  -- safe
All 5 crash scenarios returned nil safely.
```

## What you'll learn

- Why NULL pointers are a billion-dollar mistake and how nil sentinels avoid it
- The design rule: all-zeros = valid nil state (and why every field must obey this)
- Five crash scenarios that nil prevents
- Chain-dereferencing: arbitrary-depth traversal that never crashes
- `thing_is_valid()` -- the one-line validity check
- Debug assert: detecting when code accidentally writes to nil
- `memset(&pool, 0, sizeof(pool))` as the universal reset button
- The connection to `= {0}` initialization in every game course

## Prerequisites

- Lesson 02: thing struct, pool, `pool_add()`
- Lesson 03: `thing_idx` as integer index, `pool_get()` returns `&things[0]` for invalid indices
- Lesson 04: `first_child`, `next_sibling`, intrusive linked lists
- Lesson 05: circular doubly-linked lists, `prev_sibling`

---

## Step 1 -- The billion-dollar problem

> **JS bridge: `null`, `undefined`, and the nil sentinel.** In JavaScript, you have two "nothing" values: `null` (explicitly set to nothing) and `undefined` (never set at all). Accessing a property on either one throws `TypeError: Cannot read properties of null`. The JS solution? Optional chaining: `entity?.health` returns `undefined` instead of throwing if `entity` is null/undefined. Our nil sentinel does the same thing, but at the data structure level. Instead of a language-level `?.` operator, we make the data structure itself safe: every invalid lookup returns a real struct (the nil sentinel at `things[0]`) that you can read freely. The nil sentinel IS the `?.` operator, baked into the pool architecture.

Tony Hoare, the computer scientist who invented NULL in 1965, later called it his "billion-dollar mistake." Here is why.

In a typical C codebase, when you ask "give me the entity at this index" and the index is invalid, you get back a NULL pointer:

```c
/* Traditional approach: return NULL on invalid access */
thing *pool_get(thing_pool *pool, int idx) {
    if (idx < 0 || idx >= MAX_THINGS) return NULL;
    if (!pool->used[idx]) return NULL;
    return &pool->things[idx];
}
```

Now every caller must check for NULL:

```c
thing *t = pool_get(&pool, some_idx);
if (t != NULL) {                   /* must check! */
    float h = t->health;           /* safe */
} else {
    /* handle the error somehow */
}
```

Forget the check once and you get a segfault:

```c
thing *t = pool_get(&pool, some_idx);
float h = t->health;              /* SEGFAULT if t is NULL */
```

> **Why?** In JS, accessing a property on `null` throws `TypeError: Cannot read properties of null`. The program crashes with a stack trace. In C, dereferencing NULL is undefined behavior -- the program might crash, might corrupt memory, might appear to work today and explode tomorrow. There is no safety net.

And it gets worse with chaining:

```c
/* Get the health of the player's first child */
thing *player = pool_get(&pool, player_idx);
if (player != NULL) {
    thing *child = pool_get(&pool, player->first_child);
    if (child != NULL) {
        float h = child->health;   /* TWO null checks to read one value */
    }
}
```

Three levels deep? Three null checks. Four levels? Four checks. The code becomes a staircase of defensive if-statements, and missing any one of them is a crash.

```
The NULL staircase (traditional approach):
==========================================

  thing *player = pool_get(pool, player_idx);
  if (player != NULL) {
      thing *child = pool_get(pool, player->first_child);
      if (child != NULL) {
          thing *grandchild = pool_get(pool, child->first_child);
          if (grandchild != NULL) {
              float h = grandchild->health;
              // FINALLY we can read the value
          }
      }
  }

  Three levels deep = three NULL checks.
  Miss one = segfault.
  Every caller must do this. Every time.
```

> **Anton says:** "You never return NULL. You return the nil thing. The nil thing has kind = NIL, health = 0, position = 0, first_child = 0. You can read any field on it. You can follow any link from it. It just keeps returning nil. It's a black hole -- everything goes in, nothing comes out, and nothing crashes."

## Step 2 -- The nil sentinel: slot 0

Here is the fix. We reserve `things[0]` as a special entity that is always zero-initialized, always valid to read, and never represents a real game entity.

```
The nil sentinel at things[0]:
==============================

things[] array:
  [0]              [1]        [2]       [3]       [4]
  NIL SENTINEL     PLAYER     AXE       POTION    FOOD
  kind = NIL       kind = PLAYER ...
  parent = 0       parent = 0
  first_child = 0  first_child = 4
  next_sib = 0     next_sib = 0
  prev_sib = 0     prev_sib = 0
  x = 0.0          x = 10.0
  y = 0.0          y = 20.0
  health = 0.0     health = 100.0

  EVERY field of things[0] is zero.
  EVERY zero value is a safe, meaningful "nothing":
    kind = 0       = THING_KIND_NIL   (not a real entity)
    parent = 0     = "no parent"      (points to nil)
    first_child =0 = "no children"    (points to nil)
    next_sib = 0   = "no sibling"     (points to nil)
    health = 0.0   = "no health"      (safe default)
    x, y = 0.0     = "origin"         (safe default)
```

Now, whenever ANY of those crash scenarios occurs, instead of returning NULL, we return `&things[0]`. The caller gets a valid pointer to a valid struct. Reading from it gives zero values (harmless). Writing to it overwrites the sentinel (we catch this in debug mode).

> **New technique:** Nil sentinel -- a reserved element in a data structure that represents "nothing" but can be safely dereferenced. Unlike NULL, which crashes when accessed, the nil sentinel is a real object with safe default values.
>
> **What it does:** Eliminates null-pointer crashes entirely. Any invalid access returns a reference to the sentinel, which is always safe to read.
>
> **Why:** In a game running at 60 fps, a crash is unacceptable. A frame where a bullet's target does not exist should render the bullet at (0,0) or skip it -- not crash the game.
>
> **JS equivalent:** Optional chaining: `entity?.inventory?.first?.name ?? "none"`. The `?.` operator evaluates to `undefined` instead of throwing. Our nil sentinel does the same thing but at the data structure level -- you do not need special syntax, the structure itself is safe.
>
> **Worked example:** `pool_get(&pool, 9999)` returns `&things[0]`. `things[0].health` is 0.0. `things[0].first_child` is 0. `pool_get(&pool, things[0].first_child)` returns `&things[0]` again. You can chain forever and never crash.

## Step 3 -- Designing for zero: the hard constraint

This only works if **every field's zero value is safe and meaningful**. Let us verify:

| Field | Zero value | Meaning | Safe? |
|---|---|---|---|
| `kind` | `THING_KIND_NIL = 0` | "This is nothing" | Yes -- that is exactly what we want |
| `parent` | `0` | "Parent is nil (no parent)" | Yes -- nil sentinel points to itself |
| `first_child` | `0` | "No children" | Yes -- empty child list |
| `next_sibling` | `0` | "No next sibling" | Yes -- end of list (or self in circular) |
| `prev_sibling` | `0` | "No prev sibling" | Yes -- end of list (or self in circular) |
| `x`, `y` | `0.0f` | "At origin" | Yes -- valid position |
| `health` | `0.0f` | "Dead/empty" | Yes -- zero health is meaningful |
| `dx`, `dy` | `0.0f` | "Not moving" | Yes -- stationary |
| `size` | `0.0f` | "No extent" | Yes -- zero size |
| `color` | `0x00000000` | "Transparent black" | Yes -- invisible |

Every. Single. Field. Zero is safe. This is NOT a coincidence -- it is a hard design constraint.

> **Why?** In JavaScript, `{}` gives you an empty object and `undefined` is a safe "nothing." In C, `memset(&thing, 0, sizeof(thing))` gives you all-zeros. If you design your structs so all-zeros is a valid nil state, then `memset` becomes your universal constructor, reset function, and cleanup all in one. No actual constructors needed. No destructors needed. Just zeros.

> **Common mistake:** Making `THING_KIND_PLAYER = 0` in your enum. Then zero-initialized things look like players. Always make `NIL = 0` the first enum value.

This constraint ripples through every design decision:

```
The zero-init design constraint:
=================================

  Rule: If you add a field to the thing struct,
        its zero value MUST be a safe default.

  Adding a field:
    float speed;           // 0.0 = stationary. Safe.
    int ammo;              // 0 = no ammo. Safe.
    thing_idx target;      // 0 = nil (no target). Safe.
    uint32_t color;        // 0x00000000 = transparent. Safe.
    char name[32];         // all zeros = empty string. Safe.

  Fields that VIOLATE the constraint:
    float *vertices;       // NULL pointer -- dereferencing crashes!
    int max_health;        // 0 = "no max health" -- confusing
    float scale;           // 0.0 = invisible -- maybe you wanted 1.0?

  Fix: use index instead of pointer, use signed values where
       0 is meaningful, or set non-zero defaults AFTER memset.
```

The `float scale` example is interesting. If your rendering code multiplies by `thing.scale` and the default is 0.0, every nil thing is invisible -- which is actually correct for a nil entity. But if you expect newly created real entities to be visible, you need to explicitly set `scale = 1.0f` in `pool_add`. The zero-init only handles the nil case. Real entities still need their fields set.

## Step 4 -- pool_get: never returns NULL

> **JS bridge:** Imagine if `Map.get(key)` never returned `undefined`. Instead, it always returned a valid object -- just an empty one for missing keys. That is what `pool_get` does. No `if (result === undefined)` checks needed. No `TypeError` crashes. Every call returns a valid struct you can read from. The "empty" case is just a struct full of zeros.

Here is `pool_get`. Read it carefully:

```c
thing *thing_pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0]; /* nil sentinel */
}
```

Notice what it does NOT return: NULL. Ever. Under any circumstances.

- Index is negative? Returns `&things[0]`.
- Index is out of bounds? Returns `&things[0]`.
- Index is 0? Returns `&things[0]`.
- Slot is not in use? Returns `&things[0]`.

The caller always gets a valid pointer to a real thing struct. They can dereference it, read any field, follow any link. The only question is whether the thing is "real" (`kind != NIL`) or "nothing" (`kind == NIL`).

Let us trace through each crash scenario:

```
Scenario 1 -- Out of bounds (idx = -5):
  -5 > 0 is FALSE -> return &things[0]
  No crash.

Scenario 2 -- Unused slot (idx = 500, used[500] = false):
  500 > 0 TRUE, 500 < 1024 TRUE, used[500] FALSE -> return &things[0]
  No crash.

Scenario 3 -- Removed entity (idx = 3, just removed):
  3 > 0 TRUE, 3 < 1024 TRUE, used[3] FALSE -> return &things[0]
  No crash.

Scenario 4 -- Pool full (pool_add returned 0):
  0 > 0 FALSE -> return &things[0]
  No crash.

Scenario 5 -- Stale reference (idx = 3, slot recycled):
  If used[3] is TRUE (slot reused by different entity):
    returns &things[3] -- WRONG entity, but no crash.
    (Lesson 09 fixes this with generation checks.)
  If used[3] is FALSE (slot still dead):
    returns &things[0] -- nil. Safe.
```

> **Handmade Hero connection:** Casey uses a similar sentinel pattern in his entity system. Invalid entity references resolve to a "null entity" that is safe to read. The game keeps running -- you fix the bug later in debug mode.

## Step 5 -- The five crash scenarios, proven safe

Let us go through each one in detail, with the BEFORE and AFTER states.

### Scenario 1: Out-of-bounds access

The game code has a bug where it computes an index of -5 or 9999.

```
BEFORE (pointer-based system):
  thing *t = &things[9999];   // things only has 1024 slots
  t->health = 100;            // writes to random memory
  // RESULT: SEGFAULT or silent memory corruption

AFTER (nil sentinel):
  thing *t = pool_get(&pool, 9999);
  // 9999 >= MAX_THINGS -> returns &things[0]
  t->health;  // reads 0.0 -- safe, harmless
  // RESULT: nil value, no crash
```

### Scenario 2: Entity was removed

A bullet kills an enemy, the enemy is removed, but another system still has the old index.

```
BEFORE (no nil sentinel):
  pool_remove(3);             // slot 3 zeroed, used[3] = false
  thing *t = &things[3];      // points to zeroed memory
  t->health += 10;            // writes to a dead slot!
  // RESULT: "zombie entity" -- dead slot has health=10

AFTER (nil sentinel):
  pool_remove(&pool, 3);
  thing *t = pool_get(&pool, 3);
  // used[3] == false -> returns &things[0]
  t->health;  // reads 0.0
  thing_is_valid(t);  // returns false
  // RESULT: nil, game logic skips it
```

### Scenario 3: Pool is full

The game tries to spawn a new enemy but all 1024 slots are occupied.

```
BEFORE (returns NULL):
  thing *t = pool_add(&pool, THING_KIND_TROLL);
  // pool full -> returns NULL
  t->health = 50;  // SEGFAULT -- dereferencing NULL
  // RESULT: crash

AFTER (nil sentinel):
  thing_idx slot = pool_add(&pool, THING_KIND_TROLL);
  // pool full -> returns 0 (nil index)
  thing *t = pool_get(&pool, slot);
  // slot == 0 -> returns &things[0]
  t->health;  // reads 0.0
  thing_is_valid(t);  // returns false
  // RESULT: spawn silently failed, game continues
```

### Scenario 4: Stale reference after recycling

A saved index points to a slot that was removed and reused by a different entity. This is fully solved in lesson 09 (generational IDs), but even without generations, nil prevents CRASHES.

```
BEFORE (pointer-based, no generations):
  thing *old_enemy = &things[5];
  pool_remove(5);
  pool_add(KIND_GOBLIN);  // goblin lands in slot 5
  old_enemy->health -= 20;
  // RESULT: damages the WRONG entity (goblin, not dead troll)

AFTER (nil sentinel, no generations yet):
  Same problem -- pool_get returns the goblin.
  But at least it doesn't CRASH.

AFTER (nil sentinel + generations, lesson 09):
  pool_get_ref(&pool, stale_ref)
  // generation mismatch -> returns &things[0]
  // RESULT: nil, correct behavior
```

### Scenario 5: Uninitialized index

A `thing_idx` variable was never initialized. In C, local variables contain garbage.

```
BEFORE (no sentinel):
  thing_idx parent;       // contains garbage, e.g. 0x7FFF42B8
  things[parent].health;  // access random memory
  // RESULT: SEGFAULT or corruption

AFTER (nil sentinel):
  thing_idx parent = 0;   // we always init to 0 (= nil)
  pool_get(&pool, parent);
  // 0 > 0 FALSE -> returns &things[0]
  // RESULT: nil, safe

  Or with memset (which zeros everything):
  memset(&entity, 0, sizeof(entity));
  // entity.parent = 0 -> nil
  // entity.first_child = 0 -> nil
  // All indices point to nil by default.
```

> **Why?** The zero-init architecture and the nil sentinel work together as a system. memset zeros all indices to 0. Index 0 is the nil sentinel. The sentinel's indices are all 0. So every uninitialized path leads to nil, and nil leads to nil. It is a closed loop of safety.

## Step 6 -- Chain-dereferencing: the killer feature

> **JS bridge: This is optional chaining without the `?.` syntax.** In JavaScript, you write:
> ```js
> const health = entity?.inventory?.first?.child?.health ?? 0;
> ```
> If any link in the chain is `null`/`undefined`, the whole expression evaluates to `undefined`, and `?? 0` provides the fallback. Our nil sentinel achieves the EXACT same result without special syntax. Every `pool_get` call that hits an invalid index returns `&things[0]` (the nil sentinel). The nil sentinel's `first_child` is 0, which feeds back into `pool_get` and returns nil again. You can chain as deep as you want -- any broken link collapses to nil, and nil's fields are all zero. The chain "bottoms out" at zero, just like optional chaining bottoms out at `undefined`.

Here is where the nil sentinel really shines. Watch this:

```c
/* Get the health of the player's first child's first child */
float h = thing_pool_get(&pool,
            thing_pool_get(&pool,
              thing_pool_get(&pool, player_idx)->first_child
            )->first_child
          )->health;
```

Three levels of dereferencing in a single expression. No null checks. No if statements. Let us trace what happens when the player has no children:

```
Chain dereference trace (player has no children):
==================================================

  Inner: pool_get(pool, player_idx)
    -> returns &things[1] (PLAYER, valid)

  player->first_child = 0 (no children)

  Middle: pool_get(pool, 0)
    -> 0 > 0 FALSE -> returns &things[0] (NIL SENTINEL)

  nil->first_child = 0 (nil's child is also nil)

  Outer: pool_get(pool, 0)
    -> 0 > 0 FALSE -> returns &things[0] (NIL SENTINEL)

  nil->health = 0.0

  Result: h = 0.0
  Three levels deep. No crash. No null checks.
```

Every step that fails (invalid index, no children, dead entity) returns the nil sentinel. The nil sentinel's `first_child` is 0, which loops back to nil. It is a black hole: once you hit nil, you stay at nil. No matter how deep you chain.

```
The nil black hole:
====================

  things[0].first_child   = 0 -> pool_get(0) -> things[0] (nil)
  things[0].next_sibling  = 0 -> pool_get(0) -> things[0] (nil)
  things[0].prev_sibling  = 0 -> pool_get(0) -> things[0] (nil)
  things[0].parent        = 0 -> pool_get(0) -> things[0] (nil)

  Every link from nil leads back to nil.
  You can follow links forever. You always stay at nil.

  pool_get(pool, pool_get(pool, pool_get(pool, pool_get(pool,
    pool_get(pool, 0)->first_child
  )->next_sibling)->parent)->first_child)->health

  = 0.0. Always. No crash. No matter what.
```

Now compare to the JS equivalent:

```js
// JS optional chaining -- same concept, language-level syntax
const h = pool.get(pool.get(pool.get(playerIdx)?.firstChild)?.firstChild)?.health ?? 0;
```

The `?.` does the same thing: if the left side is null/undefined, short-circuit to undefined. Our nil sentinel achieves this at the data structure level. Every `pool_get` call that fails returns the sentinel, and the sentinel's links all point back to itself. No special syntax required.

Now let us trace the HAPPY path -- chain-dereferencing when entities DO exist:

```
Chain dereference trace (player has axe with enchantment):
==========================================================

  Setup:
    PLAYER (slot 1): first_child = 3 (AXE)
    AXE    (slot 3): first_child = 5 (ENCHANTMENT)
    ENCHANT(slot 5): health = 25.0 (enchantment power)

  Inner: pool_get(pool, 1)
    -> returns &things[1] (PLAYER, valid)

  player->first_child = 3 (AXE)

  Middle: pool_get(pool, 3)
    -> returns &things[3] (AXE, valid)

  axe->first_child = 5 (ENCHANTMENT)

  Outer: pool_get(pool, 5)
    -> returns &things[5] (ENCHANTMENT, valid)

  enchantment->health = 25.0

  Result: h = 25.0
  Three levels deep. Real data. Same code as the nil case.
```

The code is IDENTICAL for the success path and the failure path. You do not write two versions. The structure handles it.

> **Anton says:** "What I ultimately want to do is write `thing.first_child.next_sibling.health` and I don't want that to crash ever. If any of these things are nil, we just get zeros. We saved ourselves a whole lot of null checks."

## Step 7 -- thing_is_valid: the one-line check

> **JS bridge:** In JS, you write `if (entity)` to check truthiness, or `if (entity !== null && entity !== undefined)` for explicit null checks. In C, there is no "truthy" for structs -- a struct is always a valid block of memory. So we need our own validity check: "is this the nil sentinel, or a real entity?"

Sometimes you DO need to know if something is real or nil. Not for crash safety (that is handled), but for game logic: "does the player have a first child? Then draw the inventory UI."

```c
bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}
```

That is it. One comparison. The nil sentinel has `kind = THING_KIND_NIL = 0`. Every real entity has a non-zero kind. So `kind != NIL` is the universal validity check.

```c
/* Usage pattern -- iterate and skip nil */
thing *child = thing_pool_get(&pool, player->first_child);
if (thing_is_valid(child)) {
    /* Entity exists -- do game logic */
    draw_entity(child);
} else {
    /* No entity here -- player has no children */
}
```

> **Why?** In JS, you write `if (entity)` -- the truthy check. In C, there is no truthy for structs. But checking one field (`kind != 0`) achieves the same result. And because we guarantee that real entities ALWAYS have a non-zero kind (you pass `THING_KIND_PLAYER`, `THING_KIND_TROLL`, etc. to `pool_add`), this check is reliable.

Notice that `thing_is_valid` works on ANY thing pointer -- even one returned by `pool_get` for an invalid index. `pool_get` returns `&things[0]`, `things[0].kind == NIL`, `thing_is_valid` returns false. The whole chain is consistent.

Here is the decision flow:

```
When to use thing_is_valid vs just reading values:
===================================================

  CASE 1: You just need a value (health, position, etc.)
    -> Just read it. If nil, you get 0.0. That's fine.
    -> Example: float h = pool_get(pool, idx)->health;
    -> If idx is invalid, h = 0.0. Game continues.

  CASE 2: You need to make a decision (branch on entity existence)
    -> Use thing_is_valid.
    -> Example:
       thing *target = pool_get(pool, enemy->target);
       if (thing_is_valid(target)) {
           move_toward(enemy, target->x, target->y);
       } else {
           wander_randomly(enemy);
       }

  CASE 3: You need to iterate children
    -> Use thing_is_valid OR check the index against 0.
    -> Example:
       if (parent->first_child != 0) { /* has children */ }
       // equivalent to:
       if (thing_is_valid(pool_get(pool, parent->first_child))) { ... }
```

## Step 8 -- Debug assert: catching writes to nil

> **JS bridge:** This is the C equivalent of running your app in development mode with strict checks enabled. In React, `StrictMode` catches unsafe lifecycle methods and double-renders to surface bugs. In our C code, compiling with `-DDEBUG` enables assertions that catch corruption of the nil sentinel. In production (`-DNDEBUG` or no `-DDEBUG`), the assertions compile to nothing -- zero overhead. Think of `#ifdef DEBUG` as `if (process.env.NODE_ENV === 'development')`, except it happens at compile time so there is literally no runtime cost in production.

The nil sentinel is read-safe by design. But what about writes? What if buggy code does this:

```c
thing *t = thing_pool_get(&pool, invalid_idx);  /* returns nil sentinel */
t->health = 100.0f;  /* OOPS: writing to things[0]! */
```

This does not crash. The write succeeds because `things[0]` is a real memory location. But it corrupts the nil sentinel -- now `things[0].health = 100` instead of 0, and the entire "nil is always zero" invariant is broken.

We catch this with a debug assertion:

```c
#ifdef DEBUG
void thing_pool_assert_nil_clean(const thing_pool *pool)
{
    const unsigned char *bytes = (const unsigned char *)&pool->things[0];
    for (size_t i = 0; i < sizeof(thing); i++) {
        assert(bytes[i] == 0 &&
               "NIL sentinel was written to! "
               "A nil reference is being used as a write target.");
    }
}
#else
#define thing_pool_assert_nil_clean(pool) ((void)0)
#endif
```

> **Why?** We cast `things[0]` to raw bytes and check that every byte is zero. If any byte is non-zero, someone wrote to the nil sentinel. This is a bug in the calling code -- they got back a nil reference and wrote to it as if it were a real entity.

Call this at the end of every frame during development:

```c
/* End of frame: verify nil was not corrupted */
thing_pool_assert_nil_clean(&pool);
```

In a release build, the macro expands to nothing -- zero overhead.

```
Debug nil assertion:
====================

  things[0] in memory (should be all zeros):
  +----------------------------------------------+
  | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 .. | <- all zero? OK
  +----------------------------------------------+

  If buggy code writes t->health = 100.0:
  +----------------------------------------------+
  | 00 00 00 00 00 00 00 00 00 00 C8 42 00 00 .. | <- non-zero!
  +----------------------------------------------+
                                   ^^
                              float 100.0 in IEEE 754 (bytes C8 42 ...)

  assert fires: "NIL sentinel was written to!"
  Now you know: some code path is using a nil reference
  as a write target. Find it, fix it.
```

> **Common mistake:** Only checking `things[0].kind == NIL` instead of checking ALL bytes. A write to `things[0].x` or `things[0].health` would go undetected if you only check `kind`. The byte-by-byte scan catches writes to ANY field.

This gives you the best of both worlds:
- **Release build:** nil absorbs bad writes silently. Game never crashes. Users are happy.
- **Debug build:** assert fires immediately. Developer finds the bug. Code gets fixed.

## Step 9 -- memset as the universal reset

> **JS bridge:** In a React/Redux app, you might reset game state with `dispatch(resetAction())` which sets the store back to `initialState`. Or `Object.assign(state, initialState)`. The catch: you have to define `initialState` and keep it in sync with your state shape. If you add a new field and forget to add it to `initialState`, that field leaks across resets. In C, `memset(&pool, 0, sizeof(pool))` zeros EVERY byte mechanically. It does not care what fields exist. Add a new field to the struct tomorrow and memset already resets it. No `initialState` to maintain. No fields to forget. One function call, everything is zero.

Here is the final piece of the philosophy. Because every field's zero value is a valid nil/empty state, we can reset the ENTIRE pool with a single memset:

```c
void thing_pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;  /* slot 0 is reserved */
}
```

What does this one memset do?

```
memset(&pool, 0, sizeof(pool)) effects:
========================================

  things[0..1023]:
    Every thing is zeroed:
      kind         = 0 = THING_KIND_NIL     -> not a real entity
      parent       = 0 = nil                 -> no parent
      first_child  = 0 = nil                 -> no children
      next_sibling = 0 = nil                 -> no sibling
      prev_sibling = 0 = nil                 -> no sibling
      x, y         = 0.0                     -> at origin
      health       = 0.0                     -> dead/nil
      ...every field at its safe default

  used[0..1023]:
    Every flag is false (0)                  -> no living entities

  generations[0..1023]:
    Every counter is 0                       -> fresh start

  next_empty:
    0 (we fix this to 1 right after)

  first_free:
    0 = empty free list

  One function call. Everything reset.
  No per-entity cleanup. No destructor loop.
  No "did I forget to reset some field?" bugs.
```

Compare to a traditional OOP approach:

```cpp
// C++: reset requires touching every entity
void Pool::reset() {
    for (int i = 0; i < count; i++) {
        entities[i].~Entity();        // call destructor
        new (&entities[i]) Entity();   // call constructor
    }
    count = 0;
    // did you remember to reset the free list?
    // did you remember to clear the parent pointers?
    // did you remember to reset the generation counters?
}
```

With zero-init architecture, you can not forget a field. memset zeros EVERYTHING. If a field's zero value is not safe, the struct design is wrong -- fix the struct, not the reset code.

> **Anton says:** "I don't want to construct my things. I'm just going to zero them. I don't want constructors. If we have to do constructors, things get nasty because now we have to call the destructor manually. Blah blah blah. Don't want to."

> **Why?** In JS, you might reset state with `Object.assign(state, initialState)` or `setState(initialState)`. The C equivalent is `memset`. But JS's version only works if you remembered to include every field in `initialState`. memset is mechanical -- it zeros every byte regardless of what fields exist. Add a new field to the struct tomorrow and memset already handles it. No code changes needed.

## Step 10 -- The principle: all courses use {0} init

This is not just a trick for this one pool system. It is a principle that appears in every game programming course:

```c
/* Platform Foundation Course */
GameState state = {0};  /* all fields zeroed */

/* Asteroids Course */
SpaceObject objects[MAX_OBJECTS] = {0};  /* all ships/asteroids zeroed */

/* This Course */
thing_pool pool;
memset(&pool, 0, sizeof(pool));  /* equivalent to = {0} */
```

The `= {0}` syntax in C initializes all fields to zero. `memset(&thing, 0, sizeof(thing))` does the same thing byte-by-byte. They are interchangeable for POD structs (which all our structs are -- no pointers to heap memory, no vtables, no function pointers).

> **Cross-reference:** Every game course in this series uses `= {0}` initialization. The Platform Foundation Course zeros `GameState`. The Asteroids Course zeros the object pool. The Snake/Tetris courses zero the game board. This lesson explains WHY: it is not a C convention or a shortcut. It is a deliberate architecture decision that enables crash-safe data structures, trivial serialization, and single-instruction reset.

> **Common mistake:** Using `= {0}` for structs that contain `float *` or `char *` pointers. Zero-init sets those pointers to NULL, which is technically correct on all modern platforms (NULL is all-zero-bits), but conceptually you are setting up for NULL dereferences later. Our structs avoid this by using indices instead of pointers and fixed-size arrays instead of dynamic ones.

## Step 11 -- Putting it all together: the compilable file

```c
/* lesson_06.c -- The Nil Sentinel and Zero Initialization
 *
 * Compile:
 *   gcc -Wall -Wextra -std=c11 -DDEBUG -o lesson_06 lesson_06.c
 *
 * Run:
 *   ./lesson_06
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

typedef int thing_idx;

typedef struct thing {
    thing_kind kind;

    /* Intrusive tree links (circular doubly-linked sibling list) */
    thing_idx parent;
    thing_idx first_child;
    thing_idx next_sibling;
    thing_idx prev_sibling;

    /* Common properties */
    float x, y;
    float health;
} thing;

typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    int       next_empty;
} thing_pool;

/* ══════════════════════════════════════════════════════ */
/*                   Pool Operations                      */
/* ══════════════════════════════════════════════════════ */

void thing_pool_init(thing_pool *pool)
{
    /* THE universal reset: one memset zeros everything.
     * things[0] becomes the nil sentinel (all zeros = NIL kind).
     * All used[] flags become false. All entities become nil. */
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;  /* slot 0 reserved for nil sentinel */
}

thing_idx thing_pool_add(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) return 0; /* pool full -> nil */
    thing_idx slot = pool->next_empty;
    pool->next_empty++;
    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;
    return slot;
}

void thing_pool_remove(thing_pool *pool, thing_idx idx)
{
    /* Removing nil (0) is a no-op. Removing out-of-bounds is a no-op.
     * Removing an already-unused slot is a no-op.
     * WHY defensive: game code should not crash on double-remove. */
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));
}

/* NEVER returns NULL. NEVER crashes.
 * Returns &things[0] (nil sentinel) for any invalid access. */
thing *thing_pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0]; /* nil sentinel */
}

/* Is this thing "real" (not nil)? */
bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

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

/* ══════════════════════════════════════════════════════ */
/*                   Debug Helpers                         */
/* ══════════════════════════════════════════════════════ */

#ifdef DEBUG
void thing_pool_assert_nil_clean(const thing_pool *pool)
{
    /* Scan every byte of things[0]. If any is non-zero,
     * someone wrote to the nil sentinel -- that is a bug. */
    const unsigned char *bytes = (const unsigned char *)&pool->things[0];
    for (size_t i = 0; i < sizeof(thing); i++) {
        assert(bytes[i] == 0 &&
               "NIL sentinel was written to! "
               "A nil reference is being used as a write target.");
    }
}
#else
#define thing_pool_assert_nil_clean(pool) ((void)0)
#endif

/* ══════════════════════════════════════════════════════ */
/*              Intrusive List Operations                   */
/* ══════════════════════════════════════════════════════ */

void thing_add_child(thing_pool *pool, thing_idx parent_idx,
                     thing_idx child_idx)
{
    if (parent_idx <= 0 || parent_idx >= MAX_THINGS) return;
    if (child_idx  <= 0 || child_idx  >= MAX_THINGS) return;

    thing *parent = &pool->things[parent_idx];
    thing *child  = &pool->things[child_idx];

    child->parent = parent_idx;

    if (parent->first_child == 0) {
        parent->first_child = child_idx;
        child->next_sibling = child_idx;
        child->prev_sibling = child_idx;
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

/* ══════════════════════════════════════════════════════ */
/*                        Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("Lesson 06: The Nil Sentinel and Zero Initialization\n");
    printf("sizeof(thing) = %zu bytes\n\n", sizeof(thing));

    thing_pool pool;
    thing_pool_init(&pool);

    /* ── Verify nil sentinel state ── */
    printf("--- Nil sentinel state after init ---\n");
    thing *nil = &pool.things[0];
    printf("  things[0].kind         = %d (%s)\n",
           nil->kind, thing_kind_name(nil->kind));
    printf("  things[0].parent       = %d\n", nil->parent);
    printf("  things[0].first_child  = %d\n", nil->first_child);
    printf("  things[0].next_sibling = %d\n", nil->next_sibling);
    printf("  things[0].prev_sibling = %d\n", nil->prev_sibling);
    printf("  things[0].health       = %.1f\n", nil->health);
    printf("  things[0].x            = %.1f\n", nil->x);
    printf("  thing_is_valid(nil)    = %s\n",
           thing_is_valid(nil) ? "true" : "false");
    printf("  All zeros. Safe to read any field.\n\n");

    /* ── Add a player for testing ── */
    thing_idx player_idx = thing_pool_add(&pool, THING_KIND_PLAYER);
    thing_pool_get(&pool, player_idx)->health = 100.0f;
    thing_pool_get(&pool, player_idx)->x = 10.0f;

    /* ══════ Crash Scenario 1: Out of bounds ══════ */
    printf("--- [1] Crash scenario: Out-of-bounds access ---\n");
    thing *t1 = thing_pool_get(&pool, -5);
    printf("  pool_get(pool, -5): kind=%s, health=%.1f\n",
           thing_kind_name(t1->kind), t1->health);
    printf("  thing_is_valid = %s\n",
           thing_is_valid(t1) ? "true" : "false");
    assert(!thing_is_valid(t1));
    printf("  SAFE.\n\n");

    /* ══════ Crash Scenario 2: Unused slot ══════ */
    printf("--- [2] Crash scenario: Unused slot ---\n");
    thing *t2 = thing_pool_get(&pool, 500);
    printf("  pool_get(pool, 500): kind=%s, health=%.1f\n",
           thing_kind_name(t2->kind), t2->health);
    assert(!thing_is_valid(t2));
    printf("  SAFE.\n\n");

    /* ══════ Crash Scenario 3: Removed entity ══════ */
    printf("--- [3] Crash scenario: Removed entity ---\n");
    thing_idx enemy_idx = thing_pool_add(&pool, THING_KIND_TROLL);
    printf("  Added TROLL at slot %d\n", enemy_idx);
    thing_pool_get(&pool, enemy_idx)->health = 75.0f;
    thing_pool_remove(&pool, enemy_idx);
    printf("  Removed slot %d\n", enemy_idx);
    thing *t3 = thing_pool_get(&pool, enemy_idx);
    printf("  pool_get(pool, %d): kind=%s, health=%.1f\n",
           enemy_idx, thing_kind_name(t3->kind), t3->health);
    assert(!thing_is_valid(t3));
    printf("  SAFE.\n\n");

    /* ══════ Crash Scenario 4: Pool full ══════ */
    printf("--- [4] Crash scenario: Pool full ---\n");
    {
        thing_pool full_pool;
        thing_pool_init(&full_pool);
        /* Fill it up (slots 1 through MAX_THINGS-1) */
        int added = 0;
        while (full_pool.next_empty < MAX_THINGS) {
            thing_pool_add(&full_pool, THING_KIND_GOBLIN);
            added++;
        }
        printf("  Filled pool with %d goblins\n", added);

        /* Try to add one more */
        thing_idx overflow = thing_pool_add(&full_pool, THING_KIND_TROLL);
        printf("  pool_add when full returned: %d (nil index)\n", overflow);
        thing *t4 = thing_pool_get(&full_pool, overflow);
        printf("  pool_get(pool, %d): kind=%s\n",
               overflow, thing_kind_name(t4->kind));
        assert(!thing_is_valid(t4));
        printf("  SAFE.\n\n");
    }

    /* ══════ Crash Scenario 5: Chain dereference through nil ══════ */
    printf("--- [5] Crash scenario: Chain dereference ---\n");
    printf("  Player (slot %d) has no children.\n", player_idx);

    /* Trace step by step */
    thing *step1 = thing_pool_get(&pool, player_idx);
    printf("  Step 1: pool_get(pool, %d) -> kind=%s (valid)\n",
           player_idx, thing_kind_name(step1->kind));

    thing_idx child_idx = step1->first_child;
    printf("  Step 2: player->first_child = %d (nil)\n", child_idx);

    thing *step2 = thing_pool_get(&pool, child_idx);
    printf("  Step 3: pool_get(pool, %d) -> kind=%s (nil sentinel)\n",
           child_idx, thing_kind_name(step2->kind));

    thing_idx grandchild_idx = step2->first_child;
    printf("  Step 4: nil->first_child = %d (nil again)\n", grandchild_idx);

    thing *step3 = thing_pool_get(&pool, grandchild_idx);
    printf("  Step 5: pool_get(pool, %d) -> kind=%s, health=%.1f\n",
           grandchild_idx, thing_kind_name(step3->kind), step3->health);

    assert(!thing_is_valid(step3));
    printf("  Three levels deep. No crash. Health = 0.0 (nil default).\n");
    printf("  SAFE.\n\n");

    /* ── Same chain as a one-liner ── */
    printf("--- Chain dereference one-liner ---\n");
    float chain_health =
        thing_pool_get(&pool,
            thing_pool_get(&pool,
                thing_pool_get(&pool, player_idx)->first_child
            )->first_child
        )->health;
    printf("  chain_health = %.1f (expected 0.0)\n", chain_health);
    assert(chain_health == 0.0f);
    printf("  SAFE.\n\n");

    /* ── Chain WITH real children ── */
    printf("--- Chain dereference WITH children ---\n");
    thing_idx axe = thing_pool_add(&pool, THING_KIND_ITEM);
    thing_pool_get(&pool, axe)->health = 50.0f;
    thing_add_child(&pool, player_idx, axe);

    float axe_health =
        thing_pool_get(&pool,
            thing_pool_get(&pool, player_idx)->first_child
        )->health;
    printf("  Player -> first_child -> health = %.1f (expected 50.0)\n",
           axe_health);
    assert(axe_health == 50.0f);

    /* Go one level deeper (axe has no children -> nil -> health = 0) */
    float deep_health =
        thing_pool_get(&pool,
            thing_pool_get(&pool,
                thing_pool_get(&pool, player_idx)->first_child
            )->first_child
        )->health;
    printf("  Player -> first_child -> first_child -> health = %.1f "
           "(expected 0.0, bottoms out at nil)\n", deep_health);
    assert(deep_health == 0.0f);
    printf("  Mixed real/nil chain works correctly.\n\n");

    /* ── thing_is_valid usage ── */
    printf("--- thing_is_valid demonstration ---\n");
    printf("  thing_is_valid(PLAYER)    = %s\n",
           thing_is_valid(thing_pool_get(&pool, player_idx))
           ? "true" : "false");
    printf("  thing_is_valid(nil)       = %s\n",
           thing_is_valid(thing_pool_get(&pool, 0))
           ? "true" : "false");
    printf("  thing_is_valid(OOB -100)  = %s\n",
           thing_is_valid(thing_pool_get(&pool, -100))
           ? "true" : "false");
    printf("  thing_is_valid(OOB 9999)  = %s\n",
           thing_is_valid(thing_pool_get(&pool, 9999))
           ? "true" : "false");
    printf("\n");

    /* ── Debug assertion ── */
    printf("--- Debug nil assertion ---\n");
    thing_pool_assert_nil_clean(&pool);
    printf("  thing_pool_assert_nil_clean passed. Nil is untouched.\n\n");

    /* ── memset as universal reset ── */
    printf("--- memset as universal reset ---\n");
    printf("  Before reset:\n");
    printf("    slot %d: kind=%s, health=%.1f, x=%.1f\n",
           player_idx,
           thing_kind_name(pool.things[player_idx].kind),
           pool.things[player_idx].health,
           pool.things[player_idx].x);
    printf("    slot %d: kind=%s, health=%.1f\n",
           axe,
           thing_kind_name(pool.things[axe].kind),
           pool.things[axe].health);
    printf("    used[%d]=%d, used[%d]=%d\n",
           player_idx, pool.used[player_idx],
           axe, pool.used[axe]);

    thing_pool_init(&pool);  /* one memset resets EVERYTHING */

    printf("  After reset (one memset + next_empty=1):\n");
    printf("    slot %d: kind=%s, health=%.1f\n",
           player_idx,
           thing_kind_name(pool.things[player_idx].kind),
           pool.things[player_idx].health);
    printf("    slot %d: kind=%s, health=%.1f\n",
           axe,
           thing_kind_name(pool.things[axe].kind),
           pool.things[axe].health);
    printf("    used[%d]=%d, used[%d]=%d\n",
           player_idx, pool.used[player_idx],
           axe, pool.used[axe]);
    printf("  Everything is nil. One memset. Done.\n\n");

    thing_pool_assert_nil_clean(&pool);

    /* ── Summary ── */
    printf("══════ Summary ══════\n");
    printf("  [1] Out-of-bounds:     returned nil safely.\n");
    printf("  [2] Unused slot:       returned nil safely.\n");
    printf("  [3] Removed entity:    returned nil safely.\n");
    printf("  [4] Pool full:         returned nil safely.\n");
    printf("  [5] Chain dereference: bottomed out at nil safely.\n");
    printf("  Debug assert:          nil sentinel is clean.\n");
    printf("  Universal reset:       one memset, everything zeroed.\n");
    printf("  Zero initialization IS the architecture.\n");

    return 0;
}
```

---

## Build and run

```bash
gcc -Wall -Wextra -std=c11 -DDEBUG -o lesson_06 lesson_06.c
./lesson_06
```

Note the `-DDEBUG` flag -- this enables `thing_pool_assert_nil_clean`.

## Expected result

```
Lesson 06: The Nil Sentinel and Zero Initialization
sizeof(thing) = 32 bytes

--- Nil sentinel state after init ---
  things[0].kind         = 0 (NIL)
  things[0].parent       = 0
  things[0].first_child  = 0
  things[0].next_sibling = 0
  things[0].prev_sibling = 0
  things[0].health       = 0.0
  things[0].x            = 0.0
  thing_is_valid(nil)    = false
  All zeros. Safe to read any field.

--- [1] Crash scenario: Out-of-bounds access ---
  pool_get(pool, -5): kind=NIL, health=0.0
  thing_is_valid = false
  SAFE.

--- [2] Crash scenario: Unused slot ---
  pool_get(pool, 500): kind=NIL, health=0.0
  SAFE.

--- [3] Crash scenario: Removed entity ---
  Added TROLL at slot 2
  Removed slot 2
  pool_get(pool, 2): kind=NIL, health=0.0
  SAFE.

--- [4] Crash scenario: Pool full ---
  Filled pool with 1023 goblins
  pool_add when full returned: 0 (nil index)
  pool_get(pool, 0): kind=NIL
  SAFE.

--- [5] Crash scenario: Chain dereference ---
  Player (slot 1) has no children.
  Step 1: pool_get(pool, 1) -> kind=PLAYER (valid)
  Step 2: player->first_child = 0 (nil)
  Step 3: pool_get(pool, 0) -> kind=NIL (nil sentinel)
  Step 4: nil->first_child = 0 (nil again)
  Step 5: pool_get(pool, 0) -> kind=NIL, health=0.0
  Three levels deep. No crash. Health = 0.0 (nil default).
  SAFE.

--- Chain dereference one-liner ---
  chain_health = 0.0 (expected 0.0)
  SAFE.

--- Chain dereference WITH children ---
  Player -> first_child -> health = 50.0 (expected 50.0)
  Player -> first_child -> first_child -> health = 0.0 (expected 0.0, bottoms out at nil)
  Mixed real/nil chain works correctly.

--- thing_is_valid demonstration ---
  thing_is_valid(PLAYER)    = true
  thing_is_valid(nil)       = false
  thing_is_valid(OOB -100)  = false
  thing_is_valid(OOB 9999)  = false

--- Debug nil assertion ---
  thing_pool_assert_nil_clean passed. Nil is untouched.

--- memset as universal reset ---
  Before reset:
    slot 1: kind=PLAYER, health=100.0, x=10.0
    slot 3: kind=ITEM, health=50.0
    used[1]=1, used[3]=1
  After reset (one memset + next_empty=1):
    slot 1: kind=NIL, health=0.0
    slot 3: kind=NIL, health=0.0
    used[1]=0, used[3]=0
  Everything is nil. One memset. Done.

══════ Summary ══════
  [1] Out-of-bounds:     returned nil safely.
  [2] Unused slot:       returned nil safely.
  [3] Removed entity:    returned nil safely.
  [4] Pool full:         returned nil safely.
  [5] Chain dereference: bottomed out at nil safely.
  Debug assert:          nil sentinel is clean.
  Universal reset:       one memset, everything zeroed.
  Zero initialization IS the architecture.
```

## Common mistakes

**1. Making THING_KIND_NIL something other than 0.**

```c
typedef enum thing_kind {
    THING_KIND_PLAYER = 0,  /* BUG: now memset makes everything a PLAYER */
    THING_KIND_NIL = 99,
    ...
};
```

If NIL is not 0, then `memset(&pool, 0, sizeof(pool))` creates entities that look like PLAYERs instead of nil. The entire zero-init architecture collapses.

**2. Adding a pointer field to the thing struct.**

```c
typedef struct thing {
    thing_kind kind;
    char *name;     /* BUG: memset sets this to NULL, dereferencing = crash */
    ...
};
```

The nil sentinel's `name` would be NULL. Code that does `strlen(nil_thing->name)` crashes. Use `char name[32]` (fixed-size array, zero-init gives empty string) instead.

**3. Returning NULL from pool_get instead of the nil sentinel.**

```c
thing *pool_get(thing_pool *pool, thing_idx idx) {
    if (...) return NULL;  /* BUG: caller must check for NULL everywhere */
}
```

The entire point of the nil sentinel is that pool_get NEVER returns NULL. Every return path must return `&things[0]`.

**4. Treating slot 0 as a usable slot.**

```c
thing_idx hero = 0;  /* BUG: 0 is the nil sentinel, not a valid entity */
pool.things[0].kind = THING_KIND_PLAYER;  /* corrupts nil sentinel */
```

Slot 0 is reserved. Always. `next_empty` starts at 1 for a reason.

**5. Forgetting to compile with `-DDEBUG` during development.**

Without `-DDEBUG`, `thing_pool_assert_nil_clean` becomes a no-op and you lose the safety net that catches writes to nil. Always develop with `-DDEBUG`.

**6. Designing fields where zero is a BAD default.**

```c
typedef struct thing {
    ...
    float scale;      /* 0.0 means invisible -- is that what you want? */
    int max_health;   /* 0 means "no max health" -- confusing */
};
```

Every field must be designed so that 0 is a meaningful "nothing." If 0 is a bad default for a field, rethink the field. For `scale`, 0.0 = "invisible" is actually correct for nil entities. For real entities, set `scale = 1.0f` explicitly after adding.

## Exercises

1. **Nil-safe chain helper.** Write a macro `CHAIN_GET(pool, idx, field)` that expands to `thing_pool_get(pool, thing_pool_get(pool, idx)->field)`. Use it to simplify the chain dereference syntax. Then chain 4 levels deep and verify it bottoms out at nil.

2. **Corrupted nil detector.** Temporarily add code that writes to `things[0]` (e.g., `pool.things[0].health = 42.0f`). Run with `-DDEBUG` and observe the assert fire. Then remove the corruption. This is what it looks like when game code writes through a nil reference.

3. **Pool reset verification.** Create a pool, add 50 entities with various kinds, health values, positions, and parent-child hierarchies. Print the state. Then call `thing_pool_init`. Print the state again. Verify that EVERYTHING -- entities, links, used flags, all of it -- is reset to zero.

4. **Zero-init audit.** Review the final thing struct from `things.h` (the complete version with all fields). For each field, write down what its zero value means and confirm it is a safe default. If you find a field where zero is unsafe, propose a fix.

## Connection to your work

In JavaScript, you already have a nil sentinel -- it is called `undefined`. When you access a missing object property, you get `undefined` instead of a crash. Optional chaining (`?.`) extends this: `user?.address?.city` evaluates to `undefined` if any link in the chain is missing, instead of throwing `TypeError`.

Our nil sentinel does the same thing, but at the data structure level rather than the language level. C does not have optional chaining or undefined. Instead, we build it into the pool: every invalid access returns a "nothing" entity that is safe to read and whose links all point back to itself.

The database world has a similar concept: the NULL row. Some ORMs (like Rails' NullObject pattern) return a special "null object" instead of nil when a query finds nothing. This null object responds to all the same methods as a real record but returns safe defaults. Same idea, different language.

The key insight -- and the reason this lesson is a PRINCIPLE, not just a technique -- is that **your data structure's failure mode is a design decision**. Most C code fails by crashing (NULL dereference). Most C++ code fails by throwing exceptions. Our code fails by returning the nil sentinel -- a known, safe, zero-initialized value. Every system built on top of this pool inherits that safety for free.

> **Cross-reference:** Game Programming Patterns Chapter 19 (Object Pool) discusses pool slot management. The nil sentinel is the safety layer on top of the basic pool pattern. GEA Chapter 15 discusses entity handles -- nil sentinels make handles safe to dereference even when invalid.

## The Erlang/Docker analogy

Anton compares nil-safety to how Docker and Erlang handle failures. Docker's philosophy: if a container crashes, restart it. Erlang's philosophy: "let it crash" and the supervisor restarts the process. Anton's nil sentinel is the same idea but at the memory level: if a reference is bad, you do not crash -- you get nil (safe zeros) and keep running. The recovery happens at the smallest possible scope -- a single bad lookup returns a zero struct instead of bringing down the whole program.

Think about what Docker actually does. A web server inside a container segfaults. Docker detects the crash, kills the container, spins up a fresh one. The user sees a brief hiccup, maybe a failed request, but the system keeps running. Erlang's OTP supervisors do the same thing: a process crashes, the supervisor restarts it with clean state, and the rest of the system is unaffected.

Our nil sentinel does this at the MEMORY level. A bad index would normally crash the program (NULL dereference, out-of-bounds access). Instead, `pool_get` returns the nil sentinel -- a clean, zero-initialized struct. The code that asked for the entity gets a "nothing" back and continues. The game loop keeps running. The frame renders. The player never sees a crash.

> **Anton says:** "Ironically it's similar philosophically... here I'm saying well you crashed in the memory but we recovered for you by giving you a nil... the Erlang or Docker safety net is the same thing. It's just like we'll restart the whole app when you crash. Here I'm like saying let's micro-restart this loop."

The key difference is SCOPE. Docker restarts an entire container. Erlang restarts an entire process. Our nil sentinel "restarts" a single variable lookup -- the smallest possible unit of recovery. The cost is proportionally smaller too: Docker takes seconds to restart a container. Erlang takes milliseconds to restart a process. Returning `&things[0]` takes nanoseconds.

This analogy is especially useful for web developers who know Docker containers. You already understand the "let it fail safely and recover" philosophy. You already know that crashing is not the end of the world IF you have a recovery mechanism. The nil sentinel is just that recovery mechanism, moved from the infrastructure layer (Docker, Kubernetes, process supervisors) down to the data structure layer.

---

## Don't add std::optional to your array

Anton warns about C++ feature creep sneaking into this architecture. The zero-init design is a COMMITMENT. Once you build on the assumption that `memset(0)` produces a valid state for every entity, anything that breaks that assumption breaks EVERYTHING.

The specific danger is C++ types with non-trivial constructors and destructors. Consider what happens if someone adds `std::optional`, `std::string`, or `std::vector` as a field inside the thing struct:

```cpp
/* DO NOT DO THIS */
struct thing {
    thing_kind kind;
    thing_idx parent, first_child, next_sibling, prev_sibling;
    float x, y, health;
    std::optional<float> mana;     /* has a constructor! */
    std::string name;              /* has a constructor AND destructor! */
};
```

`memset(&thing, 0, sizeof(thing))` does not call constructors. It writes raw zeros into the bytes that `std::optional` and `std::string` occupy. Those types expect their internal state to be initialized by their constructors -- `std::string` needs to set up a pointer to its internal buffer, `std::optional` needs to set its "has value" flag. Raw zeros might happen to work on some compilers, or they might produce undefined behavior, or they might crash when the destructor runs and tries to free a "buffer" at address 0x0.

> **Anton says:** "If we have to do constructors, things get nasty because now we have to call the destructor manually. Blah blah blah. Don't want to. Don't call constructors and destructors in your log cabin."

This extends beyond individual types to team dynamics. On a team, someone who is used to modern C++ might look at the thing struct and think "I can improve this." They add `std::optional<float> mana` because "it clearly communicates that mana might not be present." They add `std::vector<thing_idx> children` because "it's more flexible than intrusive lists." Each change seems reasonable in isolation. But each one breaks the memset contract.

The rule is simple: every field in the thing struct must be a type where all-zero-bits is a valid state. That means:

- Primitive types: `int`, `float`, `bool`, `uint32_t` -- all fine (zero is valid)
- Enums: fine (as long as 0 is a valid enum value, which it is -- `THING_KIND_NIL`)
- Fixed-size arrays: `char name[32]`, `thing_idx children[8]` -- fine (all zeros)
- Indices: `thing_idx` -- fine (0 = nil)
- C++ class types with constructors: NOT fine. Do not use them here.

This is not a limitation -- it is the foundation the entire system stands on. `thing_pool_init` is one memset. `thing_pool_remove` is one memset. Game reset is one memset. If you introduce types that need constructors, every one of those operations needs to be rewritten as a loop that calls constructors/destructors individually.

---

## Game reset = just zero

Here is Anton's ideal `game_reset`: literally `things = {};` -- zero EVERYTHING in one shot. No per-entity cleanup, no destructor chains, no "did I forget to reset this field?" bugs.

Our `thing_pool_init` does exactly this:

```c
void thing_pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}
```

That memset zeros every entity, every `used[]` flag, every generation counter, the free list head -- the entire world state. Then we set `next_empty = 1` to skip the nil sentinel, and we are ready to play again.

> **Anton says:** "I want to be able to just be like things equal zero. I just want to zero all my things. That's so much to ask for. I don't want to construct my things. I'm just going to zero them."

Think about how game reset works in other architectures:

```
Traditional OOP reset:
    for each entity in world:
        entity.cleanup()        // release resources
        entity.destructor()     // call destructor
    clear entity list
    for each system in engine:
        system.reset()          // reset system state
    reconstruct initial state   // call constructors again
    wire up references          // reconnect everything

    Questions you must answer:
    - Did every entity properly clean up?
    - Did any destructor throw an exception?
    - Is the entity list in a consistent state during cleanup?
    - Did any system hold a reference to a destroyed entity?
    - Are all the observers/listeners properly unsubscribed?

Zero-init reset:
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;

    Done. No questions. Every byte is zero.
    Every entity is nil. Every flag is false.
    Every link points to the nil sentinel.
```

This extends to features like "restart level," "new game," and even "load save file." Restart level? Zero the pool, re-add the level entities. New game? Zero the pool. Load save? Zero the pool, fread the save file into it. Every operation starts from the same clean slate.

The zero-init reset also eliminates an entire class of bugs: "leftover state from last round." In a traditional system, if you forget to reset one field during cleanup, that field carries over into the next game. With memset, there is no forgetting. Every byte is zeroed, whether you remembered it or not.

---

## What's next

We have the thing struct, intrusive tree links, and the nil sentinel. But we are still using a simple `next_empty` counter for allocation. What happens when entities die and we want to reuse their slots? Lesson 07 builds the full slot map: `pool_add`, `pool_remove`, and `pool_get` with proper `used[]` tracking and a clean API. Then lesson 08 adds the free list so removed slots get recycled in O(1) instead of wasted.
