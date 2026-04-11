# Lesson 15 --- Data Layout Toggle Lab

> **Advanced Module.** This lesson requires the `--data-layout-lab` build flag. All code is conditionally compiled under `#ifdef ENABLE_DATA_LAYOUT_LAB`.

## What we're building

A laboratory that spawns 2000 entities and runs them through three different memory layouts --- Array of Structs (AoS), Struct of Arrays (SoA), and Hybrid (hot/cold separation) --- with **identical update logic** in each. You press `Tab` to cycle between layouts while the simulation runs. A performance HUD shows real timing data. The visual output is identical across all three modes. The only thing that changes is how fast the same work gets done.

When you run `./build-dev.sh --backend=raylib --data-layout-lab -r` and press `L`, the screen fills with 2000 bouncing entities. They all move, wrap around screen edges, and render as colored rectangles. Press `Tab`. The entities keep doing the exact same thing --- same positions, same velocities, same colors. But the timing number in the HUD changes. You just rearranged the bytes in memory and the CPU noticed.

This is the lab that turns lesson 13's SoA discussion from theory into something you can see and measure.

## What you'll learn

- **Cache lines matter more than algorithms** --- why rearranging data (not logic) changes performance
- **Array of Structs (AoS)** --- the intuitive layout, how it wastes cache space on irrelevant fields
- **Struct of Arrays (SoA)** --- the cache-friendly layout, how it packs useful data into every cache line
- **Hybrid (hot/cold)** --- the pragmatic middle ground, separating frequently-accessed from rarely-accessed data
- **Three loops, identical logic** --- seeing the difference in raw code, not hidden behind abstractions
- **Runtime data synchronization** --- copying entity state between layout representations on toggle
- **Performance observability** --- measuring real microsecond-level timing to prove the difference

## Prerequisites

- Completed lessons 01--13 (the full things pool system and game demo)
- Completed lesson 14 (Particle Laboratory) recommended but not required
- Familiarity with the `thing` struct fields: `x`, `y`, `dx`, `dy`, `size`, `color`, `kind`
- The game demo running on at least one backend (Raylib recommended for HUD)

---

## Building and running (quick start)

Before reading the theory, get the lab running so you can see the difference with your own eyes:

```bash
cd course/
./build-dev.sh --backend=raylib --data-layout-lab -r
```

Once the window opens, press `L` to enter the Data Layout Lab. You will see 2000 bouncing entities. The HUD shows the current layout mode and the update time in microseconds. Press `Tab` to cycle between AoS, SoA, and Hybrid modes. Watch the timing number change --- the entities behave identically, but the CPU processes them at different speeds depending on how the bytes are arranged in memory.

If you are on X11:

```bash
./build-dev.sh --backend=x11 --data-layout-lab -r
```

> **JS bridge:** Think of this like toggling between three different database schemas that store the same data. The queries return identical results, but one schema is faster because the database reads less irrelevant data from disk. Here the "database" is RAM, the "disk" is the CPU cache, and the "schema" is how you arrange struct fields in memory.

---

## Step 1 --- Why memory layout matters more than algorithms

Here is a question that will reshape how you think about performance.

You have 2000 entities. Each entity has a position (x, y), a velocity (dx, dy), a color, a size, a kind enum, health, a name string, and some flags. Your update loop does one thing: add velocity to position for every entity. That is two additions per entity --- `x += dx` and `y += dy`. Simple arithmetic.

Now here is the question: **does it matter how you arrange those fields in memory?**

Your instinct says no. Addition is addition. The CPU does not care whether `x` is at byte offset 0 or byte offset 48 in a struct. The math is the same.

Your instinct is wrong. It matters enormously. And the reason is a piece of hardware you never think about: the **cache**.

### The cache line problem

> **Why should a JS developer care about CPU cache?** In JavaScript, you never think about cache because V8 manages memory layout for you (and hides it). But here is the reality: **accessing data in the CPU cache is ~100x faster than accessing data in main RAM.** A cache hit takes ~1 nanosecond. A cache miss takes ~100 nanoseconds. That 100x gap is like the difference between reading a file from SSD versus downloading it over the internet. Every time you access memory that is NOT in the cache, the CPU stalls --- it literally sits there doing nothing for 100 nanoseconds, waiting for the data to arrive from RAM. At 60 FPS, you have 16.67 milliseconds per frame. If you cause 100,000 cache misses, that is 10 milliseconds wasted on waiting for memory. In JavaScript, this cost is hidden inside V8's internals and you cannot control it. In C, the memory layout is YOUR choice, and that choice directly determines how many cache misses happen.

