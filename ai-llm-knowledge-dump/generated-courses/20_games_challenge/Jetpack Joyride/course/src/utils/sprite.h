#ifndef UTILS_SPRITE_H
#define UTILS_SPRITE_H

/* ═══════════════════════════════════════════════════════════════════════════
 * utils/sprite.h — Jetpack Joyride (SlimeEscape)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Sprite loading (PNG via stb_image), blitting with alpha, spritesheet
 * atlas frames, and frame-based animation.
 *
 * LESSON 02 — Sprite struct, sprite_load, sprite_free, sprite_blit_raw.
 * LESSON 03 — SpriteSheet, spritesheet_frame_rect, sprite_blit (alpha).
 * LESSON 08 — SpriteAnimation, anim_update, anim_draw.
 * LESSON 11 — sprite_blit_rotated (3-shear Paeth rotation).
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdint.h>
#include "./backbuffer.h"

/* ── Sprite ──────────────────────────────────────────────────────────── */
/* A loaded image. Pixels are RGBA uint32_t in GAME_RGBA format.
 * Owned by stb_image; freed with sprite_free → stbi_image_free.         */
typedef struct {
  uint32_t *pixels; /* RGBA pixel data (owned, from stb_image) */
  int width;        /* image width in pixels                    */
  int height;       /* image height in pixels                   */
} Sprite;

/* ── SpriteSheet ─────────────────────────────────────────────────────── */
/* A sprite atlas: grid of equal-size frames.
 * frame_w * cols = sprite.width; frame_h * rows = sprite.height.        */
typedef struct {
  Sprite sprite;   /* the atlas texture */
  int frame_w;     /* individual frame width */
  int frame_h;     /* individual frame height */
  int cols;        /* number of columns */
  int rows;        /* number of rows */
} SpriteSheet;

/* ── SpriteRect ──────────────────────────────────────────────────────── */
/* Source rectangle within a sprite (pixel coordinates). */
typedef struct {
  int x, y, w, h;
} SpriteRect;

/* ── SpriteAnimation ─────────────────────────────────────────────────── */
/* Frame-based animation driven by elapsed time.
 * Tracks which frame to display; caller draws it via anim_draw.         */
typedef struct {
  SpriteSheet *sheet;     /* which spritesheet to animate     */
  int start_frame;        /* first frame index in the sheet   */
  int frame_count;        /* number of frames in this anim    */
  float fps;              /* playback speed (frames/second)   */
  float elapsed;          /* accumulated time since last frame */
  int current_frame;      /* current frame index (0-based)    */
  int loop;               /* 1 = loop, 0 = play-once         */
  int finished;           /* 1 if play-once completed         */
} SpriteAnimation;

/* ── API ─────────────────────────────────────────────────────────────── */

/* Load a PNG/BMP/TGA file into a Sprite. Returns 0 on success, -1 on failure.
 * The sprite's pixels are allocated by stb_image (heap); free with sprite_free. */
int sprite_load(const char *path, Sprite *out);

/* Free a sprite's pixel data. */
void sprite_free(Sprite *s);

/* Blit a sub-rectangle of a sprite onto the backbuffer with alpha blending.
 * dst_x, dst_y: destination position in pixel coordinates (top-left).
 * src: source rectangle within the sprite.
 * Clips to backbuffer bounds. Alpha < 128 → skip; >= 128 → draw.       */
void sprite_blit(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                 int dst_x, int dst_y);

/* Blit with full per-pixel alpha blending (slower but higher quality).
 * Uses the alpha channel [0-255] for smooth transparency.               */
void sprite_blit_alpha(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                       int dst_x, int dst_y);

/* Blit a sprite sub-rectangle rotated around its center.
 * Uses 3-shear (Paeth) rotation for pixel-perfect results.
 * angle: rotation angle in radians.                                     */
void sprite_blit_rotated(Backbuffer *bb, const Sprite *sprite, SpriteRect src,
                         int dst_cx, int dst_cy, float angle);

/* Get the source rectangle for a frame index in a spritesheet. */
SpriteRect spritesheet_frame_rect(const SpriteSheet *ss, int frame_idx);

/* Initialize a spritesheet from an already-loaded sprite. */
void spritesheet_init(SpriteSheet *ss, Sprite sprite, int frame_w, int frame_h);

/* Initialize a SpriteAnimation. */
void anim_init(SpriteAnimation *anim, SpriteSheet *sheet,
               int start_frame, int frame_count, float fps, int loop);

/* Advance the animation by dt seconds. */
void anim_update(SpriteAnimation *anim, float dt);

/* Reset animation to first frame. */
void anim_reset(SpriteAnimation *anim);

/* Draw the current animation frame at (dst_x, dst_y). */
void anim_draw(const SpriteAnimation *anim, Backbuffer *bb,
               int dst_x, int dst_y);

#endif /* UTILS_SPRITE_H */
