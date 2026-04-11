# Lesson 11: `draw_text` and Debug Labels

## Frontmatter

- Module: `03-pixel-pipeline-and-drawing`
- Lesson: `11`
- Status: Required
- Target files:
  - `src/utils/draw-text.c`
  - `src/game/main.c`
- Target symbols:
  - `draw_text`
  - `draw_char`
  - `snprintf`
  - `game_draw_world_label`

## Observable Outcome

By the end of this lesson, you should be able to explain how runtime values become visible debug labels on screen.

You should be able to trace the full chain:

```text
numeric or gameplay state
  -> formatted into a C string with snprintf
  -> draw_text walks the string character by character
  -> draw_char renders each glyph
  -> draw_rect writes the glyph blocks into the backbuffer
  -> backend presents the final image
```

## Prerequisite Bridge

Lesson 10 explained how one character becomes pixels.

The next step is straightforward but essential:

```text
how do whole strings become runtime overlays and labels?
```

This lesson answers that by connecting `draw_text(...)` to real game-side call sites.

## Why This Lesson Exists

Debug UI is not decoration in this course.

It is part of how you inspect a live runtime.

If you cannot explain how labels reach the screen, then debugging information still feels like magic.

This lesson removes that magic by showing that debug text is just:

1. string formatting
2. character iteration
3. glyph rendering
4. backbuffer presentation

## Step 1: Understand the Job of `draw_text(...)`

At a high level, `draw_text(...)` does one simple job:

```text
start at a cursor position
walk through the string one character at a time
draw each character
advance the cursor
```

That means `draw_text(...)` is primarily a layout loop, not a new rasterizer.

Its core dependency is still `draw_char(...)`.

## Step 2: The Character Loop Is the Whole Idea

Conceptually, the function behaves like this:

```c
for (const char *at = text; *at; ++at) {
  draw_char(backbuffer, cursor_x, cursor_y, *at, scale, r, g, b, a);
  cursor_x += advance;
}
```

That is the string-rendering model in its simplest form.

Each loop iteration answers two questions:

1. what glyph should be drawn now?
2. where should the next glyph start?

## Step 3: Cursor Advance Creates a Readable String

If every character were drawn at the same `x`, the string would collapse into an unreadable pile.

So `draw_text(...)` keeps a horizontal cursor and advances it after each character.

Conceptually:

```text
start at x = initial_x
draw first glyph
move right by character advance
draw second glyph
move right again
...
```

The advance is usually tied to the font cell width and the current scale.

For an `8 x 8` bitmap font, a common mental model is:

```text
advance = glyph_width * scale
```

Optionally plus small spacing.

## Visual: One String as Cursor Motion

```text
cursor start -> [H][E][L][L][O]
                ^  ^  ^  ^  ^
                draw_char called once per glyph
```

This is deliberately simple.

Readable string rendering starts with stable cursor movement, not complicated typography.

## Step 4: The Real Runtime Usually Starts With `snprintf(...)`

In `src/game/main.c`, many labels are not constant strings.

They are built from live state using `snprintf(...)`.

That means the full process often starts before `draw_text(...)` is even called:

```c
char buffer[128];
snprintf(buffer, sizeof(buffer), "player x: %.2f", player_x);
draw_text(backbuffer, x, y, buffer, scale, r, g, b, a);
```

This matters because it clarifies the separation of responsibilities:

`snprintf(...)`

- turns numbers and variables into a printable C string

`draw_text(...)`

- turns that already-formed string into pixels

## Step 5: Debug Labels Are Just Data Flow

Compress the whole thing into this diagram:

```text
runtime value
   |
   v
snprintf builds text
   |
   v
draw_text walks characters
   |
   v
draw_char draws glyphs
   |
   v
draw_rect paints blocks
   |
   v
backbuffer now contains the label
```

This is one of the best diagrams in the whole module to memorize.

It shows that debug UI is not a separate world.

It is ordinary data flowing through the same drawing pipeline.

## Step 6: `game_draw_world_label(...)` Is a Good Bridge Example

The runtime includes a helper like `game_draw_world_label(...)` that places text in the world or near world objects.

That is valuable because it shows text is not limited to fixed HUD coordinates.

The conceptual flow is:

```text
world-space thing needs a label
  -> compute or choose screen position
  -> format text if needed
  -> call draw_text at that screen position
```

