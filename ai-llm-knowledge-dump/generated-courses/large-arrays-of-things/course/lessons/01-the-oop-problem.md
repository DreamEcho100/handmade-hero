# Lesson 01: The OOP Problem

## What we're building

Nothing compilable. This lesson is a whiteboard session. We're going to stare at inheritance hierarchies, watch them collapse under their own weight, and arrive at the thing that actually works: a single fat struct with a kind enum. By the end you'll understand *why* professional game engines don't use OOP class trees for entities, and what they do instead.

## What you'll learn

- Why the classic Entity/Player/Monster/Weapon class hierarchy breaks in practice
- The diamond problem and the "empty base class" problem
- Why composition beats inheritance for game entities
- The fat struct with kind enum alternative
- Back-of-napkin memory math that kills the "but it wastes memory!" objection

## Prerequisites

- Completed the Asteroids course (you've seen `SpaceObject` -- a single struct for ships, bullets, and asteroids)
- Experience with TypeScript or any class-based language
- No C experience needed -- this lesson is conceptual. We discuss C code but don't write any.

---

## Step 1 -- The Dream: a Clean Inheritance Tree

Every programmer who learned OOP in school draws the same diagram the first time they sit down to build a game engine. It looks beautiful:

```
              ┌──────────┐
              │  Entity   │
              │───────────│
              │ x, y      │
              │ update()   │
              │ render()   │
              └─────┬─────┘
                    │
       ┌────────────┼────────────┐
       │            │            │
  ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
  │ Player  │ │ Monster │ │  Item   │
  │─────────│ │─────────│ │─────────│
  │ health  │ │ health  │ │ name    │
  │ mana    │ │ ai_type │ │ weight  │
  │ input() │ │ think() │ │ use()   │
  └─────────┘ └─────────┘ └─────────┘
```

Player extends Entity. Monster extends Entity. Item extends Entity. Each subclass adds the fields and methods it needs. Clean. Organized. Textbook.

You've done this in TypeScript:

```ts
class Entity {
  x: number = 0;
  y: number = 0;
  update(dt: number): void {}
  render(ctx: CanvasRenderingContext2D): void {}
}

class Player extends Entity {
  health: number = 100;
  mana: number = 50;
}

class Monster extends Entity {
  health: number = 30;
  aiType: 'patrol' | 'chase' = 'patrol';
}

class Item extends Entity {
  name: string = 'potion';
  weight: number = 1.0;
}
```

This works for your first week. Then the game design gets real.

---

## Step 2 -- The Cracks: Where Hierarchies Break

### Crack #1: Shared behavior that doesn't follow the tree

Your designer says: "Weapons should have a position in the world (they can be on the ground) and they also deal damage." OK, so a Weapon has `x, y` from Entity and some `damage` field. Easy. But then: "Weapons also have health -- they can break."

Now Weapon needs health. Where does health live? It was on Player and Monster, not on Entity. Do you push health up to Entity? Then *every* entity has health, including items that shouldn't. Or do you make a `HasHealth` mixin? Now you're fighting the type system.

```
              ┌──────────┐
              │  Entity   │
              │───────────│
              │ x, y      │
              │ health ← ??? pushed up, but Items don't have health
              └─────┬─────┘
                    │
       ┌────────────┼────────────┐
       │            │            │
  ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
  │ Player  │ │ Monster │ │  Item   │
  │─────────│ │─────────│ │─────────│
  │ mana    │ │ ai_type │ │ name    │
  │         │ │         │ │ weight  │
  │         │ │         │ │ health?!│  ← "potion has health" makes no sense
  └─────────┘ └─────────┘ └─────────┘
```

In TypeScript you might reach for interfaces or mixins. In C++ you might try multiple inheritance. Both are duct tape over a bad foundation.

### Crack #2: The diamond problem

Your designer says: "The boss is a monster that carries weapons." So the Boss is a Monster but also needs Weapon behavior. In a hierarchy, that means multiple inheritance -- Boss extends Monster AND Weapon:

```
              ┌──────────┐
              │  Entity   │
              └──┬────┬──┘
                 │    │
           ┌─────┘    └─────┐
           │                │
      ┌────▼────┐     ┌────▼────┐
      │ Monster │     │ Weapon  │
      └────┬────┘     └────┬────┘
           │                │
           └──────┬─────────┘
                  │
             ┌────▼────┐
             │  Boss   │  ← two copies of Entity? one? virtual?
             └─────────┘
```

Boss has TWO copies of `x, y` -- one from the Monster path and one from the Weapon path. Which one is the "real" position? C++ solves this with virtual inheritance, which adds a vtable indirection and a runtime cost that game engines don't want. TypeScript doesn't even let you do this -- you can only extend one class.

> **Anton says:** "The moment you add a second inheritance path, you're fighting the language instead of building the game. And that fight never ends."

### Crack #3: The empty base class problem

Six months into development, your hierarchy looks like this:

```
              ┌──────────────┐
              │    Entity     │
              │───────────────│
              │ x, y          │   ← OK, everyone has position
              │ health        │   ← pushed up because weapons need it
              │ mana          │   ← pushed up because boss-mage needs it
              │ ai_type       │   ← pushed up because NPCs need it
              │ weight        │   ← pushed up because physics needs it
              │ name[32]      │   ← pushed up because UI needs it
              │ damage        │   ← pushed up because...
              └───────┬───────┘
                      │
         ┌────────────┼────────────┐
         │            │            │
    ┌────▼────┐  ┌────▼────┐  ┌────▼────┐
    │ Player  │  │ Monster │  │  Item   │
    │─────────│  │─────────│  │─────────│
    │         │  │         │  │         │
    │ (empty) │  │ (empty) │  │ (empty) │  ← everything got pushed up
    └─────────┘  └─────────┘  └─────────┘
```

Every field migrated to the base class. The subclasses are empty shells. Your "clean hierarchy" is actually one giant god class with empty derived classes providing nothing except a type tag you check at runtime anyway:

```cpp
// C++ code that exists in real codebases
if (dynamic_cast<Player*>(entity)) {
    // player-specific logic
} else if (dynamic_cast<Monster*>(entity)) {
    // monster-specific logic
}
```

You're paying for virtual dispatch, vtable pointers, and dynamic_cast -- all to check a type tag you could have stored as an integer.

> **Common mistake:** "But I'll keep the hierarchy clean this time!" Every game team says this. Every game team ends up with a god class. It's not a discipline problem -- it's a *modeling* problem. Real game entities don't form trees.

---

## Step 3 -- Why Real Game Entities Don't Form Trees

Here's the fundamental insight that school doesn't teach you: game entities share behavior in *arbitrary combinations*, not in tree-shaped hierarchies.

Think about what different entities actually need:

```
                    position  health  ai   physics  inventory  damage  render
                    ─────────────────────────────────────────────────────────
    Player          yes       yes     no   yes      yes        no      yes
    Troll           yes       yes     yes  yes      no         yes     yes
    Dropped Sword   yes       no      no   yes      no         yes     yes
    Treasure Chest  yes       yes     no   no       yes        no      yes
    Arrow (flying)  yes       no      no   yes      no         yes     yes
    Campfire        yes       no      no   no       no         yes     yes
    Trigger Zone    yes       no      no   no       no         no      no
```

Look at the columns. There is no tree structure here. A Dropped Sword shares more columns with an Arrow than with the Treasure Chest it came out of. A Trigger Zone only has position. Any hierarchy you draw will force you to either: (a) push fields up where they don't belong, or (b) create parallel inheritance chains that don't compose.

This is what the Gang of Four meant by "favor composition over inheritance." But they said it in 1994 and the game industry kept using inheritance until sometime around 2005-2010, when engines started switching to data-oriented designs at scale.

### The runtime cost of OOP in C++

Even if you could somehow make the hierarchy work logically, there's a concrete performance cost. Each class with virtual methods gets a vtable pointer -- 8 bytes on a 64-bit system, hidden at the start of every object. That's 8 bytes per entity that carry zero game data. Worse, calling a virtual function means:

1. Load the vtable pointer from the object (cache miss #1)
2. Load the function pointer from the vtable (cache miss #2)
3. Call through the function pointer (branch predictor has no idea where this goes)

A switch statement on an enum? The compiler turns it into a jump table or a series of comparisons. The branch predictor learns the pattern quickly because the same switch runs every frame for every entity. The code is right there in the instruction cache. No indirection, no pointer chasing.

When you're processing 1,000 entities 60 times per second, that's 60,000 virtual calls per frame versus 60,000 switch cases per frame. The switch version is measurably faster because the CPU can predict and pipeline it.

> **Handmade Hero connection:** Casey Muratori spends the first 30+ episodes of Handmade Hero building entity types as flat structs with type flags, not class hierarchies. His entities are all the same struct with different fields populated. Same idea, same reasons. He's talked at length about why virtual dispatch is hostile to modern CPU architecture.

---

## Step 4 -- The Web Dev Bridge: Discriminated Unions

Here's the thing -- you already know the solution. You've been using it in TypeScript for years. You just didn't call it "fat struct with kind enum."

In TypeScript, when you have entities that share some fields but differ in others, you don't use class hierarchies. You use discriminated unions:

```ts
type EntityKind = 'player' | 'troll' | 'item' | 'trigger';

interface Entity {
  kind: EntityKind;
  x: number;
  y: number;
  // Optional fields -- not all entities use these
  health?: number;
  aiType?: 'patrol' | 'chase';
  damage?: number;
  inventory?: number[];  // array of entity IDs
}

// Create entities:
const player: Entity = { kind: 'player', x: 100, y: 200, health: 100 };
const troll: Entity  = { kind: 'troll', x: 300, y: 400, health: 30, aiType: 'patrol', damage: 5 };
const sword: Entity  = { kind: 'item', x: 300, y: 400, damage: 10 };
```

And then in your update loop:

```ts
switch (entity.kind) {
  case 'player':
    handlePlayerInput(entity);
    break;
  case 'troll':
    runAI(entity);
    break;
  case 'item':
    // items don't update
    break;
}
```

One interface. A `kind` field. A switch statement. No class hierarchy, no inheritance, no diamond problem.

That's *exactly* what we're going to build in C. The TypeScript optional fields (`health?: number`) become struct fields that are simply zero when unused. The `kind` string becomes an integer enum. The `switch` on `entity.kind` stays the same.

> **Why?** In TypeScript, unused optional fields are `undefined` and take no memory (they're not stored in the object). In C, unused struct fields are zero and DO take memory -- every thing struct is the same size regardless of kind. This sounds wasteful, but the memory math (Step 6) proves it doesn't matter.

---

## Step 5 -- The Fat Struct Solution

Here's where Anton lands, and where Casey lands, and where every experienced game engine programmer eventually lands:

```
   ┌─────────────────────────────────────────┐
   │              thing (one struct)          │
   │─────────────────────────────────────────│
   │ kind:     PLAYER | TROLL | ITEM | ...   │  ← type tag (was: class name)
   │ x, y:     float                         │  ← position (everyone has this)
   │ health:   float                         │  ← 0.0 if unused (items)
   │ damage:   float                         │  ← 0.0 if unused (player)
   │ ai_type:  int                           │  ← 0 if unused (non-AI)
   │ ...                                     │
   └─────────────────────────────────────────┘
```

One struct. All fields. A `kind` enum says what type of entity it is. Unused fields are zero.

No inheritance. No vtables. No dynamic_cast. No diamond problem. No empty base classes.

The "update" function is a switch on kind:

```
    for each thing in things:
        switch (thing.kind):
            case PLAYER: handle_input(thing); break;
            case TROLL:  run_ai(thing);       break;
            case ITEM:   /* nothing */        break;
```

And they all live in one big flat array:

```
    things[]:
    ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
    │  [0]    │  [1]    │  [2]    │  [3]    │  [4]    │   ...   │
    │ (nil)   │ PLAYER  │ TROLL   │ TROLL   │ ITEM    │         │
    │ x=0 y=0 │ x=5 y=3 │ x=9 y=1 │ x=2 y=7 │ x=4 y=4 │         │
    │ hp=0    │ hp=100  │ hp=30   │ hp=30   │ hp=0    │         │
    └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
```

One contiguous block of memory. No pointer chasing. No heap fragmentation. The CPU cache loves this.

> **Anton says:** "Don't make a class hierarchy. Just put everything in one struct. I know it offends your OOP sensibilities. It works. It ships games."

> **New technique: Fat struct with kind enum.** One struct holds ALL possible fields for all entity types. Unused fields are zero. A `kind` enum discriminates the type. This is the foundation of the entire course.
> - JS equivalent: TypeScript discriminated union `{ kind: 'player', x: number, health?: number, ... }`
> - What it replaces: class hierarchies with inheritance
> - Why it works: game entities share fields in arbitrary combinations that don't form trees

---

## Step 6 -- "But Doesn't That Waste Memory?"

This is the first objection everyone raises. "If my Item doesn't use `health`, aren't I wasting 4 bytes per item?"

Yes. You are. Let's see how much that matters.

### Back-of-napkin math

Our thing struct in this course will end up around 48 bytes (with game-specific fields like velocity, size, color). Let's be generous and think about a bigger game.

**Scenario 1: This course (modest game)**

```
    thing struct:   48 bytes
    MAX_THINGS:     1024

    Total memory:   1024 * 48 = 49,152 bytes = ~48 KB

    48 KB.
    That's NOTHING.
    Your L1 data cache is 32-64 KB.
    Your phone has 4-8 GB of RAM.
```

**Scenario 2: Large game with fat entities**

```
    thing struct:   100 properties * 4 bytes each = 400 bytes
    MAX_THINGS:     10,000

    Total memory:   10,000 * 400 = 4,000,000 bytes = ~4 MB

    4 MB.
    Still nothing.
    A single 1080p texture is 8 MB.
```

**Scenario 3: The worst case Anton considers**

```
    thing struct:   100 properties * 16 bytes each (float4) = 1,600 bytes
    MAX_THINGS:     10,000

    Total memory:   10,000 * 1,600 = 16,000,000 bytes = ~15 MB

    15 MB.
    One character model in a AAA game.
    Your GPU has 8-24 GB of VRAM.
```

> **Anton says:** "People worry about wasting 4 bytes per entity when they're loading 200 MB of textures. Stop optimizing things that don't matter. The struct is FINE."

The "wasted" bytes from unused fields are a rounding error in your total memory budget. What you gain is:

1. **No pointer chasing.** All entities are contiguous in memory. The CPU prefetcher loads the next entity while you're processing the current one.
2. **No allocation.** Adding an entity means writing to the next slot. No `malloc`, no fragmentation.
3. **No type confusion.** There is ONE entity type. You can put them all in one array. You can sort them, filter them, iterate them with a single loop.
4. **Trivial serialization.** Save game? Write the array to disk. Load game? Read it back. Done.

The OOP hierarchy gives you "type safety" that you immediately defeat with `dynamic_cast`. The fat struct gives you actual simplicity.

---

## Step 7 -- The Asteroids Connection

You've already built this pattern. In the Asteroids course, `SpaceObject` was a fat struct:

```c
typedef struct {
    float x, y;      // position
    float dx, dy;    // velocity
    float angle;     // heading
    float size;      // collision radius
    int   active;    // alive or dead?
} SpaceObject;
```

Ships, asteroids, and bullets were all `SpaceObject`. The only difference was how you updated them (player reads input, asteroids drift, bullets fly straight). You stored them in flat arrays: `SpaceObject asteroids[MAX_ASTEROIDS]`.

That was the fat struct pattern. You just didn't have a `kind` enum because you used separate arrays for each type. In this course, we're taking the next step: *all* entities in *one* array, distinguished by a `kind` field.

```
    Asteroids approach (separate arrays):

    SpaceObject asteroids[MAX_ASTEROIDS];   ← only asteroids
    SpaceObject bullets[MAX_BULLETS];       ← only bullets
    SpaceObject player;                     ← single player

    LOATs approach (unified pool):

    thing things[MAX_THINGS];               ← EVERYTHING goes here
    things[1].kind = PLAYER
    things[2].kind = TROLL
    things[3].kind = ITEM
    things[4].kind = TROLL
    ...
```

Why unify? Because entities need to *reference* each other. The player's inventory contains items. The troll's target is the player. An arrow was fired by a specific entity. When everything is in one array, a reference is just an integer index: `parent = 3` means "my parent is whatever is in `things[3]`." We'll build this in lessons 03 and 04.

> **Cross-reference:** GPP Chapter 14 (Component pattern) describes the same evolution -- from inheritance hierarchies to composition. Chapter 19 (Object Pool) describes the flat array. Anton's approach combines both: components are just fields in the fat struct, and the pool is a static array.

---

## Step 8 -- What About ECS? (The Elephant in the Room)

If you've been reading about game architecture online, you've definitely heard of ECS -- Entity Component System. Unity uses it. Bevy (Rust) uses it. It's the hot architecture pattern in games right now.

Here's the key insight: ECS and the fat struct approach solve the SAME problem (hierarchies don't work) with different tradeoffs.

```
    OOP Hierarchy          Fat Struct (this course)     ECS
    ──────────────         ────────────────────────     ────────────────
    Entity base class      One big struct per entity    Entity = just an ID
    Subclasses add fields  All fields in one struct     Components in separate arrays
    Virtual dispatch       Switch on kind enum          Systems iterate components
    Heap alloc per entity  Static flat array            Component arrays (SoA)
    Pointer-heavy          Index-based                  Index-based

    Complexity:  Medium    Low                          High
    Performance: Poor      Good                         Best (for huge counts)
    When to use: Never     < 10,000 entities            > 10,000 entities with
                           (most games)                 complex queries
```

ECS splits the fat struct into separate arrays: one array for positions, one for health, one for AI state. This is called Struct of Arrays (SoA) layout, and it's optimal for CPU cache when you iterate over one component at a time (e.g., "update all positions").

But ECS comes with a LOT of machinery: component registries, archetype tables, query systems, sparse sets. It's a framework. The fat struct approach is a pattern -- you can implement it in 200 lines of C.

> **Anton says:** "Start with the flat array. If profiling shows you need SoA layout for a specific hot loop, split THAT component out. Don't build an entire ECS framework on day one."

This course builds the flat array approach. Lesson 13 discusses when and how you'd transition to SoA/ECS if your entity count demands it. For the vast majority of games (anything under ~10,000 active entities), the fat struct approach is simpler, faster to develop, and performs well.

---

## Step 9 -- The Full Picture (Preview)

Don't worry about understanding all of the following yet. Each piece gets its own lesson. The point is: this is where we're going, and NONE of it requires classes, inheritance, virtual functions, or dynamic allocation.

Here's what we're building over the next 12 lessons, in one diagram:

```
    ┌─ thing_pool ──────────────────────────────────────────────────┐
    │                                                                │
    │  things[]:                                                     │
    │  ┌───────┬────────┬────────┬────────┬────────┬─── ─ ─ ─ ─ ┐  │
    │  │ [0]   │ [1]    │ [2]    │ [3]    │ [4]    │             │  │
    │  │ NIL   │ PLAYER │ TROLL  │ (dead) │ ITEM   │             │  │
    │  │ senti │ hp=100 │ hp=30  │        │ hp=0   │             │  │
    │  │ nel   │ par=0  │ par=0  │ free→2 │ par=1  │ ...1024     │  │
    │  └───────┴────────┴────────┴────────┴────────┴─── ─ ─ ─ ─ ┘  │
    │                                                                │
    │  used[]:    [F]  [T]   [T]   [F]   [T]   ...                  │
    │  gens[]:    [0]  [0]   [0]   [1]   [0]   ...                  │
    │  next_empty: 5                                                 │
    │  first_free: 3                                                 │
    └────────────────────────────────────────────────────────────────┘
```

- **things[0]** is the nil sentinel (Lesson 06) -- always zero, never a valid entity, safe to dereference
- **things[1..N]** hold living and dead entities
- **used[]** tracks which slots are alive (Lesson 07)
- **gens[]** track slot recycling for stale reference detection (Lesson 09)
- **first_free** points to a chain of dead slots for O(1) reuse (Lesson 08)
- Dead things like `[3]` store a free-list pointer in their `next_sibling` field
- All references between entities are integer indices, not pointers (Lesson 03)
- Parent-child relationships use intrusive linked lists (Lessons 04-05)

---

## Common mistakes

| Mistake | Why it's wrong |
|---------|---------------|
| "I'll use composition with small component classes" | You're still allocating components on the heap and chasing pointers. The fat struct is the component system -- fields ARE the components. |
| "I'll use an ECS library" | ECS is a valid architecture for large teams/editors, but it's massive machinery for what is fundamentally "put the data in a flat array." Learn the flat array first. |
| "I'll push common fields to a base struct and embed it" | This is C-style inheritance. It works for two levels, then you're back in the same hierarchy mess. |
| "But vtables give me polymorphism!" | A switch on an enum gives you the same dispatch with zero indirection and the compiler can optimize it (jump tables, inlining). The vtable buys you nothing in a closed set of entity types. |

---

## Exercises

1. **Enumerate your game entities.** Think of a game you want to build. List every entity type. Now list every property each entity needs (position, health, velocity, AI state, inventory, damage, render data, sound ID, ...). Draw the table from Step 3. Can you draw an inheritance tree that doesn't have the push-up problem? (Spoiler: you can't.)

2. **Count the bytes.** Take your entity table from exercise 1. Assume 4 bytes per float, 4 bytes per int. Multiply fields-per-entity by bytes-per-field by max-entities. Is the result scary? (Spoiler: it's probably under 1 MB.)

3. **Find the discriminated union.** Open a TypeScript project you've worked on. Find a place where you used a discriminated union (`type: 'something'` field with a switch). That's the fat struct pattern. The C version is identical except all fields exist on every instance.

---

## Connection to your work

If you've used Redux or Zustand, your entire application state lives in one big store object. Adding a new feature means adding new fields to the store, not creating a new class hierarchy. The fat struct is the same idea applied to game entities: one struct, all the fields, a type tag to know what you're looking at.

If you've used React, think of how a component's props object can have any shape -- you don't create a class for each prop combination. The thing struct is your universal props object for every game entity.

The ECS (Entity-Component-System) pattern you've probably heard about is a more sophisticated version of what we're building. ECS splits the fat struct into separate arrays per component for cache optimization. We're starting with the simpler approach (one array of fat structs) because it's what Anton recommends and what ships Dreams PS4. You can always split into SoA (Struct of Arrays) later if profiling demands it. We'll discuss this in Lesson 13.

---

## Step 10 -- Ryan Fleury's trait-based approach

Everything we've discussed so far uses a `kind` enum to distinguish entity types: `THING_KIND_PLAYER`, `THING_KIND_TROLL`, `THING_KIND_ITEM`. You switch on kind, and each case handles that type's specific behavior. It's simple. It works. But there's a more flexible variant championed by Ryan Fleury (hardcore member of "team fat struct") that's worth knowing about.

Instead of a single `kind` enum that says "this thing IS a player" or "this thing IS an item," you use bitflag traits that say "this thing HAS these properties." A thing can have MULTIPLE traits simultaneously:

```c
typedef enum {
    TRAIT_POSITIONABLE = (1 << 0),
    TRAIT_ENEMY        = (1 << 1),
    TRAIT_HOLDABLE     = (1 << 2),
    TRAIT_DAMAGEABLE   = (1 << 3),
    TRAIT_HAS_AI       = (1 << 4),
    TRAIT_RENDERABLE   = (1 << 5),
} thing_traits;

typedef struct thing {
    unsigned int traits;   /* bitfield of thing_traits */
    float x, y;
    float health;
    float damage;
    int   ai_type;
    /* ... all the fat struct fields as before ... */
} thing;
```

Now instead of asking "what kind is this?" you ask "does this thing have this trait?":

```c
/* A troll: positionable, enemy, damageable, has AI, renderable */
things[troll].traits = TRAIT_POSITIONABLE | TRAIT_ENEMY
                     | TRAIT_DAMAGEABLE   | TRAIT_HAS_AI
                     | TRAIT_RENDERABLE;

/* A dropped sword: positionable, holdable, damageable, renderable */
things[sword].traits = TRAIT_POSITIONABLE | TRAIT_HOLDABLE
                     | TRAIT_DAMAGEABLE   | TRAIT_RENDERABLE;

/* Check: does this thing have AI? */
if (things[idx].traits & TRAIT_HAS_AI) {
    run_ai(&things[idx]);
}

/* Check: can the player pick this up? */
if (things[idx].traits & TRAIT_HOLDABLE) {
    add_to_inventory(player_idx, idx);
}
```

The power is in combinations. A boss monster could be `TRAIT_ENEMY | TRAIT_HOLDABLE` (it's an enemy, but it also drops as an item when killed). A trap could be `TRAIT_POSITIONABLE | TRAIT_DAMAGEABLE` (it has a position and can be destroyed, but it's not an enemy and has no AI). You don't need intermediary types like "EnemyItem" or "DestructibleTrap" -- you just combine flags.

> **Anton says:** "Ryan would probably have something more like traits... positionable, enemy, holdable... and you'd have these as flags."

### The trade-off: kind enum vs. traits

The kind enum approach is simpler in one important way: you can write a single `switch` statement that handles all behavior for a type in one place. When you see `case THING_KIND_TROLL:`, all the troll behavior is right there. With traits, behavior is scattered across multiple `if (traits & TRAIT_X)` checks -- the troll's update logic might be spread across the AI system, the damage system, the rendering system, and the physics system.

```
    Kind enum (this course):
    ─────────────────────────
    switch (thing.kind) {
        case PLAYER: handle_all_player_stuff(); break;
        case TROLL:  handle_all_troll_stuff();  break;
    }
    
    Trait flags (Ryan's approach):
    ──────────────────────────────
    if (thing.traits & TRAIT_HAS_AI)      { run_ai();      }
    if (thing.traits & TRAIT_DAMAGEABLE)  { check_damage(); }
    if (thing.traits & TRAIT_RENDERABLE)  { render();       }
    if (thing.traits & TRAIT_HOLDABLE)    { check_pickup(); }
```

Neither is "better." Kind enums are easier to start with because all type-specific logic lives in one switch. Traits are more flexible because you never hit the "my entity needs to be two kinds at once" wall. For this course, we use kind enum because it's easier to learn and sufficient for everything we're building. Traits are the next level when you outgrow kind-based dispatch.

> **Alternative approach:** If you've used TypeScript intersection types, traits will feel familiar. A `type EnemyItem = Enemy & Holdable & Positionable` is the TS equivalent of `TRAIT_ENEMY | TRAIT_HOLDABLE | TRAIT_POSITIONABLE`. The key difference is that C bitflags are runtime combinable (you can add/remove traits dynamically), while TS intersection types are compile-time only. In both cases, the point is the same: entities are bags of capabilities, not nodes in a class tree.

---

## Build and run

No code to compile in this lesson. We start coding in Lesson 02. This lesson is purely conceptual -- grab a whiteboard (or a napkin) and sketch along.

---

## What's next

Lesson 02: we turn this conceptual design into real C code. You'll write the `thing` struct, the `thing_kind` enum, a static array of 1024 things, and a function that adds things to slots. First compilable code in the course.
