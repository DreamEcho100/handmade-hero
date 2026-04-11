# Lesson 03: Indices, Not Pointers

## What we're building

We're adding inter-entity references to the thing struct. Specifically, a `parent` field so entities can say "I belong to that other entity." The twist: we're NOT using pointers. We're using plain integer indices. This lesson explains why pointers break, why indices work, and gives you a compilable program that proves it.

## What you'll learn

- Why raw pointers to array elements are fragile (the reallocation problem)
- `thing_idx` -- a typedef'd integer that serves as a stable reference
- How to add a `parent` field to the thing struct and use it
- Why indices serialize trivially and pointers don't
- The mental model of "everything is an offset into the same array"

## Prerequisites

- Lesson 02 (thing struct, kind enum, static array, pool_add)
- Familiarity with JavaScript array indexing
- No prior understanding of C pointers needed -- this lesson explains them from scratch

---

## Step 1 -- The Problem: Pointers to Array Elements

> **JS bridge: What IS a pointer?** If you are coming from JavaScript, you have never explicitly used pointers. Here is the mental model. In JS, when you write `let player = entities[1]`, `player` holds a *reference* to the object -- but you never see the memory address, and the garbage collector can move the object around transparently. In C, a *pointer* is a variable that holds a raw memory address -- a number like `0x7ffe1234` that says "go look at this exact byte in RAM." The syntax: `thing *p` declares a pointer `p` that holds the address of a `thing`. `&things[1]` means "the address of `things[1]`" (the `&` is the "address-of" operator). `*p` or `p->health` means "go to that address and read/write the data there" (this is called *dereferencing*). Pointers give you direct control over memory, which is powerful -- and dangerous, as this lesson demonstrates.

Let's say the player picks up a sword. The sword needs to know its parent is the player. The most obvious C approach is a pointer:

```c
typedef struct thing {
    thing_kind kind;
    float      x, y;
    float      health;
    struct thing *parent;  /* <-- pointer to another thing */
} thing;
```

> **Why `struct thing *parent`?** The `*` means "this field holds a pointer (memory address), not a value." `struct thing *parent` means "`parent` is a memory address that points to a `thing`." In JS terms, it is like storing `player` as a direct object reference instead of storing `1` as an index into an array. The `struct thing` part (instead of just `thing`) is required here because we are inside the struct's own definition -- the `typedef` alias is not available yet.

And you'd set it like this:

```c
things[sword].parent = &things[player];  /* "sword's parent is player" */
```

> **Why `&things[player]`?** The `&` operator means "give me the memory address of this thing." So `&things[1]` evaluates to something like `0x7ffe1010` -- the physical location where `things[1]` lives in RAM. We are storing that address in `parent`. Later, `*parent` or `parent->health` goes to that address and reads the data.

This works. For now. Until you do any of the following:

1. **Reallocate the array** (switch from `thing things[1024]` to `thing things[4096]`, or use `realloc`).
2. **Copy the array** (save state, undo system, networking snapshot).
3. **Serialize to disk** (save game).
4. **Move to a different address space** (hot-reload the game DLL).

Let's draw what happens with reallocation, because it's the most common case and the most destructive.

### The reallocation disaster

Suppose you outgrow your 1024-slot array and need to grow to 2048. You allocate a bigger array and `memcpy` the old data over:

```
    BEFORE realloc:

    things[] at address 0x1000:
    ┌─────────┬─────────┬─────────┬─────────┐
    │  [0]    │  [1]    │  [2]    │  [3]    │
    │  NIL    │ PLAYER  │ TROLL   │ ITEM    │
    │         │         │         │ parent ─┼──► 0x1010 (address of things[1])
    └─────────┴─────────┴─────────┴─────────┘
    0x1000    0x1010    0x1020    0x1030

    Sword's parent pointer = 0x1010 (player's address in memory)
```

Now you `realloc` or allocate a new, bigger array:

```
    AFTER realloc:

    NEW things[] at address 0x5000:
    ┌─────────┬─────────┬─────────┬─────────┬──── ─ ─ ─ ─ ┐
    │  [0]    │  [1]    │  [2]    │  [3]    │              │
    │  NIL    │ PLAYER  │ TROLL   │ ITEM    │  ...2048     │
    │         │         │         │ parent ─┼──► 0x1010  ← DANGLING!
    └─────────┴─────────┴─────────┴─────────┴──── ─ ─ ─ ─ ┘
    0x5000    0x5010    0x5020    0x5030

    OLD memory at 0x1000 has been freed!
    Sword's parent pointer still says 0x1010.
    That memory is GONE. Dereferencing it = undefined behavior.
    Could crash. Could read garbage. Could corrupt unrelated memory.
```

