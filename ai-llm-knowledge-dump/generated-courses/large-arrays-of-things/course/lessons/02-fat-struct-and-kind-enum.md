# Lesson 02: Fat Struct and Kind Enum

## What we're building

The first real C code of the course: a `thing` struct, a `thing_kind` enum, a static array of 1024 things, and a function to add things to the array. We'll add a few entities, loop through them, and print what's there. Standalone `main()` -- no platform layer, no window. Just a terminal program that proves the data structure works.

## What you'll learn

- How to define a fat struct in C (the `thing` struct)
- How to define and use an enum as a type discriminator (`thing_kind`)
- Static array allocation -- 1024 entities with zero `malloc`
- A simple "bump the slot counter" add function
- Iterating over the array with a for loop
- Back-of-napkin memory math for why this approach scales

## Prerequisites

- Lesson 01 (conceptual understanding of why fat structs beat hierarchies)
- Comfortable with C structs, enums, `printf`, and compilation
- Completed the Asteroids course (you've seen `SpaceObject` in a flat array)

---

## Step 1 -- The Kind Enum

Before we build the struct, we need the type tag. In TypeScript you'd write `type Kind = 'player' | 'troll' | 'item'`. In C, that's an enum.

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

> **JS bridge:** In TypeScript you write `type Kind = 'player' | 'troll' | 'item'` -- a union of string literal types. In C, there are no string types for this. Instead you use an `enum`: a list of named integer constants. The compiler assigns each name a number (0, 1, 2...) and you compare those numbers. Think of it as: your TS string union, but each string is secretly an integer under the hood. The `typedef enum thing_kind { ... } thing_kind;` part gives the enum both a "tag name" (`enum thing_kind`) and a shorter alias (`thing_kind`) so you can write `thing_kind k` instead of `enum thing_kind k` everywhere.

Five things to notice:

**NIL is explicitly zero.** This is not an accident. When C zeros out memory (which we'll do a lot), every `thing_kind` field becomes `THING_KIND_NIL`. That means "this slot is empty" -- no special initialization needed. An empty slot announces itself as empty just by being zeroed.

**Values auto-increment.** PLAYER is 1, TROLL is 2, GOBLIN is 3, ITEM is 4. C enums auto-increment from the last explicitly assigned value. We only set NIL to 0; the rest follow.

**COUNT is a sentinel.** `THING_KIND_COUNT` is always the last entry. Its numeric value equals the number of "real" kinds (5 in this case: NIL through ITEM). Useful for bounds checks and lookup tables later.

**Prefix convention.** Every value starts with `THING_KIND_` because C enums are global -- they're not scoped to the enum type. Without the prefix, `PLAYER` would collide with any other `PLAYER` in your codebase.

> **Why?** In TypeScript, enum members are scoped: `Kind.Player` can't collide with anything. C has no scoping mechanism for enums, so we use naming conventions instead. This is the standard approach in C codebases -- Linux kernel, game engines, everywhere.

**The `typedef`.** We write `typedef enum thing_kind { ... } thing_kind;` so we can use `thing_kind` as a type name instead of writing `enum thing_kind` everywhere. Same as Asteroids `SpaceObject`.

> **Why `typedef`?** C has a quirk: when you define `struct thing { ... };` or `enum thing_kind { ... };`, you have to write the `struct` or `enum` keyword every time you use it: `struct thing t;`. The `typedef` creates a shortcut so you can write just `thing t;`. Think of it as `type thing = struct { ... }` in TypeScript -- you're giving the type a simpler name. Without `typedef`, every variable declaration, function parameter, and return type would need the `struct`/`enum` prefix. That's why you see `typedef struct thing { ... } thing;` everywhere in C game code.

> **Common mistake:** Forgetting the `= 0` on NIL. It would still be 0 (first enum value defaults to 0 in C), but making it explicit is documentation: "I CHOSE zero, it wasn't an accident, and the zero-init architecture depends on it."

### Why NIL = 0 is a design constraint, not a convenience

This is worth hammering home because the entire course depends on it. In C, global and static variables are initialized to zero by default. When you declare `static thing things[1024]`, the OS gives you a block of memory that's all zeros. If NIL = 0, then every slot in that array starts as `THING_KIND_NIL` -- "empty" -- for free. No initialization loop needed. No constructor. Just zeros.

Later, when we remove an entity from the pool, we'll `memset` its slot to zero. That automatically resets its kind to NIL. The slot announces itself as empty.

> **Why `memset`?** `memset` is a C standard library function that fills a block of memory with a byte value. `memset(&thing, 0, sizeof(thing))` means "starting at the address of `thing`, write zero into every byte, for `sizeof(thing)` bytes." It is the C equivalent of resetting every field to its default. In JS, you might write `Object.assign(entity, { kind: 'nil', x: 0, y: 0, health: 0 })` -- but you have to list every field. `memset` zeros ALL bytes mechanically, no matter how many fields the struct has. Add a new field tomorrow and memset already handles it.

This pattern -- "all-zeros is a valid initial state" -- is the backbone of the entire pool architecture. Every field in the thing struct is designed so that zero is a safe, meaningful default. Position (0, 0)? Fine. Health 0? Dead or not applicable. Parent index 0? Points to the nil sentinel. Kind NIL? Empty slot. The architecture is built on the guarantee that zeroed memory is valid memory.

> **Anton says:** "Design your structs so that memset to zero gives you a valid state. If you do this, you never need constructors, you never need initialization functions, and you can reset your entire game state in one line."

---

## Step 2 -- The Thing Struct

Now the fat struct itself. At this stage in the course, it's small -- just what we need:

```c
typedef struct thing {
    thing_kind kind;    /* type tag: PLAYER, TROLL, etc. */
    float      x, y;   /* position */
    float      health;  /* 0.0 if unused (items, triggers) */
} thing;
```

> **JS bridge: `typedef struct` from scratch.** If you have never seen C structs before, here is the translation. A C `struct` is like a TypeScript `interface` -- it defines a fixed set of named fields with explicit types. The difference: in TS, `{ kind: 'player', x: 5 }` is an object on the heap with dynamic shape. In C, `thing t;` allocates exactly 16 bytes on the stack (or in the array), with a fixed layout that never changes. Every `thing` is the same size. The `typedef struct thing { ... } thing;` pattern gives the struct two names: `struct thing` (the full C name) and `thing` (the shortcut). You will only ever use the shortcut.

That's it. 16 bytes. One enum (4 bytes on most platforms), three floats (4 bytes each).

Let's put this next to the TypeScript equivalent so the mapping is clear:

```
    C struct                          TypeScript interface
    ──────────────────────────────    ──────────────────────────────
    typedef struct thing {            interface Thing {
        thing_kind kind;                kind: ThingKind;
        float      x, y;               x: number;  y: number;
        float      health;              health?: number;
    } thing;                          }
```

The key difference: in TypeScript, `health?: number` means the field might not exist on the object at all (it's `undefined`). In C, `float health` is ALWAYS present in the struct -- it just holds `0.0` when unused. Every `thing` is exactly 16 bytes, whether it's a player with 100 health or an item with 0 health.

> **Why?** Fixed-size structs mean the array is perfectly uniform. `things[i]` is always at byte offset `i * sizeof(thing)`. No variable-length records, no indirection, no surprises. The CPU prefetcher can predict memory access patterns perfectly.

### Memory layout

Here's what one `thing` looks like in memory (assuming typical alignment):

```
    Byte offset:  0       4       8       12      16
                  ┌───────┬───────┬───────┬───────┐
    thing =       │ kind  │   x   │   y   │health │
                  │ (4B)  │ (4B)  │ (4B)  │ (4B)  │
                  └───────┴───────┴───────┴───────┘
                  ◄──────── 16 bytes total ────────►
```

No padding, no hidden fields, no vtable pointer. Just data. You could write this to a file and read it back on the same machine.

---

## Step 3 -- The Array and the Add Function

Now we need somewhere to put our things. In Asteroids, you had `SpaceObject asteroids[MAX_ASTEROIDS]` with an `asteroid_count`. Same pattern here, but for ALL entity types in one pool.

```c
#define MAX_THINGS 1024

static thing things[MAX_THINGS];
static int   next_empty_slot = 1;  /* slot 0 is reserved -- explained below */
```

Two important decisions:

**Static array, not heap-allocated.** `things` is a global static array. It lives in the BSS segment (zero-initialized by the OS at program start). No `malloc` call. No `free` call. It exists for the entire lifetime of the program.

> **Why `static`?** The `static` keyword on a global variable in C means "this variable is private to this file -- other `.c` files cannot see it." It is NOT the same as `static` in TypeScript/Java classes. Think of it as a file-scoped `const` -- except it is mutable. We use `static` here because `things` and `next_empty_slot` are implementation details of this file, not part of a public API. The array itself is a fixed-size block of memory: `thing things[1024]` means "reserve space for exactly 1024 `thing` structs, contiguously, right here." Unlike JS `new Array(1024)`, this does not allocate on the heap -- the OS reserves this memory when the program loads.

> **Why?** In JavaScript, `const entities = new Array(1024)` allocates on the heap. In C game engines, we avoid heap allocation for the main entity storage because: (1) the size is known at compile time, (2) we want zero-initialization for free, and (3) we never need to resize -- MAX_THINGS is a hard cap.

**Slot 0 is reserved.** `next_empty_slot` starts at 1, not 0. Slot 0 will become the "nil sentinel" in Lesson 06 -- a safe default that means "nothing." For now, just know that valid entities start at index 1. This is analogous to how database auto-increment IDs start at 1, not 0.

Now the add function:

```c
static int pool_add(thing_kind kind)
{
    if (next_empty_slot >= MAX_THINGS) {
        return 0;  /* pool full -- return nil index */
    }

    int slot = next_empty_slot;
    next_empty_slot++;

    things[slot].kind   = kind;
    things[slot].x      = 0.0f;
    things[slot].y      = 0.0f;
    things[slot].health = 0.0f;

    return slot;
}
```

This is the simplest possible allocation strategy: bump the counter. Each call claims the next slot and advances `next_empty_slot`. If we run out, return 0 (the nil index).

```
    Before pool_add(THING_KIND_PLAYER):
    ┌───────┬───────┬───────┬───────┬───────┐
    │ [0]   │ [1]   │ [2]   │ [3]   │ ...   │
    │ NIL   │ empty │ empty │ empty │       │
    └───────┴───────┴───────┴───────┴───────┘
              ▲
              next_empty_slot = 1

    After pool_add(THING_KIND_PLAYER):
    ┌───────┬────────┬───────┬───────┬───────┐
    │ [0]   │ [1]    │ [2]   │ [3]   │ ...   │
    │ NIL   │ PLAYER │ empty │ empty │       │
    └───────┴────────┴───────┴───────┴───────┘
                       ▲
                       next_empty_slot = 2

    After pool_add(THING_KIND_TROLL):
    ┌───────┬────────┬───────┬───────┬───────┐
    │ [0]   │ [1]    │ [2]   │ [3]   │ ...   │
    │ NIL   │ PLAYER │ TROLL │ empty │       │
    └───────┴────────┴───────┴───────┴───────┘
                                ▲
                                next_empty_slot = 3
```

> **Common mistake:** Initializing `next_empty_slot` to 0. Then your first entity overwrites the nil sentinel at slot 0. This is a silent bug -- everything seems to work until Lesson 06 when the nil sentinel is supposed to be all-zeros and isn't. Start at 1.

### Why no `malloc`? A comparison

In JavaScript, arrays grow dynamically. `Array.push()` just works -- V8 handles the memory behind the scenes, allocating and reallocating as needed.

In C, you have two choices:

1. **Static array** (what we do): `thing things[MAX_THINGS]` -- size fixed at compile time, lives in BSS (global) or stack (local), zero-initialized by the OS.
2. **Dynamic array** (what we avoid): `thing *things = malloc(count * sizeof(thing))` -- size chosen at runtime, lives on the heap, must be `free`'d, can be `realloc`'d to grow.

For a game entity pool, the static array wins on every metric:

| | Static array | `malloc`'d array |
|---|---|---|
| Initialization | Zero (OS does it) | Must `memset` after `malloc` |
| Allocation cost | None (exists at program start) | Heap walk, fragmentation |
| Deallocation cost | None (exists until program end) | Must remember to `free` |
| Can leak? | No | Yes |
| Can fragment heap? | No | Yes |
| Size known at compile time? | Yes | No |
| Can grow? | No (but MAX_THINGS is generous) | Yes (`realloc`) |

The only disadvantage is the fixed size. But that's actually a feature: it forces you to think about your budget. How many entities does your game need? 1024? 4096? Pick a number, `#define` it, and never think about allocation again.

> **Why?** In web development, memory management is invisible -- the garbage collector handles it. In game engines, `malloc` inside the game loop is a bug. It fragments memory, it can stall (the heap allocator might need to search for a free block), and it makes performance unpredictable. Static arrays give you predictable, zero-cost memory that's always where you expect it.

---

## Step 4 -- A Helper to Print Kind Names

For our test output, we need a way to print `"PLAYER"` instead of `1`. A simple function that switches on the kind:

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

> **Why?** This is the C equivalent of a TypeScript enum-to-string lookup. In TS you'd write `const kindNames: Record<Kind, string> = { ... }`. In C, a switch is the standard pattern. The `default` case catches any enum value we forgot to add (the compiler warning `-Wswitch` also helps).

---

## Step 5 -- The Print Loop

Now let's iterate over all occupied slots and print them. Since we know `next_empty_slot` is the first unused slot, everything from index 1 to `next_empty_slot - 1` is a valid entity:

```c
static void print_all_things(void)
{
    printf("Pool contents (%d things):\n", next_empty_slot - 1);
    for (int i = 1; i < next_empty_slot; i++) {
        printf("  [%d] kind=%-7s  x=%.1f  y=%.1f  health=%.1f\n",
               i,
               thing_kind_name(things[i].kind),
               things[i].x,
               things[i].y,
               things[i].health);
    }
}
```

Starting at 1 (skip the nil sentinel at 0). Ending before `next_empty_slot` (everything at and beyond that is unoccupied).

> **Why `printf` format strings?** If you have never used `printf` before: it is C's equivalent of JavaScript template literals. Where JS writes `` `x=${value}` ``, C writes `printf("x=%d", value)`. The `%` codes are placeholders: `%d` = integer, `%f` = float, `%.1f` = float with 1 decimal place, `%s` = string, `%zu` = size_t. The `%-7s` format left-pads the kind name to 7 characters so columns line up -- `%-7s` prints it left-justified in a 7-character field. There is no automatic type inference: you MUST match the `%` code to the argument type, or you get garbage output (the compiler flag `-Wall` helps catch mismatches).

---

## Step 6 -- Back-of-Napkin Memory Math

Before we write `main()`, let's do the math Anton walks through in the transcript. This is important because the first question anyone asks about fat structs is "doesn't that waste memory?"

```
    Our thing struct:       16 bytes
    MAX_THINGS:             1024

    Total memory for pool:  1024 * 16 = 16,384 bytes = 16 KB

    Your L1 cache:          32 KB (typical Intel/AMD)
    Your L2 cache:          256 KB - 1 MB
    Your RAM:               8-64 GB

    Conclusion: THE ENTIRE POOL FITS IN L1 CACHE.
```

Even if we made the struct much bigger -- say 100 properties at 4 bytes each = 400 bytes per thing:

```
    400 bytes * 1024 things = 409,600 bytes = 400 KB
    400 bytes * 10,000 things = 4,000,000 bytes = ~4 MB
```

4 MB for ten thousand rich entities. A single 1080p texture is 8 MB. A single MP3 is 3-5 MB. The entity pool is not your memory bottleneck.

> **Anton says:** "I'll say it loud and clear: 10,000 things times 100 properties times 16 bytes -- that's 15 megabytes. That's NOTHING. Don't over-engineer. Just put everything in the struct."

This math is liberating. It means you don't need to be clever about which fields go where. Player doesn't need AI? Leave the AI field at zero. Item doesn't need health? Zero. The "wasted" bytes per entity are a fraction of a fraction of your memory budget.

---

## Step 7 -- Putting It All Together: main()

> **Why `sizeof`?** `sizeof(thing)` is a compile-time operator that tells you how many bytes one `thing` struct occupies in memory. It is not a function call -- the compiler replaces it with a constant (like `16`) at compile time. There is no JS equivalent because JS objects do not have a fixed byte size. In C, every struct has a deterministic size known at compile time, and `sizeof` lets you query it. You will see `sizeof` used constantly in C: in `memset` (to know how many bytes to zero), in `memcpy` (to know how many bytes to copy), in `malloc` (to know how many bytes to allocate), and in `printf` (to print the size for debugging). The format specifier `%zu` is specifically for printing `sizeof` results (which have type `size_t`, an unsigned integer type).

Here's the `main()` function that exercises everything:

```c
int main(void)
{
    printf("=== Fat Struct and Kind Enum ===\n");
    printf("sizeof(thing) = %zu bytes\n", sizeof(thing));
    printf("Pool memory: %zu bytes (%.1f KB)\n",
           sizeof(things), (double)sizeof(things) / 1024.0);

    /* Add some things */
    int player = pool_add(THING_KIND_PLAYER);
    things[player].x      = 5.0f;
    things[player].y      = 3.0f;
    things[player].health = 100.0f;
    printf("Added PLAYER at slot %d\n", player);

    int troll1 = pool_add(THING_KIND_TROLL);
    things[troll1].x      = 10.0f;
    things[troll1].y      = 7.0f;
    things[troll1].health = 30.0f;
    printf("Added TROLL at slot %d\n", troll1);

    int troll2 = pool_add(THING_KIND_TROLL);
    things[troll2].x      = 2.0f;
    things[troll2].y      = 9.0f;
    things[troll2].health = 30.0f;
    printf("Added TROLL at slot %d\n", troll2);

    int sword = pool_add(THING_KIND_ITEM);
    things[sword].x = 5.0f;
    things[sword].y = 3.0f;
    /* health stays 0.0 -- items don't have health */
    printf("Added ITEM at slot %d\n", sword);

    printf("\n");
    print_all_things();

    /* Simple update loop: only trolls take damage */
    printf("\n--- Trolls take 5 damage ---\n");
    for (int i = 1; i < next_empty_slot; i++) {
        if (things[i].kind == THING_KIND_TROLL) {
            things[i].health -= 5.0f;
        }
    }
    print_all_things();

    return 0;
}
```

The pattern is: `pool_add()` returns a slot index, then you write fields into `things[slot]`. The loop iterates all occupied slots, switches on `kind`, and processes accordingly. This is the core game loop pattern for entity systems.

> **Handmade Hero connection:** Casey's entity system in HH follows this exact pattern -- add to a flat array, iterate by type, process. His `sim_entity` struct is the fat struct. His entity types are flags and enums on the struct. No class hierarchy.

---

## Build and run

Save the complete file below as `lesson02.c`, then compile and run:

```bash
gcc -Wall -Wextra -Werror -std=c11 -o lesson02 lesson02.c
./lesson02
```

---

## Expected result

```
=== Fat Struct and Kind Enum ===
sizeof(thing) = 16 bytes
Pool memory: 16384 bytes (16.0 KB)
Added PLAYER at slot 1
Added TROLL at slot 2
Added TROLL at slot 3
Added ITEM at slot 4

Pool contents (4 things):
  [1] kind=PLAYER   x=5.0  y=3.0  health=100.0
  [2] kind=TROLL    x=10.0  y=7.0  health=30.0
  [3] kind=TROLL    x=2.0  y=9.0  health=30.0
  [4] kind=ITEM     x=5.0  y=3.0  health=0.0

--- Trolls take 5 damage ---
Pool contents (4 things):
  [1] kind=PLAYER   x=5.0  y=3.0  health=100.0
  [2] kind=TROLL    x=10.0  y=7.0  health=25.0
  [3] kind=TROLL    x=2.0  y=9.0  health=25.0
  [4] kind=ITEM     x=5.0  y=3.0  health=0.0
```

Note: `sizeof(thing)` might be 16 on most platforms but could vary with alignment. The exact number doesn't matter for this lesson.

---

## Common mistakes

| Mistake | What happens | Fix |
|---------|-------------|-----|
| Start `next_empty_slot` at 0 | First entity overwrites the reserved nil slot | Initialize to 1 |
| Forget `typedef` on struct | Have to write `struct thing` everywhere | Use `typedef struct thing { ... } thing;` |
| Use `int` for kind instead of enum | Lose compiler warnings for missing switch cases | Use the `thing_kind` typedef |
| Don't check `MAX_THINGS` in `pool_add` | Array overflow, memory corruption, crash | Bounds check and return 0 |
| Iterate from 0 instead of 1 | Process the nil sentinel as if it were a real entity | `for (int i = 1; ...)` |

---

## Exercises

1. **Add more kinds.** Add `THING_KIND_ARROW` and `THING_KIND_TRAP` to the enum. Add a few of each to `main()`. Verify they print correctly. Notice that you don't change the struct at all -- just the enum and the `thing_kind_name` switch.

2. **Stress test.** Add 1023 things in a loop (filling the pool). Try adding one more. Verify that `pool_add` returns 0 (pool full). Print `next_empty_slot` to confirm it capped at `MAX_THINGS`.

3. **Size experiment.** Add more fields to the thing struct: `float dx, dy;` (velocity), `float size;` (collision radius), `int team;` (faction). Recompile and check `sizeof(thing)`. Redo the memory math: how much does the pool use now? Is it still under 64 KB?

4. **Kind-specific update.** Write an update loop where: players heal 1 HP per tick, trolls move right by 0.5 per tick, items don't update. Use a switch on `things[i].kind`. This is the game loop dispatch pattern you'll use forever.

---

## Connection to your work

The `pool_add` function is your `Array.push()` -- but with a fixed-size backing store and an integer slot ID instead of an object reference. The print loop is your `array.forEach()`. The kind-switch update is your Redux reducer's `switch (action.type)`.

In a React app, you'd store entities in a normalized Redux store: `{ byId: { 1: { kind: 'player', ... }, 2: { kind: 'troll', ... } }, allIds: [1, 2] }`. That's literally what we just built. `things[]` is `byId`. `next_empty_slot` tracks the high-water mark of `allIds`. The `kind` field is the discriminator.

The difference is: in Redux, each entity is a separate JS object on the heap, linked by reference. In C, all entities are contiguous in a static array, accessed by index. Same logical structure, radically different memory layout. The C version is what you want when you're processing 10,000 entities 60 times per second.

---

## When the fat struct gets too fat — a preview

Right now our thing struct is 16 bytes. That's tiny. But what happens when you have 50 properties and only players use 30 of them while monsters use a different 30?

> **Alternative approach:** Anton shows three strategies for when the struct grows:
>
> **Union:** Put type-specific data in a `union { player_data; monster_data; }` inside the struct. You pay `max(player, monster)` not the sum. We'll implement this in Lesson 13.
>
> **Char buffer:** `char custom_data[104]` — raw bytes, cast to whatever you need. Maximum flexibility. This is what Dreams used for truly dynamic properties.
>
> **Separate pools:** `thing_pool players; thing_pool monsters;` — each pool has its own specialized struct. Zero waste per pool.
>
> For now, the fat struct with all fields is correct. Do the napkin math — it's almost always fine. We'll revisit these alternatives in Lesson 13.

> **Anton says:** "Try the fat struct first and just see how bad is it if I just have everything. Often times it's not so bad. We shipped Dreams effectively almost on that mentality."

---

## What's next

Lesson 03: Right now we add entities and loop them, but entities can't *reference* each other. If the player picks up the sword, how does the sword know its parent is the player? We'll introduce `thing_idx` -- an integer index type that replaces pointers and survives array reallocation.

---

## Complete file: lesson02.c

```c
/* lesson02.c — Fat Struct and Kind Enum
 *
 * The first compilable code in the Large Arrays of Things course.
 * Defines a thing struct, kind enum, static array pool, and basic
 * add/iterate operations.
 *
 * Compile:
 *   gcc -Wall -Wextra -Werror -std=c11 -o lesson02 lesson02.c
 */

#include <stdio.h>

/* ══════════════════════════════════════════════════════ */
/*                    Constants                           */
/* ══════════════════════════════════════════════════════ */

#define MAX_THINGS 1024

/* ══════════════════════════════════════════════════════ */
/*                    Kind Enum                           */
/* ══════════════════════════════════════════════════════ */

/* JS: type Kind = 'player' | 'troll' | 'goblin' | 'item'
 * C:  Integer enum. NIL = 0 so zero-init produces nil. */
typedef enum thing_kind {
    THING_KIND_NIL = 0,   /* zero-init produces this */
    THING_KIND_PLAYER,
    THING_KIND_TROLL,
    THING_KIND_GOBLIN,
    THING_KIND_ITEM,
    THING_KIND_COUNT
} thing_kind;

/* ══════════════════════════════════════════════════════ */
/*                    Thing Struct                         */
/* ══════════════════════════════════════════════════════ */

/* JS: { kind: 'player', x: 0, y: 0, health: 100 }
 * C:  Fat struct — one struct for ALL entity types.
 *     Unused fields stay zero. */
typedef struct thing {
    thing_kind kind;
    float      x, y;
    float      health;
} thing;

/* ══════════════════════════════════════════════════════ */
/*                    Pool Storage                        */
/* ══════════════════════════════════════════════════════ */

/* JS: const entities = new Array(1024);
 * C:  Static array, zero-initialized by the OS.
 *     Slot 0 is reserved (will become nil sentinel in Lesson 06). */
static thing things[MAX_THINGS];
static int   next_empty_slot = 1;

/* ══════════════════════════════════════════════════════ */
/*                    Pool Operations                     */
/* ══════════════════════════════════════════════════════ */

/* JS: entities.push({ kind, x: 0, y: 0, health: 0 })
 * C:  Bump the slot counter and initialize the thing.
 *     Returns the slot index (0 = pool full). */
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
        printf("  [%d] kind=%-7s  x=%.1f  y=%.1f  health=%.1f\n",
               i,
               thing_kind_name(things[i].kind),
               things[i].x,
               things[i].y,
               things[i].health);
    }
}

/* ══════════════════════════════════════════════════════ */
/*                       Main                             */
/* ══════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== Fat Struct and Kind Enum ===\n");
    printf("sizeof(thing) = %zu bytes\n", sizeof(thing));
    printf("Pool memory: %zu bytes (%.1f KB)\n",
           sizeof(things), (double)sizeof(things) / 1024.0);

    /* Add some things */
    int player = pool_add(THING_KIND_PLAYER);
    things[player].x      = 5.0f;
    things[player].y      = 3.0f;
    things[player].health = 100.0f;
    printf("Added PLAYER at slot %d\n", player);

    int troll1 = pool_add(THING_KIND_TROLL);
    things[troll1].x      = 10.0f;
    things[troll1].y      = 7.0f;
    things[troll1].health = 30.0f;
    printf("Added TROLL at slot %d\n", troll1);

    int troll2 = pool_add(THING_KIND_TROLL);
    things[troll2].x      = 2.0f;
    things[troll2].y      = 9.0f;
    things[troll2].health = 30.0f;
    printf("Added TROLL at slot %d\n", troll2);

    int sword = pool_add(THING_KIND_ITEM);
    things[sword].x = 5.0f;
    things[sword].y = 3.0f;
    /* health stays 0.0 -- items don't have health */
    printf("Added ITEM at slot %d\n", sword);

    printf("\n");
    print_all_things();

    /* Simple update loop: only trolls take damage */
    printf("\n--- Trolls take 5 damage ---\n");
    for (int i = 1; i < next_empty_slot; i++) {
        if (things[i].kind == THING_KIND_TROLL) {
            things[i].health -= 5.0f;
        }
    }
    print_all_things();

    return 0;
}
```
