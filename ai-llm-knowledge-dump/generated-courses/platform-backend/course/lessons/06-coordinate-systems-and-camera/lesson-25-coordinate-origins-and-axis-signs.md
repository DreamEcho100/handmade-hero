# Lesson 25: Coordinate Origins, Axis Signs, and Configurable World Meaning

## Frontmatter

- Module: `06-coordinate-systems-and-camera`
- Lesson: `25`
- Status: Required
- Target files:
  - `src/utils/base.h`
  - `src/utils/render_explicit.h`
- Target symbols:
  - `CoordOrigin`
  - `COORD_ORIGIN_LEFT_BOTTOM`
  - `COORD_ORIGIN_LEFT_TOP`
  - `COORD_ORIGIN_RIGHT_BOTTOM`
  - `COORD_ORIGIN_RIGHT_TOP`
  - `COORD_ORIGIN_CENTER`
  - `COORD_ORIGIN_CUSTOM`
  - `world_config_x_sign`
  - `world_config_y_sign`
  - `camera_pan`
  - `screen_move`

## Observable Outcome

By the end of this lesson, you should be able to reduce any supported coordinate convention to three concrete answers:

- where is `(0, 0)`?
- which way does positive `x` go?
- which way does positive `y` go?

You should also be able to explain why later movement helpers use sign values instead of branching on the origin enum every time.

## Prerequisite Bridge

Lesson 24 introduced `GameWorldConfig` as the meaning layer for world space.

The next question is more specific:

```text
what exact convention does that world space use?
```

That is what `CoordOrigin` and the sign helpers make explicit.

## Why This Lesson Exists

World units alone are not enough.

The renderer still needs exact answers to these questions:

- where is zero?
- does positive `y` mean up or down?
- does positive `x` mean left-to-right or right-to-left?

If those answers stay implicit, later rectangle math and camera motion feel arbitrary.

If they become named data, the rest of the render path becomes teachable.

## Step 1: Read `CoordOrigin` as a Menu of Coordinate Conventions

In `src/utils/base.h`:

```c
typedef enum CoordOrigin {
  COORD_ORIGIN_LEFT_BOTTOM = 0,
  COORD_ORIGIN_LEFT_TOP,
  COORD_ORIGIN_RIGHT_BOTTOM,
  COORD_ORIGIN_RIGHT_TOP,
  COORD_ORIGIN_CENTER,
  COORD_ORIGIN_CUSTOM,
} CoordOrigin;
```

This is the runtime saying:

```text
coordinate convention is explicit data,
not hidden engine folklore
```

That is a strong design choice.

## Step 2: Translate the Standard Modes Into Plain English

### `COORD_ORIGIN_LEFT_BOTTOM`

```text
(0,0) is bottom-left
x grows right
y grows up
```

### `COORD_ORIGIN_LEFT_TOP`

```text
(0,0) is top-left
x grows right
y grows down
```

### `COORD_ORIGIN_RIGHT_BOTTOM`

```text
(0,0) is bottom-right
x grows left
y grows up
```

### `COORD_ORIGIN_RIGHT_TOP`

```text
(0,0) is top-right
x grows left
y grows down
```

### `COORD_ORIGIN_CENTER`

On the live branch, this is treated as a centered variant with:

```text
(0,0) at the screen center
x grows right
y grows up
```

### `COORD_ORIGIN_CUSTOM`

This is the escape hatch.

The game fills in offsets and signs directly.

## Step 3: Notice the Zero-Init Story

`COORD_ORIGIN_LEFT_BOTTOM` is explicitly `0`.

That means a zero-initialized `GameWorldConfig` naturally lands on the default bottom-left / y-up convention.

This is not just convenience.

It gives the runtime a safe baseline convention without extra setup.

## Step 4: Read the Sign Helpers as the Real Reduction Step

The most important helpers in this lesson are:

```c
static inline float world_config_y_sign(const GameWorldConfig *cfg)
static inline float world_config_x_sign(const GameWorldConfig *cfg)
```

They reduce a rich origin convention down to two small facts the rest of the runtime can reuse:

```text
x_sign
y_sign
```

This is the real design move.

Instead of repeatedly branching on the enum in hot code, later helpers can multiply by signs.

## Step 5: Walk the Live `world_config_y_sign(...)`

From `src/utils/base.h`:

```c
static inline float world_config_y_sign(const GameWorldConfig *cfg) {
  if (cfg->coord_origin == COORD_ORIGIN_CUSTOM)
    return cfg->custom_y_sign;
  return (cfg->coord_origin == COORD_ORIGIN_LEFT_TOP ||
          cfg->coord_origin == COORD_ORIGIN_RIGHT_TOP)
             ? 1.0f
             : -1.0f;
}
```

So the live rule is:

