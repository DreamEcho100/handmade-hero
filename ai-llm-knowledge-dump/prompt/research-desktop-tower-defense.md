# Desktop Tower Defense — Game Research Document

> **Scope:** This document covers the mechanics, behavior, systems, math, and design of the Desktop Tower Defense Flash game. It does **not** cover how to build a course from it — that is handled by `course-builder.md` and a separate `build-dtd-course.md`. Use this document as the authoritative reference for what DTD actually does and how it works internally.

---

## 1. Game Identification

| Field                    | Value                                                                                  |
| ------------------------ | -------------------------------------------------------------------------------------- |
| **Original title**       | Desktop Tower Defense                                                                  |
| **Developer**            | Paul Preece                                                                            |
| **Original release**     | March 2007                                                                             |
| **Platforms**            | Browser (Flash), later hosted independently at `dtd.paulpreece.com`                    |
| **Flash version**        | ActionScript 2 / Flash Player 8                                                        |
| **Original host**        | Games portal (viral spread), Paul Preece's own site                                    |
| **Notable sequels**      | Desktop Tower Defense 1.5 (added new modes), DTD Pro, various unofficial HTML5 remakes |
| **Spiritual successors** | Gemcraft, Bloons TD (contemporary), Kingdom Rush, every modern browser TD              |
| **Estimated FPS**        | 30 FPS (Flash default)                                                                 |
| **Canvas size**          | ~600 × 450 pixels visible play area                                                    |

### Source material

| Type                     | Where                                             | What to extract                                 |
| ------------------------ | ------------------------------------------------- | ----------------------------------------------- |
| Gameplay footage (1.5)   | YouTube: "Desktop Tower Defense 100 waves"        | Wave timing, creep HP scaling, tower targeting  |
| Speedruns / no-loss runs | YouTube                                           | Min-cost maze layouts, prove pathfinding rules  |
| Wiki                     | `dtd.wikia.com` (archived) / Wayback Machine      | Tower stats, creep HP tables, wave order        |
| Original SWF             | `flashpoint.bluemaxima.org` (Flashpoint archive)  | Asset inspection (legally downloaded for study) |
| Forum discussions        | Kongregate comments, original portal threads      | Edge case mechanics, hidden rules               |
| Remake source            | `github.com` search "desktop tower defense html5" | Community reverse-engineered implementations    |
| Paul Preece blog posts   | Wayback Machine `paulpreece.com`                  | Designer intent, known bugs                     |

### Legal note

DTD is a Flash game from 2007. The Flash format is effectively deprecated; Flashpoint hosts it for preservation purposes. No assets are extracted for course use — all graphics are drawn procedurally.

---

## 2. High-Level Description

Desktop Tower Defense is a **grid-based tower placement game** where enemies ("creeps") travel from an entry cell to an exit cell. Unlike most tower defense games, DTD has **no pre-defined path** — creeps use real-time **BFS pathfinding** to navigate around whatever towers the player places. The player cannot fully block the path; the game enforces a legal path check on every tower placement. This single design decision (dynamic pathfinding instead of a fixed path) is what distinguishes DTD from Bloons TD and most other Flash TDs.

**Victory condition:** Survive all 40 (Easy/Medium/Hard) or 100 (Challenge mode) waves without losing all lives.

