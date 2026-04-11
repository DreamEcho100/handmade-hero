# Lesson 12: Game Demo --- Entities in Action

> **What you'll build:** The player moves with WASD/arrow keys. Trolls spawn every second at random positions. Moving the player into a troll removes it from the pool. The HUD shows spawned, killed, and reused counters --- live proof that the free list and generational IDs are working.

---

## What you'll learn

- `game_update`: reading input, moving the player, clamping to screen bounds
- Entity spawning with timers and the pool's `add` function
- AABB (Axis-Aligned Bounding Box) collision detection
- `thing_pool_remove` in action: the free list recycles slots
- `thing_ref` in action: stale references detected by generational IDs

## Prerequisites

- **Lesson 11** completed (you have the window, the player, and the render loop)

---

## Step 1 --- Moving the player with keyboard input

Open `game.c`. Replace the `game_update` stub from Lesson 11 with real code. We will build it section by section.

The full `game_update` function has three blocks:
1. **Move player** --- read input, apply velocity, clamp to bounds
2. **Spawn trolls** --- accumulate timer, add to pool, set random position
3. **Check collisions** --- iterate pool, AABB overlap, remove on hit

We will build each block separately, then see the complete function at the end.

First, the player movement:

```c
void game_update(game_state *state, const game_input *input, float dt)
{
    /* -- Move player -- */
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    if (thing_is_valid(player)) {
        float dx = 0.0f, dy = 0.0f;
        if (input->left)  dx -= 1.0f;
        if (input->right) dx += 1.0f;
        if (input->up)    dy -= 1.0f;
        if (input->down)  dy += 1.0f;

        player->x += dx * PLAYER_SPEED * dt;
        player->y += dy * PLAYER_SPEED * dt;
```

Let us break down every line.

**`thing_pool_get_ref(&state->pool, state->player_ref)`** --- look up the player by its generational reference. The ref is `{index=1, generation=0}` (set during `game_init`). The function checks three things: is the index in range? Is `used[1]` true? Does `generations[1]` match generation 0? All yes, so it returns a pointer to `things[1]`.

If the player were somehow removed (a bug, or some future mechanic), this would return the nil sentinel --- not a dangling pointer to whatever now occupies slot 1. This is the generational ID system from Lesson 09 doing its job.

**`thing_is_valid(player)`** --- checks `player->kind != THING_KIND_NIL`. If `get_ref` returned nil (stale ref, removed player, anything wrong), we skip the entire movement block. No crash. No special error handling. Just... nothing happens. The nil sentinel absorbs the mistake.

**The dx/dy pattern:** Each input direction contributes +1 or -1 to a direction vector. If you press left and up simultaneously, `dx = -1, dy = -1`. We do not normalize the diagonal (that would require `sqrtf`), so diagonal movement is about 41% faster. For this demo, that is fine.

> **Why?** In JavaScript you might write `if (keys.left) player.x -= speed * dt`. Same idea. The difference is that in C, we fetch the player from the pool by ref instead of accessing `this.player` directly. The pool is the single source of truth --- there is no shortcut around it.

**`player->x += dx * PLAYER_SPEED * dt`** --- velocity times time equals displacement. `PLAYER_SPEED` is 200 pixels per second. At 60 FPS, `dt` is roughly 0.0167 seconds. So the player moves about 3.3 pixels per frame. This is frame-rate independent: at 30 FPS the player moves 6.6 pixels per frame but still covers 200 pixels per second.

### Clamping to screen bounds

Still inside the `thing_is_valid(player)` block:

```c
        /* Clamp to screen bounds */
        if (player->x < 0) player->x = 0;
        if (player->y < 0) player->y = 0;
        if (player->x > SCREEN_W - player->size)
            player->x = SCREEN_W - player->size;
        if (player->y > SCREEN_H - player->size)
            player->y = SCREEN_H - player->size;
    }
```

Four clamp checks. The right and bottom bounds account for the player's size so the rectangle does not hang off the edge.

> **Why `SCREEN_W - player->size` instead of just `SCREEN_W`?** The player's position is its top-left corner. If you clamp to 800, the right edge of the 20-pixel-wide player extends to x=820 --- off screen. Always subtract the entity's size from the maximum bound.

```
Correct clamping:

  0                              780  800
  |                               |    |
  [####################]          |    |
  ^--- player at x=0             |    |
                                  |    |
            [####################]|    |
            ^--- player at x=780      |
                                  20px |
                                  size |
```

If you clamp to `SCREEN_W` (800) instead of `SCREEN_W - player->size` (780), the player can slide 20 pixels off the right edge. This is one of the most common beginner mistakes in 2D games.

### Testing movement

After adding just the movement block (leave the rest of `game_update` empty), you can compile and test:

```bash
./build-dev.sh --backend=raylib -r
```

The orange rectangle should move with WASD/arrow keys and stop at the screen edges. If it moves off-screen, check your clamping bounds. If it does not move at all, check that `thing_is_valid(player)` returns true (add a `printf` inside the block to verify).

