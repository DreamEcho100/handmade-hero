# Lesson 16 --- Spatial Partition Lab

## What we're building

A laboratory that spawns 2000 entities into the things pool and runs collision detection through four different spatial query strategies --- Naive all-pairs O(N²), Uniform Grid, Spatial Hash, and Quadtree. You press `Tab` to cycle between modes while the simulation runs. A visual debug overlay renders the partition structure directly on screen: grid lines, hash bucket highlights, quadtree subdivision boxes. A performance HUD shows check counts, collision counts, and frame timing. The visual output --- entity positions, colors, collision behavior --- is identical across all four modes. The only thing that changes is how the simulation *finds* nearby entities and how fast it does it.

When you run `./build-dev.sh --backend=raylib -r` and press `G`, the screen fills with 2000 bouncing entities. In Naive mode, the HUD shows ~4,000,000 checks per frame and the simulation stutters. Press `Tab` to switch to Grid mode. The check count drops to ~30,000. The stuttering vanishes. Press `Tab` again for Hash mode --- similar performance, but the debug overlay shows hash buckets instead of a fixed grid. One more `Tab` for Quadtree --- the overlay shows recursive subdivision boxes adapting to entity density. Same entities. Same collisions. Same visual result. Radically different performance.

This is the lab that proves: scalable engines depend on smart query organization, not faster CPUs.

## What you'll learn

- **Why O(N²) is the real performance killer** --- 2000 entities means 4,000,000 pair checks per frame, which is 240 million checks per second at 60 FPS
- **Uniform grid partitioning** --- divide the world into cells, only check same + neighbor cells
- **Spatial hashing** --- grid-like performance without fixed world bounds
- **Quadtree subdivision** --- adaptive partitioning that handles uneven entity density
- **Broadphase vs narrowphase thinking** --- how real physics engines structure their collision pipeline
- **Visual debug overlays** --- rendering the spatial structure so you can see why it works
- **Performance observability** --- measuring real check counts and timing to quantify the difference

## Prerequisites

- Completed lessons 01--13 (the full things pool system and game demo)
- Completed lesson 14 (Particle Laboratory) recommended --- you have seen the pool under load
- Completed lesson 15 (Data Layout Lab) recommended --- you have seen how data organization affects performance
- The game demo running on at least one backend (Raylib recommended for HUD and debug overlay)

---

## Building and running (quick start)

Get the lab running first so you can feel the difference between O(N^2) and spatial partitioning:

```bash
cd course/
./build-dev.sh --backend=raylib -r
```

Once the window opens, press `G` to enter the Spatial Partition Lab. You will see 2000 bouncing entities with the Naive (O(N^2)) collision mode. The HUD shows ~2,000,000 pair checks per frame, and the simulation may visibly stutter. Press `Tab` to cycle through Grid, Hash, and Quadtree modes --- watch the check count drop to ~30,000 and the stuttering vanish. Press `Tab` again to cycle back to Naive and feel the contrast.

No special build flag is needed --- the Spatial Partition Lab is always compiled in.

If you are on X11:

```bash
./build-dev.sh --backend=x11 -r
```

> **JS bridge:** This is like the difference between `array.filter(a => array.some(b => collides(a, b)))` (checks everything against everything) versus using a spatial index to only check nearby items. If you have ever used a quadtree library in a canvas game or wondered why a nested `forEach` gets slow at a few hundred elements, this lab shows you exactly why --- and the fix.

---

## Step 1 --- Why O(N^2) kills your frame rate

Let me give you a number. You have 2000 entities. Each entity needs to check for collisions with every other entity. How many checks is that?

```
2000 × 1999 / 2 = 1,999,000 unique pairs
```

Round it: ~2 million pair checks. Each check involves reading two positions, computing a distance (or bounding box overlap), and comparing against a threshold. That is at minimum a subtract, a multiply, and a compare per pair. At 60 FPS, you are doing this 60 times per second:

```
2,000,000 pairs × 60 frames = 120,000,000 pair checks per second
```

In practice, because the inner loop accesses both entities' data, the actual number of memory reads is closer to 4 million per frame (two entities per pair, position data for each). The CPU's cache is thrashing because you are touching memory locations scattered across a 2000-element array in a pattern that has no spatial locality.

Here is the concrete math on why this falls apart:

```
Entity struct:   ~48 bytes (x, y, dx, dy, size, color, kind, flags)
Pool size:       2000 × 48 = 96,000 bytes = ~94 KB
L1 cache:        typically 32--64 KB
L2 cache:        typically 256 KB--1 MB

The entire pool fits in L2 but NOT in L1.
The inner loop reads entity A and entity B for every pair.
With 2000 entities, the inner loop iterates 2000 times per outer iteration.
By the time you finish the inner loop and start the next outer iteration,
entity A's cache line has been evicted. You reload it. Every. Single. Time.
```

This is not a theoretical problem. You will see it. The naive mode stutters on modern hardware with only 2000 entities. Not 200,000. Not 2,000,000. Two thousand. That is how brutal O(N²) is.

> **Why?** In JavaScript, `entities.forEach(a => entities.forEach(b => checkCollision(a, b)))` hides this cost. V8 cannot optimize the nested loop, but you do not notice because browser games rarely have 2000 collidable entities. In C, you are running close to the metal, and the metal tells you the truth: O(N^2) does not scale.

> **JS bridge: what O(N^2) means in practice.** If you have written nested `for` loops in JavaScript --- `for (const a of items) { for (const b of items) { ... } }` --- you have written O(N^2) code. "O(N^2)" means the work grows as the SQUARE of the input size. 10 items = 100 iterations. 100 items = 10,000 iterations. 2000 items = 4,000,000 iterations. This is the same reason your Node.js API gets slow when you accidentally nest database queries inside a loop: it is not the individual operation that is expensive, it is doing it N^2 times. The fix here is the same conceptual fix as adding a database index: organize the data so you can skip the items that cannot possibly match.

> **Handmade Hero connection:** Casey discusses this exact problem when entity counts grow. The solution is always the same --- stop checking every entity against every other entity. Only check things that are *near* each other. That is spatial partitioning.

