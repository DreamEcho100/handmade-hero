# Lesson 27: `TextCursor`, HUD Anchors, and Flowing Text Layout

## Frontmatter

- Module: `06-coordinate-systems-and-camera`
- Lesson: `27`
- Status: Required
- Target files:
  - `src/utils/render.h`
  - `src/game/main.c`
  - `src/game/demo_explicit.c`
- Target symbols:
  - `TextCursor`
  - `make_cursor`
  - `cursor_newline`
  - `cursor_gap`
  - `game_draw_hud`
  - `text_scale`
  - `line_height`

## Observable Outcome

By the end of this lesson, you should be able to explain how one anchored HUD position becomes a readable multi-line panel without rerunning world-to-pixel conversion for every line of text.

You should also be able to explain why the cursor stores pixel-space state rather than world-space state once the HUD flow has started.

## Prerequisite Bridge

Lesson 26 baked coordinate policy into a `RenderContext`.

The next question is practical:

```text
how do we spend that baked math for text layout
without hard-coding every y position by hand?
```

The course answers that with a deliberately tiny helper: `TextCursor`.

## Why This Lesson Exists

Once text rendering exists, a new layout problem appears immediately:

```text
how do I draw a column of related text
without recomputing positions or scattering magic y values everywhere?
```

The runtime does not solve this with a full UI system.

It solves it with a small “moving pen” abstraction in pixel space.

That is enough for the current HUD while keeping the data flow easy to teach.

## Step 1: Read `TextCursor` as a Pixel-Space Pen

In `src/utils/render.h`:

```c
typedef struct TextCursor {
  float x;
  float y;
  float scale;
  float line_h;
} TextCursor;
```

This struct is intentionally small.

It stores:

- the current pixel-space position
- the text scale to pass into `draw_text(...)`
- the vertical distance for one standard line advance

That is all the HUD needs.

## Step 2: Separate Anchor Space From Flow Space

This lesson gets much easier if you treat it as two phases.

### Anchor phase

```text
choose one starting position using world-aware render-context math
```

### Flow phase

```text
after the start is known, step in pixels line by line
```

That is the whole design move:

```text
convert once
flow many times
```

## Step 3: Read `make_cursor(...)` as a One-Time Spend of the Render Context

The live helper is:

```c
static inline TextCursor make_cursor(const RenderContext *ctx, float wx,
                                     float wy) {
  return (TextCursor){
      .x = (ctx->x_offset + ctx->x_sign * wx) * ctx->world_to_px_x,
      .y = (ctx->y_offset + ctx->y_sign * wy) * ctx->world_to_px_y,
      .scale = ctx->text_scale,
      .line_h = ctx->line_height,
  };
}
```

This helper spends the world-to-pixel math exactly once at the starting anchor.

After that, the cursor carries the already-converted result forward.

That is why the HUD does not need to rebuild the world conversion for every new line.

## Worked Example: One Anchor Conversion

Suppose the HUD chooses a start anchor at `(0.7, 8.9)` in HUD-world space.

`make_cursor(...)` converts that once into:

- one pixel `x`
- one pixel `y`
- one text scale
- one line height

From then on, the layout logic only has to update the cursor's pixel `y`.

That is the entire payoff of the helper.

## Step 4: Read the Movement Helpers as Intentionally Tiny

The follow-up helpers are:

```c
static inline void cursor_newline(TextCursor *c) { c->y += c->line_h; }

static inline void cursor_gap(TextCursor *c, float mul) {
  c->y += c->line_h * mul;
}
```

### `cursor_newline(...)`

Advance by one standard line.

### `cursor_gap(...)`

Advance by a fraction or multiple of a line.

That is a very pragmatic interface for a course HUD.

It does not try to be a generic UI library.

## Step 5: Why the Cursor Stores Pixels, Not World Units

Once text starts flowing line by line, pixel space is the right coordinate system.

Why?

Because the layout questions are now about:

- baseline spacing
- small readable gaps
- screen-stable panel placement

Those are pixel-layout concerns, not gameplay-space concerns.

So the design is:

```text
use world-aware math to choose the starting anchor
then stay in pixel space for the text flow itself
```

## Step 6: Follow the Live HUD Flow in `game_draw_hud(...)`

In `src/game/main.c`, the HUD starts with:

```c
text = make_cursor(hud, 0.7f, 8.9f);
```

Then it draws and steps repeatedly:

```c
draw_text(... game_scene_name(...), COLOR_WHITE);
text.y += (float)GLYPH_H * hud->text_scale * 1.8f;

draw_text(... game_scene_summary(...), COLOR_CYAN);
cursor_newline(&text);

draw_text(... game_scene_action_hint(...), COLOR_YELLOW);
cursor_newline(&text);
```

This shows the intended workflow clearly:

1. choose one anchor
2. draw one line
3. move the cursor
4. draw the next line
5. repeat

Sometimes the code uses a larger custom jump for a title. After that, it falls back to regular cursor stepping.

That mix is exactly what a tiny helper like this should support.

## Step 7: Notice the HUD Uses a HUD Context, Not the World Context

The HUD path in `src/game/main.c` uses a separate helper:

```c
static RenderContext game_make_hud_context(Backbuffer *backbuffer,
                                           GameWorldConfig world_config) {
  GameWorldConfig config = world_config;
  config.camera_x = 0.0f;
  config.camera_y = 0.0f;
  config.camera_zoom = 1.0f;
  return make_render_context(backbuffer, config);
}
```

That is a major architectural point.

It means the cursor's starting anchor still uses world-style units, but the HUD context deliberately strips out camera motion and zoom.

So:

```text
world content can move
the HUD panel stays pinned and readable
```

Without that split, the HUD would drift and scale with the scene camera.

## Step 8: Use `demo_explicit.c` as the Minimal Teaching Example

The retained `demo_explicit.c` path still shows the cursor pattern very clearly:

```c
TextCursor text = make_cursor(&hud, 1.0f, 9.0f);
...
cursor_newline(&text);
cursor_gap(&text, 0.4f);
```

That file is useful because it shows the same idea in a smaller, less scene-heavy setting than the main runtime HUD.

## Visual: One HUD Panel as Cursor Motion

```text
anchor -> first line
          |
          v
     cursor_newline
          |
          v
      second line
          |
          v
       cursor_gap
          |
          v
      grouped section
```

This is all the current HUD really is: anchored pixel-space cursor motion plus repeated `draw_text(...)` calls.

## Practical Exercises

### Exercise 1: Explain the Two Phases

Without looking back, explain the difference between anchor space and flow space.

Target answer:

```text
anchor space uses render-context conversion to choose the starting point;
flow space keeps moving that already-converted point in pixels line by line
```

### Exercise 2: Explain the HUD Context

Why does `game_make_hud_context(...)` zero camera pan and force `camera_zoom = 1.0f`?

Expected answer:

```text
so HUD text stays screen-stable instead of moving and scaling with the world camera
```

### Exercise 3: Explain the Value of `TextCursor`

Why is a cursor helper better than hard-coding every text line's `y` coordinate?

Expected answer:

```text
because the panel can be anchored once and then laid out consistently by stepping a small reusable cursor state forward
```

## Common Mistakes

### Mistake 1: Thinking the cursor should keep world coordinates forever

Once layout starts, pixel space is the more useful representation.

### Mistake 2: Forgetting that the HUD should ignore camera motion

That is why a separate HUD context exists.

### Mistake 3: Treating `cursor_gap(...)` as redundant

It provides lightweight grouping without forcing custom pixel constants everywhere.

## Troubleshooting Your Understanding

### “Why not just call `world_x(...)` and `world_y(...)` for every line?”

Because once the starting point is known, line spacing is a screen-layout concern, not a world-layout concern.

### “Why is the title jump sometimes manual instead of always using `cursor_gap(...)`?”

Because the title uses a larger scale and therefore wants a slightly different visual spacing than the standard line height.

## Recap

You now understand the HUD text flow model:

1. use `make_cursor(...)` once to convert a start anchor into pixel space
2. store pixel position, text scale, and line height in `TextCursor`
3. lay out the panel with `cursor_newline(...)` and `cursor_gap(...)`
4. build HUD layout against a camera-stripped HUD render context so the overlay stays stable

## Next Lesson

Lesson 28 returns to world drawing and makes the world-to-pixel boundary even more explicit through helpers like `world_x(...)`, `world_y(...)`, and `world_rect_px_x(...)` so draw sites visibly show where world meaning becomes pixel math.
