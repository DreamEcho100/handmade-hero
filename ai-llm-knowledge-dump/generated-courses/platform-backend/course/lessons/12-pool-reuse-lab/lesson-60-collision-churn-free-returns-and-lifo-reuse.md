# Lesson 60: Collision Churn, Free Returns, and LIFO Reuse

## Frontmatter

- Module: `12-pool-reuse-lab`
- Lesson: `60`
- Status: Required
- Target files:
  - `src/game/main.c`
  - `src/utils/arena.h`
- Target symbols:
  - `game_update_pool_lab`
  - `pool_lab_spawn`
  - `pool_free`
  - `last_freed`
  - `collision_count`
  - `escaped_count`
  - `barrier->energy`
  - `spawn_accum`

## Why This Lesson Exists

Lesson 59 showed how one free slot becomes a live probe.

This lesson explains the other half of the cycle: how one live probe becomes a returned slot again, and why that return path is exactly what makes reuse observable.

Without this round trip, the scene would only teach allocation. The real point of the Pool Lab is the loop:

```text
allocate
-> stay active under pressure
-> return to the free list
-> be available for reuse
```

## Observable Outcome

By the end of this lesson, you should be able to explain the churn loop in order: the scene keeps applying allocation pressure, collisions and motion shorten probe lifetimes, expired or escaped probes are returned through `pool_free(...)`, and the newest returned slot becomes the free-list head waiting for the next reuse opportunity.

## First Reading Strategy

Read the update loop in temporal order instead of jumping straight to `pool_free(...)`.

1. start with automatic spawning
2. then read the `SPACE` burst path
3. then read movement, collisions, and TTL pressure
4. then read the free path
5. only after that, read `pool_free(...)` in `arena.h`

That order matches the runtime and makes the later LIFO explanation much easier to reason about.

## Step 0: Remember What Pressure Is Doing Here

`SPACE` does not create the pool model.

It accelerates it.

The free-list cycle already exists because the scene keeps spawning probes and probes eventually expire or leave the scene. Manual burst input simply makes those events happen faster and more visibly.

## Step 1: Read the Automatic Spawn Rhythm as Ongoing Allocation Pressure

At the top of `game_update_pool_lab(...)`, the scene keeps introducing work:

```c
scene->spawn_accum += dt;
if (scene->spawn_accum >= 0.28f) {
  pool_lab_spawn(scene, 1);
  scene->spawn_accum = 0.0f;
}
```

This is important because reuse only becomes visible when new allocation pressure keeps arriving over time. A single startup burst would not be enough to demonstrate the full return-and-reuse cycle clearly.

The scene is therefore continuously asking the allocator:

```text
do you have another free slot I can activate now?
```

## Step 2: Read the `SPACE` Burst as Deliberate Churn Injection

On manual input, the scene does this:

```c
pool_lab_spawn(scene, 4);
scene->burst_count++;
if (game)
  game_log_eventf(game, "pool burst -> active %d", scene->active_count);
game_play_sound_at(audio, SOUND_TONE_HIGH);
if (game)
  game_fire_scene_warning_probe(game, GAME_SCENE_POOL_LAB);
```

That cluster of effects matters because the scene is not only making more probes appear. It is leaving synchronized evidence that the learner intentionally stressed the pool.

- new probes increase occupancy pressure
- `burst_count` records deliberate churn injection
- the event log captures the moment explicitly
- the tone and warning probe connect allocator pressure to audio evidence

This is the Pool Lab equivalent of a deliberate prove-it burst.

## Step 3: Read the Motion and Collision Block as Churn Production

Each active probe moves, loses TTL, and checks overlap against the barriers. On collision, the scene does this:

```c
probe->vx = -fabsf(probe->vx) * 0.92f;
probe->vy = probe->vy >= 0.0f ? probe->vy + 0.35f : probe->vy - 0.35f;
probe->ttl -= 0.28f;
barrier->energy = 1.0f;
scene->collision_count++;
any_collision = 1;
```

Every one of those changes serves the allocator lesson.

- bounce keeps the probe in circulation long enough to stay interesting
- extra TTL loss makes eventual return happen sooner
- `barrier->energy` leaves visible proof that churn happened here
- `collision_count` gives the trace layer measurable allocator pressure evidence

So collisions are not just gameplay noise. They are the mechanism that turns motion into allocator churn.

## Visual: The Pool Churn Loop

```text
spawn probe
  -> move through the world
  -> collide and lose time
  -> stay busy long enough to create pressure
  -> expire or escape
  -> return slot to free list
  -> later spawn may reuse that same slot
```

That is the full allocator-teaching loop of the scene.

## Step 4: Read the Free Path as the Mirror of Spawn

When a probe expires or leaves the scene bounds, the update loop does this:

```c
if (probe->ttl <= 0.0f || probe->x < 0.4f || probe->x > 15.5f) {
  if (probe->x > 15.0f)
    scene->escaped_count++;
  scene->last_freed = probe;
  pool_free(&scene->pool, probe);
  scene->active[i] = scene->active[scene->active_count - 1];
  scene->active_count--;
  continue;
}
```

