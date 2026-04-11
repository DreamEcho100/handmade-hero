# Lesson 07 -- Parallax Background

## Observable Outcome
A tiled background image (Background.png, 342x180) scrolls behind the gameplay at 30% of the camera's speed. The background tiles seamlessly -- no gaps or visible seams appear as the camera scrolls forward. The slower scroll speed creates a depth illusion, making the background feel distant while the foreground (player, floor, obstacles) moves at full speed.

## New Concepts (max 3)
1. Parallax scrolling -- background moves at a fraction of camera speed to simulate depth
2. Tiling calculation with modulo arithmetic for seamless wrapping
3. Fallback rendering when an asset fails to load

## Prerequisites
- Lesson 06 (camera follow and world-to-screen conversion)
- Lesson 03 (sprite_blit for drawing image data to the backbuffer)

## Background

Parallax scrolling is one of the oldest tricks in 2D games. By scrolling the background layer slower than the foreground, you create an illusion of depth -- distant objects appear to move less than nearby ones, mimicking real-world perspective. The technique dates back to arcade games of the 1980s and remains standard in modern 2D games.

Our background image (Background.png) is 342 pixels wide and 180 pixels tall -- exactly the height of our canvas but wider than the 320-pixel viewport. The extra width means the tile seam is never centered on screen. To tile seamlessly, we compute a starting x position based on the parallax offset, then draw enough copies side-by-side to cover the entire viewport width.

The parallax factor of 0.3 means the background scrolls at 30% of camera speed. When the camera has moved 1000 pixels, the background has moved only 300 pixels. This ratio was chosen to match the original Godot game's parallax layer settings.

The render_background function also handles the case where Background.png fails to load. Rather than crashing or showing garbage pixels, it falls back to a solid dark purple fill. This defensive pattern is important for development: you can test gameplay before all assets are ready.

## Code Walkthrough

### Step 1: Background constants

Defined in game/main.c alongside the render code:

```c
#define BG_TILE_W 342
#define BG_PARALLAX_FACTOR 0.3f
```

The background sprite is stored in GameState and loaded in game_init:

```c
typedef struct {
  /* ... */
  Sprite bg_sprite;
  float bg_scroll_offset;
  /* ... */
} GameState;

void game_init(GameState *state, PlatformGameProps *props) {
  /* ... */
  if (sprite_load("assets/sprites/Background.png",
                   &state->bg_sprite) != 0) {
    fprintf(stderr, "WARNING: Failed to load Background.png\n");
  }
}
```

Note the `WARNING:` prefix and the fact that this is not a fatal error. The game continues without a background.

### Step 2: The render_background function

This is the complete parallax rendering implementation:

```c
static void render_background(GameState *state, Backbuffer *bb) {
  if (!state->bg_sprite.pixels) {
    /* Fallback: solid color if no background loaded */
    draw_rect(bb, 0.0f, 0.0f, (float)bb->width, (float)bb->height,
              0.078f, 0.047f, 0.110f, 1.0f);
    return;
  }

  /* Parallax: background scrolls at 30% of camera speed */
  float parallax_offset = state->camera.x * BG_PARALLAX_FACTOR;

  /* Calculate the starting tile position */
  int tile_w = state->bg_sprite.width;
  int start_x = -((int)parallax_offset % tile_w);
  if (start_x > 0) start_x -= tile_w;

  /* Draw enough tiles to cover the screen */
  SpriteRect full = {0, 0, state->bg_sprite.width,
                     state->bg_sprite.height};
  for (int tx = start_x; tx < bb->width; tx += tile_w) {
    sprite_blit(bb, &state->bg_sprite, full, tx, 0);
  }
}
```

### Step 3: Understanding the parallax offset

```c
float parallax_offset = state->camera.x * BG_PARALLAX_FACTOR;
```

If the camera is at x=1000, the parallax offset is 300. This means the background has scrolled 300 pixels to the left, as if it were a layer 3x farther from the viewer than the foreground.

### Step 4: Tiling with modulo arithmetic

The key to seamless tiling is computing where the first tile starts:

```c
int start_x = -((int)parallax_offset % tile_w);
if (start_x > 0) start_x -= tile_w;
```

Breaking this down:
- `(int)parallax_offset % tile_w` gives the offset within one tile width (0 to 341)
- Negating it gives the screen x where the current tile's left edge should be
- The `if (start_x > 0)` guard handles the edge case where the modulo returns 0 (start_x would be 0, which is correct) or a positive value due to integer division quirks with negative numbers. Subtracting tile_w ensures we always start at or before the left edge of the screen.

### Step 5: Drawing the tile strip

```c
for (int tx = start_x; tx < bb->width; tx += tile_w) {
  sprite_blit(bb, &state->bg_sprite, full, tx, 0);
}
```

Starting from start_x (which is <= 0), we draw tiles at 342-pixel intervals until we pass the right edge of the backbuffer (320 pixels). With a 342-pixel-wide tile:
- If start_x = -100: draw at -100, 242. Two tiles cover the 320-pixel viewport.
- If start_x = -341: draw at -341, 1. Two tiles, with the seam barely visible at x=1.

The SpriteRect `full` covers the entire background image -- we always blit the whole 342x180 image.

### Step 6: The y=0 placement

The background is drawn at y=0 (top of screen). Since the image is 180 pixels tall and the canvas is 180 pixels tall, it fills the entire vertical extent. There is no vertical parallax in this game.

### Step 7: Render order in game_render

The background is drawn first (layer 0), before everything else:

```c
void game_render(GameState *state, Backbuffer *backbuffer, ...) {
  /* 1. Background (parallax) */
  render_background(state, backbuffer);

  /* 2. Floor */
  render_floor(backbuffer);

  /* 3. Obstacles */
  obstacles_render(...);

  /* 4. Player */
  player_render(...);

  /* 5. Particles */
  /* 6. HUD */
}
```

Each subsequent layer draws over the previous one. The floor band (a dark purple rectangle at y=165) covers the bottom of the background, and the player sprite draws over everything.

## Common Mistakes

**Using camera.x directly instead of camera.x * factor.** Without the parallax factor, the background scrolls at the same speed as the foreground, destroying the depth illusion. The background would appear bolted to the floor and player.

**Integer truncation in the modulo.** The cast `(int)parallax_offset` truncates rather than rounds. This is correct -- we want the integer pixel offset for tile placement. But if you accidentally use floating-point modulo (`fmodf`), the result is a float that may not align to pixel boundaries, causing sub-pixel shimmer.

**Drawing only one tile.** If start_x is -341, a single 342-pixel tile covers only x=-341 to x=1. The remaining 319 pixels would show whatever was previously in the backbuffer. The loop ensures we always draw enough tiles to fill the screen width.

**Calling render_background after render_floor.** The background must be the first thing drawn because it covers the entire screen. If drawn after the floor, it would overwrite the floor band. Painter's algorithm: back to front.

**Not handling the null sprite.** If Background.png fails to load, `bg_sprite.pixels` is NULL. Without the null check, sprite_blit would dereference a null pointer and crash. The fallback draw_rect keeps the game running.