> **Why test incrementally?** In JavaScript, you can hot-reload and see changes instantly. In C, the compile-run cycle takes a few seconds. Testing each block separately catches bugs early --- if movement works and spawning does not, you know the bug is in the spawning block, not somewhere vague.

---

## Step 2 --- Spawning trolls on a timer

Still inside `game_update`, after the movement block:

```c
    /* -- Spawn trolls on a timer -- */
    state->spawn_timer += dt;
    if (state->spawn_timer >= SPAWN_INTERVAL
        && state->trolls_spawned < MAX_TROLLS * 3)
    {
        state->spawn_timer = 0.0f;
```

**Timer pattern:** Accumulate `dt` into `spawn_timer`. When it reaches `SPAWN_INTERVAL` (1.0 seconds), spawn a troll and reset the timer. This is the standard fixed-interval spawn pattern used in every game.

In JavaScript, the equivalent is:

```js
this.spawnTimer += dt;
if (this.spawnTimer >= SPAWN_INTERVAL) {
    this.spawnTimer = 0;
    this.spawnTroll();
}
```

Identical logic. Just a struct field instead of a class property.

**`state->trolls_spawned < MAX_TROLLS * 3`** --- a cap so the demo does not run forever. We allow up to 60 total spawns (not 60 alive at once --- 60 total across the game's lifetime, since `MAX_TROLLS` is 20). After 60 trolls have been spawned, the timer still ticks but nothing happens.

### Tracking free list reuse

Before we add the troll, we snapshot the free list head:

```c
        thing_idx old_first_free = state->pool.first_free;
```

After the add, we will check whether the free list was consumed. This is purely for the HUD counter --- in a real game you would not track this. But for the demo, it makes the free list visible.

### Adding the troll to the pool

```c
        thing_ref troll_ref = thing_pool_add(&state->pool, THING_KIND_TROLL);
        thing *troll = thing_pool_get_ref(&state->pool, troll_ref);
        if (thing_is_valid(troll)) {
            troll->x     = rng_float(&state->rng_state)
                           * (SCREEN_W - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->y     = rng_float(&state->rng_state)
                           * (SCREEN_H - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->size  = TROLL_SIZE;
            troll->color = 0xFF4488CC; /* blue-ish */
            state->trolls_spawned++;
```

Same pattern as the player in `game_init`: add to pool, get the thing by ref, check validity, set properties. You have seen this pattern three times now --- it should be starting to feel natural.

The position formula places the troll within screen bounds with a margin of `TROLL_SIZE` (15 pixels) on each side:

```
x range: [TROLL_SIZE, SCREEN_W - TROLL_SIZE]
        = [15, 785]

rng_float() returns [0, 1)
  * (800 - 30) = [0, 770)
  + 15          = [15, 785)
```

The troll will never spawn partially off-screen.

**`0xFF4488CC`** --- the troll color. `FF` alpha (fully opaque), `44` red, `88` green, `CC` blue. A blue-ish tint, visually distinct from the orange player.

### Detecting free list reuse

```c
            if (old_first_free != 0
                && troll_ref.index <= state->pool.next_empty)
            {
                state->slots_reused++;
            }
        }
    }
```

If the free list had entries (`old_first_free != 0`) and the new troll's index is within the previously-used range (not a fresh slot from `next_empty`), then the add consumed a free list slot. Increment the reuse counter.

> **Why track this?** To make the free list visible. Without this counter, slot reuse happens silently. With it, you can watch the "Reused: 3" number climb on the HUD as you kill trolls and new ones take their slots.

Here is what happens over time:

```
t=0s:  Pool: [NIL] [PLAYER] _ _ _ ...
       next_empty=2  first_free=0 (empty)
       HUD: Spawned:0  Killed:0  Reused:0

t=1s:  Troll spawns → pool_add uses fresh slot 2
       Pool: [NIL] [PLAYER] [TROLL] _ _ ...
       next_empty=3  first_free=0
       HUD: Spawned:1  Killed:0  Reused:0

t=2s:  Troll spawns → fresh slot 3
       Pool: [NIL] [PLAYER] [TROLL] [TROLL] _ ...
       next_empty=4  first_free=0
       HUD: Spawned:2  Killed:0  Reused:0

t=3s:  You kill troll at slot 2. pool_remove(2):
         used[2] = false
         things[2] = zeroed
         generations[2] = 1  (bumped!)
         first_free = 2  (slot 2 is now the free list head)
       Pool: [NIL] [PLAYER] [__dead__] [TROLL] _ ...
       next_empty=4  first_free=2
       HUD: Spawned:2  Killed:1  Reused:0

       ...same second, new troll spawns → pool_add pops slot 2 from free list!
       Pool: [NIL] [PLAYER] [NEW TROLL] [TROLL] _ ...
       next_empty=4  first_free=0 (consumed)
       HUD: Spawned:3  Killed:1  Reused:1  ← free list reuse!

t=5s:  You kill troll at slot 3.
       generations[3] = 1
       first_free = 3
       HUD: Spawned:4  Killed:2  Reused:1

t=6s:  New troll spawns → lands in slot 3 (recycled)
       HUD: Spawned:5  Killed:2  Reused:2
```

The "Reused" counter is your live view into the free list.

### What happens inside pool_add during spawning

Let us trace through `thing_pool_add` when a troll spawns at t=4s (after one slot was freed):

```
thing_pool_add(&pool, THING_KIND_TROLL):

  Step 1: Check free list.
    pool->first_free = 2 (slot 2 was freed earlier)
    slot = 2
    pool->first_free = things[2].next_sibling = 0  (end of free list)

  Step 2: Zero the slot.
    memset(&things[2], 0, sizeof(thing))
    → kind=0, x=0, y=0, all links=0

  Step 3: Set kind.
    things[2].kind = THING_KIND_TROLL

  Step 4: Mark used.
    used[2] = true

  Step 5: Return ref.
    return { index=2, generation=1 }
    ← generation is 1 because slot 2 was recycled once

After the add:
  things[2] = { kind=TROLL, x=0, y=0, ... }  ← fresh, zeroed
  used[2] = true
  generations[2] = 1  (unchanged — bumped during remove, not during add)
  first_free = 0  (free list now empty)
```

The caller then sets `troll->x`, `troll->y`, `troll->size`, `troll->color`. The zero-init guarantees that every other field (parent, children, velocity, health) starts at zero. No initialization code needed for unused fields.

Compare this to JavaScript:

```js
// JS: create a new troll object
const troll = {
    kind: 'troll',
    x: Math.random() * 770 + 15,
    y: Math.random() * 570 + 15,
    size: 15,
    color: '#4488CC',
    // ...need to list EVERY property or risk undefined
    parent: null,
    children: [],
    health: 0,
    velocity: { x: 0, y: 0 }
};
enemies.push(troll);
```

In JS, you must explicitly initialize every property or risk `undefined` errors later. In our pool, `memset(0)` initializes everything to a safe default. The zero-init architecture saves you from forgetting to initialize a field.

---

## Step 3 --- AABB collision detection

Still inside `game_update`, after the spawning block. This is where we check whether the player is touching any trolls.

```c
    /* -- Collision: player vs trolls -- */
    if (thing_is_valid(player)) {
        for (thing_iter it = thing_iter_begin(&state->pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            if (t->kind != THING_KIND_TROLL) continue;
```

We iterate over every living thing in the pool. For each thing, we check if it is a troll (`kind != THING_KIND_TROLL` means skip). The player, the nil sentinel, and any other entity type are skipped. This is the kind enum from Lesson 02 doing its job --- one loop handles all entity types, the kind tag tells us which ones are relevant.

The `thing_is_valid(player)` guard around the entire loop prevents collision detection when the player is nil. This cannot happen in our demo (the player is never removed), but defensive code does not care about "cannot happen."

Now the actual overlap test:

```c
            float half_p = player->size / 2.0f;
            float half_t = t->size / 2.0f;
            float pcx = player->x + half_p;
            float pcy = player->y + half_p;
            float tcx = t->x + half_t;
            float tcy = t->y + half_t;

            float dx = pcx - tcx;
            float dy = pcy - tcy;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            if (dx < (half_p + half_t) && dy < (half_p + half_t)) {
```

AABB stands for **Axis-Aligned Bounding Box**. "Axis-aligned" means the rectangles are not rotated --- their edges are parallel to the X and Y axes. "Bounding box" means we treat the entity as a rectangle for collision purposes.

The algorithm: compute the center of each rectangle, find the absolute distance between centers on each axis, and check if it is less than the sum of the half-sizes.

```
                    half_p         half_t
                   ◄──────►       ◄─────►
                ┌──────────────┐  ┌─────────┐
                │              │  │         │
                │    Player    │  │  Troll  │
                │   (pcx,pcy)  │  │ (tcx,   │
                │      ●       │  │  tcy) ● │
                │              │  │         │
                └──────────────┘  └─────────┘

  Distance between centers on X axis: |pcx - tcx|

  Overlap when BOTH:
    |pcx - tcx| < half_p + half_t     (X axis overlap)
    AND
    |pcy - tcy| < half_p + half_t     (Y axis overlap)
```

We compute `abs(dx)` manually with `if (dx < 0) dx = -dx` instead of calling `fabsf` from `<math.h>`. This avoids a function call and is clearer about what is happening.

> **Why?** In JavaScript, you would use `Math.abs(dx)`. In C, you can do the same with `fabsf(dx)` from `<math.h>`. But the manual version avoids a function call, and in a hot loop over thousands of entities, that can matter. For 20 trolls it makes zero difference --- but building the habit now means you will not have to refactor later.

If both axes overlap, the player and troll are touching.

Let us trace through a concrete collision check:

```
Player: x=100, y=200, size=20
  half_p = 10
  pcx = 100 + 10 = 110
  pcy = 200 + 10 = 210

Troll:  x=105, y=195, size=15
  half_t = 7.5
  tcx = 105 + 7.5 = 112.5
  tcy = 195 + 7.5 = 202.5

dx = |110 - 112.5| = 2.5
dy = |210 - 202.5| = 7.5

Threshold = half_p + half_t = 10 + 7.5 = 17.5

dx (2.5) < 17.5?  YES
dy (7.5) < 17.5?  YES
→ COLLISION! Remove the troll.
```

Now a case where they are NOT touching:

```
Player: x=100, y=200, size=20
Troll:  x=300, y=400, size=15

pcx=110, pcy=210, tcx=307.5, tcy=407.5
dx = |110 - 307.5| = 197.5
dy = |210 - 407.5| = 197.5

Threshold = 17.5

dx (197.5) < 17.5?  NO
→ No collision. Skip.
```

The X-axis check fails, so we never even evaluate the Y-axis (short-circuit evaluation in the `&&` operator). For far-apart entities, this is fast --- one comparison and done.

---

## Step 4 --- Removing trolls on collision

When a collision is detected:

```c
                /* Collision! Remove the troll. */
                thing_pool_remove(&state->pool, it.current);
                state->trolls_killed++;
            }
        }
    }
}
```

`thing_pool_remove(&state->pool, it.current)` --- the pool remove function from Lesson 07 and 08. One call does four things:

1. Marks `used[idx] = false`
2. `memset`s the thing to zero (kind, position, color --- all gone)
3. Bumps `generations[idx]` (any existing `thing_ref` to this slot becomes stale)
4. Pushes the slot onto the intrusive free list

Let us trace through what happens when you kill the troll at slot 3:

```
BEFORE removal:
  things[3]      = { kind=TROLL, x=150, y=200, size=15, color=blue }
  used[3]        = true
  generations[3] = 0
  first_free     = 0 (empty free list)

AFTER thing_pool_remove(&pool, 3):
  things[3]      = { kind=NIL, x=0, y=0, ... all zeros }
  used[3]        = false
  generations[3] = 1          ← bumped!
  first_free     = 3          ← slot 3 is now the free list head
  things[3].next_sibling = 0  ← old first_free (end of free list)
```

The generation bump is critical. If any code still holds a `thing_ref` with `{index=3, generation=0}`, calling `thing_pool_get_ref` will compare `generation 0 != generation 1` and return the nil sentinel. The stale reference is detected and neutralized.

> **Why?** This is the reincarnation bug prevention from Lesson 09. Without generational IDs, old code holding `{index=3}` would silently access whatever NEW entity now occupies slot 3 --- potentially a completely different troll, or even a player. With generational IDs, the mismatch is caught and nil is returned. Safe.

### A note about iterator safety

We call `thing_pool_remove` while iterating. The remove zeroes the thing at `it.current` and marks it unused. Then the for-loop calls `thing_iter_next`, which advances past the current slot.

This means: if two trolls are in adjacent slots and both overlap the player, we might process them on different frames (the iterator advances past the freshly-dead slot, and the next slot gets its turn on the next `thing_iter_next`). In practice, missing one collision for one frame is unnoticeable --- the player touches the troll again on the next frame.

In a production game, you would use a **deferred removal list**: collect all the indices to remove during iteration, then remove them all after the loop finishes:

```c
/* Production pattern (NOT used in this demo): */
thing_idx to_remove[64];
int remove_count = 0;

for (thing_iter it = ...; ...; ...) {
    if (collision) {
        to_remove[remove_count++] = it.current;
    }
}

for (int i = 0; i < remove_count; i++) {
    thing_pool_remove(&pool, to_remove[i]);
}
```

For a demo with 20 trolls, immediate removal is simpler and demonstrates the pool's ability to handle mid-iteration removal without crashing.

---

## Step 5 --- Free list in action: slot reuse

Now that you can spawn and kill trolls, the free list is exercised every time. Let us trace a full play session:

```
Initial state (after game_init):
  things[]: [NIL] [PLAYER] _ _ _ _ _ _ _ _ ...
  used[]:   [ F ] [  T   ] F F F F F F F F ...
  gens[]:   [ 0 ] [  0   ] 0 0 0 0 0 0 0 0 ...
  next_empty=2  first_free=0

After 5 trolls spawn (t=5s):
  things[]: [NIL] [PLAYER] [T1] [T2] [T3] [T4] [T5] _ ...
  used[]:   [ F ] [  T   ] [ T] [ T] [ T] [ T] [ T] F ...
  next_empty=7  first_free=0

Kill T1 at slot 2 and T3 at slot 4:
  things[]: [NIL] [PLAYER] [dead] [T2] [dead] [T4] [T5] _ ...
  used[]:   [ F ] [  T   ] [  F ] [ T] [  F ] [ T] [ T] F ...
  gens[]:   [ 0 ] [  0   ] [  1 ] [ 0] [  1 ] [ 0] [ 0] ...
  first_free = 4 → 2 → 0 (end)
       ↑ slot 4 was removed second, so it is the free list head

Next troll spawns → pool_add pops slot 4 from free list:
  things[]: [NIL] [PLAYER] [dead] [T2] [NEW!] [T4] [T5] _ ...
  used[]:   [ F ] [  T   ] [  F ] [ T] [  T ] [ T] [ T] F ...
  gens[]:   [ 0 ] [  0   ] [  1 ] [ 0] [  1 ] [ 0] [ 0] ...
  first_free = 2 → 0 (end)
  HUD: Reused: 1

Next troll spawns → pool_add pops slot 2 from free list:
  things[]: [NIL] [PLAYER] [NEW!] [T2] [T6] [T4] [T5] _ ...
  first_free = 0 (empty, all recycled)
  HUD: Reused: 2

Next troll spawns → free list empty, pool_add uses next_empty=7:
  things[]: [NIL] [PLAYER] [T7] [T2] [T6] [T4] [T5] [NEW!] ...
  next_empty=8
  HUD: Reused: 2 (no change — used fresh slot)
```

The free list operates as a LIFO stack (last removed, first reused). Slots are recycled in reverse removal order. The "Reused" counter on the HUD is your live proof.

### Exercise: watch the free list yourself

1. Run the demo: `./build-dev.sh --backend=raylib -r`
2. Wait for 5 trolls to spawn (Spawned: 5)
3. Kill 3 of them (Killed: 3). The free list now has 3 slots.
4. Wait for 3 more spawns. Watch the Reused counter climb from 0 to 3.
5. Press R to reset. Everything goes to zero.

---

## Step 6 --- Generational IDs in action: stale ref safety

You cannot directly observe generational IDs in the movement demo (they work silently). But you can reason through the scenario:

```
Step 1: Troll spawns at slot 4, generation 0.
        Something saves troll_ref = {index=4, generation=0}.

Step 2: Player collides with the troll.
        thing_pool_remove(&pool, 4) fires:
          - used[4] = false
          - things[4] = zeroed
          - generations[4] = 1  (bumped from 0!)
          - slot 4 pushed onto free list

Step 3: A new troll spawns.
        Pool pops slot 4 from the free list.
        New troll at slot 4, generation 1.
        New ref is {index=4, generation=1}.

Step 4: The saved troll_ref = {index=4, generation=0} is used:
        thing_pool_get_ref(&pool, {4, 0})
          - checks: generations[4] == 0?  NO, it is 1.
          - generation mismatch → returns nil sentinel.
```

The stale reference is caught. Without generational IDs, step 4 would silently return the NEW troll --- code that thought it was talking to the old troll would be modifying a completely different entity. That is the "reincarnation bug" that generational IDs prevent.

> **Why?** Casey Muratori builds toward this exact system in Handmade Hero. His entity handles in days 33+ use generation indices for the same reason. The entity ID is not just "slot 5" --- it is "slot 5, version 3." If you ask for version 3 and the slot is now version 4, you get nil.

Press 6 on your keyboard to switch to Scene 6: Generational IDs Demo. This scene creates a ref, removes the entity, recycles the slot, and shows the stale ref returning nil. It is the scenario above made visible on screen.

### Why this matters in practice

Without generational IDs, the reincarnation bug is invisible and devastating. Imagine this scenario in a real game:

```
1. AI system saves a reference to "target troll at slot 5"
2. Player kills the troll. Slot 5 freed.
3. New FRIENDLY NPC spawns at slot 5.
4. AI system: "chase target at slot 5" → chases the NPC!
5. Player sees their allied NPC being attacked by enemies.
   "This game is broken."
```

With generational IDs:

```
1. AI saves ref = {index=5, generation=2}
2. Player kills troll. generation[5] bumps to 3.
3. NPC spawns at slot 5 with generation=3.
4. AI: thing_pool_get_ref(&pool, {5, 2})
   → generation 2 != 3 → returns nil
   → AI: "target is gone, pick new target"
5. Game works correctly.
```

The generation check costs ONE integer comparison per lookup. The bug it prevents would take hours to debug.

---

## Step 7 --- The full game loop

Here is the complete `game_update` function with all three sections. This replaces the stub from Lesson 11:

```c
void game_update(game_state *state, const game_input *input, float dt)
{
    /* -- Move player -- */
    thing *player = thing_pool_get_ref(&state->pool, state->player_ref);
    if (thing_is_valid(player)) {
        float dx = 0.0f, dy = 0.0f;
        if (input->left)  dx -= 1.0f;
        if (input->right) dx += 1.0f;
        if (input->up)    dy -= 1.0f;
        if (input->down)  dy += 1.0f;

        player->x += dx * PLAYER_SPEED * dt;
        player->y += dy * PLAYER_SPEED * dt;

        if (player->x < 0) player->x = 0;
        if (player->y < 0) player->y = 0;
        if (player->x > SCREEN_W - player->size)
            player->x = SCREEN_W - player->size;
        if (player->y > SCREEN_H - player->size)
            player->y = SCREEN_H - player->size;
    }

    /* -- Spawn trolls on a timer -- */
    state->spawn_timer += dt;
    if (state->spawn_timer >= SPAWN_INTERVAL
        && state->trolls_spawned < MAX_TROLLS * 3)
    {
        state->spawn_timer = 0.0f;

        thing_idx old_first_free = state->pool.first_free;

        thing_ref troll_ref = thing_pool_add(&state->pool, THING_KIND_TROLL);
        thing *troll = thing_pool_get_ref(&state->pool, troll_ref);
        if (thing_is_valid(troll)) {
            troll->x     = rng_float(&state->rng_state)
                           * (SCREEN_W - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->y     = rng_float(&state->rng_state)
                           * (SCREEN_H - TROLL_SIZE * 2) + TROLL_SIZE;
            troll->size  = TROLL_SIZE;
            troll->color = 0xFF4488CC;
            state->trolls_spawned++;

            if (old_first_free != 0
                && troll_ref.index <= state->pool.next_empty)
            {
                state->slots_reused++;
            }
        }
    }

    /* -- Collision: player vs trolls -- */
    if (thing_is_valid(player)) {
        for (thing_iter it = thing_iter_begin(&state->pool);
             thing_iter_valid(&it);
             thing_iter_next(&it))
        {
            thing *t = thing_iter_get(&it);
            if (t->kind != THING_KIND_TROLL) continue;

            float half_p = player->size / 2.0f;
            float half_t = t->size / 2.0f;
            float pcx = player->x + half_p, pcy = player->y + half_p;
            float tcx = t->x + half_t, tcy = t->y + half_t;

            float dx = pcx - tcx;
            float dy = pcy - tcy;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;

            if (dx < (half_p + half_t) && dy < (half_p + half_t)) {
                thing_pool_remove(&state->pool, it.current);
                state->trolls_killed++;
            }
        }
    }
}
```

The complete update cycle, every frame, visualized as a flow:

```
         ┌──────────────────┐
         │   Read input     │  (platform fills game_input)
         └────────┬─────────┘
                  ▼
         ┌──────────────────┐
         │  Move player     │  input → velocity → position → clamp
         └────────┬─────────┘
                  ▼
         ┌──────────────────┐
         │  Spawn trolls    │  timer tick → pool_add → set properties
         └────────┬─────────┘
                  ▼
         ┌──────────────────┐
         │  Check collisions│  iterate pool → AABB test → pool_remove
         └────────┬─────────┘
                  ▼
         ┌──────────────────┐
         │  Render          │  clear → iterate pool → draw_rect → HUD
         └──────────────────┘
```

Each step is independent and sequential. No callbacks. No events. No observer pattern. Just read the data, modify the data, draw the data. This is the game loop in its simplest form.

### Comparing with JavaScript game loops

In JavaScript, a simple game loop looks like this:

```js
function gameLoop(timestamp) {
    const dt = (timestamp - lastTime) / 1000;
    lastTime = timestamp;

    // Move player
    if (keys.left) player.x -= SPEED * dt;
    if (keys.right) player.x += SPEED * dt;

    // Spawn enemies
    spawnTimer += dt;
    if (spawnTimer >= SPAWN_INTERVAL) {
        spawnTimer = 0;
        enemies.push({ x: Math.random() * 800, y: Math.random() * 600 });
    }

    // Check collisions
    for (let i = enemies.length - 1; i >= 0; i--) {
        if (overlaps(player, enemies[i])) {
            enemies.splice(i, 1);
            score++;
        }
    }

    // Render
    ctx.clearRect(0, 0, 800, 600);
    ctx.fillStyle = 'orange';
    ctx.fillRect(player.x, player.y, 20, 20);
    for (const e of enemies) {
        ctx.fillStyle = 'blue';
        ctx.fillRect(e.x, e.y, 15, 15);
    }

    requestAnimationFrame(gameLoop);
}
```

Our C version does the exact same thing. The differences:

| JavaScript | C (this lesson) |
|---|---|
| `enemies.push(...)` | `thing_pool_add(...)` (no allocation, free list reuse) |
| `enemies.splice(i, 1)` | `thing_pool_remove(...)` (O(1), no array shift) |
| `enemies[i]` | `thing_pool_get_ref(...)` (generation-checked) |
| `enemies.forEach(...)` | `thing_iter` (skips dead slots automatically) |
| GC cleans up spliced enemies | Free list recycles slots immediately |

The logic is identical. The data management is fundamentally different. JavaScript's `splice` shifts all subsequent elements left --- O(n). Our `pool_remove` marks a slot unused --- O(1). JavaScript's GC eventually frees the spliced objects --- unpredictable timing. Our free list recycles the slot immediately --- deterministic.

---

## Build and run

```bash
cd course/
./build-dev.sh --backend=raylib -r
```

Verification checklist:

- [ ] Player (orange) moves with WASD and arrow keys
- [ ] Player stays within screen bounds (no edge overflow)
- [ ] Blue-ish trolls spawn every second at random positions
- [ ] Moving the player into a troll removes it (troll disappears)
- [ ] "Killed" counter increments on each collision
- [ ] "Reused" counter starts climbing after a few kills
- [ ] "Spawned" stops at 60 (MAX_TROLLS * 3)
- [ ] Pressing R resets everything to initial state
- [ ] Pressing Esc quits cleanly
- [ ] X11 backend works too: `./build-dev.sh --backend=x11 -r`
- [ ] Test harness still passes: `./build-dev.sh --test`

---

## What is happening in memory --- a play-by-play

Let us trace 10 seconds of actual gameplay through the pool data structures:

```
t=0s  game_init:
  Pool: [NIL] [PLAYER x=400 y=300] _ _ _ _ _ ...
  used: [ F ] [  T  ] F F F F F ...
  gens: [ 0 ] [  0  ] 0 0 0 0 0 ...
  next_empty=2  first_free=0
  HUD: Spawned:0  Killed:0  Reused:0

t=1s  first troll spawns:
  Pool: [NIL] [PLAYER] [TROLL x=234 y=456] _ _ ...
  used: [ F ] [  T  ] [  T  ] F F ...
  next_empty=3  first_free=0
  HUD: Spawned:1  Killed:0  Reused:0

t=2s  second troll:
  Pool: [NIL] [PLAYER] [TROLL] [TROLL x=567 y=123] _ ...
  next_empty=4  first_free=0
  HUD: Spawned:2  Killed:0  Reused:0

t=3s  player collides with troll at slot 2:
  thing_pool_remove(&pool, 2):
    used[2] = false
    things[2] = zeroed (kind=NIL, x=0, y=0, ...)
    generations[2] = 1  (was 0, bumped)
    first_free = 2
    things[2].next_sibling = 0  (old first_free, end of list)

  Pool: [NIL] [PLAYER] [___dead___] [TROLL] _ ...
  used: [ F ] [  T  ] [    F     ] [  T  ] F ...
  gens: [ 0 ] [  0  ] [    1     ] [  0  ] 0 ...
  next_empty=4  first_free=2
  HUD: Spawned:2  Killed:1  Reused:0

  ...third troll spawns in the same second:
  thing_pool_add pops slot 2 from the free list!

  Pool: [NIL] [PLAYER] [TROLL x=678 y=345] [TROLL] _ ...
    ^ slot 2 RECYCLED! generation is now 1.
    ^ anyone holding old ref {2,0} gets nil.
  first_free=0 (free list consumed)
  HUD: Spawned:3  Killed:1  Reused:1

t=5s  player kills troll at slot 3:
  generations[3] bumped to 1
  first_free = 3

t=6s  new troll spawns at slot 3 (recycled):
  HUD: Spawned:5  Killed:2  Reused:2
```

Every pool feature is visible in this trace:

| Feature | Where it appears |
|---|---|
| Nil sentinel (slot 0) | Never touched, never iterated, never drawn |
| `thing_pool_add` | Player creation in `game_init`, troll spawning every second |
| `thing_pool_get_ref` | Player lookup every frame (generational safety) |
| `thing_is_valid` | Guards all player access, guards troll initialization |
| `thing_pool_remove` | Collision response: zero + gen bump + free list push |
| Free list | New trolls land in recycled slots (Reused counter) |
| Generational IDs | Old refs to removed trolls return nil |
| Pool iterator | Render loop draws all living things; collision loop checks trolls |
| Kind enum | Collision loop filters by `THING_KIND_TROLL` |
| Zero-init | `memset(0)` on game_init resets everything; removed things are zeroed |

Notice what is NOT in this trace: no `malloc`, no `free`, no `new`, no `delete`, no garbage collection. The pool's memory footprint was fixed at startup (~53 KB). During 10 seconds of gameplay with spawning, killing, and recycling, not a single byte was allocated or freed. The pool just reuses what it already has.

---

### The iterator during collision

During collision detection, the iterator walks the pool like this:

```
Slot 0: NIL sentinel   → skip (used[0] = false)
Slot 1: PLAYER          → it.current=1, t->kind=PLAYER, skip (not TROLL)
Slot 2: TROLL (alive)   → it.current=2, t->kind=TROLL, check AABB → HIT → remove
Slot 3: (dead, removed) → skip (used[3] = false, never yielded by iterator)
Slot 4: TROLL (alive)   → it.current=4, t->kind=TROLL, check AABB → miss
Slot 5: (never used)    → iterator reaches next_empty → stops
```

The iterator from Lesson 10 skips dead slots automatically. You never iterate over removed trolls. You never iterate over the nil sentinel. The `for` loop just works.

### Exercise: the HUD counters

The HUD shows three counters that map directly to pool features:

| Counter | What it shows | Pool feature exercised |
|---|---|---|
| **Spawned** | Total trolls ever created | `thing_pool_add` (Lesson 07) |
| **Killed** | Total trolls removed by collision | `thing_pool_remove` (Lesson 07), free list push (Lesson 08), generation bump (Lesson 09) |
| **Reused** | Times a new troll landed in a recycled slot | Free list pop (Lesson 08) |

### Exercise: watch the full lifecycle

1. Run the demo. Wait for 5 trolls (Spawned: 5).
2. Kill 3 of them (Killed: 3). The free list now has 3 slots.
3. Wait for 3 more spawns. Watch Reused climb from 0 to 3.
4. Kill all trolls. Wait for more to spawn. All new trolls reuse old slots.
5. Press R to reset. Everything goes to zero. Fresh start.

---

## Comparing with the Asteroids compact_pool

If you did the Asteroids course, you used a different removal strategy: swap-with-tail compaction.

```
Asteroids approach (compact_pool):
  - All living things packed at the front of the array
  - Remove: swap dead thing with last living thing, decrement count
  - Pro: tight iteration (no gaps to skip)
  - Con: order changes on every removal;
         no stable indices; no generational safety

LOATs approach (free list + generation):
  - Living things can have gaps between them
  - Remove: mark unused, push onto free list, bump generation
  - Pro: stable indices (thing_idx never changes);
         generational safety; O(1) remove
  - Con: iterator must skip gaps
         (but the scan is cheap at 1024 slots)
```

The LOATs approach is strictly more powerful. You get stable references, stale detection, and O(1) removal. The cost is iterator overhead (scanning `used[]`), which is negligible for 1024 entities. At 10,000+ entities you might feel it, but by then you would be doing SoA refactoring anyway (Lesson 13 discusses this).

---

## Common mistakes

**Clamping to `SCREEN_W` instead of `SCREEN_W - player->size`.** The player's position is its top-left corner. If you clamp to 800, the right edge at x=820 is off screen. Always subtract the entity size.

**Forgetting `thing_is_valid` after `pool_add`.** Even in a 1024-slot pool with 5 entities, check anyway. The pattern costs one comparison and prevents silent nil-writes.

**Using `fabsf` in a tight collision loop.** Manual `if (dx < 0) dx = -dx` avoids a function call. It also makes the code self-explanatory.

**Removing during iteration without understanding the consequence.** Mid-iteration removal works in our pool (the iterator just advances past the dead slot), but you may skip an adjacent entity for one frame. For a demo, this is fine. For a shipped game, use deferred removal.

**Not resetting the spawn timer.** If you forget `state->spawn_timer = 0.0f` after spawning, the timer stays above `SPAWN_INTERVAL` and a new troll spawns every single frame (60 per second). You will see a wall of blue rectangles. The fix: always reset the timer.

**Spawning at the screen edge.** If you use `rng_float() * SCREEN_W` without the margin, trolls spawn at x=0 or x=800 --- partially off-screen. The formula `rng_float() * (SCREEN_W - TROLL_SIZE * 2) + TROLL_SIZE` ensures a margin on both sides.

**Comparing float positions with `==`.** Never check `player->x == troll->x`. Floating-point positions are almost never exactly equal. Always use range checks (like AABB) or threshold checks (`abs(a - b) < epsilon`).

---

## Connection to your work

Every feature in this lesson maps to a real-world game system:

| This lesson | Real game equivalent |
|---|---|
| Player movement with dt | Frame-rate-independent physics |
| Timer-based spawning | Wave systems, respawn timers |
| AABB collision | Broadphase collision detection |
| Pool remove on collision | Entity destruction |
| Free list reuse | Object pooling (Unity, Unreal both use this) |
| Generational ref check | Handle validation (Unreal's FName/FHandle) |
| HUD counters | Debug overlay / performance telemetry |

You now have a running game that exercises every piece of the things pool. Every feature from lessons 02--10 is live and visible.

### What you just proved

Think about what this game demo proves:

1. **The pool works under real conditions.** Not just `printf` in a test harness --- real entities being created, moved, destroyed, and recycled at 60 FPS.

2. **The nil sentinel prevents crashes.** Every `pool_get_ref` call checks the generation. If something goes wrong, you get nil, not a segfault.

3. **The free list works without you thinking about it.** You call `pool_add`. The pool checks the free list first. If there is a recycled slot, it uses it. If not, it takes a fresh slot. You never manage this manually.

4. **The iterator handles live modification.** You remove entities during iteration and the loop does not crash, does not skip valid entities (except possibly one for one frame in the adjacent-slot edge case), and does not access freed memory.

5. **Zero-init handles reset.** Press R and the entire game state --- pool, player, counters, RNG --- resets to a clean state. One `memset`. No cleanup code.

This is not a toy. This is the production pattern. Scale the entity count to 1000 (the Particle Lab does this), to 2000 (the Spatial Partition Lab does this), to infinite (the Growth Lab does this). The architecture holds.

---

## What's next

Lesson 13 is a review lesson --- no new code. We will:

- Draw the complete architecture diagram (every piece in one picture)
- Discuss what we gained and what we traded away
- Explore advanced variants: union, char buffer, separate pools, SoA
- Map our pool to Game Programming Patterns, Game Engine Architecture, and Handmade Hero
- Survey the five laboratories and what each one teaches

**Tip:** While the demo is running, press P for the Particle Laboratory (1000 entities, 6 archetypes) or L for the Data Layout Lab (AoS vs SoA comparison). These labs take the same pool system you just built and push it to its limits.
