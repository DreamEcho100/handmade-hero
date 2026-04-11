# Lesson 03 -- Sprite Blitting and Atlas

## Observable Outcome
A single 20x20 slime frame is extracted from the 40x40 Slime.png atlas and drawn on screen with correct transparency. The purple background shows through the transparent pixels around the slime character. No pink or black halos appear around the sprite edges.

## New Concepts (max 3)
1. SpriteSheet struct -- atlas grid with rows, columns, and frame indexing
2. SpriteRect and spritesheet_frame_rect -- extracting sub-rectangles from an atlas
3. Alpha-test blitting (sprite_blit) vs. per-pixel alpha blending (sprite_blit_alpha)

## Prerequisites
- Lesson 02 (Sprite struct loaded from Slime.png via stb_image)

## Background

Pixel-art games rarely store one image per frame. Instead, all frames for a character are packed into a single atlas (spritesheet). Slime.png is a 40x40 image containing a 2x2 grid of 20x20 frames. Frame 0 (top-left) is the "flying up" pose, frame 1 (top-right) is "falling", and frames 2-3 (bottom row) are the two-frame ground walk cycle.

To draw a single frame, we need two things: the source rectangle within the atlas (which 20x20 region to read from) and a blitting function that copies those pixels to the backbuffer while handling transparency. We provide two blit strategies: `sprite_blit` (binary alpha test: alpha >= 128 draws, otherwise skip) and `sprite_blit_alpha` (full per-pixel blending). The alpha-test version is fast and sufficient for pixel art with hard edges. The blended version is needed for smooth semi-transparent effects like particles or UI overlays.

## Code Walkthrough

### Step 1: SpriteSheet and SpriteRect

The SpriteSheet wraps a loaded Sprite and adds grid metadata:

```c
typedef struct {
  Sprite sprite;    /* the atlas texture */
  int frame_w;      /* individual frame width */
  int frame_h;      /* individual frame height */
  int cols;         /* number of columns */
  int rows;         /* number of rows */
} SpriteSheet;

typedef struct {
  int x, y, w, h;
} SpriteRect;
```

### Step 2: spritesheet_init

Initialization computes the grid dimensions from the sprite size and frame size:

```c
void spritesheet_init(SpriteSheet *ss, Sprite sprite,
                      int frame_w, int frame_h) {
  ss->sprite = sprite;
  ss->frame_w = frame_w;
  ss->frame_h = frame_h;
  ss->cols = sprite.width / frame_w;
  ss->rows = sprite.height / frame_h;
}
```

For Slime.png (40x40 with 20x20 frames): cols=2, rows=2, giving us 4 frames indexed 0-3.

In player_init:

```c
spritesheet_init(&player->sheet, player->sprite_slime,
                 PLAYER_W, PLAYER_H);  /* 20, 20 */
```

### Step 3: spritesheet_frame_rect

Given a linear frame index, this function computes the pixel-space rectangle within the atlas:

```c
SpriteRect spritesheet_frame_rect(const SpriteSheet *ss,
                                  int frame_idx) {
  int col = frame_idx % ss->cols;
  int row = frame_idx / ss->cols;
  SpriteRect r = {
    .x = col * ss->frame_w,
    .y = row * ss->frame_h,
    .w = ss->frame_w,
    .h = ss->frame_h,
  };
  return r;
}
```

For frame 0: col=0, row=0, rect=(0,0,20,20) -- top-left of the atlas.
For frame 2: col=0, row=1, rect=(0,20,20,20) -- bottom-left, first walk frame.

### Step 4: sprite_blit -- alpha-test blitting

This is the workhorse blit function. For each source pixel, if alpha >= 128, write it to the backbuffer; otherwise skip. No multiplication, no blending -- just a branch per pixel:

