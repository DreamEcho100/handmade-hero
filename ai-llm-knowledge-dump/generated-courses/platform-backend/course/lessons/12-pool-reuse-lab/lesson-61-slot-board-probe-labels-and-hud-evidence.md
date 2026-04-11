# Lesson 61: Slot Board, Probe Labels, and HUD Evidence

## Frontmatter

- Module: `12-pool-reuse-lab`
- Lesson: `61`
- Status: Required
- Target files:
  - `src/game/main.c`
- Target symbols:
  - `game_render_pool_lab`
  - `pool_lab_slot_is_active`
  - `hottest_slot`
  - `slot_reuses`
  - `game_draw_hud`
  - `Pool active:%d collisions:%u escapes:%u hottest:%d bursts:%u`

## Why This Lesson Exists

Lessons 58 through 60 established the allocator rules.

This lesson explains how the runtime turns those invisible rules into evidence the learner can actually read.

The Pool Lab would be much weaker if the learner had to infer everything from disappearing probes and a few counters. Instead, the scene deliberately exposes slot identity in both world space and slot space, then compresses the same story into the HUD and trace layers.

## Observable Outcome

By the end of this lesson, you should be able to explain why the slot board is the strongest visual proof in the module: it keeps backing-store identity fixed while moving probes come and go, so reuse can be observed without guessing.

## First Reading Strategy

Read the render path in layers.

1. barriers and probes in world space
2. per-probe slot/generation labels
3. slot-board cells across `scene->pool.slot_count`
4. world-space teaching labels printed into the scene
5. the HUD line that compresses the same story into one metric summary

Do not treat the board as an afterthought. It is the clearest allocator diagram in the entire module.

## Step 0: Ask What Would Be Lost Without the Slot Board

If the scene only showed moving probes, the learner could tell that occupancy changes over time, but it would be much harder to prove which stable slot was freed, stayed free for a while, and later became active again.

The board exists because motion, swap-remove, and repeated spawns make the world-space view intentionally noisy. The slot board keeps the backing-store identity anchored even when the moving scene gets harder to reason about.

## Step 1: Read the World Render First

`game_render_pool_lab(...)` starts by drawing barriers and live probes.

Barrier glow is driven by recent collision energy, which keeps churn causes visible. Then every active probe is drawn at its current world position.

Every third probe also gets a label:

```c
"s%d g%u"
```

That label is small, but it is one of the strongest teaching choices in the module.

- `s` shows stable slot identity
- `g` shows which numbered life of that slot you are looking at now

The label turns one animated square into allocator evidence.

## Step 2: Understand Why Not Every Probe Gets a Label

The code labels probes only when `(i % 3) == 0`.

That is a practical rendering decision, not a conceptual one. If every probe were labeled all the time, the world view would become visually noisy and harder to scan.

The scene therefore uses a mixed strategy:

- some probes carry explicit slot/generation labels in world space
- all slots remain visible in the fixed slot board

That balance keeps the world readable while still exposing enough identity evidence to teach the lesson.

## Step 3: Read the Slot Board as the Backing Store Diagram

The render path then iterates over every pool slot:

```c
for (size_t i = 0; i < scene->pool.slot_count; ++i)
```

and draws a small cell for each one.

This is not a second gameplay layer. It is a live map of the allocator backing storage. Because the code iterates over the real slot count, the board stays tied to actual pool capacity instead of a made-up teaching number.

## Visual: What the Slot Board Proves

```text
slot 0  slot 1  slot 2  slot 3 ...
  on      off     on      off

bright   -> active now
dim      -> currently free
warmer   -> accumulated more reuse
special tint -> current hottest reused slot
```

The crucial property is stability. Slot 3 is still slot 3 whether it is active, free, or active again later.

## Step 4: Read the Cell State Rules Carefully

Each cell derives two important booleans:

- `used = pool_lab_slot_is_active(scene, (int)i)`
- `hottest = scene->hottest_slot == (int)i`

It then varies color using three real facts:

- whether the slot is active right now
- how much reuse history it has accumulated through `slot_reuses[i]`
- whether this slot is currently the hottest reuse location

So one cell is carrying three kinds of meaning at once:

