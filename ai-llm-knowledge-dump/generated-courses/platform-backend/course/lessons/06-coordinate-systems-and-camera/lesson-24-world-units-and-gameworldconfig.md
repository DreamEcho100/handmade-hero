# Lesson 24: World Units, HUD Anchors, and `GameWorldConfig`

## Frontmatter

- Module: `06-coordinate-systems-and-camera`
- Lesson: `24`
- Status: Required
- Target files:
  - `src/utils/base.h`
  - `src/platform.h`
  - `src/game/main.c`
- Target symbols:
  - `GAME_W`
  - `GAME_H`
  - `GAME_UNITS_W`
  - `GAME_UNITS_H`
  - `HUD_TOP_Y`
  - `GameWorldConfig`
  - `platform_game_props_init`
  - `game_make_world_context`

## Observable Outcome

By the end of this lesson, you should be able to explain why the runtime keeps two size systems at once:

- pixel dimensions for the backbuffer storage target
- world-unit dimensions for logical gameplay layout

You should also be able to explain what `GameWorldConfig` means before any draw helper is called, and why HUD anchoring needs a small bridge macro like `HUD_TOP_Y(...)`.

## Prerequisite Bridge

Modules 03 through 05 taught two important pipeline rules:

```text
rendering needs a shared image contract
audio needs a shared sample contract
```

Module 06 adds the spatial equivalent:

```text
the game also needs a shared meaning for coordinates
before camera math and draw helpers can be trustworthy
```

Without that meaning layer, later scene code would fall back into raw-pixel guessing.

## Why This Lesson Exists

The backbuffer already knows how big it is in pixels.

That is not enough for gameplay reasoning.

The game still needs cleaner questions like:

- where is the player in world space?
- how big is one bar, tile, or lane logically?
- how far from the top should a HUD element sit?
- what does camera pan or zoom actually mean?

Those questions are easier in world units than in raw pixels.

## Step 1: Read the Two Size Systems Side by Side

In `src/utils/base.h`:

```c
#define GAME_W 800
#define GAME_H 600

#define GAME_UNITS_W 16.0f
#define GAME_UNITS_H 12.0f
```

These are not duplicates.

They answer different questions.

### Pixel dimensions

```text
GAME_W / GAME_H
```

tell you how large the base backbuffer is in pixels.

### World dimensions

```text
GAME_UNITS_W / GAME_UNITS_H
```

tell you how large the logical gameplay space is in world units.

## Worked Example: Base Pixels Per World Unit

At the default dimensions:

$$
\frac{800}{16} = 50
$$

and:

$$
\frac{600}{12} = 50
$$

So the base runtime chooses:

```text
1 world unit = 50 pixels horizontally
1 world unit = 50 pixels vertically
```

That symmetry is deliberate. It means a world-square stays square in the base backbuffer.

## Step 2: Separate the Two Questions Early

When you see both sets of dimensions, always ask two different questions:

```text
how big is the storage target?
  -> pixels

how big is the logical game space?
  -> world units
```

If those collapse together, camera and HUD lessons become much harder.

## Visual: Two Spaces, One Runtime

```text
world space:
  16 x 12 logical units
  good for gameplay reasoning

pixel space:
  800 x 600 stored pixels
  good for the backbuffer and raster output
```

The rest of the module is about bridging these two spaces cleanly.

## Step 3: Why World Units Exist At All

If gameplay lived directly in pixels, all of these would become messier:

- camera pan and zoom
- scene layout that survives resize policy changes
- HUD anchoring that should feel stable on screen
- teaching coordinate conventions independently from storage resolution

World units give the game a logical space that can be reasoned about before anything is turned into pixels.

## Worked Example: One World Position at Base Scale

If one world unit is `50` pixels, then a world point at `(4, 3)` conceptually lands at:

$$
x = 4 \times 50 = 200 \text{ px}
$$

$$
y = 3 \times 50 = 150 \text{ px}
$$

That is not the whole render-context story yet, because origin and camera still matter.

But it is the right first intuition.

## Step 4: Read `HUD_TOP_Y(...)` as a Small But Important Bridge

Also in `src/utils/base.h`:

```c
#define HUD_TOP_Y(offset_from_top) \
  ((float)GAME_UNITS_H - (float)(offset_from_top))
```

The default world convention is bottom-left with positive `y` going up.

But HUD reasoning is often human-top-oriented:

```text
place this 0.08 units below the top edge
```

`HUD_TOP_Y(...)` converts that top-relative thought into the bottom-left world `y` value the renderer expects.

### Example

```text
HUD_TOP_Y(0.08) = 12.0 - 0.08 = 11.92
```

So the macro is not decoration.

It is a bridge between two ways of describing the same vertical placement.

