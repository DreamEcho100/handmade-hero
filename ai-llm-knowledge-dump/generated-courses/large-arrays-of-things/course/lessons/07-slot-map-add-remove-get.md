# Lesson 07: Slot Map -- Add, Remove, Get

## What we're building

A managed pool that tracks which slots are alive, adds new things, removes dead ones, and never crashes on invalid access. By the end of this lesson you'll have a `thing_pool` struct with three operations -- `pool_add`, `pool_get`, `pool_remove` -- all O(1), all bulletproof. You start with the thing struct and nil sentinel from lesson 06 and build management on top.

## What you'll learn

- The `thing_pool` struct: `things[]` + `used[]` + `next_empty`
- `pool_add`: find a slot, zero it, set kind, mark used
- `pool_get`: bounds check + used check, return thing or nil sentinel
- `pool_remove`: mark unused, zero the slot
- Two liveness approaches: kind-check vs `used[]` array
- Why "deactivated objects are empty gaps" and what that means

## Prerequisites

- Lesson 06 (nil sentinel and zero initialization)

---

## Step 1 -- The problem: who's alive and who's dead?

Think about what we built so far. We have a flat array of things:

```c
thing things[MAX_THINGS];
```

And we've been adding things by hand:

```c
int next_slot = 1;
things[next_slot].kind = THING_KIND_PLAYER;
next_slot++;
things[next_slot].kind = THING_KIND_TROLL;
next_slot++;
```

That works for spawning a handful of things at startup. But a real game needs three operations that happen constantly:

1. **Spawn** -- a goblin enters the screen. Where does it go?
2. **Kill** -- the player kills the goblin. How do we mark it dead?
3. **Access** -- the AI wants to check the goblin's health. Is it still alive?

Right now we have no answer for any of these. If the player kills the troll at slot 2, what do we do? Set its kind to `THING_KIND_NIL`? But then slot 2 is a dead gap -- the next spawn goes to slot 6 instead of reusing slot 2. And if someone hands us index 7, is that a living thing or a corpse?

> **Why?** In JavaScript, you'd use a `Map<number, Entity>` with `set()`, `get()`, `delete()`. The Map handles all the bookkeeping -- you never think about which internal slot an entity occupies. In C, WE are the Map. We build the bookkeeping ourselves. The upside: because it's a flat array, it's faster than any hash map. The downside: we have to do the work.

Here's what we need:

| Operation | What it does | JS equivalent |
|---|---|---|
| **Add** | Find an empty slot, zero it, set its kind, mark it alive | `map.set(id, entity)` |
| **Get** | Given an index, return the thing if alive, nil if not | `map.get(id) ?? null` |
| **Remove** | Mark a slot as dead, zero the thing | `map.delete(id)` |

All three must be O(1) -- constant time, no scanning, no searching.

Let's build them.

---

## Step 2 -- The thing_pool struct

Before we write any functions, we need a struct that holds the array AND the bookkeeping data. I call it `thing_pool`.

Picture a row of numbered boxes -- slots 0 through `MAX_THINGS - 1`. Each box either holds a living thing or is empty. We need to know two things at all times:

1. **Which boxes are occupied?** We'll use a `used[]` array -- one `bool` per slot.
2. **Where's the next empty box?** We'll use `next_empty` -- an integer that bumps forward each time we fill a slot.

Let me show you the picture:

```
Index:    [0]    [1]      [2]     [3]      [4]    [5]    [6]  ...
Things:   NIL    PLAYER   TROLL   GOBLIN   ----   ----   ----
Used:     false  true     true    true     false  false  false
                                           ^
                                           next_empty = 4
```

Slot 0 is always the nil sentinel from lesson 06 -- never used, never allocated, never touched. Real things start at index 1.

Here's the code. Create a new file called `lesson_07.c` and type this in:

```c
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
```

> **Why?** These are C standard library headers. Think of them like JavaScript `import` statements.
> - `assert.h` gives us `assert()` -- like throwing an error if a condition is false. We use it for tests.
> - `stdbool.h` gives us `bool`, `true`, `false`. C doesn't have these by default (unlike JS where `boolean` is built-in). Without this header, you'd have to use `int` with 0/1.
> - `stdint.h` gives us exact-size integer types like `uint32_t` (an unsigned 32-bit integer). In JS, all numbers are 64-bit floats. In C, you pick the exact size you want.
> - `stdio.h` gives us `printf()` -- like `console.log()`.
> - `string.h` gives us `memset()` -- a function that fills a block of memory with a value. We'll use it to zero things out. There's no JS equivalent because JS doesn't let you touch raw memory.

Now add the constant and types:

```c
#define MAX_THINGS 64
```

> **Why?** `#define` is C's version of `const MAX_THINGS = 64` in JS. It's a preprocessor macro -- before the compiler even runs, every occurrence of `MAX_THINGS` in your code gets replaced with `64`. We use 64 for this lesson so the output is small and readable. A real game might use 1024 or 4096.

```c
typedef enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;
```

> **Why?** This is like a TypeScript string union: `type Kind = 'nil' | 'player' | 'troll' | 'goblin' | 'item'`. But in C, enums are integers. `THING_KIND_NIL` is 0, `THING_KIND_PLAYER` is 1, and so on -- the compiler auto-increments for you. The critical design choice: `NIL = 0`. That means when we zero out a thing with `memset`, its kind automatically becomes NIL. Zero is "nothing." This is the zero-init architecture from lesson 06 paying off.

```c
typedef int thing_idx;
```

> **Why?** `thing_idx` is just an `int` with a nicer name. It's a type alias -- like `type ThingIdx = number` in TypeScript. We use it everywhere an array index appears so the code reads clearly: "this variable is a thing index, not a health value or a pixel coordinate."

Now the thing struct itself:

```c
typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;
```

> **Why?** This is the same struct from earlier lessons. It's like a JavaScript object: `{ kind: 'player', parent: 0, firstChild: 0, nextSibling: 0, prevSibling: 0, x: 0, y: 0, health: 0 }`. Every entity type -- player, troll, goblin, item -- uses the same struct. Unused fields stay zero. That's the "fat struct" approach from lesson 02.

And now the pool:

```c
typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    thing_idx next_empty;
} thing_pool;
```

Let me explain each field:

**`thing things[MAX_THINGS]`** -- The flat array holding all entities. `things[0]` is always the nil sentinel (all zeros). Real things live in slots 1 through `MAX_THINGS - 1`. In JS terms, this is like `const entities: Entity[] = new Array(64)`.

**`bool used[MAX_THINGS]`** -- One boolean per slot. `used[i] == true` means slot `i` holds a living thing. `used[i] == false` means it's dead or never been used. This is the key addition over a raw array. In JS you might track this with a `Set<number>` of active indices.

**`thing_idx next_empty`** -- The index of the next slot that has NEVER been used. Starts at 1 (because slot 0 is the nil sentinel). Every time we add a thing, we put it at `next_empty` and bump this forward.

> **Why `used[]` instead of just checking `kind != NIL`?** Good question. Since `THING_KIND_NIL = 0` and removed things get zeroed, couldn't we just check `things[idx].kind != THING_KIND_NIL`? Yes, for now that would work. But in lesson 08 we'll build a free list where dead things store bookkeeping data in their fields -- the kind would still be NIL, but `next_sibling` would hold a free list pointer. Having a separate `used[]` array keeps liveness tracking independent of the thing's data. Think of it this way: `used[]` is the pool's opinion, `kind` is the thing's data. Separating concerns.

Here's what the empty pool looks like:

```
After initialization:

Index:    [0]    [1]    [2]    [3]    ...  [63]
Things:   NIL    ----   ----   ----         ----    (all zeros)
Used:     false  false  false  false        false   (all false)
                 ^
                 next_empty = 1
```

Every slot is empty. `things[0]` is the nil sentinel. `next_empty` is 1, meaning "the first available slot is slot 1."

**Build checkpoint.** Your file should now have the includes, the constant, the enum, the `thing_idx` typedef, the `thing` struct, and the `thing_pool` struct. It won't compile yet because we have no `main()` function. That's fine -- we'll add it step by step.

---

## Step 3 -- Helper: thing_kind_name

Before we write the pool operations, let's add a helper that turns a `thing_kind` enum into a human-readable string. This is purely for `printf` output -- it makes our test output readable.

```c
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
```

> **Why?** In JS, your enum might be string values already (`kind: 'player'`), so you'd just print them. In C, `thing_kind` is an integer (0, 1, 2, 3...). If you `printf("%d", kind)` you get `1`, not `PLAYER`. This function does the translation.
>
> The `static` keyword means "this function is only visible in this file." Like a non-exported function in a JS module. If you wrote this in a `.h` header shared across files, you'd remove `static`.

---

## Step 4 -- pool_init: reset everything to zero

Before we can add anything, we need to initialize the pool. Here's the function:

```c
static void pool_init(thing_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->next_empty = 1;
}
```

That's it. Two lines of real work. Let me explain them.

**`memset(pool, 0, sizeof(*pool))`** -- This is the C equivalent of "set every byte to zero." `memset` takes three arguments: a pointer to the memory you want to fill, the value to fill with (0), and the number of bytes. `sizeof(*pool)` gives us the size of the entire `thing_pool` struct -- the things array, the used array, everything.

> **Why?** `memset` zeros raw bytes. It doesn't understand structs or types -- it just blasts zeros into a block of memory. This works because we designed every field so that zero is a valid default:
> - `thing_kind` 0 = `THING_KIND_NIL` (nothing)
> - `thing_idx` 0 = nil sentinel (safe to dereference)
> - `float` 0 bits = 0.0f (valid position/velocity/health)
> - `bool` 0 = false (not used)
>
> This is the zero-init architecture from lesson 06. One `memset` resets the entire world.
>
> In JS, you'd do `this.entities = new Array(64).fill(null)` and `this.used = new Set()`. In C, we zero 2 KB of contiguous memory in one function call. Microseconds.

**`pool->next_empty = 1`** -- Slot 0 is the nil sentinel. It's permanently reserved. The first real thing goes in slot 1. So we set `next_empty` to 1.

> **Why `pool->` instead of `pool.`?** In C, when you have a pointer to a struct, you use `->` to access its fields. When you have the struct directly, you use `.`. `pool` here is a pointer (`thing_pool *pool`), so we use `->`. In JS, you always use `.` regardless -- JS handles the indirection for you.

After `pool_init`, the pool looks like this:

```
After pool_init:

Index:    [0]    [1]    [2]    [3]    ...  [63]
Things:   NIL    ----   ----   ----         ----    (all zeros)
Used:     false  false  false  false        false
                 ^
                 next_empty = 1

Every slot is empty. Slot 0 is the nil sentinel.
The next available slot is slot 1.
```

**Build checkpoint.** Let's add a minimal `main()` so we can compile and run. Add this at the bottom of your file:

```c
int main(void)
{
    thing_pool pool;
    pool_init(&pool);
    printf("Pool initialized. next_empty = %d\n", pool.next_empty);
    printf("sizeof(thing) = %zu bytes\n", sizeof(thing));
    printf("sizeof(thing_pool) = %zu bytes\n", sizeof(thing_pool));
    return 0;
}
```

> **Why `&pool`?** The `&` is the "address-of" operator. `pool` is a `thing_pool` struct sitting on the stack. `&pool` gives us a pointer to it. `pool_init` expects a pointer (`thing_pool *pool`) so it can modify the struct. Without `&`, we'd be passing a COPY of the struct, and the function would modify the copy instead of the original. In JS, objects are always passed by reference, so you never think about this.