```text
top-based y-down origins  -> +1
all standard y-up origins -> -1
custom mode               -> whatever the game supplied
```

This lets later code ask a small question:

```text
does positive world y agree with top-down pixel growth,
or should it be flipped?
```

## Step 6: Walk the Live `world_config_x_sign(...)`

Also from `src/utils/base.h`:

```c
static inline float world_config_x_sign(const GameWorldConfig *cfg) {
  if (cfg->coord_origin == COORD_ORIGIN_CUSTOM)
    return cfg->custom_x_sign;
  return (cfg->coord_origin == COORD_ORIGIN_RIGHT_BOTTOM ||
          cfg->coord_origin == COORD_ORIGIN_RIGHT_TOP)
             ? -1.0f
             : 1.0f;
}
```

So the live rule is:

```text
right-side / RTL origins -> -1
left-side / LTR origins  -> +1
custom mode              -> whatever the game supplied
```

That is the horizontal version of the same reduction.

## Visual: Convention Reduced to Data

```text
CoordOrigin
  -> x_sign
  -> y_sign
  -> offsets chosen later by the render context
```

The enum is the high-level label.
The sign helpers are the reusable low-level facts.

## Step 7: Why Movement Helpers Want Signs Instead of the Enum

In `src/utils/render_explicit.h`, helpers like `camera_pan(...)` and `screen_move(...)` accept sign values rather than re-decoding the enum internally.

That is a deliberate separation of responsibility:

```text
base.h decides what the active convention means
movement helpers consume small numeric facts derived from that meaning
```

This keeps movement logic generic and avoids repeated convention-specific branches.

## Worked Example: Why X and Y Need Separate Sign Logic

Suppose the current origin is `COORD_ORIGIN_RIGHT_TOP`.

Then:

```text
x_sign = -1
y_sign = +1
```

That means:

- positive world `x` moves left
- positive world `y` moves down

If a helper had only one “flipped or not” flag, it would lose this axis-specific distinction.

That is why the runtime keeps separate x and y signs.

## Step 8: Understand `COORD_ORIGIN_CUSTOM` as Full Responsibility Mode

Custom mode is powerful because it does not try to reinterpret your choices.

The sign helpers simply forward:

- `custom_x_sign`
- `custom_y_sign`

That means the developer owns the convention completely.

The runtime does not “fix” it later.

This is exactly why the helpers stay small and honest.

## Practical Exercises

### Exercise 1: Decode the Convention

For each origin, write the meaning in plain language:

1. `COORD_ORIGIN_LEFT_BOTTOM`
2. `COORD_ORIGIN_LEFT_TOP`
3. `COORD_ORIGIN_RIGHT_TOP`

Expected structure:

```text
where zero is
which way +x goes
which way +y goes
```

### Exercise 2: Predict the Signs

What signs do the live helpers return for:

1. `COORD_ORIGIN_LEFT_BOTTOM`
2. `COORD_ORIGIN_LEFT_TOP`
3. `COORD_ORIGIN_RIGHT_BOTTOM`
4. `COORD_ORIGIN_RIGHT_TOP`

Expected result:

```text
LEFT_BOTTOM  -> x +1, y -1
LEFT_TOP     -> x +1, y +1
RIGHT_BOTTOM -> x -1, y -1
RIGHT_TOP    -> x -1, y +1
```

### Exercise 3: Explain the Design

Why is it useful that `camera_pan(...)` and `screen_move(...)` can accept sign values directly?

Expected answer:

```text
because the coordinate convention can be reduced once into x/y sign facts and then reused by generic movement helpers without repeated enum branches
```

## Common Mistakes

### Mistake 1: Treating origin choice as only a rendering detail

It also affects movement reasoning, camera behavior, and rectangle-edge selection.

### Mistake 2: Thinking one sign is enough

X and Y can differ independently, so the runtime needs separate axis signs.

### Mistake 3: Forgetting that custom mode is developer-owned

The runtime forwards custom signs as-is. It does not reinterpret them later.

## Troubleshooting Your Understanding

### “I keep memorizing the enum names instead of understanding them”

Translate every mode into the same three questions:

```text
where is zero?
which way is +x?
which way is +y?
```

### “Why do top origins get `y_sign = +1`?”

Because top-based conventions align with top-down pixel-space growth instead of requiring a flip.

## Recap

You now have the runtime's coordinate-convention reduction model:

1. `CoordOrigin` names the active convention
2. `world_config_x_sign(...)` and `world_config_y_sign(...)` reduce it to reusable axis facts
3. those sign values let later helpers stay generic
4. custom mode hands full responsibility to the game layer

## Next Lesson

Lesson 26 uses these signs together with offsets, scale, and camera state to bake a full `RenderContext` once per frame so draw helpers can stay branch-free in the hot path.