- occupancy now
- reuse history over time
- focused attention on the strongest reuse candidate

That makes the slot board much more than a binary used/free grid.

## Step 5: Read `pool_lab_slot_is_active(...)` as the Occupancy Bridge

This helper checks whether any live probe currently reports the requested slot index.

That detail matters because the board is not driven by duplicated bookkeeping. It is derived from the same live occupancy the world render uses.

The runtime is therefore presenting the same truth twice:

- moving probes in world space
- fixed slot occupancy in allocator space

Those two views look different, but they must agree.

## Worked Example: Reading One Reused Slot Across Surfaces

Suppose slot 6 was recently freed, then allocated again.

What should the learner be able to observe?

1. one live probe may show a label like `s6 g2`
2. the slot-board cell for slot 6 should be bright again because it is active
3. the same cell may look warmer because `slot_reuses[6]` increased
4. if slot 6 now leads long-term reuse, `hottest_slot` may point at it

That is the same allocator fact expressed through several synchronized surfaces.

## Step 6: Read the Scene’s Own Labels as Teaching Guardrails

The world render also prints two plain-language labels:

```text
barriers force churn so freed slots get reused instead of forgotten
slot board: active, freed, and hottest reused slot
```

That wording matters. The scene is not forcing the learner to infer the intended allocator lesson from graphics alone. It names the lesson while the evidence is visible.

## Step 7: Read the Pool HUD Metric Line as the Summary Layer

In `game_draw_hud(...)`, the Pool summary line is:

```text
Pool active:%d collisions:%u escapes:%u hottest:%d bursts:%u
```

This line compresses the pool story into five high-value numbers:

- current occupancy
- collision-driven churn
- probes that escaped before return
- the currently hottest reused slot index
- recent manual burst pressure

The board shows structure and identity. The HUD shows totals and trend.

## Step 8: Notice How the Slot Board and HUD Divide Labor

The slot board answers spatial questions:

- which exact slot is active?
- which slots are free?
- where is reuse concentrating?

The HUD answers aggregate questions:

- how much churn has happened?
- how much pressure did the learner inject?
- which slot is currently most reused overall?

That division is why both surfaces are needed. Neither one replaces the other.

## Common Mistake: Thinking the Slot Board Is Only a Debug Grid

It is more than that.

The board is the most reliable visual proof in the module because it preserves slot identity while everything else moves, disappears, and gets recycled. Without it, the learner would have to infer reuse mostly from counters and occasional labels.

## JS-to-C Bridge

This is like pairing a live list of active workers with a stable dashboard of worker slots underneath. The moving list tells you what is currently busy. The stable board tells you which reusable slots have been carrying the load over time.

## Try Now

Open `src/game/main.c`, then do these checks:

1. Read `game_render_pool_lab(...)` and explain why the probe labels combine slot index and generation.
2. Read the slot-board loop and explain what each cell is proving.
3. Read `pool_lab_slot_is_active(...)` and explain why the board stays tied to real occupancy.
4. Find the Pool HUD metric line and explain why `hottest` belongs beside collisions and bursts.
5. Explain why the scene uses both world-space labels and a fixed slot board instead of only one of them.

## Exercises

1. Explain why the slot board is a stronger proof surface than moving probes alone.
2. Explain why the hottest slot should be highlighted visually.
3. Explain the difference between world-space evidence and slot-space evidence.
4. Describe how the Pool scene uses labels and summary metrics to reduce ambiguity for the learner.

## Runtime Proof Checkpoint

At this point, you should be able to explain how the Pool scene visualizes allocator truth: live probes show current occupants, labels attach slot identity and generation to some of those occupants, the slot board shows stable backing-store state, and the HUD summarizes churn pressure at a glance.

## Recap

This lesson explained the Pool Lab proof surfaces:

- world probes show current slot occupants under motion
- selected probe labels map occupants back to stable slot identities and generations
- the slot board visualizes occupancy and reuse concentration in fixed slot space
- world labels explain the allocator lesson directly
- the HUD line compresses the same story into a quick summary

Next, we connect those proof surfaces to the scene's guided exercise flow and audio warning logic so the learner can read delayed reuse and saturation pressure as one coherent story.