Compile and run:

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c
./lesson_07
```

> **Why these flags?**
> - `-Wall` -- enable all common warnings
> - `-Wextra` -- enable extra warnings
> - `-Werror` -- treat warnings as errors (forces you to fix them)
> - `-std=c11` -- use the C11 standard (gives us `bool` and other modern features)
> - `-DDEBUG` -- define the `DEBUG` preprocessor macro (we'll use this for asserts later)
> - `-o lesson_07` -- name the output binary `lesson_07`

Expected output:

```
Pool initialized. next_empty = 1
sizeof(thing) = 32 bytes
sizeof(thing_pool) = 2116 bytes
```

It works. The pool is about 2 KB. For 64 entities, that's nothing. A real game with `MAX_THINGS = 1024` would use about 33 KB -- fits in CPU cache.

---

## Step 5 -- pool_add: claim a slot

Now the first real operation. When the game wants to spawn a new entity, it calls `pool_add` with a kind. The pool finds an empty slot, zeros it, stamps it with the kind, marks it used, and returns the slot index.

Delete your `main()` function for now (we'll write a better one later) and add this function after `pool_init`:

```c
static thing_idx pool_add(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) {
        return 0;
    }

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}
```

Let me walk through every line.

**`if (pool->next_empty >= MAX_THINGS)`** -- Is there room? `next_empty` starts at 1 and only goes up. If it has reached `MAX_THINGS` (which is 64), the array is full. Every slot from 0 to 63 has been touched.

**`return 0;`** -- Pool is full. Return 0, which is the nil index. The caller can check: if the returned index is 0, the add failed. Any code that does `pool_get(pool, 0)` gets the nil sentinel -- safe, not a crash.

> **Why?** In JS, you might return `null` or throw an exception when a collection is full. In C, we return 0 -- the index of the nil sentinel. Since every function in our system treats index 0 as "nothing," this is the C equivalent of returning `null`. But safer, because you can't accidentally dereference null.

**`thing_idx slot = pool->next_empty;`** -- Grab the current next-empty slot number.

**`pool->next_empty++;`** -- Advance the marker. The next add will use the slot after this one.

> **Why?** This two-line pattern -- "read the current value, then increment" -- is the simplest possible allocation. It's like doing `const id = this.nextId++` in JS. Constant time, no searching, no hashing.

**`memset(&pool->things[slot], 0, sizeof(thing));`** -- Zero the entire slot. "But wait," you might say, "the pool was already zeroed by `pool_init`!" True -- but later (in lesson 08) this slot might be getting reused after a removal. Zeroing here guarantees a clean slate every time, regardless of history.

> **Why?** `memset` fills the memory of one `thing` struct with zeros. `&pool->things[slot]` is the address of the thing at position `slot`. `sizeof(thing)` is 32 bytes (the size of one thing struct). After this line, every field of the thing -- kind, parent, first_child, x, y, health -- is zero. In JS, this is like `entities[slot] = { kind: 'nil', parent: 0, firstChild: 0, x: 0, y: 0, health: 0 }`.

**`pool->things[slot].kind = kind;`** -- Stamp it with the kind. This is what makes it a "real" thing instead of nil.

**`pool->used[slot] = true;`** -- Mark it alive. This is the explicit liveness flag.

**`return slot;`** -- Hand back the slot index. The caller stores this and uses it later to access or remove the thing.

Let's see what happens when we add three things. Add a new `main()`:

```c
int main(void)
{
    thing_pool pool;
    pool_init(&pool);

    thing_idx player = pool_add(&pool, THING_KIND_PLAYER);
    printf("Added PLAYER at slot %d\n", player);

    thing_idx troll = pool_add(&pool, THING_KIND_TROLL);
    printf("Added TROLL  at slot %d\n", troll);

    thing_idx goblin = pool_add(&pool, THING_KIND_GOBLIN);
    printf("Added GOBLIN at slot %d\n", goblin);

    printf("next_empty = %d\n", pool.next_empty);
    return 0;
}
```

**Build and run:**

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c
./lesson_07
```

Expected output:

```
Added PLAYER at slot 1
Added TROLL  at slot 2
Added GOBLIN at slot 3
next_empty = 4
```

Here's the pool state after those three adds:

```
After adding PLAYER, TROLL, GOBLIN:

Index:    [0]    [1]      [2]     [3]      [4]    [5]  ...
Things:   NIL    PLAYER   TROLL   GOBLIN   ----   ----
Used:     false  true     true    true     false  false
                                           ^
                                           next_empty = 4

Slot 0: nil sentinel (always zero, always safe)
Slot 1: PLAYER (alive)
Slot 2: TROLL (alive)
Slot 3: GOBLIN (alive)
Slot 4+: empty (never used)
```

Each add was O(1) -- no searching, no hashing, no allocation. Just bump a counter and write to a known slot.

---

## Step 6 -- pool_get: safe access with nil fallback

Given an index, return a pointer to the thing. But here's the rule: **never return NULL, never crash.**

If the index is out of bounds, return the nil sentinel. If the slot is unused, return the nil sentinel. If the index IS zero, return the nil sentinel (because that IS the nil sentinel).

Add this function:

```c
static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}
```

Five lines. Three conditions. Let me break them down.