The pointer `0x1010` was the address of the player *in the old array*. After reallocation, the player lives at `0x5010` but the pointer still says `0x1010`. The old memory is freed. You have a dangling pointer.

> **JS bridge: Why doesn't this happen in JavaScript?** In JS, the garbage collector manages memory. When V8 moves an object to a new location in the heap (compacting GC), it updates ALL references to that object automatically. You never see the address, so you never hold a stale one. C has no garbage collector. When you store a raw address and the data moves, nobody updates your stored address. It just points to freed memory. That is the fundamental difference: JS references are managed. C pointers are raw.

> **Common mistake:** "I'm using a static array, so I'll never realloc." True for THIS program. But the moment you embed things in a larger struct that gets copied, or you hot-reload a DLL that changes the base address, or you add networking that snapshots state -- all your pointers break. Indices are future-proof. Pointers are a ticking time bomb.

### Three more scenarios where pointers break

The reallocation case is the most dramatic, but it's not the only one. Here are three more failure modes that hit real game engines:

**1. Save/Load (serialization).** You write the entity pool to a save file. The player quits, reboots, and loads the save. The array is now at a completely different memory address. Every pointer in the loaded data is garbage. With indices, the save file is just an array of ints and floats -- `things[3].parent = 1` works on any load, on any machine, on any OS.

**2. Hot-reload (game DLL reloading).** You change game code, recompile the game DLL, and the engine reloads it without restarting. The new DLL might be loaded at a different address. Any function pointers in the old DLL are now dangling. But more subtly: if the game state struct was allocated by the old DLL, its address might be the same BUT any pointers stored inside entity data that pointed to other entities are still valid only by luck. Indices don't care about addresses at all.

**3. Networking (state synchronization).** You send the player's entity state to another machine. The other machine has its own `things[]` array at a completely different address. A pointer `0x7ffe1234` means nothing on the other machine. An index `3` means "slot 3" on both machines.

> **Anton says:** "Pointers are a liability. Every pointer in your data is a thing you have to fix up on load, fix up on DLL reload, fix up on network sync. Indices are free. They just work."

---

## Step 2 -- The Fix: Integer Indices

Instead of storing the player's *address*, store the player's *slot number*:

```c
typedef int thing_idx;  /* just an int -- an index into things[] */

typedef struct thing {
    thing_kind kind;
    float      x, y;
    float      health;
    thing_idx  parent;  /* index, not pointer */
} thing;
```

Now setting the parent:

```c
things[sword].parent = player;  /* "sword's parent is at index 1" */
```

And following the reference:

```c
thing *parent = &things[things[sword].parent];  /* look up by index */
```

> **JS bridge:** This is identical to `const parent = entities[entities[sword].parentId]` -- look up the sword's `parentId`, then use that ID to index into the same array. The `&` in front just means "give me a pointer to the result" so we can store it in `thing *parent` (a pointer variable). If you ignore the `&` and `*`, the core operation is just an array lookup by index -- something you do every day in JS.

Let's see what happens when the array reallocates:

```
    BEFORE realloc:

    things[] at address 0x1000:
    ┌─────────┬─────────┬─────────┬─────────┐
    │  [0]    │  [1]    │  [2]    │  [3]    │
    │  NIL    │ PLAYER  │ TROLL   │ ITEM    │
    │         │         │         │parent=1 │  ← just the number 1
    └─────────┴─────────┴─────────┴─────────┘

    AFTER realloc (array moves to new address):

    NEW things[] at address 0x5000:
    ┌─────────┬─────────┬─────────┬─────────┬──── ─ ─ ─ ─ ┐
    │  [0]    │  [1]    │  [2]    │  [3]    │              │
    │  NIL    │ PLAYER  │ TROLL   │ ITEM    │  ...2048     │
    │         │         │         │parent=1 │  ← STILL 1. STILL CORRECT.
    └─────────┴─────────┴─────────┴─────────┴──── ─ ─ ─ ─ ┘

    things[3].parent = 1
    things[1] = PLAYER     ← still the player, at the same index
```