### The key insight

The collision between entity A and entity B can only happen if they are close to each other. Entity A at position (50, 50) cannot collide with entity B at position (700, 800). Checking that pair is pure waste. If you could somehow only check entities that are in the same *region* of the world, you would eliminate almost all of those 2 million useless pair checks.

That is the entire idea behind spatial partitioning. Organize the world into regions. Put each entity in its region. When checking collisions for entity A, only look at entities in the same region (and neighboring regions, to catch entities near region boundaries).

```
BEFORE (Naive):
Every entity checks every other entity.
2000 × 2000 = 4,000,000 checks (including symmetric and self).

┌─────────────────────────────────────────────────┐
│ ·  ·  ·     ·        ·    ·    ·      ·         │
│    ·     ·     ·   ·    ·        ·  ·     ·     │
│  ·    ·    ·     ·       ·  ·       ·    ·      │
│     ·   ·     ·    ·  ·      ·        ·         │
│  ·     ·   ·    ·       ·      ·   ·            │
│    ·      ·      ·    ·    ·     ·    ·    ·    │
│  ·   ·      ·  ·       ·     ·       ·          │
│       ·   ·       ·  ·    ·       ·     ·       │
└─────────────────────────────────────────────────┘
  Every dot checks every other dot. O(N²).

AFTER (Grid):
World divided into cells. Entity only checks its cell + neighbors.

┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ ·  ·     │   ·      │  ·    ·  │  ·       │    ·     │
│    ·     │·     ·   │·    ·    │    ·  ·  │  ·     · │
├──────────┼──────────┼──────────┼──────────┼──────────┤
│  ·    ·  │  ·     · │     ·    │·  ·      │    ·     │
│     ·    │·     ·   │ ·    ·   │     ·    │  ·       │
├──────────┼──────────┼──────────┼──────────┼──────────┤
│  ·     · │  ·    ·  │     ·    │  ·    ·  │    ·     │
│    ·     │      ·   │·    ·    │     ·    │·      ·  │
├──────────┼──────────┼──────────┼──────────┼──────────┤
│       ·  │ ·      · │  ·    ·  │  ·       │   ·    · │
│  ·   ·   │     ·    │      ·   │    ·  ·  │      ·   │
└──────────┴──────────┴──────────┴──────────┴──────────┘
  Each dot only checks dots in same + adjacent cells. O(N × K).
  K = average entities per cell neighborhood ≈ 5--20.
```

From 4,000,000 checks to maybe 30,000 checks. Same collisions detected. Same visual result. That is the power of spatial partitioning.

---

## Step 2 --- The naive baseline: all-pairs collision

We build the naive version first. Not because it is good --- it is terrible. We build it because you need to see it fail. If you never watch O(N²) stutter, you will never viscerally understand why partitioning matters.

The structure is simple. Two nested loops:

```c
/* ══════════════════════════════════════════════════════ */
/*           Naive All-Pairs Collision                    */
/* ══════════════════════════════════════════════════════ */

static int collision_naive(thing_pool *pool, int *out_collisions) {
    int checks = 0;
    int collisions = 0;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        thing *a = &pool->things[i];

        for (int j = i + 1; j < pool->next_empty; j++) {
            if (!pool->used[j]) continue;
            thing *b = &pool->things[j];

            checks++;

            float dx = a->x - b->x;
            float dy = a->y - b->y;
            float dist_sq = dx * dx + dy * dy;
            float min_dist = a->size + b->size;

            if (dist_sq < min_dist * min_dist) {
                collisions++;
                resolve_collision(a, b);
            }
        }
    }

    *out_collisions = collisions;
    return checks;
}
```

Every line here is doing real work:

- The outer loop walks every alive entity starting at index 1 (skipping the nil sentinel at 0).
- The inner loop starts at `i + 1` to avoid checking pairs twice (A-B and B-A) and to avoid self-checks.
- The `!pool->used[i]` check skips dead slots --- the iterator in action, just inlined.
- The distance check uses squared distance to avoid a `sqrt` call. If `dist_sq < (a->size + b->size)²`, the circles overlap.
- `resolve_collision` does whatever you want: bounce the entities apart, change colors, swap velocities. The point is it happens for every detected collision.

The `checks` counter is the critical metric. It tells you how many pair comparisons actually happened. For 2000 entities, this will report roughly 1,999,000 checks per frame.

> **Common mistake:** Using `dist < min_dist` with `dist = sqrt(dx*dx + dy*dy)`. That `sqrt` is expensive and unnecessary. Squaring both sides of the comparison eliminates it: `dist_sq < min_dist * min_dist`. Same mathematical result, no square root. This is standard practice in every game engine.

### Why we count checks, not just time

Timing is noisy. Your OS runs background tasks. Your CPU throttles. A garbage collector runs in a neighboring process. Timing fluctuates frame to frame.

Check counts are deterministic. 2000 entities always produces ~1,999,000 checks in naive mode. When you switch to grid mode, the check count drops to ~30,000 regardless of CPU speed, background load, or phase of the moon. The check count is the true measure of algorithmic efficiency.

We display both --- timing (microseconds) and check count --- but the check count is the number you should trust.

---

## Step 3 --- Uniform grid: divide the world into cells

The first real spatial partition. The idea is dead simple: overlay a grid on the world. Each cell is a fixed size (say, 64x64 pixels). Every frame, put each entity into the cell containing its center. When checking collisions for entity A, only check entities in the same cell and the 8 neighboring cells.

### Data structure

```c
#define GRID_COLS 16
#define GRID_ROWS 12
#define GRID_CELL_SIZE 64
#define MAX_ENTITIES_PER_CELL 64

typedef struct {
    int entity_indices[MAX_ENTITIES_PER_CELL];
    int count;
} grid_cell;

typedef struct {
    grid_cell cells[GRID_ROWS][GRID_COLS];
} uniform_grid;
```

This is a flat 2D array of cells. Each cell holds up to `MAX_ENTITIES_PER_CELL` entity indices and a count. No linked lists. No dynamic allocation. Just arrays.