## Step 5: Read `GameWorldConfig` as the Meaning of World Space

In `src/utils/base.h`:

```c
typedef struct GameWorldConfig {
  CoordOrigin coord_origin;
  float custom_x_offset;
  float custom_y_offset;
  float custom_x_sign;
  float custom_y_sign;
  float camera_x;
  float camera_y;
  float camera_zoom;
} GameWorldConfig;
```

This struct does not store rendered pixels.

It stores how world space should be interpreted for this frame.

### `coord_origin`

Which coordinate convention is active.

### `custom_*`

Override data for the fully custom convention.

### `camera_x`, `camera_y`, `camera_zoom`

The current view transform that should affect world rendering.

So `GameWorldConfig` is the runtime's coordinate-policy package.

## Step 6: Notice the Safe Default Initialization Path

In `platform_game_props_init(...)` inside `src/platform.h`, the platform fills the shared world config like this:

```c
props->world_config.coord_origin = COORD_ORIGIN_LEFT_BOTTOM;
props->world_config.camera_zoom = 1.0f;
props->world_config.camera_x = 0.0f;
props->world_config.camera_y = 0.0f;
```

That gives the runtime a safe baseline meaning:

```text
bottom-left origin
no pan
no zoom distortion
```

This matters because later scene code can start from a coherent default instead of relying on accidental zero-init behavior for zoom.

## Step 7: Scene Camera State Can Override the Shared Default

The shared `PlatformGameProps.world_config` is not the final word for every draw.

In `src/game/main.c`, world rendering uses:

```c
static RenderContext game_make_world_context(Backbuffer *backbuffer,
                                             GameWorldConfig world_config,
                                             const GameCamera *camera) {
  GameWorldConfig config = world_config;
  if (camera) {
    config.camera_x = camera->x;
    config.camera_y = camera->y;
    config.camera_zoom = camera->zoom;
  }
  return make_render_context(backbuffer, config);
}
```

This is a very important design choice.

It means:

```text
the platform provides the default coordinate policy
the active scene camera can inject the current view state at render time
```

So the runtime keeps a shared baseline while still allowing scene-owned camera behavior.

## Visual: Where World Meaning Comes From

```text
platform_game_props_init
  -> default world_config

active scene camera
  -> current pan/zoom

game_make_world_context
  -> combine both into one render-time config
```

That is the full ownership story for world meaning on the live branch.

## Step 8: Keep Three Layers Separate

This module gets easier if you keep these layers distinct from the start:

```text
pixel space
  -> where the backbuffer stores output

world space
  -> where gameplay positions and sizes make sense

config/camera space
  -> the policy that explains how world space should be interpreted right now
```

That separation is the beginner runway for the rest of Module 06.

## Practical Exercises

### Exercise 1: Compute Base Pixels Per Unit

Using the live constants, compute the base pixels-per-unit ratio on both axes.

Expected answer:

```text
800 / 16 = 50
600 / 12 = 50
```

### Exercise 2: Explain the Macro

Why is `HUD_TOP_Y(...)` useful in a bottom-left world?

Expected answer:

```text
because humans often describe HUD placement as distance from the top,
but the default world convention measures y upward from the bottom
```

### Exercise 3: Explain the Override Path

Why does `game_make_world_context(...)` copy `world_config` and then override camera fields?

Expected answer:

```text
so the shared coordinate policy can stay intact while the active scene camera injects the current view state for this frame
```

## Common Mistakes

### Mistake 1: Treating world dimensions as a duplicate of backbuffer dimensions

They solve different problems.

### Mistake 2: Thinking `HUD_TOP_Y(...)` is just a cosmetic convenience macro

It encodes a real bridge between top-relative HUD reasoning and bottom-left world coordinates.

### Mistake 3: Forgetting that `GameWorldConfig` is policy, not pixels

It describes how later conversion should happen. It is not the converted result.

## Troubleshooting Your Understanding

### “Why not just keep everything in pixels?”

Because the game wants a logical space for layout and camera reasoning that is not tied directly to one storage resolution.

### “Does the scene camera replace `GameWorldConfig`?”

No. It overrides only the view-related fields at render time. The broader coordinate policy still comes from the config.

## Recap

You now have the spatial meaning layer that the rest of the module depends on:

1. the backbuffer has pixel dimensions
2. the game has world-unit dimensions
3. `HUD_TOP_Y(...)` bridges top-anchored HUD reasoning into the default world convention
4. `GameWorldConfig` stores coordinate policy plus camera-related fields
5. scene camera state can override the shared default when building the world render context

## Next Lesson

Lesson 25 drills into the next missing piece:

```text
where is zero,
which direction is +x,
and which direction is +y?
```

That is the coordinate-origin and axis-sign layer built on top of `GameWorldConfig`.