```c
void sprite_blit(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                 int dst_x, int dst_y) {
  /* Clip source rect to sprite bounds */
  int sx0 = src.x < 0 ? 0 : src.x;
  int sy0 = src.y < 0 ? 0 : src.y;
  int sx1 = src.x + src.w;
  int sy1 = src.y + src.h;
  if (sx1 > sprite->width) sx1 = sprite->width;
  if (sy1 > sprite->height) sy1 = sprite->height;
  /* ... clip destination to backbuffer bounds ... */

  for (int row = 0; row < draw_h; row++) {
    uint32_t *dst_row = bb->pixels + (dy0 + row) * bb_pitch + dx0;
    const uint32_t *src_row =
        sprite->pixels + (sy0 + clip_top + row) * sp_pitch
                       + sx0 + clip_left;

    for (int col = 0; col < draw_w; col++) {
      uint32_t pixel = src_row[col];
      unsigned int a = (pixel >> 24) & 0xFF;
      if (a >= 128) {
        dst_row[col] = pixel;
      }
    }
  }
}
```

The clipping logic handles four cases: sprite partially off the left edge, top edge, right edge, or bottom edge of the backbuffer. This is critical because the player can be at the screen boundary.

### Step 5: sprite_blit_alpha -- smooth blending

For effects that need semi-transparency (particles, shield glow), we use full per-pixel alpha blending:

```c
void sprite_blit_alpha(Backbuffer *bb, const Sprite *sprite,
                       SpriteRect src, int dst_x, int dst_y) {
  /* ... clipping identical to sprite_blit ... */

  for (int col = 0; col < draw_w; col++) {
    uint32_t sp = src_row[col];
    unsigned int sa = (sp >> 24) & 0xFF;

    if (sa == 0) continue;        /* fully transparent */
    if (sa == 255) {               /* fully opaque -- fast path */
      dst_row[col] = sp;
      continue;
    }

    /* Per-channel blend: out = src*a/255 + dst*(255-a)/255 */
    uint32_t dp = dst_row[col];
    unsigned int inv_a = 255 - sa;
    unsigned int or = (sr * sa + dr * inv_a) / 255;
    unsigned int og = (sg * sa + dg * inv_a) / 255;
    unsigned int ob = (sb * sa + db * inv_a) / 255;
    unsigned int oa = sa + (da * inv_a) / 255;

    dst_row[col] = (oa << 24) | (or << 16) | (og << 8) | ob;
  }
}
```

The fast paths (sa==0 skip, sa==255 direct write) avoid the expensive per-channel multiply for the majority of pixels in pixel art, where most pixels are either fully opaque or fully transparent.

### Step 6: Drawing a single frame

In player_render, we extract a frame rect and blit it:

```c
int frame = 0;  /* flying-up pose */
SpriteRect rect = spritesheet_frame_rect(&player->sheet, frame);
sprite_blit(bb, &player->sprite_slime, rect, screen_x, screen_y);
```

This draws only the 20x20 region of Slime.png corresponding to frame 0.

## Common Mistakes

**Using sprite_blit_alpha everywhere.** The per-channel multiply in the blending loop is significantly slower than the alpha-test branch. For pixel art with hard transparency edges (like character sprites), sprite_blit is the right choice. Reserve sprite_blit_alpha for actual semi-transparent effects.

**Off-by-one in frame indexing.** Frame indices are 0-based and laid out left-to-right, top-to-bottom. In a 2-column sheet, frame 2 is at (col=0, row=1), not (col=2, row=0). If your atlas has more columns than you expect, frames will appear shifted.

**Forgetting to clip to backbuffer bounds.** Without the clipping logic, a sprite at x=-5 would write to negative memory indices, corrupting the heap. The clipping code in sprite_blit adjusts both the source and destination rectangles so only the visible portion is processed.

**Pitch vs. width confusion.** The backbuffer uses `bb->pitch / 4` (bytes per row divided by bytes per pixel) for row stride, not `bb->width`. These are usually equal, but using pitch is correct by construction and handles any future padding.