> **JS bridge:** Think of the grid like CSS Grid dividing a page into cells. Each cell is a fixed-size rectangle of the game world. When you check collision for an entity, you only look inside that entity's cell and the 8 neighboring cells --- just like how a CSS Grid item only interacts with items in adjacent cells. The grid converts a global search problem ("check every entity") into a local search problem ("check nearby entities"). In JavaScript terms, it is the difference between `array.filter(...)` scanning everything versus `map.get(key)` fetching directly by location.

### Memory math

```
grid_cell:     64 ints × 4 bytes + 1 int = 260 bytes
grid:          16 × 12 = 192 cells
total:         192 × 260 = 49,920 bytes ≈ 49 KB

For comparison, the entity pool itself is ~96 KB.
The grid costs about half as much as the entities it indexes.
```

That is cheap. A 49 KB data structure that turns 2,000,000 checks into 30,000 checks. That is the kind of trade-off you take every day and twice on Sunday.

### Building the grid each frame

```c
static void grid_clear(uniform_grid *grid) {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            grid->cells[r][c].count = 0;
        }
    }
}

static void grid_insert(uniform_grid *grid, thing *t, int entity_idx) {
    int col = (int)(t->x / GRID_CELL_SIZE);
    int row = (int)(t->y / GRID_CELL_SIZE);

    /* Clamp to grid bounds */
    if (col < 0) col = 0;
    if (col >= GRID_COLS) col = GRID_COLS - 1;
    if (row < 0) row = 0;
    if (row >= GRID_ROWS) row = GRID_ROWS - 1;

    grid_cell *cell = &grid->cells[row][col];
    if (cell->count < MAX_ENTITIES_PER_CELL) {
        cell->entity_indices[cell->count] = entity_idx;
        cell->count++;
    }
    /* If cell is full, entity is silently dropped from collision.
     * In production you would grow the cell or use a linked list.
     * For the lab, 64 per cell is more than enough. */
}

static void grid_build(uniform_grid *grid, thing_pool *pool) {
    grid_clear(grid);
    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        grid_insert(grid, &pool->things[i], i);
    }
}
```

Every frame: clear the grid (set all counts to 0), then insert every alive entity into the cell that contains its position. The `grid_build` function is O(N) --- one pass through all entities.

> **Why rebuild every frame?** Because entities move. Entity A was in cell (3,4) last frame but moved to cell (3,5) this frame. Rather than tracking which entities changed cells and patching the grid incrementally, we just wipe it and rebuild. At 2000 entities this takes microseconds. Incremental update would be faster in theory but adds complexity that is not worth it at this scale.

### Querying the grid

```c
static int collision_grid(uniform_grid *grid, thing_pool *pool,
                          int *out_collisions) {
    int checks = 0;
    int collisions = 0;

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            grid_cell *cell = &grid->cells[r][c];

            /* Check all pairs within this cell */
            for (int i = 0; i < cell->count; i++) {
                thing *a = &pool->things[cell->entity_indices[i]];

                /* Same cell: remaining entities */
                for (int j = i + 1; j < cell->count; j++) {
                    thing *b = &pool->things[cell->entity_indices[j]];
                    checks++;

                    float dx = a->x - b->x;
                    float dy = a->y - b->y;
                    float dist_sq = dx * dx + dy * dy;
                    float min_dist = a->size + b->size;
                    if (dist_sq < min_dist * min_dist) {
                        collisions++;
                        resolve_collision(a, b);
                    }
                }

                /* Neighbor cells: only check RIGHT, BELOW, and DIAGONALS
                 * to avoid double-counting */
                int neighbors[][2] = {
                    {r, c+1}, {r+1, c-1}, {r+1, c}, {r+1, c+1}
                };
                for (int n = 0; n < 4; n++) {
                    int nr = neighbors[n][0];
                    int nc = neighbors[n][1];
                    if (nr < 0 || nr >= GRID_ROWS) continue;
                    if (nc < 0 || nc >= GRID_COLS) continue;

                    grid_cell *neighbor = &grid->cells[nr][nc];
                    for (int j = 0; j < neighbor->count; j++) {
                        thing *b = &pool->things[neighbor->entity_indices[j]];
                        checks++;

                        float dx = a->x - b->x;
                        float dy = a->y - b->y;
                        float dist_sq = dx * dx + dy * dy;
                        float min_dist = a->size + b->size;
                        if (dist_sq < min_dist * min_dist) {
                            collisions++;
                            resolve_collision(a, b);
                        }
                    }
                }
            }
        }
    }

    *out_collisions = collisions;
    return checks;
}
```

The critical detail here is the neighbor selection. We only check RIGHT, BELOW-LEFT, BELOW, and BELOW-RIGHT neighbors. This avoids double-counting: cell (3,4) checks against cell (3,5), but cell (3,5) does NOT check back against cell (3,4). Same trick as the naive `j = i + 1` pattern, applied to cells.

### Why this works

```
2000 entities distributed across 192 cells ≈ 10 entities per cell (average).

Within a cell:    10 × 9 / 2 = 45 pair checks
Cross 4 neighbors: 10 × 10 × 4 = 400 pair checks
Total per cell:   ~445 checks

192 cells × 445 = ~85,000 checks total

Compare to naive: 1,999,000 checks.
Reduction: ~96%.
```

In practice, entity distribution is not uniform. Some cells have 20 entities, some have 2. The actual check count depends on the simulation, but for 2000 randomly bouncing entities, you typically see 20,000--50,000 checks. That is a 40--100x improvement over naive.

> **Anton says:** "Don't over-engineer. Do the dumb thing first. Measure. If it's not fast enough, do the slightly less dumb thing." The uniform grid is the slightly less dumb thing. And it is usually enough.

### ASCII diagram: entity-to-cell mapping