**`idx > 0`** -- Rejects negative numbers AND zero. Zero is the nil sentinel itself -- you're not allowed to "get" it as if it's a real entity. And negative numbers are obviously invalid. In C, `int` can be negative (unlike JS's array indices which are always non-negative).

**`idx < MAX_THINGS`** -- Rejects out-of-bounds indices. If someone passes 9999, we don't read memory past the end of the array. That would be a buffer overrun -- one of the most common security vulnerabilities in C. This check prevents it.

**`pool->used[idx]`** -- Rejects dead slots. Even if the memory at `things[idx]` has leftover data from a previous entity, we won't hand it back if the slot is marked unused. The `used` array is the source of truth.

If ANY condition fails, we return `&pool->things[0]` -- the nil sentinel. All zeros. Kind = NIL. Safe to read every field. Safe to pass to any function.

> **Why?** This is the nil sentinel pattern from lesson 06 in action. In C, returning `NULL` means the caller MUST check for NULL before dereferencing. If they forget, segfault. By returning the nil sentinel instead, the caller can always safely do:
>
> ```c
> thing *t = pool_get(&pool, some_idx);
> /* Even without checking, t->health is just 0.0, not a crash */
> ```
>
> In JS, this is like `map.get(key) ?? defaultEntity`. The nil sentinel IS your null-coalescing operator -- but built into the data structure.

Let's also add the validity check function:

```c
static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}
```

> **Why?** Since `THING_KIND_NIL = 0`, and the nil sentinel has `kind = 0`, and removed things get zeroed to `kind = 0`, this single check covers every case. It's like `if (entity)` in JS -- a truthiness check. If it returns false, the thing is either the nil sentinel, a removed thing, or something that was never alive.

Now let's test it. Replace your `main()`:

```c
int main(void)
{
    thing_pool pool;
    pool_init(&pool);

    thing_idx player = pool_add(&pool, THING_KIND_PLAYER);
    thing_idx troll  = pool_add(&pool, THING_KIND_TROLL);
    thing_idx goblin = pool_add(&pool, THING_KIND_GOBLIN);

    /* Valid gets */
    thing *t;

    t = pool_get(&pool, player);
    printf("Get slot %d: kind=%s valid=%d\n",
           player, thing_kind_name(t->kind), thing_is_valid(t));

    t = pool_get(&pool, troll);
    printf("Get slot %d: kind=%s valid=%d\n",
           troll, thing_kind_name(t->kind), thing_is_valid(t));

    /* Invalid gets -- all return nil sentinel */
    t = pool_get(&pool, 0);
    printf("Get slot 0:    kind=%s valid=%d  <- nil sentinel\n",
           thing_kind_name(t->kind), thing_is_valid(t));

    t = pool_get(&pool, -5);
    printf("Get slot -5:   kind=%s valid=%d  <- negative\n",
           thing_kind_name(t->kind), thing_is_valid(t));

    t = pool_get(&pool, 9999);
    printf("Get slot 9999: kind=%s valid=%d  <- out of bounds\n",
           thing_kind_name(t->kind), thing_is_valid(t));

    t = pool_get(&pool, 50);
    printf("Get slot 50:   kind=%s valid=%d  <- unused slot\n",
           thing_kind_name(t->kind), thing_is_valid(t));

    return 0;
}
```

**Build and run:**

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c
./lesson_07
```

Expected output:

```
Get slot 1: kind=PLAYER valid=1
Get slot 2: kind=TROLL valid=1
Get slot 0:    kind=NIL valid=0  <- nil sentinel
Get slot -5:   kind=NIL valid=0  <- negative
Get slot 9999: kind=NIL valid=0  <- out of bounds
Get slot 50:   kind=NIL valid=0  <- unused slot
```

Here's the diagram of what happened:

```
pool_get(pool, 1)    -> &things[1]  (PLAYER, valid)     [checkmark]
pool_get(pool, 2)    -> &things[2]  (TROLL, valid)      [checkmark]

pool_get(pool, 0)    -> &things[0]  (NIL sentinel)      [safe fallback]
pool_get(pool, -5)   -> &things[0]  (NIL sentinel)      [safe fallback]
pool_get(pool, 9999) -> &things[0]  (NIL sentinel)      [safe fallback]
pool_get(pool, 50)   -> &things[0]  (NIL sentinel)      [safe fallback]

Every bad index returns the nil sentinel. No crash. No NULL.
```

Every bad index returns the same safe nil sentinel. No crash, no NULL pointer, no undefined behavior.

---

## Step 7 -- pool_remove: mark a slot as dead

Removing a thing doesn't deallocate anything -- there's nothing to deallocate. It's a flat array. We just:

1. Check the index is valid
2. Mark the slot as unused
3. Zero the thing data

Add this function:

```c
static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));
}
```

Let me walk through it.

**`if (idx <= 0 || idx >= MAX_THINGS) return;`** -- Can't remove the nil sentinel (index 0), can't remove negative indices, can't remove out-of-bounds indices. Just silently return. No crash.

> **Why silently return instead of crashing?** In a game, you might accidentally try to remove an entity that's already been killed by collision code earlier in the same frame. With this guard, that's harmless. In JS, `Map.delete()` on a key that doesn't exist is also a no-op -- same philosophy.

**`if (!pool->used[idx]) return;`** -- Already dead? No-op. This makes double-remove safe. You can call `pool_remove(pool, 5)` twice and nothing bad happens.

> **Why?** The `!` operator is "not" -- same as JavaScript. `!pool->used[idx]` means "if this slot is NOT used." If it's already unused, there's nothing to remove.

**`pool->used[idx] = false;`** -- Mark it dead. From this moment on, `pool_get` will return the nil sentinel for this index.

**`memset(&pool->things[idx], 0, sizeof(thing));`** -- Zero all the thing's data. The kind becomes `THING_KIND_NIL`, all links become 0, position becomes 0.0, health becomes 0.0.

> **Why zero the data?** Three reasons:
> 1. **Consistency.** If someone reads `pool->things[idx].kind` directly (bypassing `pool_get`), they see NIL, not stale TROLL data.
> 2. **Debug clarity.** When you inspect memory in a debugger, dead slots are all-zeros. Immediately obvious.
> 3. **Clean reuse.** When the slot eventually gets reused (lesson 08), it starts perfectly clean. No ghost data from the previous occupant.
>
> Without zeroing, dead slots still contain `kind = THING_KIND_TROLL` and `health = 50.0`. If any code reads the thing data without checking `used[]`, it thinks there's a living troll. That's the ghost-entity bug. Zeroing kills ghosts.

Let's test it. Replace your `main()`:

```c
int main(void)
{
    thing_pool pool;
    pool_init(&pool);

    /* Add 5 things */
    thing_idx player = pool_add(&pool, THING_KIND_PLAYER);
    thing_idx troll1 = pool_add(&pool, THING_KIND_TROLL);
    thing_idx goblin = pool_add(&pool, THING_KIND_GOBLIN);
    thing_idx troll2 = pool_add(&pool, THING_KIND_TROLL);
    thing_idx item   = pool_add(&pool, THING_KIND_ITEM);

    printf("Added 5 things. next_empty = %d\n\n", pool.next_empty);

    /* Show pool state before removal */
    printf("BEFORE removal:\n");
    for (int i = 0; i <= 6; i++) {
        printf("  [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        printf("\n");
    }

    /* Remove troll at slot 2 */
    printf("\nRemoving TROLL at slot %d...\n\n", troll1);
    pool_remove(&pool, troll1);

    /* Show pool state after removal */
    printf("AFTER removal:\n");
    for (int i = 0; i <= 6; i++) {
        printf("  [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        if (i == troll1) printf("  <- REMOVED");
        printf("\n");
    }

    /* Try to get the removed slot */
    thing *t = pool_get(&pool, troll1);
    printf("\npool_get(pool, %d) -> kind=%s valid=%d  <- dead slot returns nil\n",
           troll1, thing_kind_name(t->kind), thing_is_valid(t));

    /* Living things still work */
    t = pool_get(&pool, player);
    printf("pool_get(pool, %d) -> kind=%s valid=%d  <- still alive\n",
           player, thing_kind_name(t->kind), thing_is_valid(t));

    /* Suppress unused variable warnings */
    (void)goblin;
    (void)troll2;
    (void)item;

    return 0;
}
```

**Build and run:**

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c
./lesson_07
```

Expected output:

```
Added 5 things. next_empty = 6

BEFORE removal:
  [0] kind=NIL     used=0  <- NIL SENTINEL
  [1] kind=PLAYER  used=1
  [2] kind=TROLL   used=1
  [3] kind=GOBLIN  used=1
  [4] kind=TROLL   used=1
  [5] kind=ITEM    used=1
  [6] kind=NIL     used=0

Removing TROLL at slot 2...

AFTER removal:
  [0] kind=NIL     used=0  <- NIL SENTINEL
  [1] kind=PLAYER  used=1
  [2] kind=NIL     used=0  <- REMOVED
  [3] kind=GOBLIN  used=1
  [4] kind=TROLL   used=1
  [5] kind=ITEM    used=1
  [6] kind=NIL     used=0

pool_get(pool, 2) -> kind=NIL valid=0  <- dead slot returns nil
pool_get(pool, 1) -> kind=PLAYER valid=1  <- still alive
```

Look at the before and after:

```
BEFORE removing slot 2:

Index:    [0]    [1]      [2]     [3]      [4]     [5]      [6]
Things:   NIL    PLAYER   TROLL   GOBLIN   TROLL   ITEM     ----
Used:     false  true     true    true     true    true     false
                                                            ^
                                                            next_empty = 6


AFTER removing slot 2:

Index:    [0]    [1]      [2]     [3]      [4]     [5]      [6]
Things:   NIL    PLAYER   ----    GOBLIN   TROLL   ITEM     ----
Used:     false  true     false   true     true    true     false
                                                            ^
                                                            next_empty = 6  (unchanged!)

Slot 2 is now a GAP -- empty, but sitting between living things.
next_empty didn't move. It's still 6.
```

Notice: `next_empty` didn't change. It's still 6. Slot 2 is now dead -- a gap in the middle of the array. The next time we add a thing, it goes into slot 6, NOT slot 2. Slot 2 is wasted. We'll fix that in lesson 08 with the free list.

---

## Step 8 -- Two liveness approaches: kind-check vs used[]

Before we write the full test, I want to show you both ways to check if a slot is alive. We chose `used[]` -- but there's a simpler alternative.

**Variant A: check `kind != NIL`**

```c
static thing *pool_get_by_kind(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS
        && pool->things[idx].kind != THING_KIND_NIL) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}
```

This checks the thing's OWN data. If its kind is not NIL, it must be alive. No `used[]` array needed -- saves `MAX_THINGS` bytes of memory.

**Variant B: check `used[idx]` (what we use)**

```c
static thing *pool_get_by_used(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}
```

This checks the POOL's bookkeeping, independent of the thing's data.

Right now, both produce identical results. A living thing has `kind = PLAYER` (non-zero) AND `used = true`. A dead thing has `kind = NIL` (zero from memset) AND `used = false`. Both checks agree.

So why use `used[]`?

**Reason 1: Explicit vs implicit.** `used[idx] = true` is a deliberate statement from the pool: "I declare this slot alive." Checking `kind != NIL` is an inference: "this slot APPEARS to contain something." The distinction matters when debugging. If `used[5]` is true but `things[5].kind` is NIL, you know something went wrong -- the inconsistency is a signal. Without `used[]`, you lose that signal.

**Reason 2: Free list compatibility.** In lesson 08, dead things will store free list bookkeeping data in their `next_sibling` field. The `kind` will still be NIL (because we memset on remove), but the struct is NOT all-zeros anymore. Having `used[]` as a separate source of truth means the free list can store whatever it wants in dead slots without affecting liveness checks.

> **Why?** Think of it like this. In JS, you might use `WeakRef` to track whether an object still exists -- that's external tracking. Checking `obj.active === true` is internal tracking. If someone modifies `obj.active` by accident, your external tracker still knows the truth. Same idea: `used[]` is the external truth, `kind` is internal data.

For a minimal prototype where you'll never need a free list, Variant A is perfectly valid and saves memory. We use Variant B because we're building toward the complete system.

---

## Step 9 -- Putting it all together: add, get, remove cycle

Now let's write a full test that exercises everything: add 5 things, get them, remove 2, try to get the removed ones (should return nil), add one more (observe the gap problem), and verify double-remove is safe.

Replace your `main()` with this:

```c
int main(void)
{
    printf("=== Lesson 07: Slot Map -- Add, Remove, Get ===\n\n");

    thing_pool pool;
    pool_init(&pool);
    printf("Pool initialized. next_empty = %d\n\n", pool.next_empty);
```

> **Why?** We declare a `thing_pool` on the stack and initialize it. In JS terms: `const pool = new ThingPool()`. The `pool_init` zeros everything and sets `next_empty = 1`.

```c
    /* --- Add 5 things --- */
    printf("--- Adding 5 things ---\n");
    thing_idx player = pool_add(&pool, THING_KIND_PLAYER);
    printf("  Added PLAYER at slot %d\n", player);

    thing_idx troll1 = pool_add(&pool, THING_KIND_TROLL);
    printf("  Added TROLL  at slot %d\n", troll1);

    thing_idx goblin = pool_add(&pool, THING_KIND_GOBLIN);
    printf("  Added GOBLIN at slot %d\n", goblin);

    thing_idx troll2 = pool_add(&pool, THING_KIND_TROLL);
    printf("  Added TROLL  at slot %d\n", troll2);

    thing_idx item = pool_add(&pool, THING_KIND_ITEM);
    printf("  Added ITEM   at slot %d\n", item);
```

After these 5 adds:

```
Index:    [0]    [1]      [2]     [3]      [4]     [5]      [6]
Things:   NIL    PLAYER   TROLL   GOBLIN   TROLL   ITEM     ----
Used:     false  true     true    true     true    true     false
                                                            ^
                                                            next_empty = 6
```

```c
    /* --- Print pool state --- */
    printf("\n  Pool state (next_empty=%d):\n", pool.next_empty);
    for (int i = 0; i <= 6; i++) {
        printf("    [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        printf("\n");
    }
```

```c
    /* --- Get: valid and invalid --- */
    printf("\n--- Getting things ---\n");
    thing *t;

    t = pool_get(&pool, player);
    printf("  Get slot %d: kind=%s valid=%d\n",
           player, thing_kind_name(t->kind), thing_is_valid(t));
    assert(thing_is_valid(t));

    t = pool_get(&pool, 0);
    printf("  Get slot 0:    kind=%s valid=%d  <- nil sentinel\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, 9999);
    printf("  Get slot 9999: kind=%s valid=%d  <- out of bounds\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, -1);
    printf("  Get slot -1:   kind=%s valid=%d  <- negative\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));
```

> **Why `assert()`?** `assert(expression)` crashes the program if the expression is false. It's like `console.assert()` in JS, but in C it actually terminates the process (in debug builds). We use it to verify our assumptions. If any assert fails, we know exactly which line has a bug.

```c
    /* --- Remove two trolls --- */
    printf("\n--- Removing trolls at slots %d and %d ---\n",
           troll1, troll2);
    pool_remove(&pool, troll1);
    pool_remove(&pool, troll2);

    printf("  Pool state (next_empty=%d):\n", pool.next_empty);
    for (int i = 0; i <= 6; i++) {
        printf("    [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        if (i == troll1 || i == troll2) printf("  <- REMOVED");
        printf("\n");
    }
```

After removing trolls at slots 2 and 4:

```
Index:    [0]    [1]      [2]     [3]      [4]     [5]      [6]
Things:   NIL    PLAYER   ----    GOBLIN   ----    ITEM     ----
Used:     false  true     false   true     false   true     false
                                                            ^
                                                            next_empty = 6  (unchanged!)

Slots 2 and 4 are GAPS -- dead, but still sitting in the array.
```

```c
    /* --- Get the removed slots -- should return nil --- */
    printf("\n--- Getting removed slots ---\n");

    t = pool_get(&pool, troll1);
    printf("  Get slot %d (removed): kind=%s valid=%d\n",
           troll1, thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, troll2);
    printf("  Get slot %d (removed): kind=%s valid=%d\n",
           troll2, thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    /* Living things are still fine */
    t = pool_get(&pool, player);
    printf("  Get slot %d (player):  kind=%s valid=%d  <- still alive\n",
           player, thing_kind_name(t->kind), thing_is_valid(t));
    assert(thing_is_valid(t));
```

```c
    /* --- Add a new thing -- watch where it goes --- */
    printf("\n--- Adding one more thing ---\n");
    thing_idx new_item = pool_add(&pool, THING_KIND_ITEM);
    printf("  Added ITEM at slot %d\n", new_item);
    printf("  Slots %d and %d are WASTED! New item went to slot %d.\n",
           troll1, troll2, new_item);

    printf("\n  Pool state (next_empty=%d):\n", pool.next_empty);
    for (int i = 0; i <= 7; i++) {
        printf("    [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        if (i == troll1 || i == troll2) printf("  <- GAP");
        printf("\n");
    }
```

After adding one more item:

```
Index:    [0]    [1]      [2]     [3]      [4]     [5]     [6]     [7]
Things:   NIL    PLAYER   ----    GOBLIN   ----    ITEM    ITEM    ----
Used:     false  true     false   true     false   true    true    false
                                                                   ^
                                                                   next_empty = 7

The new ITEM went to slot 6, NOT slot 2 or 4.
Slots 2 and 4 are wasted gaps.
```

```c
    /* --- Double-remove is safe --- */
    printf("\n--- Safety checks ---\n");
    pool_remove(&pool, troll1);
    printf("  Double-remove slot %d: no crash (no-op)\n", troll1);

    pool_remove(&pool, 0);
    printf("  Remove nil (slot 0):   no crash (no-op)\n");

    pool_remove(&pool, -99);
    printf("  Remove slot -99:       no crash (no-op)\n");

    printf("\n=== Lesson 07 PASSED ===\n");
    return 0;
}
```

**Build and run:**

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c
./lesson_07
```

---

## Step 10 -- "Deactivated objects are empty gaps"

This is worth pausing on. It's the single most important consequence of the slot map design.

When you remove slot 2, slot 2 becomes a gap. The thing data is zeroed, `used[2]` is false, but the slot still occupies space in the array. Slots 1 and 3 are alive; slot 2 is dead air between them.

This is fundamentally different from how JavaScript arrays work. In JS, if you `splice()` an element out of an array, everything shifts down to fill the hole. No gaps. But indices change -- what was at position 3 is now at position 2.

```
JS splice (packed array):
  Before:  [PLAYER, TROLL, GOBLIN]   length=3
  splice(1, 1):
  After:   [PLAYER, GOBLIN]          length=2
  GOBLIN moved from index 2 to index 1!

Our slot map:
  Before:  [NIL] [PLAYER] [TROLL] [GOBLIN]   used: F T T T
  Remove slot 2:
  After:   [NIL] [PLAYER] [----]  [GOBLIN]   used: F T F T
  GOBLIN stays at slot 3. Slot 2 is a gap.
```

In a game engine, stable indices matter. If a troll holds a reference to "my parent is at slot 5," and then we remove slot 3, causing everything to shift down... now the parent is at slot 4, but the troll still thinks it's at slot 5. Every reference in the entire game is broken.

The gaps are the price we pay for stable indices. And stable indices are what make parent/child links, saved references, network sync, and generational IDs all work. It's a good trade.

But the gaps DO need to be reclaimed. If we keep adding and removing, the gaps pile up and `next_empty` advances past all of them:

```
After many add/remove cycles:

Index:  [0] [1] [2] [3] [4] [5] [6] [7] [8] [9] [10] ... [63]
Used:    -   X   -   X   -   -   X   -   X   -    -   ...   -
                                                              ^
                                                              next_empty = 64
                                                              POOL FULL!

But only 4 slots are actually used (1, 3, 6, 8).
60 slots are wasted gaps.
```

Lesson 08 fixes this with the **free list** -- an intrusive linked list of dead slots that gets checked before bumping `next_empty`.

---

## Build and run

Full compile command:

```bash
gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c
./lesson_07
```

## Expected result

```
=== Lesson 07: Slot Map -- Add, Remove, Get ===

Pool initialized. next_empty = 1

--- Adding 5 things ---
  Added PLAYER at slot 1
  Added TROLL  at slot 2
  Added GOBLIN at slot 3
  Added TROLL  at slot 4
  Added ITEM   at slot 5

  Pool state (next_empty=6):
    [0] kind=NIL     used=0  <- NIL SENTINEL
    [1] kind=PLAYER  used=1
    [2] kind=TROLL   used=1
    [3] kind=GOBLIN  used=1
    [4] kind=TROLL   used=1
    [5] kind=ITEM    used=1
    [6] kind=NIL     used=0

--- Getting things ---
  Get slot 1: kind=PLAYER valid=1
  Get slot 0:    kind=NIL valid=0  <- nil sentinel
  Get slot 9999: kind=NIL valid=0  <- out of bounds
  Get slot -1:   kind=NIL valid=0  <- negative

--- Removing trolls at slots 2 and 4 ---
  Pool state (next_empty=6):
    [0] kind=NIL     used=0  <- NIL SENTINEL
    [1] kind=PLAYER  used=1
    [2] kind=NIL     used=0  <- REMOVED
    [3] kind=GOBLIN  used=1
    [4] kind=NIL     used=0  <- REMOVED
    [5] kind=ITEM    used=1
    [6] kind=NIL     used=0

--- Getting removed slots ---
  Get slot 2 (removed): kind=NIL valid=0
  Get slot 4 (removed): kind=NIL valid=0
  Get slot 1 (player):  kind=PLAYER valid=1  <- still alive

--- Adding one more thing ---
  Added ITEM at slot 6
  Slots 2 and 4 are WASTED! New item went to slot 6.

  Pool state (next_empty=7):
    [0] kind=NIL     used=0  <- NIL SENTINEL
    [1] kind=PLAYER  used=1
    [2] kind=NIL     used=0  <- GAP
    [3] kind=GOBLIN  used=1
    [4] kind=NIL     used=0  <- GAP
    [5] kind=ITEM    used=1
    [6] kind=ITEM    used=1
    [7] kind=NIL     used=0

--- Safety checks ---
  Double-remove slot 2: no crash (no-op)
  Remove nil (slot 0):   no crash (no-op)
  Remove slot -99:       no crash (no-op)

=== Lesson 07 PASSED ===
```

## Common mistakes

1. **Forgetting `next_empty` starts at 1, not 0.** Slot 0 is the nil sentinel. If you start at 0, your first `pool_add` overwrites the nil sentinel and breaks everything.

2. **Not checking `used[]` in `pool_get`.** If you only check bounds (`idx > 0 && idx < MAX_THINGS`) without checking `pool->used[idx]`, you'll hand back dead things as if they're alive. Ghost entities.

3. **Removing index 0 (the nil sentinel).** The nil sentinel must NEVER be modified. That's why `pool_remove` checks `idx <= 0` and returns immediately. If you allowed removing index 0, the sentinel gets zeroed (which it already is, so no harm), but you might set `used[0] = false`... which it already is. Still, the guard is important for clarity: index 0 is off limits.

4. **Forgetting to zero the thing on remove.** If you only set `used[idx] = false` but don't `memset`, the dead slot still has `kind = THING_KIND_TROLL`. Any code that reads `things[idx].kind` directly (without going through `pool_get`) thinks there's a living troll. Zero the data.

5. **Passing `pool` instead of `&pool`.** `pool_init` and the other functions expect a pointer (`thing_pool *pool`). If you write `pool_init(pool)` instead of `pool_init(&pool)`, you'll get a compiler error about incompatible types. Always use `&` when passing the address of a stack variable.

## Exercises

1. **Fill the pool.** Write a loop that adds things until `pool_add` returns 0 (pool full). How many things fit? (Answer: `MAX_THINGS - 1` = 63, because slot 0 is the nil sentinel.)

2. **Add the `pool_get_by_kind` variant.** Implement Variant A (check `kind != NIL` instead of `used[]`). Run the same tests. Verify it produces identical results for all cases.

3. **Count living things.** Write a function `pool_count_alive(thing_pool *pool)` that iterates from slot 1 to `next_empty - 1` and counts how many slots have `used[i] == true`. Call it after every add and remove to watch the count change.

4. **Measure the gap waste.** After adding 20 things and removing 10, calculate: `wasted_gaps = next_empty - 1 - alive_count`. How bad is the waste? This motivates lesson 08's free list.

## Connection to your work

If you've ever built a JavaScript app with a list of items -- a todo list, a chat room, an inventory screen -- you've done the logical equivalent of what we just built. `Array.push()` is `pool_add`. `Array.find(id)` is `pool_get`. `Array.splice()` is `pool_remove`. The difference: in JS, the runtime handles memory, indices shift around, and everything is heap-allocated. In C, we control the memory layout, indices are stable, and everything is one contiguous block that the CPU can blast through.

The slot map pattern shows up everywhere in game engines. Unity's ECS uses slot maps. Unreal's actor arrays use a similar scheme. Any time you need O(1) add/remove/get with stable indices, this is the answer.

## What's next

We identified a problem: removed slots become gaps. New adds skip over them. After enough add/remove cycles, `next_empty` hits `MAX_THINGS` and the pool is "full" -- even though most slots are empty. Lesson 08 fixes this with the **free list**: when a thing is removed, its slot joins a linked list of free slots. When a new thing is added, the pool checks the free list FIRST before bumping `next_empty`. Dead slots get recycled. Zero waste.

---

## Complete file: lesson_07.c

Copy-paste this entire file. Compile with `gcc -Wall -Wextra -Werror -std=c11 -DDEBUG -o lesson_07 lesson_07.c`. Run with `./lesson_07`.

```c
/* ══════════════════════════════════════════════════════════════════════ */
/*  lesson_07.c -- Slot Map: Add, Remove, Get                          */
/*                                                                      */
/*  Course: Large Arrays of Things                                      */
/*  Stage:  Pool with things[], used[], next_empty.                     */
/*          NO generations yet. NO free list yet.                       */
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

typedef struct thing {
    thing_kind kind;
    thing_idx  parent;
    thing_idx  first_child;
    thing_idx  next_sibling;
    thing_idx  prev_sibling;
    float      x, y;
    float      health;
} thing;

typedef struct thing_pool {
    thing     things[MAX_THINGS];
    bool      used[MAX_THINGS];
    thing_idx next_empty;
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
}

static thing_idx pool_add(thing_pool *pool, thing_kind kind)
{
    if (pool->next_empty >= MAX_THINGS) {
        return 0;
    }

    thing_idx slot = pool->next_empty;
    pool->next_empty++;

    memset(&pool->things[slot], 0, sizeof(thing));
    pool->things[slot].kind = kind;
    pool->used[slot] = true;

    return slot;
}

static thing *pool_get(thing_pool *pool, thing_idx idx)
{
    if (idx > 0 && idx < MAX_THINGS && pool->used[idx]) {
        return &pool->things[idx];
    }
    return &pool->things[0];
}

static void pool_remove(thing_pool *pool, thing_idx idx)
{
    if (idx <= 0 || idx >= MAX_THINGS) return;
    if (!pool->used[idx]) return;

    pool->used[idx] = false;
    memset(&pool->things[idx], 0, sizeof(thing));
}

static bool thing_is_valid(const thing *t)
{
    return t->kind != THING_KIND_NIL;
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== Lesson 07: Slot Map -- Add, Remove, Get ===\n\n");

    /* --- Initialize --- */
    thing_pool pool;
    pool_init(&pool);
    printf("Pool initialized. next_empty = %d\n\n", pool.next_empty);

    /* --- Add 5 things --- */
    printf("--- Adding 5 things ---\n");
    thing_idx player = pool_add(&pool, THING_KIND_PLAYER);
    printf("  Added PLAYER at slot %d\n", player);

    thing_idx troll1 = pool_add(&pool, THING_KIND_TROLL);
    printf("  Added TROLL  at slot %d\n", troll1);

    thing_idx goblin = pool_add(&pool, THING_KIND_GOBLIN);
    printf("  Added GOBLIN at slot %d\n", goblin);

    thing_idx troll2 = pool_add(&pool, THING_KIND_TROLL);
    printf("  Added TROLL  at slot %d\n", troll2);

    thing_idx item = pool_add(&pool, THING_KIND_ITEM);
    printf("  Added ITEM   at slot %d\n", item);

    printf("\n  Pool state (next_empty=%d):\n", pool.next_empty);
    for (int i = 0; i <= 6; i++) {
        printf("    [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        printf("\n");
    }

    /* --- Get: valid and invalid --- */
    printf("\n--- Getting things ---\n");
    thing *t;

    t = pool_get(&pool, player);
    printf("  Get slot %d: kind=%s valid=%d\n",
           player, thing_kind_name(t->kind), thing_is_valid(t));
    assert(thing_is_valid(t));

    t = pool_get(&pool, 0);
    printf("  Get slot 0:    kind=%s valid=%d  <- nil sentinel\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, 9999);
    printf("  Get slot 9999: kind=%s valid=%d  <- out of bounds\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, -1);
    printf("  Get slot -1:   kind=%s valid=%d  <- negative\n",
           thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    /* --- Remove two trolls --- */
    printf("\n--- Removing trolls at slots %d and %d ---\n",
           troll1, troll2);
    pool_remove(&pool, troll1);
    pool_remove(&pool, troll2);

    printf("  Pool state (next_empty=%d):\n", pool.next_empty);
    for (int i = 0; i <= 6; i++) {
        printf("    [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        if (i == troll1 || i == troll2) printf("  <- REMOVED");
        printf("\n");
    }

    /* --- Get the removed slots --- */
    printf("\n--- Getting removed slots ---\n");

    t = pool_get(&pool, troll1);
    printf("  Get slot %d (removed): kind=%s valid=%d\n",
           troll1, thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, troll2);
    printf("  Get slot %d (removed): kind=%s valid=%d\n",
           troll2, thing_kind_name(t->kind), thing_is_valid(t));
    assert(!thing_is_valid(t));

    t = pool_get(&pool, player);
    printf("  Get slot %d (player):  kind=%s valid=%d  <- still alive\n",
           player, thing_kind_name(t->kind), thing_is_valid(t));
    assert(thing_is_valid(t));

    /* --- Add one more -- observe the gap problem --- */
    printf("\n--- Adding one more thing ---\n");
    thing_idx new_item = pool_add(&pool, THING_KIND_ITEM);
    printf("  Added ITEM at slot %d\n", new_item);
    printf("  Slots %d and %d are WASTED! New item went to slot %d.\n",
           troll1, troll2, new_item);

    printf("\n  Pool state (next_empty=%d):\n", pool.next_empty);
    for (int i = 0; i <= 7; i++) {
        printf("    [%d] kind=%-7s used=%d",
               i, thing_kind_name(pool.things[i].kind), pool.used[i]);
        if (i == 0) printf("  <- NIL SENTINEL");
        if (i == troll1 || i == troll2) printf("  <- GAP");
        printf("\n");
    }

    /* --- Safety: double-remove, nil-remove, out-of-bounds remove --- */
    printf("\n--- Safety checks ---\n");
    pool_remove(&pool, troll1);
    printf("  Double-remove slot %d: no crash (no-op)\n", troll1);

    pool_remove(&pool, 0);
    printf("  Remove nil (slot 0):   no crash (no-op)\n");

    pool_remove(&pool, -99);
    printf("  Remove slot -99:       no crash (no-op)\n");

    /* Suppress unused variable warnings */
    (void)goblin;

    printf("\n=== Lesson 07 PASSED ===\n");
    return 0;
}
```