Modern CPUs do not read individual bytes from RAM. They read in chunks called **cache lines** --- typically 64 bytes. When you access `entity.x`, the CPU does not fetch just those 4 bytes. It fetches a 64-byte block containing `entity.x` and whatever happens to be sitting next to it in memory.

If your struct looks like this:

```c
struct entity {
    float x, y;           /*  8 bytes --- you NEED these */
    float dx, dy;          /*  8 bytes --- you NEED these */
    float size;            /*  4 bytes --- you DON'T need for position update */
    float health;          /*  4 bytes --- you DON'T need */
    uint32_t color;        /*  4 bytes --- you DON'T need */
    int kind;              /*  4 bytes --- you DON'T need */
    char name[32];         /* 32 bytes --- you DEFINITELY don't need */
};
/* Total: 64 bytes per entity. One entity fills an entire cache line. */
```

When you read `entity[0].x`, the CPU loads all 64 bytes of `entity[0]` into cache. You use 16 of those bytes (x, y, dx, dy) and throw away 48 bytes of name, health, color, kind, and size. Then for `entity[1].x`, another 64-byte fetch. Another 48 bytes wasted. For 2000 entities, you load 128,000 bytes from RAM but only use 32,000. **You are wasting 75% of every cache line.**

> **Why?** In JavaScript, `entities.forEach(e => { e.x += e.dx; e.y += e.dy; })` hides this problem because V8's garbage collector moves objects around in memory unpredictably. In C, YOU control the layout. That means you can fix this --- but you have to understand it first.

### What if the useful data were packed together?

Imagine instead that all the `x` values are in one contiguous array, all the `y` values in another, all the `dx` values in another, and all the `dy` values in another:

```
positions_x:  [x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, ...]
positions_y:  [y0, y1, y2, y3, y4, y5, y6, y7, y8, y9, y10, y11, y12, y13, y14, y15, ...]
velocities_x: [dx0, dx1, dx2, dx3, dx4, dx5, dx6, dx7, ...]
velocities_y: [dy0, dy1, dy2, dy3, dy4, dy5, dy6, dy7, ...]
```

Now when the CPU fetches a 64-byte cache line starting at `positions_x[0]`, it gets 16 consecutive float values --- positions for entities 0 through 15. **Every byte is useful.** No wasted cache space. The next 16 entities are already in the next cache line, ready to go.

This is the difference between **Array of Structs (AoS)** and **Struct of Arrays (SoA)**. Same data. Same logic. Different layout. Different performance.

> **Anton says:** "Actually kind of related... it helps performance to rip the struct apart into a bunch of arrays."

He is talking about exactly this transformation. And in this lab, you are going to see it with your own eyes.

### ASCII diagram: AoS vs SoA memory layout

```
ARRAY OF STRUCTS (AoS) --- one cache line per entity:

Cache line 0 (64 bytes):
┌──────────────────────────────────────────────────────────────────┐
│ entity[0].x │ .y │ .dx │ .dy │ .size │ .health │ .color │ .kind │ .name[32]          │
│  USED       │USED│USED │USED │unused │ unused  │unused  │unused │ unused             │
└──────────────────────────────────────────────────────────────────┘
                     ↑ 16 bytes used, 48 bytes wasted

Cache line 1 (64 bytes):
┌──────────────────────────────────────────────────────────────────┐
│ entity[1].x │ .y │ .dx │ .dy │ .size │ .health │ .color │ .kind │ .name[32]          │
│  USED       │USED│USED │USED │unused │ unused  │unused  │unused │ unused             │
└──────────────────────────────────────────────────────────────────┘
                     ↑ Same waste, repeated 2000 times


STRUCT OF ARRAYS (SoA) --- 16 entities per cache line:

Cache line 0 of positions_x (64 bytes):
┌───────────────────────────────────────────────────────────────────────┐
│ x[0] │ x[1] │ x[2] │ x[3] │ x[4] │ x[5] │ x[6] │ x[7] │ ... x[15]│
│ ALL USED --- 16 entities' x-positions in one fetch                    │
└───────────────────────────────────────────────────────────────────────┘

Cache line 0 of velocities_x (64 bytes):
┌───────────────────────────────────────────────────────────────────────┐
│dx[0] │dx[1] │dx[2] │dx[3] │dx[4] │dx[5] │dx[6] │dx[7] │ ...dx[15] │
│ ALL USED --- 16 entities' x-velocities in one fetch                   │
└───────────────────────────────────────────────────────────────────────┘
```