```
World space (1024 × 768 pixels):

                    col 0     col 1     col 2     col 3
                  ┌─────────┬─────────┬─────────┬─────────┬───
         row 0   │  A   B   │    C    │         │   D     │
                  │         │         │         │         │
                  ├─────────┼─────────┼─────────┼─────────┼───
         row 1   │         │  E  F   │   G     │         │
                  │         │   H     │         │         │
                  ├─────────┼─────────┼─────────┼─────────┼───
         row 2   │    I    │         │  J  K   │   L     │
                  │         │         │         │         │
                  └─────────┴─────────┴─────────┴─────────┴───

Checking entity E (in cell [1,1]):
  - Same cell: check E vs F, E vs H                    → 2 checks
  - Neighbor [1,2] (right): check E vs G               → 1 check
  - Neighbor [2,0] (below-left): check E vs I          → 1 check
  - Neighbor [2,1] (below): check E vs (none)          → 0 checks
  - Neighbor [2,2] (below-right): check E vs J, E vs K → 2 checks
  Total for E: 6 checks

Naive would check E against ALL 11 other entities: 11 checks.
Grid cut it to 6. And those 6 are the only ones that COULD collide.
```

---

## Step 4 --- Spatial hash: grid without fixed bounds

The uniform grid has one limitation: it requires a fixed world size. You need to know `GRID_COLS` and `GRID_ROWS` at compile time, which means you need to know the world dimensions. For a bounded game window, that is fine. For an infinite world (think Minecraft, procedural terrain, space games), it breaks.

A **spatial hash** gives you the same performance as a grid but without fixed bounds. Instead of mapping position to a (row, col) cell, you hash the position into a bucket:

```
bucket_index = hash(floor(x / cell_size), floor(y / cell_size)) % NUM_BUCKETS
```

The hash converts any (x, y) position --- even negative coordinates, even coordinates far outside any predefined boundary --- into a bucket index between 0 and `NUM_BUCKETS - 1`.

### Data structure

```c
#define HASH_BUCKETS 256
#define MAX_ENTITIES_PER_BUCKET 32

typedef struct {
    int entity_indices[MAX_ENTITIES_PER_BUCKET];
    int count;
    int cell_x;  /* which logical cell this bucket represents (for debug) */
    int cell_y;
} hash_bucket;

typedef struct {
    hash_bucket buckets[HASH_BUCKETS];
} spatial_hash;
```

### The hash function

```c
static uint32_t hash_cell(int cell_x, int cell_y) {
    /* Simple hash: combine cell coordinates with prime multipliers */
    uint32_t h = (uint32_t)(cell_x * 92837111) ^ (uint32_t)(cell_y * 689287499);
    return h % HASH_BUCKETS;
}
```

This is not a cryptographic hash. It is a fast, cheap hash that distributes cells across buckets with reasonable uniformity. Two different cells CAN map to the same bucket (a collision in hash terminology --- confusing when we are also talking about entity collisions). When that happens, entities from different spatial regions end up in the same bucket and get checked against each other unnecessarily. This adds some wasted checks but does not produce incorrect results.

> **Why?** In JavaScript, `Map` gives you a hash table for free. In C, you build one yourself. The spatial hash is a hash table where the key is a grid cell coordinate and the value is a list of entity indices in that cell. We use open addressing with a fixed bucket array instead of chaining --- simpler, no allocation, cache-friendly.

> **JS bridge:** A spatial hash is exactly like a JavaScript `Map` where the key is a position-based string like `"3,4"` (the grid cell coordinates) and the value is an array of entity indices. In JS you might write: `const buckets = new Map(); const key = Math.floor(entity.x / cellSize) + "," + Math.floor(entity.y / cellSize); if (!buckets.has(key)) buckets.set(key, []); buckets.get(key).push(entityId);`. The C version does the same thing, but instead of string keys it uses an integer hash function, and instead of a dynamically growing `Map` it uses a fixed-size array of buckets. The hash function converts `(cellX, cellY)` into a bucket index, just like JS's `Map` internally hashes your string key into a bucket index. The difference is that we control the hash function and the bucket array size, which means we control the performance characteristics.

### Building and querying

The build phase is almost identical to the grid:

```c
static void spatial_hash_clear(spatial_hash *sh) {
    for (int i = 0; i < HASH_BUCKETS; i++) {
        sh->buckets[i].count = 0;
    }
}

static void spatial_hash_insert(spatial_hash *sh, thing *t, int entity_idx,
                                float cell_size) {
    int cell_x = (int)floorf(t->x / cell_size);
    int cell_y = (int)floorf(t->y / cell_size);
    uint32_t bucket_idx = hash_cell(cell_x, cell_y);

    hash_bucket *bucket = &sh->buckets[bucket_idx];
    if (bucket->count < MAX_ENTITIES_PER_BUCKET) {
        bucket->entity_indices[bucket->count] = entity_idx;
        bucket->count++;
    }
}
```

The query phase checks the entity's bucket plus the 8 neighboring cell buckets (hashed):

```c
static int collision_spatial_hash(spatial_hash *sh, thing_pool *pool,
                                  float cell_size, int *out_collisions) {
    int checks = 0;
    int collisions = 0;

    for (int i = 1; i < pool->next_empty; i++) {
        if (!pool->used[i]) continue;
        thing *a = &pool->things[i];

        int cell_x = (int)floorf(a->x / cell_size);
        int cell_y = (int)floorf(a->y / cell_size);

        /* Check 3x3 neighborhood of cells */
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                uint32_t bucket_idx = hash_cell(cell_x + dx, cell_y + dy);
                hash_bucket *bucket = &sh->buckets[bucket_idx];

                for (int j = 0; j < bucket->count; j++) {
                    int b_idx = bucket->entity_indices[j];
                    if (b_idx <= i) continue; /* avoid double-check */
                    thing *b = &pool->things[b_idx];

                    checks++;
                    float ex = a->x - b->x;
                    float ey = a->y - b->y;
                    float dist_sq = ex * ex + ey * ey;
                    float min_dist = a->size + b->size;
                    if (dist_sq < min_dist * min_dist) {
                        collisions++;
                        resolve_collision(a, b);
                    }
                }
            }
        }
    }

    *out_collisions = collisions;
    return checks;
}
```

