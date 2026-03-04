# Lesson 05 — Snake Body as a Ring Buffer

**Source files:** `src/game.h`, `src/game.c`
**Functions:** `snake_init()`, `snake_spawn_food()`, body traversal in `snake_update()` and `snake_render()`

---

## What We're Building

The snake's body stored as a **ring buffer** — a fixed-size array that wraps
around so the head and tail can advance without ever copying data.  Understanding
this data structure unlocks the movement, collision, and growth mechanics in
the next lesson.

---

## The Concept

### Why not a linked list or `memmove`?

Three obvious alternatives:

| Approach | Write | Read | Notes |
|---|---|---|---|
| `memmove` shift | O(N) | O(1) | Shifts 1000 elements every step = slow |
| Linked list | O(1) | O(N) | Pointer-per-segment = heap alloc per step, cache misses |
| Ring buffer | O(1) | O(N) | Fixed array, zero allocation, cache-friendly |

For a 1200-cell snake at 60 FPS the ring buffer never allocates memory and
never copies data — head and tail just increment.

### JS analogy — circular buffer backed by a fixed Array

Here is the same data structure in JavaScript so you can see the mechanics
before reading the C:

```js
const MAX_SNAKE = 1200;

const snake = {
    segments: new Array(MAX_SNAKE).fill(null).map(() => ({ x: 0, y: 0 })),
    head:   9,   // index of the most-recently written segment
    tail:   0,   // index of the oldest segment
    length: 10,
};

// Advance head (write new segment)
function advanceHead(snake, newX, newY) {
    snake.head = (snake.head + 1) % MAX_SNAKE;
    snake.segments[snake.head].x = newX;
    snake.segments[snake.head].y = newY;
    snake.length++;
}

// Advance tail (discard oldest segment)
function advanceTail(snake) {
    snake.tail = (snake.tail + 1) % MAX_SNAKE;
    snake.length--;
}

// Walk from tail to head
function walkBody(snake, fn) {
    let idx = snake.tail;
    let rem = snake.length;
    while (rem > 0) {
        fn(snake.segments[idx]);
        idx = (idx + 1) % MAX_SNAKE;
        rem--;
    }
}
```

The C code is a direct translation of this.

---

## The Code

### The ring buffer fields in `GameState` (`src/game.h`)

```c
typedef struct {
    Segment segments[MAX_SNAKE];   /* Fixed array; MAX_SNAKE = 1200             */
    int head;    /* Index of most-recently written segment (the snake's head)   */
    int tail;    /* Index of oldest segment (the snake's tail tip)              */
    int length;  /* Number of valid segments (head - tail + 1, wrapping)        */
    /* … other fields … */
} GameState;
```

`Segment` is just two integers:

```c
typedef struct { int x, y; } Segment;
```

The array `segments[]` holds ALL 1200 possible positions.  At game start only
10 of them are live (the initial snake).  The ring grows and shrinks without
ever touching memory outside the array.

### Memory layout — visualised

After `snake_init()` the initial 10-segment snake occupies indices 0–9:

```
Index: 0    1    2    3    4    5    6    7    8    9    10 … 1199
       [10,10][11,10][12,10]…[19,10][ ? ][ ? ][ ? ]…[ ? ]
        ↑                              ↑
       tail=0                        head=9  (most-recent = rightmost)
```

`tail=0`, `head=9`, `length=10`.

After one move step the head advances:

```
Index: 0    1    …    9    10   11 … 1199
       [10,10]…[19,10][20,10][ ? ]…[ ? ]
        ↑               ↑
       tail=0 (will advance next unless growing)
                       head=10
```

After the tail also advances:

```
Index: 0    1    …    9    10   11 … 1199
       [ ? ][11,10]…[19,10][20,10][ ? ]…
              ↑               ↑
             tail=1           head=10   length still 10
```

The `0` slot is now dead — it will be overwritten the next time head wraps
around to that index (after 1190 more steps).

### Advancing head — adding a new segment (`src/game.c`)