For a position-update pass over 2000 entities:

- **AoS**: 2000 cache line fetches (one per entity), 75% waste each
- **SoA**: 125 cache line fetches for x, 125 for y, 125 for dx, 125 for dy = 500 total, 0% waste

Same number of additions. Same result. But SoA touches RAM **4x fewer times** for this particular operation.

> **Handmade Hero connection:** Casey discusses SoA for SIMD optimization. When your data is laid out as contiguous arrays of floats, the CPU can use SIMD instructions (SSE/AVX) to process 4 or 8 entities per instruction. AoS makes SIMD nearly impossible because the data for consecutive entities is 64 bytes apart, not 4 bytes apart.

> **Common mistake:** Assuming SoA is always better. It is not. If your update loop touches ALL fields of every entity (position, velocity, color, health, name), then AoS wastes nothing --- the whole cache line is useful. SoA wins when you have **property-specific passes** that touch a subset of fields. Most real engines have many such passes (physics touches position+velocity, rendering touches position+color, AI touches position+health).

---

## Step 2 --- Array of Structs (AoS): the intuitive layout

The AoS layout is what you have been building throughout this entire course. The `thing` struct holds all fields for one entity, and `things[]` is an array of those structs:

```c
#define DATA_LAYOUT_MAX_ENTITIES 2000

typedef struct {
    float x, y;
    float dx, dy;
    float size;
    uint32_t color;
    int kind;
    bool active;
} dl_entity_aos;

static dl_entity_aos aos_entities[DATA_LAYOUT_MAX_ENTITIES];
```

The update loop for AoS is straightforward --- the loop you have written a hundred times:

```c
static void update_aos(dl_entity_aos *entities, int count, float dt,
                        float screen_w, float screen_h)
{
    for (int i = 0; i < count; i++) {
        if (!entities[i].active) continue;

        entities[i].x += entities[i].dx * dt;
        entities[i].y += entities[i].dy * dt;

        /* Wrap around screen edges */
        if (entities[i].x < 0)        entities[i].x += screen_w;
        if (entities[i].x > screen_w) entities[i].x -= screen_w;
        if (entities[i].y < 0)        entities[i].y += screen_h;
        if (entities[i].y > screen_h) entities[i].y -= screen_h;
    }
}
```

Nothing surprising here. You access `entities[i].x`, `entities[i].dx`, `entities[i].y`, `entities[i].dy` --- all fields of the same struct, all in the same cache line. For a position update, this works fine because position and velocity are adjacent in the struct.

But notice: every time you load `entities[i]`, you also load `size`, `color`, `kind`, and `active` into the cache --- even though this loop never reads them. For a small struct like this one (around 28 bytes), the waste is moderate. For a real game entity struct at 128--256 bytes, the waste is severe.

> **Web dev bridge:** Think of AoS like a row-oriented database table. Each row is one entity, each column is a property. Reading one entity's data is fast (one row fetch). Scanning a single column across all entities is slow (you load every row to get one column).

> **JS bridge:** AoS is how JavaScript objects naturally work. When you write `const entities = [{ x: 1, y: 2, dx: 3, dy: 4, name: "goblin", health: 100 }, ...]`, each object bundles all its properties together. Iterating `entities.forEach(e => e.x += e.dx)` touches ALL properties of each object even though you only need `x` and `dx`. In V8's internal representation, this is somewhat like AoS --- each object's hidden class groups its properties. SoA would be like writing `const xs = [1, 2, 3, ...]; const dxs = [3, 4, 5, ...];` and iterating `for (let i = 0; i < xs.length; i++) xs[i] += dxs[i];`. The second form is actually faster in JavaScript too (V8 can optimize typed arrays), but nobody writes JS that way because it is awkward. In C, you choose whichever layout the profiler says is faster.