Notice the `b_idx <= i` guard. Because we iterate all entities and each checks its 3x3 neighborhood, pair (A, B) would be checked once when processing A (B is in A's neighborhood) and once when processing B (A is in B's neighborhood). The index comparison prevents this double-counting.

### Grid vs hash: when to use which

| Feature | Uniform Grid | Spatial Hash |
|---|---|---|
| World bounds | Must be known at compile time | Unlimited --- any position works |
| Memory | Fixed 2D array, predictable size | Fixed bucket array, size independent of world |
| Hash collisions | None --- each cell is unique | Possible --- different cells can share a bucket |
| Debug visualization | Grid lines at fixed positions | Bucket occupancy (less intuitive) |
| Implementation complexity | Slightly simpler | Slightly more complex (hash function) |
| Performance | Identical for bounded worlds | Identical, possibly slightly worse due to hash collisions |

For a fixed-size game window (like this lab), the uniform grid is strictly better --- simpler, no hash collisions, easier to debug. The spatial hash shines in unbounded or very large worlds where a grid would be impractically large.

> **Common mistake:** Choosing `HASH_BUCKETS` too small. With 256 buckets and 192 logical cells, you get ~1.3 hash collisions on average. That is fine. With 256 buckets and 10,000 logical cells, you get ~39 cells per bucket --- your "spatial" hash has lost all spatial locality. Rule of thumb: `HASH_BUCKETS` should be at least as large as the number of occupied cells.

---

## Step 5 --- Quadtree: adaptive subdivision

The grid and hash both divide space uniformly. Every cell is the same size regardless of how many entities are in it. This works well when entities are roughly evenly distributed. But what if 1500 of your 2000 entities are clustered in one corner (think: a battle happening in one area while the rest of the map is empty)?

A uniform grid with 192 cells puts ~1500 entities in a handful of cells. Those cells degenerate to near-O(N²) internally. The rest of the grid is empty and wasted.

> **Why a quadtree?** A quadtree is recursive subdivision --- like binary search, but in 2D. Binary search splits a sorted array in half to find an element in O(log N) instead of O(N). A quadtree splits a 2D area into four quadrants to find nearby entities in O(log N) instead of O(N). Each split divides the problem by 4 instead of by 2. If you have ever used binary search on a sorted array in JavaScript (`while (lo < hi) { const mid = (lo + hi) >> 1; ... }`), a quadtree is the 2D generalization of that idea: instead of splitting a 1D range into left/right halves, you split a 2D area into four quadrants (top-left, top-right, bottom-left, bottom-right). Dense areas get subdivided further; sparse areas stay as large rectangles.

A **quadtree** adapts to density. It starts with a single rectangle covering the entire world. If that rectangle contains more than a threshold of entities (say, 8), it subdivides into 4 equal quadrants. Each quadrant that exceeds the threshold subdivides again. Dense areas get small cells. Sparse areas stay as large cells.

```
Quadtree subdivision (1500 entities clustered in top-left):

┌────────────────────────────────────────────────┐
│ ┌────────┬────────┐                             │
│ │ ┌──┬──┐│        │                             │
│ │ │··│· ││   ·    │                             │
│ │ │··│··││        │                             │
│ │ ├──┼──┤│        │              ·              │
│ │ │· │··││        │                             │
│ │ │··│· ││   ·    │                             │
│ │ └──┴──┘│        │        ·                    │
│ ├────────┼────────┤                             │
│ │  ·  ·  │   ·    │                  ·          │
│ │   ·    │        │                             │
│ └────────┴────────┘           ·                 │
│                                                 │
│         ·              ·              ·          │
│                                                 │
│    ·              ·                        ·    │
│                                                 │
└────────────────────────────────────────────────┘

Dense cluster (top-left): subdivided 3 times → small cells
Sparse area (rest): never subdivided → one large cell
```

### Data structure

```c
#define QUADTREE_MAX_DEPTH 6
#define QUADTREE_NODE_CAPACITY 8
#define QUADTREE_MAX_NODES 1024

typedef struct {
    float x, y, w, h;           /* bounding rectangle */
    int entity_indices[QUADTREE_NODE_CAPACITY];
    int count;
    int children[4];            /* indices into node array, 0 = no child */
    bool subdivided;
} quadtree_node;

typedef struct {
    quadtree_node nodes[QUADTREE_MAX_NODES];
    int node_count;
} quadtree;
```

Look at the structure. Quadtree nodes are stored in a flat array, just like our things pool. Children are referenced by integer index, not pointer. The nil value is 0 (index 0 is the root, but children default to 0 meaning "no child" --- we check `subdivided` first). This is the LOATs pattern applied to the quadtree itself.

> **Why?** In a textbook, quadtree nodes are heap-allocated with `malloc` and connected by pointers. That is garbage for cache performance and makes serialization impossible. Our quadtree is a flat array. Rebuilding it every frame is cheap because there is nothing to free --- just reset `node_count = 1`.

### Insert and subdivide