The index `1` means "slot 1 in whatever array I'm looking at." It doesn't care where the array lives in memory. The array could be at address `0x1000` or `0x5000` or on the moon. Index 1 is index 1.

> **Why?** An index is a *relative* reference: "the thing at position N in the array." A pointer is an *absolute* reference: "the thing at memory address 0x1010." Relative references survive movement. Absolute references don't. This is the same reason why relative URLs (`./page2.html`) are more portable than absolute URLs (`http://localhost:3000/page2.html`).

> **New technique: thing_idx -- integer index as entity reference.** A `typedef int thing_idx` that stores a slot number instead of a pointer. It's just an int, but the typedef documents intent: "this int is a thing reference, not a count or a coordinate."
> - JS equivalent: `entityId: number` -- an index into `entities[]`
> - What it replaces: `struct thing *parent`
> - Why it works: indices are relative to the array, not to memory. They survive reallocation, copying, and serialization.

---

## Step 3 -- The typedef

```c
typedef int thing_idx;
```

> **JS bridge:** This is exactly like TypeScript's `type ThingIdx = number`. At runtime, a `ThingIdx` IS a `number` -- TypeScript erases the alias. Same in C: `thing_idx` IS `int`. The compiler treats them identically. No runtime cost. No conversion. It is purely a documentation mechanism.

This is a type alias. `thing_idx` IS `int`. The compiler treats them identically. No runtime cost. No conversion. It's purely a documentation mechanism.

Why bother if it's just an int?

Because when you read `thing_idx parent` you instantly know: "this is a reference to another thing." When you read `int parent` you have to guess: "is this a count? A coordinate? An ID? An index?" The typedef removes ambiguity.

> **Why?** TypeScript developers do this constantly: `type UserId = number`. It's still a `number` at runtime. But it makes function signatures self-documenting: `getUser(id: UserId)` vs `getUser(id: number)`. Same idea in C, same benefit.

The convention for this course:

- `thing_idx` = an index into `things[]`. Value 0 = nil (empty/no-parent).
- To follow a `thing_idx`: `things[idx]` gives you the thing.
- To get a `thing_idx`: `pool_add()` returns one.

---

## Step 4 -- Adding the Parent Field

Let's update the thing struct from Lesson 02:

```c
typedef struct thing {
    thing_kind kind;
    float      x, y;
    float      health;
    thing_idx  parent;   /* NEW: index of parent thing (0 = no parent) */
} thing;
```

Memory layout with the new field:

```
    Byte offset:  0       4       8       12      16      20
                  ┌───────┬───────┬───────┬───────┬───────┐
    thing =       │ kind  │   x   │   y   │health │parent │
                  │ (4B)  │ (4B)  │ (4B)  │ (4B)  │ (4B)  │
                  └───────┴───────┴───────┴───────┴───────┘
                  ◄──────────── 20 bytes total ────────────►
```

20 bytes per thing, up from 16. The pool goes from 16 KB to 20 KB. Still fits in L1 cache.

The `parent` field is 0 when unused. Zero is the nil index -- it points to `things[0]`, which is the nil sentinel (all zeros). So "no parent" is simply the default zero-initialized value. No special sentinel value like `-1` needed.

> **Why?** Using 0 as nil (instead of -1 or some other magic number) means zero-initialization automatically sets every reference to "nothing." When you `memset` the pool to zero, every `parent` field becomes 0 = nil. No initialization loops needed. This is a deliberate design choice that pays off massively in Lesson 06.

---

## Step 5 -- Setting Up Parent-Child Relationships

Now let's give the player an inventory. The player picks up a sword and a potion:

```c
int player = pool_add(THING_KIND_PLAYER);
things[player].x      = 5.0f;
things[player].y      = 3.0f;
things[player].health = 100.0f;

int sword = pool_add(THING_KIND_ITEM);
things[sword].x      = 5.0f;
things[sword].y      = 3.0f;
things[sword].parent = player;  /* sword belongs to player */

int potion = pool_add(THING_KIND_ITEM);
things[potion].x      = 5.0f;
things[potion].y      = 3.0f;
things[potion].parent = player;  /* potion belongs to player */
```

After this, the array looks like:

```
    things[]:
    ┌─────────┬──────────┬──────────┬──────────┬──────────┐
    │  [0]    │  [1]     │  [2]     │  [3]     │  [4]     │
    │  NIL    │  PLAYER  │  TROLL   │  ITEM    │  ITEM    │
    │  par=0  │  par=0   │  par=0   │  par=1   │  par=1   │
    │         │          │          │  (sword) │  (potion)│
    └─────────┴──────────┴──────────┴──────────┴──────────┘
                   ▲                     │          │
                   └─────────────────────┴──────────┘
                        "our parent is index 1"
```

To find an entity's parent, you follow the index:

```c
thing_idx parent_idx = things[sword].parent;      /* = 1 */
thing    *parent     = &things[parent_idx];        /* = &things[1] = PLAYER */
printf("Sword's parent is %s\n",
       thing_kind_name(parent->kind));             /* "PLAYER" */
```

To find all children of an entity, you scan (for now -- Lesson 04 adds intrusive linked lists for O(1) child traversal):

```c
printf("Player's inventory:\n");
for (int i = 1; i < next_empty_slot; i++) {
    if (things[i].parent == player) {
        printf("  slot %d: %s\n", i, thing_kind_name(things[i].kind));
    }
}
```

> **Common mistake:** Writing `things[sword].parent = &things[player]`. This stores a POINTER, not an index. The compiler might warn you (assigning `thing*` to `int`), or it might silently truncate the address to an int. Either way: wrong, dangerous, and exactly the bug this lesson prevents.

---

## Step 6 -- The Serialization Benefit

Here's a benefit that might not be obvious yet but becomes critical in real games: indices serialize trivially.

If you want to save the game state to a file, every `thing_idx` field is just an integer. You write the array to disk:

```
    Writing to disk:
    ┌───────────────────────────────────────┐
    │ thing[0]: kind=0 x=0.0 ... parent=0  │  ← all integers and floats
    │ thing[1]: kind=1 x=5.0 ... parent=0  │
    │ thing[2]: kind=2 x=10. ... parent=0  │
    │ thing[3]: kind=4 x=5.0 ... parent=1  │  ← "1" is still valid after load
    │ thing[4]: kind=4 x=5.0 ... parent=1  │
    └───────────────────────────────────────┘
```

When you load the file back, the indices still make sense. Slot 3's parent is 1. Slot 1 is still the player. Done.

With pointers? Disaster:

```
    Writing to disk:
    │ thing[3]: kind=4 x=5.0 ... parent=0x7ffe1234  │  ← memory address!
    
    Loading on a different run:
    │ thing[3]: kind=4 x=5.0 ... parent=0x7ffe1234  │  ← WRONG ADDRESS
    │ things[] is now at 0x7ffe5678 — 0x7ffe1234 is garbage
```

Pointers are meaningless outside the process that created them. Indices are meaningful anywhere the same array exists.

> **Why?** If you've ever serialized a Redux state to `localStorage` as JSON, you know this intuitively. Your state object uses IDs (`userId: 5`), not object references. The IDs survive serialization because they're just numbers. C indices are the same thing -- just numbers that refer to array positions.

> **Handmade Hero connection:** Casey uses handles (index + generation) for entity references in Handmade Hero, never raw pointers. His entity references survive across frames, across save/load, and across hot-reload of the game DLL. We'll add the generation part in Lesson 09.

### Indices vs. pointers: the complete comparison

Here's the full tradeoff table so you can reference it later:

```
    Feature                 Pointer             Index (thing_idx)
    ───────────────────     ─────────────       ────────────────────
    Follow reference        *ptr (1 deref)      things[idx] (1 deref)
    Speed                   Identical            Identical
    Survives realloc        NO                   YES
    Survives memcpy         NO                   YES
    Survives save/load      NO                   YES
    Survives hot-reload     NO                   YES
    Survives network sync   NO                   YES
    Size (64-bit)           8 bytes              4 bytes (int)
    Null representation     NULL (address 0)     0 (index 0 = nil sentinel)
    Bounds-checkable        Hard                 Easy (0 < idx < MAX)
    Debugger-friendly       Shows address        Shows slot number
```

Pointers are faster by zero nanoseconds (both are one memory access) and cost more memory (8 bytes vs 4 on 64-bit). There is no advantage to pointers for intra-pool references. None.

The one thing pointers CAN do that indices can't is reference objects in DIFFERENT arrays or on the heap. But within a single pool -- which is our entire entity system -- indices are strictly superior.

---

## Step 7 -- Following the Index Safely

One thing to watch out for: what if `parent` is 0 (no parent) and you try to follow it?