---

## Step 3 --- Struct of Arrays (SoA): the cache-friendly layout

Now rip the struct apart. Instead of one array of structs, create one array per field:

```c
typedef struct {
    float x[DATA_LAYOUT_MAX_ENTITIES];
    float y[DATA_LAYOUT_MAX_ENTITIES];
    float dx[DATA_LAYOUT_MAX_ENTITIES];
    float dy[DATA_LAYOUT_MAX_ENTITIES];
    float size[DATA_LAYOUT_MAX_ENTITIES];
    uint32_t color[DATA_LAYOUT_MAX_ENTITIES];
    int kind[DATA_LAYOUT_MAX_ENTITIES];
    bool active[DATA_LAYOUT_MAX_ENTITIES];
} dl_entities_soa;

static dl_entities_soa soa_entities;
```

The update loop has **identical logic** --- same math, same wrapping, same conditional skip. Only the data access syntax changes:

```c
static void update_soa(dl_entities_soa *e, int count, float dt,
                        float screen_w, float screen_h)
{
    for (int i = 0; i < count; i++) {
        if (!e->active[i]) continue;

        e->x[i] += e->dx[i] * dt;
        e->y[i] += e->dy[i] * dt;

        if (e->x[i] < 0)        e->x[i] += screen_w;
        if (e->x[i] > screen_w) e->x[i] -= screen_w;
        if (e->y[i] < 0)        e->y[i] += screen_h;
        if (e->y[i] > screen_h) e->y[i] -= screen_h;
    }
}
```

Read both loops side by side. The logic is identical. The only difference is `entities[i].x` vs `e->x[i]`. But what happens in memory is completely different.

When the CPU fetches `e->x[0]`, it loads a 64-byte cache line containing `x[0]` through `x[15]`. The next 15 iterations of the inner part of the loop hit cache --- no RAM access needed. Then `e->dx[0]` loads `dx[0]` through `dx[15]`. Again, 15 free cache hits.

For the position update pass:

```
AoS: load entity[0] (64 bytes, use 16) → load entity[1] (64 bytes, use 16) → ...
     2000 cache misses, 75% waste per line

SoA: load x[0..15] (64 bytes, use 64) → load x[16..31] (64 bytes, use 64) → ...
     125 cache misses for x, 125 for y, 125 for dx, 125 for dy
     500 cache misses total, 0% waste per line
```

> **Why?** This is the same principle behind column-oriented databases. PostgreSQL stores rows together (AoS). ClickHouse and BigQuery store columns together (SoA). Analytics queries that scan one column across millions of rows are 10--100x faster in column stores. Same data, same query, different layout, massive performance difference.

> **Alternative approach:** SoA has a real downside: adding or removing an entity requires updating EVERY parallel array, not just one struct. If you have 20 arrays, that is 20 separate operations per add/remove. AoS is one `memcpy`. For pools with high churn (lots of add/remove per frame), the add/remove cost of SoA can eat into the iteration savings. This is a real trade-off, not a theoretical one.

---

## Step 4 --- Hybrid: hot/cold data separation

The hybrid layout is the pragmatic middle ground. You observe that during a typical frame, the update loop touches position and velocity every frame (hot data), but touches color, kind, size, and name only during rendering or special events (cold data). So you split the struct into two:

```c
/* Hot data --- touched every frame during physics update */
typedef struct {
    float x, y;
    float dx, dy;
    bool active;
} dl_entity_hot;

/* Cold data --- touched only during rendering or events */
typedef struct {
    float size;
    uint32_t color;
    int kind;
} dl_entity_cold;

static dl_entity_hot  hybrid_hot[DATA_LAYOUT_MAX_ENTITIES];
static dl_entity_cold hybrid_cold[DATA_LAYOUT_MAX_ENTITIES];
```

The update loop touches only the hot array:

```c
static void update_hybrid(dl_entity_hot *hot, int count, float dt,
                           float screen_w, float screen_h)
{
    for (int i = 0; i < count; i++) {
        if (!hot[i].active) continue;

        hot[i].x += hot[i].dx * dt;
        hot[i].y += hot[i].dy * dt;

        if (hot[i].x < 0)        hot[i].x += screen_w;
        if (hot[i].x > screen_w) hot[i].x -= screen_w;
        if (hot[i].y < 0)        hot[i].y += screen_h;
        if (hot[i].y > screen_h) hot[i].y -= screen_h;
    }
}
```

The hot struct is 20 bytes (with padding, likely 20 bytes). A 64-byte cache line holds 3 complete hot entities. The cold data --- size, color, kind --- stays out of the cache entirely during the physics pass. The rendering pass reads both hot (for position) and cold (for color/size), but that is one pass per frame, not the bottleneck.

```
HYBRID layout in memory:

hot[] array (20 bytes per entity):
┌─────────────────────┬─────────────────────┬─────────────────────┐
│ hot[0]              │ hot[1]              │ hot[2]              │ ... (3 per cache line)
│ x, y, dx, dy, active│ x, y, dx, dy, active│ x, y, dx, dy, active│
│ ALL used by physics │ ALL used by physics │ ALL used by physics │
└─────────────────────┴─────────────────────┴─────────────────────┘

cold[] array (12 bytes per entity, untouched during physics):
┌───────────────────┬───────────────────┬───────────────────┬───────────────────┬───────────────────┐
│ cold[0]           │ cold[1]           │ cold[2]           │ cold[3]           │ cold[4]           │
│ size, color, kind │ size, color, kind │ size, color, kind │ size, color, kind │ size, color, kind │
└───────────────────┴───────────────────┴───────────────────┴───────────────────┴───────────────────┘
                     ↑ stays in RAM, never loaded during update
```

> **JS bridge:** The hybrid approach is like separating "hot state" from "cold config" in a React component. Imagine a game entity component where `position` and `velocity` change every frame (hot), but `name`, `spriteUrl`, and `maxHealth` are set once at creation (cold). In React, you might split these into `useRef` for the hot data (avoids re-render) and `useState` for the cold config. In C, you split them into separate structs. The physics loop only touches the hot struct, so the cold data never pollutes the CPU cache during physics.

> **Why hybrid instead of full SoA?** SoA is the theoretical optimum for single-property passes, but it scatters entity identity across many arrays. Debugging is harder --- "show me entity 47" means looking at 8 different arrays. Add/remove requires updating 8 arrays instead of 2. The hybrid keeps entity identity coherent within each sub-struct while still eliminating the worst cache waste. Most production engines use some form of hybrid, not pure SoA.

> **Handmade Hero connection:** Casey's approach to entity storage evolves over the series. He starts with AoS (every entity is one struct), then begins separating simulation-relevant data from rendering-relevant data --- exactly this hot/cold pattern.

---

## Step 5 --- Three loops, identical logic, different access

This is the core of the lab. Three update functions with **identical logic** and **different data access**. Put them side by side:

```c
/* ══════════════════════════════════════════════════════ */
/*                AoS Update Loop                        */
/* ══════════════════════════════════════════════════════ */
for (int i = 0; i < count; i++) {
    if (!aos[i].active) continue;
    aos[i].x += aos[i].dx * dt;
    aos[i].y += aos[i].dy * dt;
    if (aos[i].x < 0)   aos[i].x += sw;  if (aos[i].x > sw) aos[i].x -= sw;
    if (aos[i].y < 0)   aos[i].y += sh;  if (aos[i].y > sh) aos[i].y -= sh;
}

/* ══════════════════════════════════════════════════════ */
/*                SoA Update Loop                        */
/* ══════════════════════════════════════════════════════ */
for (int i = 0; i < count; i++) {
    if (!soa->active[i]) continue;
    soa->x[i] += soa->dx[i] * dt;
    soa->y[i] += soa->dy[i] * dt;
    if (soa->x[i] < 0)   soa->x[i] += sw;  if (soa->x[i] > sw) soa->x[i] -= sw;
    if (soa->y[i] < 0)   soa->y[i] += sh;  if (soa->y[i] > sh) soa->y[i] -= sh;
}

/* ══════════════════════════════════════════════════════ */
/*                Hybrid Update Loop                     */
/* ══════════════════════════════════════════════════════ */
for (int i = 0; i < count; i++) {
    if (!hot[i].active) continue;
    hot[i].x += hot[i].dx * dt;
    hot[i].y += hot[i].dy * dt;
    if (hot[i].x < 0)   hot[i].x += sw;  if (hot[i].x > sw) hot[i].x -= sw;
    if (hot[i].y < 0)   hot[i].y += sh;  if (hot[i].y > sh) hot[i].y -= sh;
}
```