```c
static void quadtree_init(quadtree *qt, float x, float y, float w, float h) {
    qt->node_count = 1;
    qt->nodes[0].x = x;
    qt->nodes[0].y = y;
    qt->nodes[0].w = w;
    qt->nodes[0].h = h;
    qt->nodes[0].count = 0;
    qt->nodes[0].subdivided = false;
}

static void quadtree_subdivide(quadtree *qt, int node_idx) {
    quadtree_node *node = &qt->nodes[node_idx];
    float hw = node->w / 2.0f;
    float hh = node->h / 2.0f;
    float x = node->x;
    float y = node->y;

    for (int i = 0; i < 4; i++) {
        if (qt->node_count >= QUADTREE_MAX_NODES) return;
        int child_idx = qt->node_count++;
        node = &qt->nodes[node_idx]; /* re-fetch after potential resize */
        node->children[i] = child_idx;

        quadtree_node *child = &qt->nodes[child_idx];
        child->count = 0;
        child->subdivided = false;

        switch (i) {
            case 0: child->x = x;      child->y = y;      break; /* top-left */
            case 1: child->x = x + hw; child->y = y;      break; /* top-right */
            case 2: child->x = x;      child->y = y + hh; break; /* bottom-left */
            case 3: child->x = x + hw; child->y = y + hh; break; /* bottom-right */
        }
        child->w = hw;
        child->h = hh;
    }
    node->subdivided = true;
}

static void quadtree_insert(quadtree *qt, int node_idx, thing *t,
                            int entity_idx, int depth) {
    quadtree_node *node = &qt->nodes[node_idx];

    /* If not subdivided and has capacity, just add */
    if (!node->subdivided && node->count < QUADTREE_NODE_CAPACITY) {
        node->entity_indices[node->count] = entity_idx;
        node->count++;
        return;
    }

    /* Need to subdivide (if not already) */
    if (!node->subdivided) {
        if (depth >= QUADTREE_MAX_DEPTH) {
            /* Max depth reached, just overflow this node */
            if (node->count < QUADTREE_NODE_CAPACITY) {
                node->entity_indices[node->count] = entity_idx;
                node->count++;
            }
            return;
        }
        quadtree_subdivide(qt, node_idx);
        /* Re-insert existing entities into children */
        node = &qt->nodes[node_idx]; /* re-fetch */
        for (int i = 0; i < node->count; i++) {
            int existing = node->entity_indices[i];
            thing *et = &qt->nodes[0].entity_indices[0]; /* dummy --- see below */
            /* We need the pool to re-insert. In practice, pass pool to insert. */
        }
        node->count = 0;
    }

    /* Find which child quadrant contains this entity */
    float mid_x = node->x + node->w / 2.0f;
    float mid_y = node->y + node->h / 2.0f;
    int quadrant;
    if (t->x < mid_x) {
        quadrant = (t->y < mid_y) ? 0 : 2;
    } else {
        quadrant = (t->y < mid_y) ? 1 : 3;
    }
    quadtree_insert(qt, node->children[quadrant], t, entity_idx, depth + 1);
}
```

> **Course Note:** The re-insertion of existing entities during subdivision is simplified here. In the actual source code (`lesson_16.c`), the `quadtree_insert` function takes the pool as a parameter so it can look up entity positions for re-insertion. The pattern is: subdivide, move all existing entities from parent to appropriate children, then insert the new entity. This keeps the code educational without hiding the mechanics.

### Querying the quadtree

For collision, you query which entities are in the same leaf node as a given entity, plus entities in the neighboring region. The simplest approach: for each entity, find its leaf node and check all entities in that leaf, then recurse up and check sibling nodes.

A simpler (and sufficient for the lab) approach: walk the tree. At each leaf node, check all pairs within that node:

```c
static int collision_quadtree_node(quadtree *qt, int node_idx,
                                   thing_pool *pool, int *collisions) {
    quadtree_node *node = &qt->nodes[node_idx];
    int checks = 0;

    if (node->subdivided) {
        /* Recurse into children */
        for (int i = 0; i < 4; i++) {
            checks += collision_quadtree_node(qt, node->children[i],
                                              pool, collisions);
        }
        /* Also check cross-child pairs (entities near cell boundaries) */
        /* ... simplified: check all pairs in all children against each other */
    } else {
        /* Leaf node: check all pairs within */
        for (int i = 0; i < node->count; i++) {
            thing *a = &pool->things[node->entity_indices[i]];
            for (int j = i + 1; j < node->count; j++) {
                thing *b = &pool->things[node->entity_indices[j]];
                checks++;
                float dx = a->x - b->x;
                float dy = a->y - b->y;
                float dist_sq = dx * dx + dy * dy;
                float min_dist = a->size + b->size;
                if (dist_sq < min_dist * min_dist) {
                    (*collisions)++;
                    resolve_collision(a, b);
                }
            }
        }
    }
    return checks;
}
```

### Quadtree trade-offs

| Advantage | Disadvantage |
|---|---|
| Adapts to density --- sparse areas use large nodes | More complex to implement than grid |
| Works well for highly clustered distributions | Tree traversal has pointer-chasing overhead (even with flat array) |
| Memory usage scales with entity count, not world size | Rebuild cost is O(N log N) vs O(N) for grid |
| Basis for more advanced structures (octree for 3D, BVH) | Cross-boundary collisions require careful handling |

For 2000 uniformly distributed entities, the quadtree performs similarly to the grid. Its advantage appears when distribution is uneven --- which is the common case in real games (battles cluster, empty areas are empty).

> **Alternative approach:** A **loose quadtree** expands each node's bounds by a margin, so entities near boundaries are always contained in a single node. This eliminates the cross-boundary problem at the cost of more overlap between sibling nodes. For the lab, the tight quadtree is clearer to understand.

---

## Step 6 --- Runtime toggle and identical behavior

All four modes --- Naive, Grid, Hash, Quadtree --- must produce the same collision results and the same visual output. The only difference is performance. This is the experimental control. If switching modes changed what you see, you could not isolate the algorithmic impact from behavioral changes.

### The mode enum and toggle

```c
typedef enum {
    PARTITION_NAIVE = 0,
    PARTITION_GRID,
    PARTITION_HASH,
    PARTITION_QUADTREE,
    PARTITION_COUNT
} partition_mode;

static const char *partition_mode_names[] = {
    "Naive O(N²)",
    "Uniform Grid",
    "Spatial Hash",
    "Quadtree"
};
```

The `Tab` key advances the mode:

```c
if (input->tab_pressed) {
    state->mode = (state->mode + 1) % PARTITION_COUNT;
}
```

No restart. No reinitialization. The entities keep moving, the simulation keeps running, only the collision query strategy changes. The mode switch happens between frames --- the next `game_update` call uses the new mode.

### The dispatch (explicit, not hidden)

