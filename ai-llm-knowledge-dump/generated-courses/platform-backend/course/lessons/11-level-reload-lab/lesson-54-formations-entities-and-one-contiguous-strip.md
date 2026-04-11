# Lesson 54: Formations, Entities, and One Contiguous Strip

## Frontmatter

- Module: `11-level-reload-lab`
- Lesson: `54`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_fill_level_reload`
  - `ARENA_PUSH_ZERO_ARRAY`
  - `LevelFormation`
  - `LevelEntity`
  - `formation_count`
  - `entity_count`
  - `reload_seed`
  - `build_serial`

## Observable Outcome

By the end of this lesson, you should be able to explain the rebuilt payload precisely: three formation headers plus one contiguous strip of eighteen entities, all regenerated from the current reload seed.

## Why This Lesson Exists

Lesson 53 explained when the scene rebuilds.

This lesson explains what gets rebuilt.

That matters because the Level Reload Lab is only convincing if the learner can point to the exact payload shape and say:

```text
these headers and these entities were allocated together
and the scene can rebuild them deterministically from one reload seed
```

## First Reading Strategy

Read the fill function from allocation shape to generated content:

1. the two array allocations
2. formation header construction
3. entity payload construction
4. the seeded focus hook at the end

That order keeps the ownership structure visible while the procedural details accumulate.

## Step 0: Separate Headers From Payload Before Reading The Loops

Before reading either loop, keep one distinction explicit:

```text
formations describe groups
entities are the real scene payload being grouped
```

If that distinction stays clear, the contiguous strip design becomes much easier to understand.

## Step 1: Read the Allocation Block First

Inside `game_fill_level_reload(...)` the scene begins with:

```c
scene->formation_count = GAME_LEVEL_FORMATION_COUNT;
scene->entity_count = GAME_LEVEL_ENTITY_COUNT;
scene->formations =
    ARENA_PUSH_ZERO_ARRAY(level, scene->formation_count, LevelFormation);
scene->entities =
    ARENA_PUSH_ZERO_ARRAY(level, scene->entity_count, LevelEntity);
scene->build_serial = reload_count;
scene->reload_seed = reload_count * 17u + 3u;
```

This block says almost everything important.

The scene allocates exactly two arrays:

- formation headers
- entity payload

Both come from the level arena.

Both belong to the same rebuild.

## Step 2: Read the Counts as Scene Shape, Not Incidental Numbers

The scene uses:

- `GAME_LEVEL_FORMATION_COUNT`
- `GAME_LEVEL_ENTITY_COUNT`

The runtime then treats those counts as a fixed scene shape.

In the current build that means:

- 3 clusters
- 18 entities

This is not random decoration.

It is small enough to read visually and large enough to make contiguity meaningful.

## Visual: Scene Payload Shape

```text
formations[3]
  cluster_1 -> entity_start 0  entity_count 6
  cluster_2 -> entity_start 6  entity_count 6
  cluster_3 -> entity_start 12 entity_count 6

entities[18]
  [0][1][2][3][4][5][6]...[17]
```

The formations describe the groups.

The entity array is the actual strip.

## Step 3: Understand What the Seed Is Doing

The scene computes:

```c
scene->reload_seed = reload_count * 17u + 3u;
```

This is a very simple deterministic seed.

Its job is not cryptographic randomness.

Its job is proof.

On every rebuild:

- the seed changes
- cluster colors shift
- entity jitter shifts
- the learner can see fresh data was generated

That lets the scene prove rebuild without requiring complicated procedural logic.

## Step 4: Read the Formation Fill Loop as Header Construction

Each formation gets:

- a stable x/y region
- a width and height
- a hue-derived color
- an `entity_start`
- an `entity_count`
- a label like `cluster_1`

The important fields are not the colors.

They are:

- `entity_start`
- `entity_count`

Those two fields are the bridge from visual cluster boxes to the contiguous entity strip.

They mean:

```text
cluster header describes where its entity run begins
but does not own a separate allocation
```

## Step 5: Read the Entity Fill Loop as Payload Construction

The entity loop computes:

- `formation_index`
- local column and row
- seeded jitter
- world position
- display color
- heat
- `serial`

The most important proof field here is:

```c
entity->serial = (unsigned int)entity_index + 1u + reload_count * 100u;
```

This gives each rebuilt entity a serial range tied to the current rebuild.

So the entity strip is not only positioned differently.

It carries rebuild identity inside the payload.

## Step 6: Explain Why the Entity Strip Is the Real Lesson

The scene could have allocated one array per formation.

It does not.

Instead, it allocates one contiguous entity array and lets formations point into it.

That is the ownership lesson.

```text
many groups
one scene-owned payload strip
one reset point
```

## Worked Example: Why Three Separate Entity Arrays Would Weaken The Lesson

If each formation owned its own separate entity allocation, the scene could still render three clusters, but the lesson about one contiguous strip would disappear.

The current design is stronger because the clusters are only a header view over one shared payload run. That is what makes the bottom memory strip honest proof instead of a decorative simplification.

This is why later scan logic can walk the strip linearly.

## Step 7: Notice the Seeded Highlight Hook

At the end of fill:

```c
scene->highlighted_entity =
    (int)(scene->reload_seed % (unsigned int)scene->entity_count);
```

Even the initial highlighted entity comes from the rebuild seed.

That is useful because it means the learner can see that rebuild affects:

- layout
- color
- focus

The scene is making rebuild visible on purpose.

## Common Mistake: Thinking the Clusters Own Their Own Child Arrays

They do not.

The cluster headers only describe ranges.

The entity data lives in one shared strip.

That is why one level-arena reset can discard the whole payload cleanly.

## JS-to-C Bridge

This is like storing section headers plus offsets into one flat array instead of nesting many separately allocated lists. The headers still describe groups, but the payload is kept contiguous so reset and traversal stay cheap.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Find the allocation block in `game_fill_level_reload(...)` and explain why there are exactly two array allocations.
2. Read the formation loop and explain what `entity_start` and `entity_count` mean.
3. Read the entity loop and explain how `reload_seed` changes both layout and identity.
4. Find `highlighted_entity` initialization and explain why it belongs in the fill function.

## Exercises

1. Explain why the entity strip is the real ownership payload of the scene.
2. Explain why deterministic seeding is better here than hard-coded static coordinates.
3. Explain why `entity->serial` is a stronger proof hook than color alone.
4. Describe the relationship between cluster headers and the contiguous entity strip.

## Runtime Proof Checkpoint

At this point, you should be able to explain the rebuilt payload precisely: three scene-owned formation headers point into one contiguous array of eighteen scene-owned entities, and both the visuals and the serial range are derived from the current reload seed.

## Recap

This lesson explained the content of a Level Reload rebuild:

- the fill function allocates exactly two level-owned arrays
- formations are headers that describe cluster ranges
- entities are the real contiguous payload
- `reload_seed` and `serial` make each rebuild visibly different

Next, we follow the update loop so the learner can separate passive scan traversal, rebuild flash decay, and the manual `SPACE` spike that heats the scene without reallocating it.