Look at them. The bodies are the same. The arithmetic is the same. The branching is the same. The compiler will generate nearly identical instructions for all three. The only difference is the memory addresses the CPU fetches from.

And that difference --- invisible in the source code --- is what makes one of these loops faster than the others.

**This is the lesson.** Not "SoA is better." The lesson is: **memory layout is a performance variable, and you control it.** Most programmers never realize they have this lever. They think performance means better algorithms. In reality, for data-parallel workloads (updating 2000 entities per frame), the biggest performance lever is often how you arrange the bytes.

> **Anton says:** "You could have 100,000 things and you're spending 150 megabytes. You're going to be completely bottlenecked by something else." That "something else" is often the cache. The pool itself is cheap. The question is how efficiently the CPU can iterate through it.

---

## Step 6 --- Runtime toggle and data sync

The lab lets you switch layouts at runtime by pressing `Tab`. When you switch, the simulation must continue seamlessly --- same entity positions, same velocities, same visual output. This means copying data between layout representations.

### The layout enum

```c
typedef enum {
    DATA_LAYOUT_AOS = 0,
    DATA_LAYOUT_SOA,
    DATA_LAYOUT_HYBRID,
    DATA_LAYOUT_COUNT
} data_layout_mode;

static data_layout_mode current_layout = DATA_LAYOUT_AOS;
```

### Syncing AoS to SoA

When switching FROM AoS TO SoA, unpack each struct into the parallel arrays:

```c
static void sync_aos_to_soa(dl_entity_aos *aos, dl_entities_soa *soa, int count)
{
    for (int i = 0; i < count; i++) {
        soa->x[i]      = aos[i].x;
        soa->y[i]      = aos[i].y;
        soa->dx[i]     = aos[i].dx;
        soa->dy[i]     = aos[i].dy;
        soa->size[i]   = aos[i].size;
        soa->color[i]  = aos[i].color;
        soa->kind[i]   = aos[i].kind;
        soa->active[i] = aos[i].active;
    }
}
```

### Syncing SoA to AoS

The reverse --- pack parallel arrays back into structs:

```c
static void sync_soa_to_aos(dl_entities_soa *soa, dl_entity_aos *aos, int count)
{
    for (int i = 0; i < count; i++) {
        aos[i].x      = soa->x[i];
        aos[i].y      = soa->y[i];
        aos[i].dx     = soa->dx[i];
        aos[i].dy     = soa->dy[i];
        aos[i].size   = soa->size[i];
        aos[i].color  = soa->color[i];
        aos[i].kind   = soa->kind[i];
        aos[i].active = soa->active[i];
    }
}
```

### Syncing AoS to Hybrid

Split into hot and cold:

```c
static void sync_aos_to_hybrid(dl_entity_aos *aos,
                                dl_entity_hot *hot, dl_entity_cold *cold,
                                int count)
{
    for (int i = 0; i < count; i++) {
        hot[i].x      = aos[i].x;
        hot[i].y      = aos[i].y;
        hot[i].dx     = aos[i].dx;
        hot[i].dy     = aos[i].dy;
        hot[i].active = aos[i].active;

        cold[i].size  = aos[i].size;
        cold[i].color = aos[i].color;
        cold[i].kind  = aos[i].kind;
    }
}
```

### The toggle function