```c
thing *parent = &things[things[sword].parent];
```

If `parent = 0`, this gives you `&things[0]`, which is the nil sentinel. That's actually fine -- `things[0]` is always zero-initialized, kind = NIL, all fields zero. You won't crash. You'll just get a nil entity.

But what if `parent` is out of bounds? Say someone corrupts the value to 9999:

```c
things[sword].parent = 9999;
thing *parent = &things[things[sword].parent]; /* things[9999] -- OUT OF BOUNDS */
```

That's a buffer overrun. In this lesson's simple code, we don't guard against it. In Lesson 06, we'll build `pool_get()` which validates the index before accessing the array. For now, just know that the full system will have bounds checking.

> **Common mistake:** Not checking bounds on `thing_idx` values before using them as array indices. In this lesson we're keeping it simple, but production code ALWAYS validates. Lesson 06 makes this bullet-proof.

---

## Build and run

Save the complete file below as `lesson03.c`, then compile and run:

```bash
gcc -Wall -Wextra -Werror -std=c11 -o lesson03 lesson03.c
./lesson03
```

---

## Expected result

```
=== Indices, Not Pointers ===
sizeof(thing) = 20 bytes
Pool memory: 20480 bytes (20.0 KB)

Added PLAYER at slot 1
Added TROLL at slot 2
Added ITEM (sword) at slot 3, parent = 1
Added ITEM (potion) at slot 4, parent = 1

Pool contents (4 things):
  [1] kind=PLAYER   x=5.0  y=3.0  health=100.0  parent=0
  [2] kind=TROLL    x=10.0  y=7.0  health=30.0  parent=0
  [3] kind=ITEM     x=5.0  y=3.0  health=0.0  parent=1
  [4] kind=ITEM     x=5.0  y=3.0  health=0.0  parent=1

--- Following indices ---
Sword (slot 3) parent index = 1
Sword's parent is: PLAYER at (5.0, 3.0)
Potion (slot 4) parent index = 1
Potion's parent is: PLAYER at (5.0, 3.0)

--- Player's children (linear scan) ---
  slot 3: ITEM
  slot 4: ITEM

--- Nil parent demo ---
Player (slot 1) parent index = 0
Player's parent is: NIL (no parent -- this is things[0], the nil sentinel)

--- Reallocation-safe demo ---
Copying things[] to a new location (simulating realloc)...
In the COPY: sword's parent index = 1
In the COPY: things_copy[1].kind = PLAYER
Indices survived the copy. Pointers would not.
```

Note: `sizeof(thing)` might differ due to alignment/padding. 20 is typical on most 64-bit platforms.

---

## Common mistakes

| Mistake | What happens | Fix |
|---------|-------------|-----|
| Store a pointer instead of an index | Dangling pointer after any reallocation or copy | Use `thing_idx` (int), store slot numbers |
| Use `-1` for "no parent" | Must special-case `-1` everywhere; doesn't match zero-init | Use `0` -- it's the nil sentinel index |
| Forget to update parent when moving entities | Orphaned references, wrong parent lookups | Always update both sides of any relationship |
| Access `things[idx]` without bounds check | Buffer overrun if idx is corrupted | Lesson 06 adds `pool_get()` with validation |

---

## Exercises

1. **Chain references.** Add a troll that carries an item: troll at slot 2, item at slot 5, `things[5].parent = 2`. Now follow the chain: item's parent is troll, troll's parent is 0 (nil). Print each step.

2. **Copy survival test.** Declare a second array `thing things_copy[MAX_THINGS]`. Use `memcpy` to copy the entire pool. Verify that all `parent` indices in the copy still resolve to the correct entities in the copy. Try the same with pointers and watch it break (or reason about why it would break).

3. **Reparenting.** Write a `reparent(thing_idx child, thing_idx new_parent)` function that changes a thing's parent. Move the sword from the player to the troll. Print the state before and after. Notice that reparenting is just `things[child].parent = new_parent` -- one integer assignment. With pointers, you'd need to update the pointer AND worry about the old parent's child list.

4. **Count children.** Write a `count_children(thing_idx parent_idx)` function that counts how many things have `parent == parent_idx`. This requires a linear scan (O(n) over all things). Think about why this is slow for large pools and what data structure would make it O(1). (Hint: Lesson 04.)

---

## Connection to your work

In React/Redux, you store entity relationships as IDs:

```ts
// Redux normalized state
const state = {
  entities: {
    1: { id: 1, kind: 'player', parentId: null },
    3: { id: 3, kind: 'item', parentId: 1 },    // sword belongs to player
    4: { id: 4, kind: 'item', parentId: 1 },    // potion belongs to player
  }
};

// Look up parent:
const sword = state.entities[3];
const parent = state.entities[sword.parentId];   // entities[1] = player
```

You never store a JavaScript object reference as a "parent" field in Redux. You store an ID. Then you look it up in the normalized store by that ID. The reason is the same: references (pointers) don't survive serialization, time-travel debugging, or state rehydration. IDs (indices) do.

What we just built in C is structurally identical to a normalized Redux store. The `things[]` array is the store. The `thing_idx parent` field is the `parentId`. The lookup `things[parent_idx]` is `state.entities[parentId]`. You already think in indices when you write frontend code. Now you're doing the same thing in C, but without the JSON overhead.

---

## Step 8 -- typedef vs struct wrapper: the type safety debate

We defined `thing_idx` as a typedef:

```c
typedef int thing_idx;
```

This gives us documentation: when you see `thing_idx parent`, you know it's an entity reference. But how much safety does it actually provide? Let's look at three variants.

### Variant A: Raw int -- no safety

```c
int parent;
int child_count;
int damage;
```

All three are `int`. Nothing stops you from writing `parent = damage`. The compiler won't blink. You'll spend an hour debugging why the player's parent is "7" and slot 7 is a random goblin.

### Variant B: typedef -- documentation only

```c
typedef int thing_idx;
thing_idx parent;
```

This is what we use in this course. It's better than raw `int` because a human reader instantly knows `thing_idx` means "entity reference." But the compiler still treats it as plain `int`. This compiles without warnings:

```c
thing_idx parent = 42;          /* fine -- 42 is an int, thing_idx is an int */
int count = 5;
thing_idx oops = count;         /* fine -- both are int */
thing_idx double_oops = parent + count;  /* fine -- int + int = int */
```

The typedef is a label, not a fence. It documents intent but doesn't enforce it.

### Variant C: struct wrapper -- real type safety

```c
typedef struct { int val; } thing_idx;
thing_idx parent;
```

Now `thing_idx` is a distinct type. The compiler will reject accidental mixing:

```c
thing_idx parent = {1};
int count = 5;
thing_idx oops = count;          /* ERROR: can't assign int to thing_idx */
thing_idx nope = parent + count; /* ERROR: can't add thing_idx + int */

/* You must be explicit: */
thing_idx ok = { count };                    /* explicit construction */
int slot = parent.val;                       /* explicit extraction */
thing *t = &things[parent.val];              /* access through .val */
```

Every accidental assignment becomes a compiler error. You can't mix a `thing_idx` with an `int` or with a different index type (say `tile_idx`) without explicitly going through `.val`.

> **Anton says:** "In the more production version we would use `struct thing_idx { int val; }`... because that will actually give you real type safety."

This course uses variant B (typedef) because it keeps the code simple -- you write `things[parent]` instead of `things[parent.val]` everywhere. But for production code where multiple index types coexist (`thing_idx`, `tile_idx`, `animation_idx`), variant C is worth the extra `.val` to prevent cross-type bugs.

> **Alternative approach:** In TypeScript, `type ThingIdx = number` gives you variant B. For variant C, you'd use a branded type: `type ThingIdx = number & { __brand: 'ThingIdx' }`. Same trade-off: branded types prevent `ThingIdx` and `TileIdx` from mixing at the cost of explicit casting at boundaries.

---

## Step 9 -- Serialization and hot reload: the hidden benefits

We covered the reallocation survival story in step 2. But indices unlock two more capabilities that pointers can never have: trivial serialization and hot-reload compatibility.

### Serialization: write the pool, read it back

Because every `thing_idx` is just an integer, and every other field in the thing struct is a float or int, the entire pool is plain-old-data. Saving the game is embarrassingly simple:

```c
/* Save */
FILE *f = fopen("save.dat", "wb");
fwrite(&pool, sizeof(pool), 1, f);
fclose(f);

/* Load */
FILE *f = fopen("save.dat", "rb");
fread(&pool, sizeof(pool), 1, f);
fclose(f);
```