```c
static void scene_spatial_partition_update(spatial_partition_state *state,
                                           thing_pool *pool, float dt) {
    /* Move all entities (same regardless of mode) */
    move_all_entities(pool, dt);

    /* Collision detection (mode-dependent) */
    int checks = 0;
    int collisions = 0;

    switch (state->mode) {
        case PARTITION_NAIVE:
            checks = collision_naive(pool, &collisions);
            break;
        case PARTITION_GRID:
            grid_build(&state->grid, pool);
            checks = collision_grid(&state->grid, pool, &collisions);
            break;
        case PARTITION_HASH:
            spatial_hash_clear(&state->hash);
            spatial_hash_build(&state->hash, pool, HASH_CELL_SIZE);
            checks = collision_spatial_hash(&state->hash, pool,
                                            HASH_CELL_SIZE, &collisions);
            break;
        case PARTITION_QUADTREE:
            quadtree_init(&state->qt, 0, 0, WORLD_W, WORLD_H);
            quadtree_build(&state->qt, pool);
            checks = collision_quadtree(&state->qt, pool, &collisions);
            break;
        default:
            break;
    }

    state->last_checks = checks;
    state->last_collisions = collisions;
}
```

A raw `switch`. Four cases. Each calls a different collision function. No strategy pattern, no vtable, no function pointers, no abstraction. You can read the code and know exactly what happens in each mode. This is what the GLOBAL_STYLE_GUIDE means by "the code IS the algorithm."

> **Why no abstraction?** Because abstraction hides the thing we are trying to teach. If you had a `CollisionStrategy` interface with a `check()` method, the student would see `strategy->check(pool)` and not know what is happening underneath. The explicit switch with four visible code paths is the point.

---

## Step 7 --- Visual debug overlay

The HUD numbers tell you the performance story. The debug overlay tells you the spatial story. When you see grid lines on screen, you understand viscerally why entities in the same cell get checked and entities in distant cells do not.

### Grid overlay

```c
static void draw_grid_overlay(uniform_grid *grid, platform_backbuffer *bb) {
    /* Draw vertical grid lines */
    for (int c = 1; c < GRID_COLS; c++) {
        int x = c * GRID_CELL_SIZE;
        draw_line(bb, x, 0, x, bb->height, 0x333333);
    }
    /* Draw horizontal grid lines */
    for (int r = 1; r < GRID_ROWS; r++) {
        int y = r * GRID_CELL_SIZE;
        draw_line(bb, 0, y, bb->width, y, 0x333333);
    }
    /* Highlight cells that contain entities */
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (grid->cells[r][c].count > 0) {
                int x = c * GRID_CELL_SIZE;
                int y = r * GRID_CELL_SIZE;
                /* Tint occupied cells --- brighter = more entities */
                uint8_t alpha = (uint8_t)(grid->cells[r][c].count * 15);
                if (alpha > 100) alpha = 100;
                draw_rect_alpha(bb, x, y, GRID_CELL_SIZE, GRID_CELL_SIZE,
                               0x004400, alpha);
            }
        }
    }
}
```

Occupied cells glow green. Brighter green means more entities. Empty cells stay dark. You can literally see the spatial distribution of your entities and understand why each cell only checks a handful of pairs.

### Hash overlay

The hash overlay is less intuitive because buckets do not correspond to fixed screen positions. Instead, draw a small indicator panel showing bucket occupancy:

```c
static void draw_hash_overlay(spatial_hash *sh, platform_backbuffer *bb) {
    /* Draw a 16x16 grid of bucket occupancy in the corner */
    int panel_x = bb->width - 170;
    int panel_y = 10;
    int cell_w = 10;
    int cell_h = 10;

    for (int i = 0; i < HASH_BUCKETS; i++) {
        int row = i / 16;
        int col = i % 16;
        int x = panel_x + col * cell_w;
        int y = panel_y + row * cell_h;

        if (sh->buckets[i].count > 0) {
            uint8_t brightness = (uint8_t)(sh->buckets[i].count * 20);
            if (brightness > 200) brightness = 200;
            draw_rect(bb, x, y, cell_w - 1, cell_h - 1,
                     (brightness << 16) | 0x002200);
        } else {
            draw_rect(bb, x, y, cell_w - 1, cell_h - 1, 0x111111);
        }
    }
}
```

### Quadtree overlay

The quadtree overlay is the most visually informative. Draw the subdivision rectangles recursively:

```c
static void draw_quadtree_overlay(quadtree *qt, int node_idx,
                                  platform_backbuffer *bb) {
    quadtree_node *node = &qt->nodes[node_idx];

    /* Draw this node's bounds */
    uint32_t color = node->subdivided ? 0x333333 : 0x004400;
    draw_rect_outline(bb, (int)node->x, (int)node->y,
                      (int)node->w, (int)node->h, color);

    /* If leaf with entities, tint it */
    if (!node->subdivided && node->count > 0) {
        uint8_t alpha = (uint8_t)(node->count * 15);
        if (alpha > 80) alpha = 80;
        draw_rect_alpha(bb, (int)node->x, (int)node->y,
                       (int)node->w, (int)node->h, 0x004400, alpha);
    }

    /* Recurse into children */
    if (node->subdivided) {
        for (int i = 0; i < 4; i++) {
            draw_quadtree_overlay(qt, node->children[i], bb);
        }
    }
}
```

When you run the quadtree mode, you see large rectangles in sparse areas and tiny subdivided rectangles where entities cluster. The visual matches the data structure. Dense regions get more computational attention. Sparse regions get less.

---

## Step 8 --- Performance observability

The HUD displays five metrics, updated every frame:

```c
static void draw_spatial_hud(platform_backbuffer *bb,
                             spatial_partition_state *state) {
    char buf[256];

    snprintf(buf, sizeof(buf), "Mode: %s",
             partition_mode_names[state->mode]);
    draw_text(bb, 10, 10, buf, 0xFFFFFF);

    snprintf(buf, sizeof(buf), "Entities: %d", state->entity_count);
    draw_text(bb, 10, 30, buf, 0xCCCCCC);

    snprintf(buf, sizeof(buf), "Checks: %d", state->last_checks);
    draw_text(bb, 10, 50, buf,
              state->last_checks > 500000 ? 0xFF4444 : 0x44FF44);

    snprintf(buf, sizeof(buf), "Collisions: %d", state->last_collisions);
    draw_text(bb, 10, 70, buf, 0xCCCCCC);

    snprintf(buf, sizeof(buf), "Frame: %.2f ms", state->last_frame_ms);
    draw_text(bb, 10, 90, buf,
              state->last_frame_ms > 16.0f ? 0xFF4444 : 0x44FF44);
}
```