```c
static void toggle_layout(void)
{
    data_layout_mode next = (current_layout + 1) % DATA_LAYOUT_COUNT;

    /* Sync FROM current layout TO canonical (AoS is canonical) */
    switch (current_layout) {
        case DATA_LAYOUT_SOA:
            sync_soa_to_aos(&soa_entities, aos_entities, DATA_LAYOUT_MAX_ENTITIES);
            break;
        case DATA_LAYOUT_HYBRID:
            sync_hybrid_to_aos(hybrid_hot, hybrid_cold, aos_entities,
                               DATA_LAYOUT_MAX_ENTITIES);
            break;
        default: break; /* AoS is already canonical */
    }

    /* Sync FROM canonical (AoS) TO next layout */
    switch (next) {
        case DATA_LAYOUT_SOA:
            sync_aos_to_soa(aos_entities, &soa_entities, DATA_LAYOUT_MAX_ENTITIES);
            break;
        case DATA_LAYOUT_HYBRID:
            sync_aos_to_hybrid(aos_entities, hybrid_hot, hybrid_cold,
                               DATA_LAYOUT_MAX_ENTITIES);
            break;
        default: break;
    }

    current_layout = next;
}
```

The sync uses AoS as the canonical representation. On every toggle: current layout -> AoS -> next layout. This means at most two conversion passes per toggle. Toggling is not a hot path --- it happens once when the user presses a key --- so the conversion cost does not matter.

> **Why AoS as canonical?** Because it is the simplest to reason about. One struct per entity. When debugging, you can inspect `aos_entities[47]` and see the complete entity state in one place. The SoA and Hybrid representations are derived from AoS for measurement purposes only.

> **Web dev bridge:** This is similar to how React uses a virtual DOM as the canonical representation and "syncs" it to the actual DOM. You have one source of truth (AoS / virtual DOM) and derived representations (SoA, Hybrid / actual DOM) that get rebuilt when something changes.

---

## Step 7 --- Performance observability

The whole point of this lab is measurement. Without timing data, you are just trusting theory. Theory is necessary but insufficient. You need numbers.

### Timing the update loop

```c
#include <time.h>

static double get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
}
```

Wrap each update call with timing:

```c
double t0 = get_time_us();

switch (current_layout) {
    case DATA_LAYOUT_AOS:
        update_aos(aos_entities, entity_count, dt, screen_w, screen_h);
        break;
    case DATA_LAYOUT_SOA:
        update_soa(&soa_entities, entity_count, dt, screen_w, screen_h);
        break;
    case DATA_LAYOUT_HYBRID:
        update_hybrid(hybrid_hot, entity_count, dt, screen_w, screen_h);
        break;
}

double t1 = get_time_us();
double update_time_us = t1 - t0;
```

### The HUD display

The HUD shows:

```
Layout: SoA
Entities: 2000
Update: 43.7 us
FPS: 60
```

Use a rolling average to smooth out frame-to-frame jitter:

```c
#define TIMING_HISTORY 60

static double timing_history[TIMING_HISTORY];
static int timing_index = 0;

static void record_timing(double us)
{
    timing_history[timing_index] = us;
    timing_index = (timing_index + 1) % TIMING_HISTORY;
}

static double average_timing(void)
{
    double sum = 0;
    for (int i = 0; i < TIMING_HISTORY; i++) {
        sum += timing_history[i];
    }
    return sum / TIMING_HISTORY;
}
```

### What to expect

On modern hardware with 2000 entities and a simple position update:

- **AoS**: Fastest or comparable. The struct is small enough (~28 bytes) that 2--3 entities fit per cache line. The cache waste is moderate.
- **SoA**: Fastest for the pure position update pass. Every cache line is 100% useful data.
- **Hybrid**: Between AoS and SoA. Hot struct is smaller than full AoS, so more entities per cache line.

The differences may be small at 2000 entities --- perhaps 10--50 microseconds apart. That is fine. The point is not "SoA is 10x faster." The point is: **you changed zero logic and the timing changed.** At 50,000 entities or with a larger struct (128--256 bytes), the difference becomes dramatic.

> **Common mistake:** Running the lab in debug mode (`-O0`) and concluding that layout does not matter. Without optimization, the compiler generates so much overhead per memory access that the cache effects are buried. Always measure with `-O2` or `-O3`. The build script handles this --- `build-dev.sh` uses `-O2` by default for the data layout lab.

> **Common mistake:** Expecting a 10x speedup at 2000 entities with a 28-byte struct. The struct is too small and the entity count too low for dramatic differences. The lab teaches the PRINCIPLE. Scale to 50,000 entities or inflate the struct to 256 bytes (add padding arrays) if you want to see dramatic numbers. The lesson is that the lever EXISTS, not that it is always dominant at small scale.

---

## Step 8 --- Building and running