This is the central allocator moment in the module.

The scene:

1. decides the probe's current life is over
2. records the exact pointer in `last_freed`
3. returns that pointer to the free list
4. removes the probe from active occupancy

That one pointer can now become the next immediate reuse hit.

Notice again that `last_freed` is captured before `pool_free(...)` mutates the free-list head. The scene is preserving direct evidence about which exact slot just returned.

## Step 5: Connect `pool_free(...)` to LIFO Behavior

In `src/utils/arena.h`, `pool_free(...)` is:

```c
node = (PoolFreeNode *)ptr;
node->next = pool->free_list;
pool->free_list = node;
++pool->free_count;
```

That is stack-like, or LIFO, behavior.

The most recently freed slot becomes the next candidate for allocation because it is pushed onto the front of the free list.

That is why `probe == scene->last_freed` is such a strong proof hook. When the next allocation returns the same pointer, the learner is seeing the free-list discipline directly instead of merely inferring it from theory.

## Worked Example: One Probe's Round Trip

Imagine one probe hits a barrier twice, loses enough TTL, then exits the scene.

1. `collision_count` increases as it bounces
2. a barrier flashes because `energy` jumps to `1.0f`
3. the probe eventually fails the lifetime or bounds check
4. its pointer is copied into `last_freed`
5. `pool_free(...)` pushes that pointer onto the free-list head
6. the next spawn cycle may pop that same pointer right back out

That is not just a gameplay event. It is the allocator cycle made visible.

## Step 6: Notice the Scene Uses Swap-Remove for `active[]`

After freeing a probe, the scene keeps the active array dense by swap-removing:

```c
scene->active[i] = scene->active[scene->active_count - 1];
scene->active_count--;
continue;
```

That choice fits the whole module.

```text
fixed storage
cheap returns
cheap active-list maintenance
```

The scene does not care about preserving stable ordering inside `active[]`. It cares about cheap occupancy bookkeeping while the real long-term identity lives at the slot level.

## Step 7: Read `escaped_count` and `collision_count` as Different Kinds of Churn Evidence

The scene tracks both:

- `collision_count`: how much turbulence and contact pressure occurred
- `escaped_count`: how many probes made it far enough right to count as exits before being returned

Those counters are not duplicates.

One describes how noisy the path to return was. The other describes one specific class of end-of-life event. Together they help the learner understand that slots can be returned through different world situations while still feeding the same free-list mechanism.

## Step 8: Read Barrier Energy as Visible Cause

After collisions, each barrier's energy cools back down:

```c
scene->barriers[i].energy -= dt * 2.2f;
if (scene->barriers[i].energy < 0.0f)
  scene->barriers[i].energy = 0.0f;
```

That glow is not ornamental. It leaves short-lived evidence of where churn came from so the learner does not have to infer everything from counters and disappearing probes.

## Common Mistake: Thinking Reuse Comes Only From Pressing `SPACE`

`SPACE` increases pressure, but it does not create the reuse mechanism.

The actual free-list cycle is always:

- allocate over time
- collide and age under pressure
- return a slot to the free list
- allocate again later

`SPACE` only makes that sequence happen faster and with clearer synchronized evidence.

## JS-to-C Bridge

This is like returning one worker object to a stack-based object pool and then getting that same worker back on the next request. The key fact is not that the old object disappeared. The key fact is that the same slot became free and immediately borrowable again.

## Try Now

Open `src/game/main.c` and `src/utils/arena.h`, then do these checks:

1. Read the automatic spawn rhythm and explain why the scene needs continuous allocation pressure.
2. Read the `SPACE` burst path and explain how it differs from the normal spawn cadence.
3. Read the collision block and explain how barrier hits contribute to churn.
4. Read the free block and explain why `last_freed` is captured before `pool_free(...)`.
5. Read `pool_free(...)` and explain why the next allocation may immediately reuse the same pointer.

## Exercises

1. Explain the full free-list cycle from spawn to free to reuse.
2. Explain why `escaped_count` and `collision_count` are both useful churn metrics.
3. Explain why swap-remove is a good choice for `active[]` here.
4. Describe how the scene turns collisions into allocator-teaching evidence.

## Runtime Proof Checkpoint

At this point, you should be able to explain why the Pool scene is a reuse lab instead of just a motion lab: continuous pressure produces returns, returns push slots to the free-list head, and the newest returned slot is positioned to become the next visible reuse candidate.

## Recap

This lesson explained how pool churn is generated and observed:

- the scene applies both steady and burst allocation pressure
- collisions shorten lifetimes while leaving visible evidence behind
- expired or escaped probes return slots through `pool_free(...)`
- the LIFO free list makes immediate reuse a natural next event
- swap-remove keeps current occupancy cheap while slot identity stays stable elsewhere

Next, we move from behavior to proof surfaces and read how the slot board, probe labels, HUD metrics, and hottest-slot highlight make that invisible allocator cycle visible on screen.