```c
/* Write the new head position: */
s->head = (s->head + 1) % MAX_SNAKE;   /* advance; wrap at array end */
s->segments[s->head].x = new_x;
s->segments[s->head].y = new_y;
s->length++;
```

`% MAX_SNAKE` is the wrap operator.  When `head = 1199`, `(1199 + 1) % 1200 = 0`.
The next write goes to index 0 — the beginning of the array.  This is safe
because by the time head wraps all the way around (1200 steps), the tail has
also advanced past index 0.

JS equivalent: `snake.head = (snake.head + 1) % MAX_SNAKE`.

### Advancing tail — discarding the oldest segment (`src/game.c`)

```c
/* (Only when NOT growing — see grow_pending below) */
s->tail = (s->tail + 1) % MAX_SNAKE;
s->length--;
```

The old tail slot is not zeroed — there's no need.  It's simply "forgotten"
by the buffer.  On the next visit the head will overwrite it with fresh data.

### Walking the buffer — tail to head

The traversal pattern used in self-collision detection and rendering:

```c
/* Walk every live segment from oldest (tail) to newest (head): */
int idx = s->tail;
int rem = s->length;
while (rem > 0) {
    /* use s->segments[idx] here */
    idx = (idx + 1) % MAX_SNAKE;
    rem--;
}
```

Why `rem` and not `idx != head + 1`?

- `head + 1` wraps around — the arithmetic comparison is fragile.
- `rem = length` always gives the exact count of live elements; decrementing
  it is safe regardless of where head and tail sit relative to the array ends.

JS equivalent: the `walkBody` function in the analogy above.

### `grow_pending` — deferring tail advance for 5 ticks

```c
/* grow_pending is incremented by 5 when food is eaten:          */
if (new_x == s->food_x && new_y == s->food_y) {
    s->score++;
    s->grow_pending += 5;
    snake_spawn_food(s);
    /* … spatial audio … */
}

/* Each tick, head always advances.
 * Tail advances ONLY when grow_pending == 0.                    */
if (s->grow_pending > 0) {
    s->grow_pending--;   /* skip tail advance — snake grows by 1 */
} else {
    s->tail = (s->tail + 1) % MAX_SNAKE;
    s->length--;         /* constant length when not growing      */
}
```

Without `grow_pending`, eating food would need to insert a segment somewhere
in the middle of the array — an O(N) operation.  Instead, we simply skip the
tail advance for 5 ticks after eating.  Each skipped advance means one extra
segment at the tail persists — the snake grows from the back naturally, without
any insertion.

Timeline after eating food:

```
Tick 0 (food eaten): grow_pending = 5. Head advances, tail stays. length++
Tick 1:              grow_pending = 4. Head advances, tail stays. length++
Tick 2:              grow_pending = 3. Head advances, tail stays. length++
Tick 3:              grow_pending = 2. Head advances, tail stays. length++
Tick 4:              grow_pending = 1. Head advances, tail stays. length++
Tick 5:              grow_pending = 0. Head advances, tail advances. length stays.
```

Net effect: length grows by 5 over 5 ticks, then stabilises.

JS analogy:
```js
if (snake.growPending > 0) {
    snake.growPending--;
    // don't advance tail — body gets longer from the back
} else {
    advanceTail(snake);
}
```

### `snake_init` — initial ring state (`src/game.c`)

```c
/* Place 10 segments horizontally, columns 10–19, row GRID_HEIGHT/2 = 10 */
s->head   = 9;   /* most-recent = rightmost (segment[9] = {19, 10})      */
s->tail   = 0;   /* oldest     = leftmost  (segment[0] = {10, 10})       */
s->length = 10;
for (i = 0; i < 10; i++) {
    s->segments[i].x = 10 + i;   /* columns 10, 11, …, 19               */
    s->segments[i].y = GRID_HEIGHT / 2;
}
```

After `snake_init`, `segments[0]` is the tail (the leftmost cell),
`segments[9]` is the head (the rightmost cell).  The snake moves rightward,
so the head will advance to index 10 on the first move tick.

### `snake_spawn_food` — O(N) retry loop (`src/game.c`)