### Build commands

```bash
# Raylib backend with Data Layout Lab enabled
./build-dev.sh --backend=raylib --data-layout-lab -r

# X11 backend with Data Layout Lab enabled
./build-dev.sh --backend=x11 --data-layout-lab -r
```

The `--data-layout-lab` flag defines `ENABLE_DATA_LAYOUT_LAB` at compile time. The scene is registered as Scene 11 in `game.c`, accessible by pressing `L`.

### Controls

| Key | Action |
|---|---|
| `L` | Enter Data Layout Lab scene |
| `Tab` | Cycle layout: AoS -> SoA -> Hybrid -> AoS -> ... |
| `1`--`9` | Switch to other scenes (exits the lab) |

### What you should see

1. Press `L`. The screen fills with 2000 colored rectangles bouncing around.
2. The HUD shows `Layout: AoS`, the entity count, and the update time in microseconds.
3. Press `Tab`. The entities continue moving identically. The HUD now shows `Layout: SoA`. Note the update time.
4. Press `Tab` again. `Layout: Hybrid`. Note the update time.
5. Press `Tab` to cycle back to `AoS`. Compare all three numbers.

The visual output is identical across all three modes. If the entities visibly jump, stutter, or change color on toggle, the sync function has a bug.

### Conditional compilation

All Data Layout Lab code is guarded:

```c
#ifdef ENABLE_DATA_LAYOUT_LAB
/* ... all lab code ... */
#endif
```

Without the `--data-layout-lab` flag, zero lab code is compiled into the binary. The default game demo is completely unaffected.

---

## Connection to your work

This lab is the bridge between "Large Arrays of Things" and the rest of your engine education.

**Where you have been:** Lessons 01--13 taught you how to manage entities --- flat arrays, nil sentinels, free lists, generational IDs. Lesson 14 proved the pool works at scale. All of that used AoS because it is the simplest and most debuggable layout.

**Where you are now:** You have seen that the SAME pool, with the SAME logic, performs differently depending on how the bytes are arranged. Layout is a performance variable you control.

**Where this goes:**

- **Game Programming Patterns, Ch 17 (Data Locality)** covers this exact topic. Bob Nystrom explains cache lines, AoS vs SoA, and hot/cold splitting. You have now BUILT and MEASURED what that chapter describes.
- **Game Engine Architecture, Ch 6 (memory management)** discusses memory access patterns and cache behavior in the context of engine subsystems. The Data Layout Lab is a micro-version of the decisions engine teams make when designing their entity storage.
- **ECS architectures** (Unity DOTS, Flecs, EnTT) are fundamentally SoA systems. Components are stored in contiguous arrays per component type, not per entity. The Data Layout Lab gives you the intuition for WHY ECS stores data that way.
- **Handmade Hero** --- Casey's entity system evolves toward separating simulation data from rendering data, which is the hot/cold hybrid pattern. When you see him reorganize entity storage, you will understand the cache-level reasoning behind it.

> **Anton says:** On SoA and ripping structs apart --- the performance benefit is real, but the ergonomic cost is real too. In a small team building a "log cabin," AoS might be all you need. In a AAA engine processing 100,000 entities, SoA or hybrid is mandatory. Know the technique so you can apply it WHEN the profiler tells you to, not before.

The key takeaway is not "use SoA everywhere." The key takeaway is: **you now have a lever most programmers do not even know exists.** When your profiler says the bottleneck is iteration over entities, you do not need a better algorithm. You need a better layout. And you know exactly how to build one.

## What's next

If you have completed lessons 01--15, you have built:

- A complete entity pool with intrusive lists, nil sentinels, free lists, and generational IDs (lessons 01--10)
- A working game demo exercising every pool feature (lessons 11--12)
- An architectural understanding of the design space and trade-offs (lesson 13)
- A particle stress test proving the pool works at 1000 entities (lesson 14)
- A data layout laboratory proving that memory organization affects performance (lesson 15)

You have gone from "how do I store game entities?" to "how do I store game entities so the CPU can process them efficiently?" That is the journey from application programmer to systems programmer. Every engine decision you encounter from here --- in Handmade Hero, in Game Engine Architecture, in ECS frameworks --- will make more sense because you have felt the hardware through the data.