That's it. The entire entity system -- every thing, every parent-child relationship, every sibling chain, every generation counter (lesson 09) -- written to disk in one call and read back in one call. All indices are still valid because they're relative to the pool, not to memory addresses.

Now imagine if `parent` was a pointer:

```c
/* Save -- BROKEN */
fwrite(&pool, sizeof(pool), 1, f);
/* File now contains raw memory addresses: 0x7ffe1234, 0x7ffe5678, ... */

/* Load on next run -- pool is at a different address */
fread(&pool, sizeof(pool), 1, f);
/* pool.things[3].parent = 0x7ffe1234 -- DANGLING. Pool is now at 0x5600abcd. */
/* Every single pointer in the loaded data is garbage. */
```

With pointers, you'd need a "fixup pass" after loading: walk every entity, find every pointer field, recompute the correct address from some saved offset. That's exactly the bookkeeping that indices eliminate.

> **Anton says:** "Pointers don't serialize. This is why this approach works -- indices are just numbers. Write them to disk, read them back, done."

### Hot reload: recompile without invalidating state

If you use Visual Studio's Edit and Continue, or any DLL hot-reload system, the game state stays in memory while the code gets replaced. The new code needs to access the same entity data. If entity references were pointers, recompiling could change struct layouts, move the pool in memory, or remap DLL addresses -- and every stored pointer would be wrong.

With indices, the new code just says `things[3]` and gets the right entity. The index 3 is meaningful as long as the array exists. It doesn't matter that the code was recompiled, that function addresses changed, or that the DLL was remapped. The data is pure integers and floats -- no code addresses, no vtable pointers, no pointer fixups.

This is one of the reasons Casey Muratori's Handmade Hero architecture works with hot-reload: the game DLL and the platform layer share entity state as flat arrays of plain data. Recompiling the game DLL and reloading it mid-frame Just Works because there are zero pointers in the entity data.

---

## What's next

Lesson 04: Right now, finding a parent's children requires scanning the entire array (O(n)). When the player has 3 items and the pool has 1024 slots, that's wasteful. We'll embed `first_child` and `next_sibling` directly in the thing struct -- an intrusive linked list that lets us walk from parent to child to sibling in O(1) per step.

---

## Complete file: lesson03.c