```c
void snake_spawn_food(GameState *s) {
    int ok, idx, rem;
    do {
        /* Random cell, 1 cell inside the wall border */
        s->food_x = 1 + rand() % (GRID_WIDTH  - 2);
        s->food_y = 1 + rand() % (GRID_HEIGHT - 2);

        /* Walk ring buffer — check if any segment occupies this cell */
        ok  = 1;
        idx = s->tail;
        rem = s->length;
        while (rem > 0) {
            if (s->segments[idx].x == s->food_x &&
                s->segments[idx].y == s->food_y) {
                ok = 0;
                break;
            }
            idx = (idx + 1) % MAX_SNAKE;
            rem--;
        }
    } while (!ok);
}
```

This is an O(N) retry loop: pick a random cell, scan all segments, retry if
occupied.  For a short snake this is almost always one iteration.  For a near-
maximum snake (1000+ segments) the probability of picking a free cell is low
and retries increase.  For a game of this scale it remains well within
microseconds per call — premature optimisation would add complexity without
measurable benefit.

`rand() % N` produces a value in `[0, N-1]`.  Adding 1 to the lower bound and
using `GRID_WIDTH - 2` keeps food one cell away from the walls (cells 0 and
`GRID_WIDTH-1` are wall cells and must stay clear).

JS equivalent: `Math.floor(Math.random() * (GRID_WIDTH - 2)) + 1`.

---

## What To Notice

- **`% MAX_SNAKE` is the whole trick** — all ring buffer mechanics reduce to
  one modulo on advance.  No pointer arithmetic, no bounds checking inside the
  loop, no special-case for wraparound.

- **`length` tracks validity, not index math** — you don't need to compute
  `(head - tail + MAX_SNAKE) % MAX_SNAKE` every traversal.  Just count down
  `rem = length` from `tail`.  This avoids off-by-one errors at the wrap point.

- **`grow_pending` is a countdown, not a boolean** — it defers tail advance for
  exactly 5 ticks per food eaten.  If the snake eats two items in rapid
  succession, `grow_pending` accumulates to 10, and the snake grows 10
  segments over 10 ticks.

- **No allocation in the game loop** — `segments[]` is declared inside
  `GameState` (which lives on the stack in `main()`).  The ring buffer never
  calls `malloc`.  This is the primary reason the game has no memory-
  management bugs related to body segments.

---

## Exercises

1. **Visualise the ring buffer slots.**
   In `snake_render()`, after drawing the snake body, add a temporary loop
   that draws ALL 1200 slots — occupied or not — in a dim colour, and draws
   the current tail and head slots in bright colours:
   ```c
   /* Show ring buffer state — temporary debug visualisation */
   for (int i = 0; i < MAX_SNAKE; i++) {
       uint32_t c = (i == s->tail) ? GAME_RGB(255,0,0) :
                    (i == s->head) ? GAME_RGB(0,0,255) :
                                     GAME_RGB(30,30,30);
       /* Place in a debug strip above the header (y = -CELL_SIZE) */
       /* This won't display on screen but you can read s->tail/head */
   }
   ```
   Add `printf("tail=%d head=%d length=%d\n", s->tail, s->head, s->length);`
   in `snake_update` and watch the counters move.

2. **Change `grow_pending` amount.**
   In `snake_update`, change `s->grow_pending += 5` to `s->grow_pending += 1`.
   Run the game — the snake now grows by only 1 segment per food item.
   Change it to `+= 10` and observe the more dramatic growth.
   Restore `+= 5` before the next lesson.

3. **Break the modulo and observe the crash.**
   Temporarily change `s->head = (s->head + 1) % MAX_SNAKE;` to
   `s->head = s->head + 1;` (removing the modulo).  Run the game for long
   enough that the snake eats several items.  When `head` reaches 1200,
   `s->segments[1200]` writes past the end of the array — undefined behaviour
   that will likely corrupt adjacent struct fields or crash.  Restore the modulo
   immediately.  This exercise demonstrates why the modulo is non-optional.