So the renderer does not care whether the text is a HUD label, a debug stat, or a world annotation.

It only sees:

```text
string + position + scale + color
```

## Worked Example: One Runtime Stat Label

Suppose the game wants to display the player's speed.

### Step 1: Format the value

```c
char label[64];
snprintf(label, sizeof(label), "speed: %.2f", speed);
```

If `speed == 3.50f`, then `label` becomes:

```text
"speed: 3.50"
```

### Step 2: Render the string

```c
draw_text(backbuffer, 20.0f, 20.0f, label, 2, 1.0f, 1.0f, 1.0f, 1.0f);
```

### Step 3: What actually happens internally

```text
's' -> draw_char -> several draw_rect calls
'p' -> draw_char -> several draw_rect calls
'e' -> draw_char -> several draw_rect calls
...
'0' -> draw_char -> several draw_rect calls
```

So one visible label is really the accumulation of many small rectangle writes.

## Step 7: Why This Renderer Is Good for Debug Text

This text system is limited compared to a real UI toolkit, but those limitations are useful here.

It is:

- deterministic
- easy to inspect
- easy to scale
- cheap to reason about
- fully integrated with the course's software renderer

That makes it excellent for debug overlays and instrumentation.

## Step 8: What `draw_text(...)` Does Not Do

Being clear about non-goals is important.

This text system does not attempt:

- kerning
- proportional fonts
- Unicode shaping
- rich layout
- line wrapping engines

That is intentional.

The course needs text as an observability tool, not as a full typography subsystem.

## Step 9: The Full Composition Ladder for the Whole Module

At this point, Module 03 has built a complete ladder:

```text
Backbuffer memory contract
  -> backend upload/present path
  -> draw_rect primitive
  -> draw_char composition
  -> draw_text composition
  -> runtime debug labels
```

That is a major milestone.

You are no longer dealing with isolated helpers.

You are looking at a coherent pixel pipeline.

## Practical Exercises

### Exercise 1: Trace a Formatted Label by Hand

Take this code mentally:

```c
char text[64];
snprintf(text, sizeof(text), "hp: %d", 42);
draw_text(backbuffer, 10, 10, text, 1, 1, 1, 1, 1);
```

Write the pipeline in your own words from integer `42` to visible pixels.

Expected structure:

```text
42 -> formatted into "hp: 42" -> characters iterated -> glyphs drawn -> rectangles written -> presented frame
```

### Exercise 2: Predict Cursor Positions

Assume:

```text
start_x = 50
scale = 2
glyph cell width = 8
```

If the renderer uses `advance = 8 * scale = 16`, what are the `x` positions for the four characters in `"ABCD"`?

Expected result:

```text
A -> 50
B -> 66
C -> 82
D -> 98
```

### Exercise 3: Explain Why Labels Inherit Clipping

Why is it safe for text near the edge of the screen to be partially visible instead of crashing the renderer?

Expected answer:

```text
draw_text uses draw_char, draw_char uses draw_rect, and draw_rect performs clipping against the backbuffer bounds
```

## Common Mistakes

### Mistake 1: Thinking `snprintf(...)` draws text

It does not.

It only creates the string content.

### Mistake 2: Thinking `draw_text(...)` knows how to render whole words directly

It does not.

It renders one character at a time by repeatedly calling `draw_char(...)`.

### Mistake 3: Thinking debug UI is separate from the renderer

It is not.

Debug labels are just another client of the same pixel pipeline.

## Troubleshooting Your Understanding

### “I understand `draw_text(...)`, but the runtime labels still feel indirect”

Look for the nearest `snprintf(...)` call before the `draw_text(...)` call.

That pairing usually makes the data flow obvious.

### “Why does the course spend so much time on debug labels?”

Because real engine and platform work depends heavily on trustworthy runtime instrumentation.

Being able to render internal state is part of the engineering workflow, not an optional extra.

## Recap

You now have the complete text path:

1. runtime data is formatted into a C string
2. `draw_text(...)` iterates through the string
3. `draw_char(...)` renders each glyph from font bits
4. `draw_rect(...)` writes the actual pixel blocks
5. the backends present the final labeled image

## Module Bridge

Module 03 has now established the full beginner rendering pipeline for this course:

- memory layout
- presentation bridge
- rectangle rasterization
- glyph rasterization
- string rendering
- runtime debug labels

That gives the later modules a concrete visual instrumentation layer to build on.