**Failure condition:** Losing all lives (creeps reach the exit; each one subtracts lives equal to the creep's "lives cost").

---

## 3. Grid System

### Grid dimensions

| Field        | Value                                             |
| ------------ | ------------------------------------------------- |
| Grid columns | 30                                                |
| Grid rows    | 20                                                |
| Cell size    | ~20 × 20 pixels (approximate; varies by version)  |
| Total cells  | 600                                               |
| Entry cell   | Column 0, Row 9–10 (left edge, vertical centre)   |
| Exit cell    | Column 29, Row 9–10 (right edge, vertical centre) |

> In some versions entry is top-centre and exit is bottom-centre. The exact positions vary by mode. The important invariant is: **one entry cell and one exit cell on opposite edges**.

### Cell states

Each grid cell is in exactly one of these states:

```
CELL_EMPTY     — passable; available for tower placement
CELL_TOWER     — blocked by a tower; impassable to ground creeps
CELL_ENTRY     — hardcoded spawn point; cannot be built on
CELL_EXIT      — hardcoded goal; cannot be built on
CELL_PATH      — (debug/render only) cell is on the current shortest BFS path
```

### Coordinate system

- Origin `(0,0)` = top-left cell
- `col` increases rightward, `row` increases downward
- Cell centre in pixels: `(col * CELL_W + CELL_W/2, row * CELL_H + CELL_H/2)`
- **Y-down** — this is a grid game; the cellular coordinate system is Y-down by definition

### Flat array indexing

The grid is stored as a flat array:

```c
#define GRID_COLS  30
#define GRID_ROWS  20
uint8_t grid[GRID_ROWS * GRID_COLS];  /* cell state */
int     dist[GRID_ROWS * GRID_COLS];  /* BFS distance from exit (filled each frame or on placement) */

/* Access: */
int idx = row * GRID_COLS + col;
```

---

## 4. Pathfinding

This is the central technical system of DTD. **Everything else is simpler than this.**

### Algorithm: BFS distance field (flow field)

DTD does **not** use A\*. It uses a backwards BFS starting from the **exit cell** that fills every reachable empty cell with a distance value. Creeps then move greedily toward the neighbour with the smallest distance.

This approach is called a **flow field** (or distance map / influence map). Its key advantage: one BFS computation serves all creeps simultaneously. A\* would need to run per creep.

```c
/* Pseudocode — run after every tower placement */
void bfs_fill_distance(int grid[], int dist[], int rows, int cols, int exit_col, int exit_row) {
    /* Fill all with -1 (unreachable) */
    memset(dist, -1, rows * cols * sizeof(int));

    /* Queue: standard BFS */
    int queue[rows * cols];
    int head = 0, tail = 0;

    int exit_idx = exit_row * cols + exit_col;
    dist[exit_idx] = 0;
    queue[tail++] = exit_idx;

    int dx[] = { 1, -1, 0,  0 };
    int dy[] = { 0,  0, 1, -1 };

    while (head < tail) {
        int idx = queue[head++];
        int col = idx % cols;
        int row = idx / cols;

        for (int d = 0; d < 4; d++) {
            int nc = col + dx[d];
            int nr = row + dy[d];
            if (nc < 0 || nc >= cols || nr < 0 || nr >= rows) continue;
            int nidx = nr * cols + nc;
            if (dist[nidx] != -1) continue;          /* already visited */
            if (grid[nidx] == CELL_TOWER) continue;  /* blocked */
            dist[nidx] = dist[idx] + 1;
            queue[tail++] = nidx;
        }
    }
}
```

**Complexity:** O(rows × cols) = O(600) per placement. Negligible at 30 fps even if run every frame.

### Creep movement using the distance field

Each creep moves toward the adjacent cell with the smallest `dist` value:

```c
/* Every creep tick: pick next cell */
int best_dist = INT_MAX;
int best_col = creep.col, best_row = creep.row;

for (int d = 0; d < 4; d++) {
    int nc = creep.col + dx[d];
    int nr = creep.row + dy[d];
    if (out_of_bounds(nc, nr)) continue;
    int nidx = nr * GRID_COLS + nc;
    if (dist[nidx] >= 0 && dist[nidx] < best_dist) {
        best_dist = dist[nidx];
        best_col = nc;
        best_row = nr;
    }
}
```

Creeps move smoothly at float sub-cell positions; the cell they "target" is determined by the integer distance field. Movement is linear interpolation between cell centres.

### Tower placement legality check

**A tower placement is ILLEGAL if placing it would make `dist[entry_idx] == -1` after the BFS.** The game prevents this:

```c
int can_place_tower(int col, int row) {
    if (grid[row * GRID_COLS + col] != CELL_EMPTY) return 0;

    /* Temporarily mark as tower */
    grid[row * GRID_COLS + col] = CELL_TOWER;
    bfs_fill_distance(grid, dist, GRID_ROWS, GRID_COLS, exit_col, exit_row);
    int legal = (dist[entry_row * GRID_COLS + entry_col] != -1);

    /* Undo */
    grid[row * GRID_COLS + col] = CELL_EMPTY;

    /* If legal, keep the BFS result; otherwise re-run the clean BFS */
    if (!legal) bfs_fill_distance(grid, dist, GRID_ROWS, GRID_COLS, exit_col, exit_row);

    return legal;
}
```

This is a double BFS per click at worst — still under 1 µs.

### Flying creep pathfinding

Flying creeps ignore the grid entirely. They move in a **straight line** from entry to exit (or directly toward the exit point). They are immune to maze designs. Towers still shoot at them normally; they cannot be blocked by tower placement.

```c
/* Flying creep target: direct vector toward exit centre */
float dx = exit_cx - creep.x;
float dy = exit_cy - creep.y;
float len = sqrtf(dx*dx + dy*dy);
creep.vx = (dx / len) * creep.speed;
creep.vy = (dy / len) * creep.speed;
```

### Tie-breaking (when multiple neighbours have equal distance)

In the original DTD, tie-breaking is **consistent** — the same path is chosen every time for the same configuration. One common approach: prefer the first direction in a fixed priority order (e.g., right → down → left → up). This matters for determinism: two creeps in the same cell at the same time must take identical paths.

---

## 5. Tower Catalog

All stats below are from the DTD 1.5 version. "DPS" = damage per second = `damage × fire_rate`.

### Tower stats table

| Tower               | Cost | Damage      | Attack Range (cells) | Fire Rate (shots/s) | DPS     | Special                          | Upgrade path |
| ------------------- | ---- | ----------- | -------------------- | ------------------- | ------- | -------------------------------- | ------------ |
| **Pellet**          | 5    | 3           | 3                    | 2.5                 | 7.5     | None                             | Squirt       |
| **Squirt**          | 15   | 10          | 3.5                  | 1.5                 | 15      | None                             | Dart         |
| **Dart**            | 40   | 35          | 4                    | 1.0                 | 35      | None                             | None         |
| **Snap**            | 75   | 125         | 4                    | 0.4                 | 50      | None                             | None         |
| **Swarm**           | 125  | 15 splash   | 4                    | 0.7                 | ~45 AoE | Splash radius ≈ 1.5 cells        | None         |
| **Frost**           | 60   | 0           | 3.5                  | 2.0                 | 0       | Slows to 50% speed; cannot stack | None         |
| **Bash**            | 90   | 40          | 3                    | 0.5                 | 20      | Stuns target for 0.5 s           | None         |
| **Flying Fortress** | 200  | 50 × 4 jets | 5                    | 1.0                 | 200     | Multi-target; orbiting jets      | Upgrade-only |

> ⚠ All stat values are approximations from community wikis and video analysis. Treat as representative, not pixel-perfect. The course implementation should use these as a starting point and tune for feel.

### Targeting modes

Each tower has a targeting priority the player can cycle through:

| Mode          | Behaviour                                                      |
| ------------- | -------------------------------------------------------------- |
| **First**     | Target the creep closest to the exit (lowest `dist` value)     |
| **Last**      | Target the creep farthest from the exit (highest `dist` value) |
| **Strongest** | Target the creep with the most current HP                      |
| **Weakest**   | Target the creep with the least current HP                     |
| **Closest**   | Target the creep closest to the tower (Euclidean distance)     |

Default is **First**. Implementing all 5 requires no extra data structure — just iterate the creep pool and pick based on the chosen comparator.

### Tower visual design

Each tower is rendered as a simple geometric shape on the grid cell — no sprite sheets:

- **Pellet:** small grey square, single barrel (line) pointing at target
- **Squirt:** blue-tinted square, thicker barrel
- **Dart:** dark square, elongated barrel
- **Snap:** red/dark square, thick barrel with muzzle flash on fire
- **Swarm:** orange square, 4 small barrels radiating outward
- **Frost:** light blue square with snowflake-like radiating lines
- **Bash:** brown/tan square, stubby hammer-like barrel
- **Flying Fortress:** circular base, 4 small jet sprites orbiting the centre

All towers rotate their barrel (or jets) to face the current target. Rotation is `atan2f(target_y - tower_y, target_x - tower_x)`.

### Tower projectiles

Each tower fires one projectile per shot (Swarm fires towards its target but deals splash on impact; Flying Fortress fires 4 simultaneously from jet positions):

```c
typedef struct {
    float x, y;         /* current position (pixels, float) */
    float vx, vy;       /* velocity (pixels/second)          */
    int   damage;
    float splash_radius; /* 0 = single target                */
    int   target_id;    /* which creep we're tracking        */
    int   active;
} Projectile;
```

Projectiles home toward their target's position at fire time; DTD does **not** do predictive leading. If the target dies before the projectile lands, the projectile either disappears or continues to the last known position (version-dependent — either is acceptable).

Projectile speed is fast enough that homing vs ballistic is visually indistinguishable in practice.

### Upgrade system

Some towers upgrade into better versions by selling the old tower and buying the new one **on the same cell** for a discounted price. There is no in-place upgrade UI in the original; the player sells for ~70% of purchase cost and places the new tower.

DTD 1.5 added an explicit upgrade button for some towers that converts in-place. For a course implementation, selling + replacing is simpler and teaches sell mechanics cleanly.

---

## 6. Creep Catalog

### Creep types

| Type            | Visual           | HP (Wave 1) | Speed (cells/s) | Lives cost | Special                                       |
| --------------- | ---------------- | ----------- | --------------- | ---------- | --------------------------------------------- |
| **Normal**      | Grey square      | 100         | 3.0             | 1          | None                                          |
| **Fast**        | Yellow/orange    | 70          | 6.0             | 1          | Double speed                                  |
| **Flying**      | White with wings | 80          | 4.0             | 1          | Ignores grid; flies straight                  |
| **Armoured**    | Dark grey        | 200         | 2.5             | 1          | 50% damage reduction from most towers         |
| **Spawn**       | Green, medium    | 150         | 3.0             | 1          | Splits into 4 small "spawn children" on death |
| **Spawn child** | Green, small     | 30          | 3.5             | 0          | Does not split further                        |
| **Boss**        | Large red        | 1000–5000+  | 2.0             | 5–20       | Does not spawn from pool; 1 per boss wave     |
| **Group**       | Purple           | 40          | 4.0             | 1          | Arrives in groups of 10–20 closely spaced     |

### HP scaling by wave

HP scales roughly exponentially. Observed scaling factor ≈ 1.07–1.12× per wave:

```
wave_hp(base, wave) = base * pow(1.10, wave - 1)
```

Approximations from video analysis (Normal creep):

| Wave | Approx HP |
| ---- | --------- |
| 1    | 100       |
| 5    | 150       |
| 10   | 260       |
| 20   | 670       |
| 30   | 1,745     |
| 40   | 4,530     |
| 50   | 11,740    |
| 100  | ~137,000  |

### Creep movement details

- Movement is smooth floating-point between cell centres
- Creeps move at constant speed (no acceleration/deceleration)
- Each creep samples the BFS distance field at its **current integer cell** each frame to determine its next target cell
- Creeps at the exact cell centre switch target to the next cell immediately
- When two creeps are at the same cell, each moves independently (no flocking or separation — they can overlap perfectly)

### Spawn creep split mechanics

When a Spawn creep dies, it spawns 4 Spawn Children at its death position with a small random velocity offset. Children use the same BFS distance field as all other creeps. Children contribute to the lives cost only if they reach the exit (unlike the parent which already cost 1 life in some versions — treat parent and children as independent).

### Armoured creep damage reduction

Armoured creeps take **50% damage** from Pellet, Squirt, Dart, and Snap. They take **full damage** from Swarm (splash type bypasses armour in DTD). Frost and Bash apply normally regardless of armour.

```c
int effective_damage(int raw_damage, CreepType type, TowerType tower) {
    if (type == CREEP_ARMOURED) {
        /* Swarm (splash) bypasses armour */
        if (tower != TOWER_SWARM) return raw_damage / 2;
    }
    return raw_damage;
}
```

---

## 7. Wave System

### Wave structure

Each wave is a timed spawn sequence. A wave:

1. Has a **type** (e.g., Normal, Fast, Flying, Armoured, Boss)
2. Has a **count** (e.g., 20 normal creeps)
3. Has a **spawn interval** (seconds between each creep in the wave)
4. Has an **HP multiplier** derived from the wave number

### Wave order (DTD 40-wave mode, standard)

Approximate wave order from video analysis. Exact wave data varies by mode (Easy/Medium/Hard/Challenge):

| Wave  | Type                 | Count  | Notes                   |
| ----- | -------------------- | ------ | ----------------------- |
| 1–3   | Normal               | 10–15  | Tutorial                |
| 4     | Fast                 | 15     | First speed challenge   |
| 5–6   | Normal               | 20     | More HP                 |
| 7     | Flying               | 10     | First flying wave       |
| 8     | Normal               | 25     |                         |
| 9     | Armoured             | 15     | First armour challenge  |
| 10    | Boss                 | 1      | First boss              |
| 11–12 | Normal + Fast        | mixed  |                         |
| 13    | Spawn                | 10     | First spawn wave        |
| 14    | Flying               | 20     | More flyers             |
| 15    | Boss                 | 1      | Harder boss             |
| 16–19 | Mixed progressive    |        |                         |
| 20    | Boss + Normal        | 1 + 20 |                         |
| 21–24 | Fast + Flying mix    |        |                         |
| 25    | Boss (×2)            | 2      | Two simultaneous bosses |
| 26–29 | Armoured + Spawn mix |        |                         |
| 30    | Boss                 | 1      | Level 3 boss            |
| 31–39 | All types mixed      |        |                         |
| 40    | Final boss           | 1      | Highest HP boss         |

### Sending waves early

The player can click "Send Next Wave" before the current wave is fully cleared. Reward: **gold bonus** (~5–20 gold, scales with how early the wave is sent). This creates a risk/reward decision that is central to high-score play.

```c
/* Early send reward formula (approximation): */
int early_gold_bonus = (int)(remaining_wave_time * GOLD_PER_SECOND_EARLY);
/* typical: 5–10 gold for a couple seconds early, 20–40 for a full wave early */
```

### Wave timer and overlap

Waves can overlap — a boss from wave 25 can still be alive when wave 26 starts. The game does not pause or wait; both waves are active simultaneously. This is handled by having no "wait for wave clear" logic — the wave timer simply expires and the next wave begins regardless.

---

## 8. Economy System

### Gold income

| Source                    | Amount                         |
| ------------------------- | ------------------------------ |
| Kill Normal creep         | 1–5 gold (scales with wave HP) |
| Kill Fast creep           | 1–3 gold                       |
| Kill Flying creep         | 2–4 gold                       |
| Kill Armoured creep       | 2–6 gold                       |
| Kill Spawn creep (parent) | 3 gold                         |
| Kill Spawn child          | 1 gold                         |
| Kill Boss                 | 10–50 gold (scales)            |
| Wave completion bonus     | 5 gold                         |
| Early send bonus          | 5–40 gold                      |
| Starting gold             | 50 gold                        |

Exact values from wiki (approximation): kill reward ≈ `ceil(base_gold * pow(1.05, wave - 1))`. The key design principle is that **you earn roughly enough gold per wave to afford 1–2 new towers**, forcing constant economic decisions.

### Selling towers

Towers sell for **70% of their purchase price** (a common TD convention). There is no sell penalty beyond the 30% loss. Players regularly sell cheap early towers to buy better ones as gold income increases.

```c
int sell_value(int purchase_cost) {
    return (int)(purchase_cost * 0.70f);
}
```

### Interest system (DTD 1.5)

DTD 1.5 added an **interest mechanic**: at the end of each wave, the player earns 5% of their current gold as a bonus (rounded down). This incentivises banking gold rather than spending everything.

```c
int interest_bonus = (int)(player_gold * 0.05f);
player_gold += interest_bonus;
```

---

## 9. Game State Machine

```c
typedef enum {
    PHASE_TITLE,         /* Title screen / mode select                    */
    PHASE_BUILDING,      /* Between waves: player places/sells towers      */
    PHASE_WAVE,          /* Wave active: simulation running + placement OK */
    PHASE_WAVE_COMPLETE, /* Brief pause between waves (1–2 seconds)       */
    PHASE_GAME_OVER,     /* All lives lost                                 */
    PHASE_VICTORY,       /* All waves survived                             */
    PHASE_COUNT
} GAME_PHASE;
```

### Phase transition table

| From          | To            | Trigger                                                     |
| ------------- | ------------- | ----------------------------------------------------------- |
| TITLE         | BUILDING      | Mode selected (Easy/Medium/Hard/Challenge)                  |
| BUILDING      | WAVE          | Player clicks "Start Wave" or first wave auto-starts        |
| WAVE          | WAVE_COMPLETE | All creeps in current wave dead or reached exit             |
| WAVE          | GAME_OVER     | Lives reach 0                                               |
| WAVE_COMPLETE | BUILDING      | Brief timer expires (or player immediately sends next wave) |
| BUILDING      | WAVE          | Player clicks "Send Next Wave" (early)                      |
| WAVE          | VICTORY       | Final wave complete + all creeps dead                       |

### DTD-specific phase notes

- **Tower placement is allowed during WAVE** — players can place towers while creeps are moving. This is intentional and central to strategy.
- **BFS must re-run immediately on placement**, even mid-wave, and all creeps must follow the updated path from their current position.
- The BUILDING phase exists only as a conceptual pause (the game allows building at all times); many implementations simply collapse BUILDING and WAVE into one active phase with a flag for whether a wave is currently running.

---

## 10. Targeting System

### Range check

Tower range is circular. A tower can fire at any creep whose centre is within `range_pixels` of the tower's centre:

```c
int in_range(Tower t, Creep c) {
    float dx = c.x - t.cx;
    float dy = c.y - t.cy;
    return (dx*dx + dy*dy) <= (t.range * t.range);
}
```

`t.range` is stored in **pixels** (= `range_cells * CELL_SIZE`).

### Target acquisition per frame

For each tower that is not on cooldown:

1. Filter the creep pool to those in range
2. Apply the targeting mode comparator (First/Last/Strongest/Weakest/Closest)
3. Fire at the top-ranked creep

This is O(towers × creeps) each frame. With 200 towers and 50 active creeps, that is 10,000 comparisons per frame — acceptable at 30 fps. For larger pools (100+ waves mode), a spatial grid reduces this to O(towers × nearby_creeps).

### Attack cooldown

Each tower has a `cooldown_timer` that counts down. When it reaches 0, the tower fires and resets:

```c
tower.cooldown_timer -= dt;
if (tower.cooldown_timer <= 0.0f) {
    fire(tower, best_target);
    tower.cooldown_timer = 1.0f / tower.fire_rate;
}
```

### Swarm (AoE) splash

When a Swarm projectile reaches its target cell:

1. Find all creeps within `splash_radius` pixels of impact point
2. Apply `damage` to each one
3. Armoured creeps take **full damage** from Swarm (balance reason: Swarm's cost is lower DPS vs single-target but counters armour)

### Flying Fortress multi-target

The Flying Fortress has 4 "jets" that orbit its centre at a fixed radius. Each jet:

- Has its own `cooldown_timer` (staggered by `1 / (fire_rate * 4)` initially so shots spread evenly)
- Targets the same "best target" as the parent tower
- Fires from the jet's current orbit position toward the target

```c
/* Jet position (orbiting) */
float jet_angle = base_angle + (jet_index * PI / 2.0f) + orbit_speed * time;
float jet_x = tower.cx + orbit_radius * cosf(jet_angle);
float jet_y = tower.cy + orbit_radius * sinf(jet_angle);
```

---

## 11. Input System

### Mouse input only

DTD has no keyboard gameplay input beyond menu navigation. All tower placement, selling, and mode selection use mouse clicks.

```c
typedef struct {
    int   x, y;               /* current mouse pixel position           */
    int   prev_x, prev_y;     /* last frame mouse position              */
    int   left_down;          /* 1 = left button currently held         */
    int   left_just_pressed;  /* 1 = pressed this frame only            */
    int   left_just_released; /* 1 = released this frame only           */
    int   right_just_pressed; /* right-click = cancel / deselect        */
} MouseState;
```

### UI regions (hit testing)

The game area is split into:

- **Grid area** — left ~600 × 400 pixels — tower placement zone
- **Side panel** — right ~150 × 450 pixels — tower shop, stats, wave info, send button
- **Bottom bar** — wave number, lives, gold display

All UI hit testing is simple bounding box checks against pixel regions.

### Tower placement workflow

1. Player left-clicks a tower button in the side panel → `selected_tower_type` set
2. Hovering over the grid shows a preview of the tower on the hovered cell (green = legal, red = illegal)
3. Left-click on grid cell → call `can_place_tower(col, row)`:
   - If legal: deduct gold, add tower, re-run BFS
   - If illegal: flash red, play error sound, do nothing
4. Right-click → cancel selection
5. Tower already placed: left-click on it to show sell button / targeting mode selector

### Hover preview

The BFS legality check runs on every frame while a tower type is selected and the mouse is over the grid — this gives the "green/red cell" preview. At 30 fps with a 600-cell grid, this is micro-cost.

---

## 12. Constants and Formulas

### Values used in simulation

| Constant               | Value                 | Purpose                                  |
| ---------------------- | --------------------- | ---------------------------------------- |
| `CELL_SIZE`            | 20 px                 | Grid cell dimensions (square)            |
| `CREEP_SPEED_NORMAL`   | ~60 px/s (3 cells/s)  | Normal creep movement                    |
| `CREEP_SPEED_FAST`     | ~120 px/s (6 cells/s) | Fast creep movement                      |
| `CREEP_SPEED_FLYING`   | ~80 px/s (4 cells/s)  | Flying creep straight-line speed         |
| `CREEP_SPEED_BOSS`     | ~40 px/s (2 cells/s)  | Boss movement                            |
| `PROJECTILE_SPEED`     | 300–500 px/s          | Visual only; high enough to look instant |
| `WAVE_SPAWN_INTERVAL`  | 0.5–1.0 s per creep   | Time between creep spawns in a wave      |
| `BOSS_WAVE_TRANSITION` | 1.0 s pause           | Brief pause before boss wave             |
| `SELL_RATIO`           | 0.70                  | Sell price as fraction of buy price      |
| `INTEREST_RATE`        | 0.05                  | Per-wave gold bonus (DTD 1.5)            |
| `HP_SCALE_PER_WAVE`    | 1.10×                 | Creep HP multiplier each wave            |

### Coordinate system

- **Y-down, pixel units** — grid cell `(0,0)` is top-left; row index increases downward
- No `PIXELS_PER_METER`; all positions are in raw pixels
- Grid snapped: tower positions are always `(col * CELL_SIZE, row * CELL_SIZE)`; only creep positions are float

### Rotation math (tower barrel)

```c
/* Tower barrel rotation to face target */
float angle = atan2f(target_y - tower.cy, target_x - tower.cx);
/* Render barrel from tower centre at angle, length = BARREL_LEN pixels */
float tip_x = tower.cx + cosf(angle) * BARREL_LEN;
float tip_y = tower.cy + sinf(angle) * BARREL_LEN;
draw_line(bb, tower.cx, tower.cy, (int)tip_x, (int)tip_y, COLOR_BARREL);
```

### BFS distance range

Maximum BFS distance in a 30×20 grid with no towers: 30 + 20 = 50 (Manhattan-ish). With a maze: up to ~500+ steps. Use `int` (not `uint8_t`) for distance values.

---

## 13. Visual Design and Rendering

### No sprites — fully procedural

The original DTD renders everything from primitive shapes. This makes it ideal for a CPU backbuffer course — nothing is loaded from disk in the core game.

| Element            | How it's drawn                                         |
| ------------------ | ------------------------------------------------------ |
| Grid background    | Alternating light/dark grey cells (like a chess board) |
| Empty cell         | Lighter grey (`#DDDDDD` approx)                        |
| Tower cell         | Covered by tower graphic                               |
| Entry/Exit cells   | Highlighted (green entry, red exit)                    |
| Path cells         | Slightly tinted (optional debug overlay)               |
| Towers             | Coloured square + rotating barrel line (or orbit jets) |
| Creeps             | Coloured square or circle, scaled by HP type           |
| HP bar             | Green/red bar drawn above each creep                   |
| Projectiles        | Small coloured dot or short line                       |
| Frost effect       | Blue tint overlay on slowed creep                      |
| Bash stun          | Stars or concentric rings drawn above creep            |
| Gold/Lives display | Bitmap text, side panel                                |
| Wave counter       | Bitmap text                                            |
| Sell button        | Rectangle with text when tower selected                |

### Color palette (approximate)

| Element        | Color     |
| -------------- | --------- |
| Grid (even)    | `#DDDDDD` |
| Grid (odd)     | `#C8C8C8` |
| Entry cell     | `#88FF88` |
| Exit cell      | `#FF8888` |
| Pellet tower   | `#888888` |
| Squirt tower   | `#4488CC` |
| Dart tower     | `#224488` |
| Snap tower     | `#CC2222` |
| Swarm tower    | `#FF8800` |
| Frost tower    | `#88CCFF` |
| Bash tower     | `#886644` |
| Normal creep   | `#888888` |
| Fast creep     | `#FFCC00` |
| Flying creep   | `#FFFFFF` |
| Armoured creep | `#444444` |
| Spawn creep    | `#44CC44` |
| Boss creep     | `#CC0000` |
| HP bar (full)  | `#00CC00` |
| HP bar (low)   | `#CC0000` |
| Gold text      | `#FFCC00` |
| Lives text     | `#FF4444` |

### UI panel layout (approximate pixel positions)

```
+-----------------------------+-----------+
|                             | Wave: 15  |
|   30×20 GRID AREA           | Lives: 12 |
|   (600 × 400 pixels)        | Gold: 245 |
|                             |           |
|                             | [Pellet 5]|
|                             | [Squirt15]|
|                             | [Dart  40]|
|                             | [Snap  75]|
+-----------------------------+ [Swarm125]|
| Wave 15/40    [Send Next]   | [Frost 60]|
+-----------------------------+ [Bash  90]|
                                [FF   200]|
                               +-----------+
```

---

## 14. Audio Design

### Sound effects

| Event                          | Sound description        |
| ------------------------------ | ------------------------ |
| Tower fire (Pellet)            | Short high-pitched click |
| Tower fire (Squirt)            | Wet squirt sound         |
| Tower fire (Dart)              | Sharp crack              |
| Tower fire (Snap)              | Deep boom                |
| Tower fire (Swarm)             | Burst/splatter           |
| Frost tower pulse              | Ice/freeze whoosh        |
| Bash hit                       | Thud / impact            |
| Creep death (normal)           | Soft pop                 |
| Boss death                     | Larger explosion         |
| Creep reaches exit (life lost) | Alarm beep or buzzer     |
| Tower placed                   | Light click              |
| Tower sold                     | Cash register sound      |
| Wave complete                  | Short fanfare chime      |
| Game over                      | Negative sting           |
| Victory                        | Celebratory fanfare      |

### Music

DTD has minimal background music — a short looping ambient/electronic track. Volume is low to avoid fatigue over long sessions.

---

## 15. Modes and Difficulty

### DTD modes

| Mode                  | Waves | Grid entry/exit | Notes                    |
| --------------------- | ----- | --------------- | ------------------------ |
| Easy (Desktop)        | 40    | Left→Right      | Standard horizontal      |
| Medium (Desktop)      | 40    | Top→Bottom      | Vertical flow            |
| Hard (Desktop)        | 40    | Corner→Corner   | Diagonal pressure        |
| Challenge (100 waves) | 100   | Left→Right      | Extended HP scaling      |
| Co-op (DTD 1.5)       | 40    | Shared grid     | Two players place towers |

### Mode differences affecting implementation

- Entry/exit positions must be configurable (not hardcoded)
- BFS must be re-initialized with the correct entry/exit for each mode
- HP scaling exponent may differ by difficulty level

---

## 16. Known Edge Cases and Bugs

These are documented from speedrun and forum discussions and should be handled correctly in any reimplementation:

| Edge case                | Description                                                             | Correct behavior                                                          |
| ------------------------ | ----------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| Block all paths          | Player tries to place a tower that would fully enclose the entry        | Reject placement; flash cell red                                          |
| Creep at cell boundary   | Creep is exactly on a cell edge when BFS updates                        | Continue movement toward previous target cell; pick new target on arrival |
| Boss + wave overlap      | Boss from wave N is alive when wave N+1 spawns                          | Both run simultaneously; no blocking                                      |
| Spawn child in exit zone | Parent dies right at the exit                                           | Children spawn and can immediately enter the exit — they cost lives       |
| Sell tower mid-wave      | Player sells a tower while creeps are navigating through that cell area | BFS re-runs; creeps re-route; no illegal state                            |
| Gold overflow            | Player accumulates very high gold in 100-wave mode                      | Use `int` (32-bit) — max gold ~100,000, never overflows 16-bit            |
| Flying creep vs Frost    | Flying creep gets hit by Frost tower                                    | Speed reduction applies normally even though flying                       |
| Two creeps on one cell   | Common; handled by each creep moving independently                      | No collision between creeps — they fully overlap                          |
| Frost + Bash combo       | Creep is slowed AND stunned                                             | Both effects apply; speed = 0 during stun, reverts to 50% after stun      |

---

## 17. Performance Characteristics

For a 640×480 CPU backbuffer implementation at 30 fps:

| System                  | Worst-case load                              | Notes                                                  |
| ----------------------- | -------------------------------------------- | ------------------------------------------------------ |
| BFS pathfinding         | 600 cells × 4 neighbours = 2400 ops          | Negligible; runs on placement only                     |
| Creep movement          | 100 active creeps × 4 neighbour checks       | Negligible                                             |
| Tower targeting         | 200 towers × 100 creeps = 20,000 comparisons | Still < 1 ms                                           |
| Projectile update       | 200 active projectiles × distance check      | Trivial                                                |
| Pixel fill (backbuffer) | 640 × 480 = 307,200 pixels per frame         | CPU clear + 600 cells + UI = very fast                 |
| HP bar render           | 100 × draw_rect() call                       | Negligible                                             |
| BFS on hover preview    | Runs every frame while tower selected        | Add dirty flag: only re-run if mouse moved to new cell |

**Spatial partitioning is not required for the standard 40-wave mode.** O(towers × creeps) targeting at 200 × 50 = 10,000 comparisons is well under 1 ms at 30 fps. For the 100-wave mode with potentially 200+ creeps, a simple grid partition (each cell stores a list of creeps in it; towers only check cells within their range) reduces to O(towers × 4–8).

---

## 18. Minimum Viable Feature Set (for a faithful recreation)

A DTD recreation is feature-complete if it implements:

- [ ] 30×20 grid, Y-down pixel coordinates, cell size 20 px
- [ ] BFS distance field from exit, updated on each tower placement
- [ ] Tower placement legality check (BFS reachability)
- [ ] 7 tower types with correct damage/range/rate (Flying Fortress optional for a first version)
- [ ] 5 targeting modes (First/Last/Strongest/Weakest/Closest)
- [ ] Tower rotation toward current target
- [ ] 6 creep types (Normal, Fast, Flying, Armoured, Spawn, Boss)
- [ ] HP scaling formula per wave
- [ ] 40-wave sequence with correct type ordering
- [ ] Economy: kill gold, sell at 70%, starting gold 50
- [ ] Lives system: creep reaching exit subtracts lives
- [ ] Send wave early with gold bonus
- [ ] Frost slow + Bash stun effects
- [ ] Swarm AoE splash (bypasses armour)
- [ ] Flying creep straight-line movement
- [ ] Spawn creep splits into 4 children on death
- [ ] Game-over when lives = 0; Victory after wave 40
- [ ] Side panel UI: tower shop, wave counter, lives, gold
- [ ] Procedural rendering (no sprites): coloured squares + barrel lines

**Stretch features** (not needed for a faithful recreation):

- Interest mechanic (DTD 1.5)
- Co-op mode
- Flying Fortress orbital jets
- Multiple game modes (Easy/Medium/Hard entry-exit positions)
- Tower upgrades (in-place)
- 100-wave challenge mode

---

## 19. Timing Data

All timing values are at **30 fps** (frame duration = 33 ms). Frame counts are the best available approximation from video analysis; treat ±1–2 frames as within margin.

### Tower fire animations

| Tower           | Fire cooldown (frames) | Projectile travel (frames) | Notes                                      |
| --------------- | ---------------------- | -------------------------- | ------------------------------------------ |
| Pellet          | 30 (1.0 s)             | 2–3                        | Single dot; nearly instant travel          |
| Squirt          | 45 (1.5 s)             | 3–4                        | Blue blob; travels to target               |
| Dart            | 20 (0.67 s)            | 2                          | Fast dart; short visual flash              |
| Snap            | 90 (3.0 s)             | 1                          | Single slow shot but high damage           |
| Swarm (each)    | 60 (2.0 s)             | 4–6 per mini-jet           | 5 jets orbit; each fires independently     |
| Frost           | 30 (1.0 s) pulse       | —                          | AoE pulse; no projectile; slow lasts 60 fr |
| Bash            | 60 (2.0 s)             | —                          | Instant AoE; stun lasts 30 fr              |
| Flying Fortress | 120 (4.0 s) per volley | 3–5                        | 4 orbital jets; fires when target in range |

### Tower placement animation

| Event             | Duration (frames) | Description                              |
| ----------------- | ----------------- | ---------------------------------------- |
| Placement flash   | 4                 | Cell briefly brightens then settles      |
| BFS overlay fade  | 6                 | "No-path" red flash on rejected place    |
| Tower appear      | 1                 | Instant (no build animation)             |
| Sell confirmation | 8                 | Cell fades to grid color; gold +animates |

### Creep animations

| Event             | Duration (frames) | Description                         |
| ----------------- | ----------------- | ----------------------------------- |
| Spawn (fade-in)   | 4                 | Creep alpha 0→1 at entry cell       |
| Death (pop)       | 6                 | Creep shrinks to 0; gold text rises |
| Boss death        | 12                | Larger flash; distinct sound        |
| Spawn child spawn | 2 (after parent)  | 4 children appear immediately       |
| Frost slow visual | 60 (active)       | Blue tint overlay for slow duration |
| Bash stun visual  | 30 (active)       | Stars drawn above creep; movement=0 |
| Reach exit (life) | 3                 | Creep flashes red, then removed     |
| HP bar threshold  | Continuous        | Bar colour switches at 33% HP       |

### Wave timing

| Event                  | Duration (frames) | Notes                                        |
| ---------------------- | ----------------- | -------------------------------------------- |
| Between-wave idle      | ~90 (3.0 s)       | Time player has before next wave auto-starts |
| Send-early gold flash  | 20                | Gold text animates when "Send Next" clicked  |
| Boss pre-wave pause    | 30 (1.0 s)        | Brief pause before boss-only wave spawns     |
| Wave complete chime    | 20                | Sound plays, wave counter increments         |
| Creep spawn interval   | 15–30 (0.5–1.0 s) | Per-creep stagger within a wave              |
| Victory screen delay   | 60 (2.0 s)        | After wave 40 last creep dies                |
| Game-over screen delay | 30 (1.0 s)        | After lives reach 0                          |

### UI timing

| Event                  | Duration (frames) | Description                              |
| ---------------------- | ----------------- | ---------------------------------------- |
| Tower select highlight | 4                 | Panel button lights up on click          |
| Gold gain text         | 24                | "+N gold" text floats up and fades       |
| Lives lost text        | 30                | "-1 life" flash on lives counter         |
| Hover preview update   | 1 (every frame)   | Green/red preview redrawn each frame     |
| Mode select transition | 10                | Brief cross-fade or wipe between screens |

---

## 20. Alignment with course-builder.md

This section maps DTD-specific mechanics to the patterns and vocabulary used in `course-builder.md`, so the course agent knows where to adapt and where to diverge.

### Coordinate system

`course-builder.md` defaults to **Y-up**; DTD uses **Y-down pixel coordinates** (row 0 = top). The course must document this explicitly at the start of every coordinate-handling lesson.

```c
/* DTD uses top-left origin, Y-down */
/* pixel_x = col * CELL_SIZE, pixel_y = row * CELL_SIZE */
/* NO world-to-screen flip needed: screen IS the world */
```

No `PIXELS_PER_METER` exists. The unit system is: **1 cell = 20 pixels**; speeds are stated in px/s or cells/s.

### Unit system

| course-builder.md concept | DTD equivalent                                |
| ------------------------- | --------------------------------------------- |
| `PIXELS_PER_METER`        | `CELL_SIZE = 20` (cells, not meters)          |
| World units (meters)      | Grid cells (integer row/col indices)          |
| Float world positions     | Float pixel positions for creeps only         |
| Snap-to-grid              | Tower at `(col * CELL_SIZE, row * CELL_SIZE)` |

### State machine

Map `GAME_PHASE` from `course-builder.md` directly:

```c
typedef enum {
    GAME_PHASE_TITLE,       /* mode select screen */
    GAME_PHASE_PLACING,     /* between waves: player builds */
    GAME_PHASE_WAVE,        /* wave in progress */
    GAME_PHASE_WAVE_CLEAR,  /* wave ended; brief pause */
    GAME_PHASE_GAME_OVER,
    GAME_PHASE_VICTORY,
} GamePhase;
```

The `GAME_PHASE_PLACING` ↔ `GAME_PHASE_WAVE` transition is the core loop driven by either auto-timer or "Send Next" click.

### Entity management

`course-builder.md` recommends fixed-size free-list pools. DTD uses three pools:

```c
#define MAX_TOWERS     256     /* 30×20 = 600 cells; can't place more than ~300 */
#define MAX_CREEPS     256     /* 40-wave peak; 100-wave needs ~512 */
#define MAX_PROJECTILES 512

typedef struct { ... } Tower;
typedef struct { ... } Creep;
typedef struct { ... } Projectile;

Tower      towers[MAX_TOWERS];
Creep      creeps[MAX_CREEPS];
Projectile projectiles[MAX_PROJECTILES];
int        tower_count;
int        creep_count;
int        projectile_count;
```

Swap-remove pattern applies on creep/projectile death.

### Audio pattern

Use `SoundDef` table from `course-builder.md`:

```c
typedef struct { int16_t *samples; int sample_count; float volume; } SoundDef;

/* Index with an enum */
typedef enum {
    SFX_TOWER_FIRE_PELLET,
    SFX_TOWER_FIRE_SQUIRT,
    SFX_CREEP_DEATH,
    SFX_BOSS_DEATH,
    SFX_LIFE_LOST,
    SFX_TOWER_PLACE,
    SFX_TOWER_SELL,
    SFX_WAVE_COMPLETE,
    SFX_GAME_OVER,
    SFX_VICTORY,
    SFX_COUNT
} SfxId;

SoundDef sfx_table[SFX_COUNT];
```

No `game_sustain_sound_update()` needed — all DTD sounds are one-shot. Background music is a simple loop playback.

### Input

Adapt `MouseState` from `course-builder.md`. DTD uses only mouse (no keyboard in core gameplay):

```c
typedef struct {
    int x, y;              /* current pixel position */
    int prev_x, prev_y;
    int left_down;         /* held */
    int left_pressed;      /* edge: 0→1 this frame */
    int left_released;     /* edge: 1→0 this frame */
} MouseState;
```

Right-click / Escape = deselect tower from shop. No key bindings required for MVP.

---

## 21. Verification Checklist

Use this checklist to confirm a recreation matches the original DTD behavior before considering the lesson complete.

### Pathfinding

- [ ] BFS fills every reachable empty cell with a valid distance value
- [ ] BFS marks entry cell as reachable (distance > 0)
- [ ] Placement is rejected whenever it would disconnect any empty cell from the exit
- [ ] Flying creeps ignore the BFS distance field and travel in a straight line to the exit pixel
- [ ] BFS re-runs immediately after every tower placement or removal
- [ ] Hover preview shows green (legal) / red (illegal) on every frame while a tower is selected

### Towers

- [ ] All 7 tower types are present with correct cost, damage, range, and fire rate
- [ ] Pellet, Squirt, Dart, Snap each fire a single projectile per shot
- [ ] Swarm AoE damage bypasses Armoured creep's 50% reduction
- [ ] Frost tower applies a speed multiplier of 0.5 for the correct duration after each pulse
- [ ] Bash tower stuns (speed = 0) for the correct duration and hits all creeps in range
- [ ] Tower barrel rotates to face current target each frame
- [ ] 5 targeting modes (First/Last/Strongest/Weakest/Closest) all work correctly
- [ ] Selling a tower returns 70% of its cost in gold and removes it from the grid
- [ ] BFS re-runs after sell (path may re-open)

### Creeps

- [ ] HP scaling formula applied per wave: `hp = base_hp * 1.10^(wave - 1)`
- [ ] Armoured creep takes 50% damage from all sources except Swarm AoE
- [ ] Fast creep moves at 2× Normal speed
- [ ] Flying creep moves straight-line to exit, ignores tower placement blocking
- [ ] Spawn creep produces 4 Normal children immediately on death
- [ ] Boss creep costs `floor(boss_hp / 100)` lives when it reaches exit (minimum 1)
- [ ] Creep smoothly interpolates between cell centres each frame using float pixel position
- [ ] HP bar updates each frame; turns red below 33% HP

### Economy

- [ ] Starting gold is 50
- [ ] Each kill awards the tower's designated kill gold
- [ ] Selling a tower awards `floor(cost * 0.70)` gold
- [ ] "Send Next Wave Early" grants the correct early-send gold bonus
- [ ] Interest mechanic (if implemented): +5% of current gold awarded at wave start

### Wave system

- [ ] Waves 1–40 follow the documented creep type order
- [ ] Creeps within a wave spawn with the correct interval stagger
- [ ] Boss waves pause briefly before spawning
- [ ] Multiple waves can be active simultaneously (boss alive when next wave starts)
- [ ] Victory triggers after the last creep of wave 40 is killed or reaches exit

### Edge cases

- [ ] Placing a tower that would block ALL paths is rejected with a red flash
- [ ] A creep mid-path re-routes correctly after a tower is placed or sold
- [ ] Spawn children spawning at exit tile immediately lose the player a life
- [ ] Frost + Bash combo: stun sets speed = 0; after stun, speed restores to 50% (Frost active)
- [ ] Gold never exceeds `INT_MAX` (use `int32_t`; max ~100,000 in 100-wave mode)
- [ ] No creep-to-creep collision; multiple creeps occupy same cell without issues

---

## 22. Open Questions

Items that could not be verified with certainty from available sources. Resolve before finalizing the course.

### Unverified mechanics

| Question                                                               | Best guess / source                                | How to verify                           |
| ---------------------------------------------------------------------- | -------------------------------------------------- | --------------------------------------- |
| Exact kill gold for each tower type                                    | Table in §6 is inferred from forum posts           | Record gameplay and count gold per kill |
| Frost slow duration (exact frames at 30 fps)                           | Estimated 60 frames (2.0 s); may be 45 or 90       | Frame-by-frame video analysis           |
| Flying Fortress orbital jet speed and fire rate                        | Estimated from visual inspection only              | Same; it's a minor feature              |
| Boss HP by wave (wave 10 vs wave 20 boss)                              | Formula `3.0 × base_hp × 1.10^(wave-1)` is a guess | Decompile flash SWF or test in-game     |
| Spawn child HP at time of split                                        | Assumed = Normal creep base HP × wave scale        | Test with a Spawn creep near death      |
| Early-send gold formula (exact amount per wave)                        | "Remaining time bonus" — exact multiplier unknown  | Record early-send rewards across waves  |
| Targeting tie-break order (two creeps at equal distance for "Closest") | Assumed: first in creep array                      | Observable but low importance           |
| Bash stun radius (exact pixel or cell count)                           | Assumed 2 cells; may be 1.5 or 2.5                 | Frame analysis or SWF decompile         |

### Design decisions for the course

| Decision                                    | Recommendation                                          | Rationale                                             |
| ------------------------------------------- | ------------------------------------------------------- | ----------------------------------------------------- |
| Include interest mechanic?                  | No — mark as stretch feature; keep economy simple       | Adds complexity without teaching new patterns         |
| Implement Flying Fortress in first version? | Defer — use placeholder tower with fixed range/damage   | Orbital jet system is unique; deserves its own lesson |
| Support multiple entry/exit modes?          | Config struct from day 1; only implement Left→Right MVP | Avoids hardcoding without adding lesson complexity    |
| In-place tower upgrade UI                   | Stretch only; not in original DTD                       | Out of scope for a faithful recreation                |
| 100-wave challenge mode                     | Stretch only; same code with larger wave table          | No new mechanics; just data extension                 |

### Performance questions

| Question                                                              | Threshold estimate                                     |
| --------------------------------------------------------------------- | ------------------------------------------------------ |
| At what creep count does O(towers × creeps) targeting become visible? | > 300 creeps and 200 towers simultaneously (~60k ops)  |
| Does incremental BFS update beat full-grid BFS for large grids?       | Not needed at 30×20; borderline at 60×40               |
| When should a spatial grid replace naive targeting loop?              | Around 500+ active creeps (100-wave plus Spawn splits) |