```c
/* lesson03.c — Indices, Not Pointers
 *
 * Builds on lesson02: adds thing_idx for inter-entity references.
 * Demonstrates why indices survive reallocation and pointers don't.
 *
 * Compile:
 *   gcc -Wall -Wextra -Werror -std=c11 -o lesson03 lesson03.c
 */

#include <stdio.h>
#include <string.h>  /* memcpy */

/* ══════════════════════════════════════════════════════ */
/*                    Constants                           */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 1024

/* ══════════════════════════════════════════════════════ */
/*                    Types                               */
/* ══════════════════════════════════════════════════════ */

/* JS: type Kind = 'player' | 'troll' | 'goblin' | 'item'
 * C:  Integer enum. NIL = 0 so zero-init produces nil. */
typedef enum thing_kind {
    THING_KIND_NIL = 0,
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

/* JS: type ThingId = number — just an array index.
 * C:  typedef'd int. Survives reallocation. Trivially serializable.
 *     Value 0 means "nil" (points to the nil sentinel at things[0]). */
typedef int thing_idx;

/* JS: { kind: 'player', x: 0, y: 0, health: 100, parentId: null }
 * C:  Fat struct — now with a parent reference (as an index, not pointer). */
typedef struct thing {
    thing_kind kind;
    float      x, y;
    float      health;
    thing_idx  parent;   /* index of parent thing (0 = no parent) */
} thing;

/* ══════════════════════════════════════════════════════ */
/*                    Pool Storage                        */
/* ══════════════════════════════════════════════════════ */

static thing things[MAX_THINGS];
static int   next_empty_slot = 1;  /* slot 0 reserved for nil sentinel */

/* ══════════════════════════════════════════════════════ */
/*                    Pool Operations                     */
/* ══════════════════════════════════════════════════════ */

static int pool_add(thing_kind kind)
{
    if (next_empty_slot >= MAX_THINGS) {
        return 0;  /* pool full */
    }

    int slot = next_empty_slot;
    next_empty_slot++;

    things[slot].kind   = kind;
    things[slot].x      = 0.0f;
    things[slot].y      = 0.0f;
    things[slot].health = 0.0f;
    things[slot].parent = 0;  /* no parent by default */

    return slot;
}

/* ══════════════════════════════════════════════════════ */
/*                    Debug Helpers                        */
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

static void print_all_things(void)
{
    printf("Pool contents (%d things):\n", next_empty_slot - 1);
    for (int i = 1; i < next_empty_slot; i++) {
        printf("  [%d] kind=%-7s  x=%.1f  y=%.1f  health=%.1f  parent=%d\n",
               i,
               thing_kind_name(things[i].kind),
               things[i].x,
               things[i].y,
               things[i].health,
               things[i].parent);
    }
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== Indices, Not Pointers ===\n");
    printf("sizeof(thing) = %zu bytes\n", sizeof(thing));
    printf("Pool memory: %zu bytes (%.1f KB)\n",
           sizeof(things), (double)sizeof(things) / 1024.0);

    /* ── Add entities ── */
    printf("\n");

    int player = pool_add(THING_KIND_PLAYER);
    things[player].x      = 5.0f;
    things[player].y      = 3.0f;
    things[player].health = 100.0f;
    printf("Added PLAYER at slot %d\n", player);

    int troll = pool_add(THING_KIND_TROLL);
    things[troll].x      = 10.0f;
    things[troll].y      = 7.0f;
    things[troll].health = 30.0f;
    printf("Added TROLL at slot %d\n", troll);

    int sword = pool_add(THING_KIND_ITEM);
    things[sword].x      = 5.0f;
    things[sword].y      = 3.0f;
    things[sword].parent = player;  /* sword belongs to player */
    printf("Added ITEM (sword) at slot %d, parent = %d\n", sword, player);

    int potion = pool_add(THING_KIND_ITEM);
    things[potion].x      = 5.0f;
    things[potion].y      = 3.0f;
    things[potion].parent = player;  /* potion belongs to player */
    printf("Added ITEM (potion) at slot %d, parent = %d\n", potion, player);

    /* ── Print pool state ── */
    printf("\n");
    print_all_things();

    /* ── Follow indices ── */
    printf("\n--- Following indices ---\n");

    thing_idx sword_parent_idx = things[sword].parent;
    printf("Sword (slot %d) parent index = %d\n", sword, sword_parent_idx);
    printf("Sword's parent is: %s at (%.1f, %.1f)\n",
           thing_kind_name(things[sword_parent_idx].kind),
           things[sword_parent_idx].x,
           things[sword_parent_idx].y);

    thing_idx potion_parent_idx = things[potion].parent;
    printf("Potion (slot %d) parent index = %d\n", potion, potion_parent_idx);
    printf("Potion's parent is: %s at (%.1f, %.1f)\n",
           thing_kind_name(things[potion_parent_idx].kind),
           things[potion_parent_idx].x,
           things[potion_parent_idx].y);

    /* ── Find all children of player (linear scan) ── */
    printf("\n--- Player's children (linear scan) ---\n");
    for (int i = 1; i < next_empty_slot; i++) {
        if (things[i].parent == player) {
            printf("  slot %d: %s\n", i, thing_kind_name(things[i].kind));
        }
    }

    /* ── Nil parent demo ── */
    printf("\n--- Nil parent demo ---\n");
    thing_idx player_parent_idx = things[player].parent;
    printf("Player (slot %d) parent index = %d\n", player, player_parent_idx);
    printf("Player's parent is: %s (no parent -- this is things[0], the nil sentinel)\n",
           thing_kind_name(things[player_parent_idx].kind));

    /* ── Reallocation-safe demo ── */
    printf("\n--- Reallocation-safe demo ---\n");
    printf("Copying things[] to a new location (simulating realloc)...\n");

    /* Simulate reallocation by copying to a new array.
     * memcpy(dest, src, bytes) copies raw bytes from src to dest.
     * JS equivalent: structuredClone(things) -- but byte-level, not object-level. */
    thing things_copy[MAX_THINGS];
    memcpy(things_copy, things, sizeof(things));

    /* In the copy, all indices still work because they're just ints. */
    thing_idx copy_sword_parent = things_copy[sword].parent;
    printf("In the COPY: sword's parent index = %d\n", copy_sword_parent);
    printf("In the COPY: things_copy[%d].kind = %s\n",
           copy_sword_parent,
           thing_kind_name(things_copy[copy_sword_parent].kind));
    printf("Indices survived the copy. Pointers would not.\n");

    return 0;
}
```