The check count and frame time are color-coded:
- **Green** = healthy (checks < 500K, frame < 16ms)
- **Red** = danger (checks > 500K or frame > 16ms, meaning you are dropping below 60 FPS)

When you switch from Naive to Grid, the check count goes from red (~2M) to green (~30K) and the frame time drops from red (~25ms) to green (~2ms). That color change is the emotional punchline of the entire lab.

### What to observe when running

| Mode | Expected checks | Expected frame time | What you see |
|---|---|---|---|
| Naive | ~1,999,000 | 15--30 ms (stuttering) | All entities checked, no overlay |
| Grid | ~20,000--50,000 | 1--3 ms | Grid lines, occupied cells highlighted |
| Hash | ~25,000--60,000 | 1--4 ms | Bucket occupancy panel in corner |
| Quadtree | ~15,000--40,000 | 2--5 ms | Recursive subdivision boxes, density-adaptive |

The quadtree may have slightly higher overhead than the grid despite fewer checks, because tree traversal involves more branching and pointer-chasing (even with our flat array). The grid's simplicity and cache-friendliness often win in practice. This is a valuable insight: fewer algorithmic operations does not always mean faster wall-clock time.

> **Anton says:** "Keep it simple. The dumb thing that's cache-friendly often beats the smart thing that thrashes cache." The grid is the dumb thing. The quadtree is the smart thing. Sometimes the dumb thing wins. Measure, don't assume.

---

## Step 9 --- Building and running

Build the lab:

```bash
cd course/
./build-dev.sh --backend=raylib -r
```

The Spatial Partition Lab is always compiled in --- no separate build flag needed. Press `G` to enter the scene.

### Controls

| Key | Action |
|---|---|
| G | Enter Spatial Partition Lab |
| Tab | Cycle partition mode (Naive → Grid → Hash → Quadtree) |
| R | Reset scene (re-spawn all entities) |

### What to do

1. **Press G.** The screen fills with 2000 bouncing entities. The HUD shows Naive mode. Watch the check count: ~2,000,000. Watch the frame time: likely above 16ms. The simulation may stutter visibly.

2. **Press Tab.** Grid mode. The check count drops 50--100x. Frame time drops below 5ms. Grid lines appear. Occupied cells glow green. The simulation runs smoothly.

3. **Press Tab again.** Hash mode. Similar check count. The grid lines disappear; a bucket occupancy panel appears in the corner. Performance is comparable to grid.

4. **Press Tab once more.** Quadtree mode. Subdivision boxes appear on screen. Dense clusters show small boxes; sparse areas show large boxes. Check count may be lower than grid, but frame time may be slightly higher due to tree overhead.

5. **Press Tab to return to Naive.** Watch the stutter return. The contrast is the lesson.

### Full compile command (if building manually)

```bash
gcc -std=c11 -Wall -Wextra -Werror \
    -o spatial_partition_lab \
    src/lessons/lesson_16.c \
    -lraylib -lm
```

---

## Connection to your work

This lab connects forward in several directions.

Every physics engine in existence --- Box2D, Bullet, PhysX, Havok --- starts with a **broadphase** that does exactly what we just built. The broadphase quickly eliminates entity pairs that cannot possibly collide (they are too far apart). Then a **narrowphase** does precise collision detection on the remaining pairs. Our grid/hash/quadtree IS a broadphase.

> **Handmade Hero connection:** Casey discusses spatial partitioning when entity counts grow beyond trivial. The same flat-array + integer-index philosophy applies to the partition data structures themselves. Our quadtree stores nodes in a flat array indexed by integers --- not heap-allocated nodes connected by pointers. This is the LOATs pattern applied recursively.

Cross-references for further study:

- **GPP Ch 20 --- Spatial Partition:** Bob Nystrom's pattern description. Covers grids and spatial hashing with game examples.
- **GEA Ch 12 --- Collision Detection:** Jason Gregory's engine architecture treatment. Covers broadphase/narrowphase pipeline, sweep-and-prune, bounding volume hierarchies (BVH).
- **Game Physics Engine Development (Millington):** Full broadphase implementations with octrees and spatial hashing.

> **Anton says:** "The whole point of this architecture is that your data is simple. Simple data means simple algorithms. Simple algorithms mean you can actually understand what's happening and fix it when it's slow." The Spatial Partition Lab is proof: four simple algorithms, each a few dozen lines of C, no abstraction, no frameworks. You can read every line and understand exactly why it is fast or slow.

The key takeaway: performance at scale is not about micro-optimization. It is not about SIMD, or cache line alignment tricks, or compiler flags. It is about **not doing work you do not need to do**. The naive mode does 2 million checks. The grid mode does 30,000. That is a 60x improvement achieved not by making each check faster, but by eliminating 97% of the checks entirely. Algorithmic improvement beats micro-optimization every single time.

---

## What's next

You have now completed all three advanced laboratories:

- **Lesson 14 --- Particle Lab** proved the pool works at 1000 entities, 60 FPS, zero allocation.
- **Lesson 15 --- Data Layout Lab** proved that how you organize data in memory changes how fast the same code runs.
- **Lesson 16 --- Spatial Partition Lab** proved that how you query data determines whether your simulation scales.

Together, these three labs cover the three pillars of engine performance: **data structure** (the pool), **data layout** (cache locality), and **data access pattern** (spatial queries). Everything else --- SIMD, multithreading, GPU compute --- builds on top of these fundamentals.

You now have the vocabulary, the mental models, and the hands-on experience to understand what happens when Handmade Hero, Game Programming Patterns, or Game Engine Architecture discusses entity systems, data locality, or spatial partitioning. You have built it. You have measured it. You own it.
